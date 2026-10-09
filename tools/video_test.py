#!/usr/bin/env python3
"""Run the animal detector's model and decision logic on video files, without a camera.

For each video the script
  1. decodes frames with ffmpeg at a fixed frame rate and squashes every frame to the model's 300x300
     input, like the app does in its default (whole image) mode,
  2. runs the same SSD MobileNet v2 (COCO) model on the CPU (ai-edge-litert),
  3. applies the same rules as the app: only the configured animal classes, the smallest-animal filter
     (MinAnimalPct) and then the app's own tracker (app/animal_events.c, app/animal_any.c, compiled from
     the repository and called through ctypes) with Threshold, StartFrames and HoldSec,
  4. prints the events the camera would have sent ("start"/"stop" per species and the "any animal" state).

Differences to the camera (so the results are close, not identical):
  - the camera's DLPU runs the same quantised model but not bit for bit; scores can differ a little,
  - the camera takes frames from its raw sensor stream, a video file is compressed and scaled by ffmpeg,
  - the camera's frame rate depends on the speed of its analysis; here it is --fps (StartFrames counts
    frames, so it matters),
  - region mode (small crops around objects found by the camera's analytics) is not reproduced.

Setup (once):
  python3 -m venv ~/.venvs/animal-video-test
  ~/.venvs/animal-video-test/bin/pip install ai-edge-litert numpy pillow
Needs ffmpeg and a C compiler (cc) on the PATH. Run with that interpreter:
  ~/.venvs/animal-video-test/bin/python tools/video_test.py my_video.mp4 --threshold 50

The model and the label file are not in the repository; take them from build/ after a build
(build/model/converted_model.tflite, build/label/labels.txt) or from the unpacked .eap, and pass them
with --model / --labels if they are somewhere else.
"""
import argparse
import ctypes
import math
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parent.parent
DEFAULT_CLASSES = "bird, cat, dog, horse, sheep, cow, elephant, bear, zebra, giraffe"
INPUT = 300  # the model's input size (width = height)


class Det(ctypes.Structure):  # animal_det_t
    _fields_ = [("label", ctypes.c_int), ("score", ctypes.c_float)]


class Evt(ctypes.Structure):  # animal_event_t
    _fields_ = [("start", ctypes.c_bool), ("label", ctypes.c_int),
                ("best_score", ctypes.c_float), ("duration_ms", ctypes.c_int64)]


def build_tracker_lib(workdir):
    """Compile the app's own tracker code (no camera dependencies) into a shared library."""
    lib = Path(workdir) / "libanimal.so"
    cmd = ["cc", "-O2", "-shared", "-fPIC", "-I", str(REPO / "app"), "-o", str(lib),
           str(REPO / "app" / "animal_events.c"), str(REPO / "app" / "animal_any.c")]
    subprocess.run(cmd, check=True)
    L = ctypes.CDLL(str(lib))
    L.animal_tracker_new.argtypes = [ctypes.POINTER(ctypes.c_bool), ctypes.c_int, ctypes.c_float,
                                     ctypes.c_int, ctypes.c_int]
    L.animal_tracker_new.restype = ctypes.c_void_p
    L.animal_tracker_free.argtypes = [ctypes.c_void_p]
    L.animal_tracker_update.argtypes = [ctypes.c_void_p, ctypes.POINTER(Det), ctypes.c_int,
                                        ctypes.c_int64, ctypes.POINTER(Evt), ctypes.c_int]
    L.animal_tracker_update.restype = ctypes.c_int
    L.animal_any_new.argtypes = [ctypes.c_int]
    L.animal_any_new.restype = ctypes.c_void_p
    L.animal_any_free.argtypes = [ctypes.c_void_p]
    L.animal_any_update.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_bool]
    L.animal_any_update.restype = ctypes.c_int
    return L


def read_labels(path):
    return [line.strip() for line in Path(path).read_text(encoding="utf-8").splitlines()]


def allowed_flags(labels, classes):
    """Like the app: names of the AnimalClasses setting, matched against the label file."""
    flags = (ctypes.c_bool * len(labels))()
    for name in [c.strip() for c in classes.split(",") if c.strip()]:
        if name in labels:
            flags[labels.index(name)] = True
        else:
            print(f"warning: animal class '{name}' is not in the label file", file=sys.stderr)
    return flags


def frames(video, fps, start, duration):
    """Yield (time in seconds, uint8 array 300x300x3) from ffmpeg."""
    cmd = ["ffmpeg", "-v", "error", "-nostdin"]
    if start:
        cmd += ["-ss", str(start)]
    if duration:
        cmd += ["-t", str(duration)]
    cmd += ["-i", str(video), "-vf", f"fps={fps},scale={INPUT}:{INPUT}:flags=bilinear",
            "-pix_fmt", "rgb24", "-f", "rawvideo", "-"]
    size = INPUT * INPUT * 3
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    i = 0
    while True:
        buf = proc.stdout.read(size)
        if len(buf) < size:
            break
        yield (start or 0) + i / fps, np.frombuffer(buf, np.uint8).reshape(INPUT, INPUT, 3)
        i += 1
    proc.stdout.close()
    if proc.wait() != 0:
        sys.exit(f"ffmpeg failed for {video}")


def stamp(t):
    return f"{int(t // 60):02d}:{t % 60:04.1f}"


def save_hit(directory, video, t, label, score, image, boxes):
    from PIL import Image, ImageDraw
    img = Image.fromarray(image)
    draw = ImageDraw.Draw(img)
    for (l, tp, r, b, name, sc) in boxes:
        draw.rectangle([l * INPUT, tp * INPUT, r * INPUT, b * INPUT], outline=(255, 0, 0), width=2)
        draw.text((l * INPUT + 3, tp * INPUT + 2), f"{name} {sc:.2f}", fill=(255, 255, 0))
    directory.mkdir(parents=True, exist_ok=True)
    name = f"{Path(video).stem}_{stamp(t).replace(':', 'm')}_{label}_{score:.2f}.jpg"
    img.save(directory / name, quality=90)


def run_video(video, args, interp, labels, allowed, lib):
    inp = interp.get_input_details()[0]
    out = sorted(interp.get_output_details(), key=lambda d: d["index"])
    if len(out) != 4:
        sys.exit("unexpected model: expected 4 output tensors (boxes, classes, scores, count)")
    threshold = args.threshold / 100.0
    min_size = args.min_animal_pct / 100.0
    tracker = lib.animal_tracker_new(allowed, len(labels), threshold, args.start_frames,
                                     int(args.hold_sec * 1000))
    any_ = lib.animal_any_new(len(labels))
    stats = {}  # label -> [episodes, frames_over_threshold, best score]
    best_frame = {}  # label -> (score, t, image, boxes) of the current episode
    events = (Evt * 16)()
    n_frames, last_t, active_any = 0, 0.0, 0
    print(f"\n=== {video}")
    for t, image in frames(video, args.fps, args.start, args.duration):
        n_frames, last_t = n_frames + 1, t
        interp.set_tensor(inp["index"], image[np.newaxis, ...])
        interp.invoke()
        boxes = interp.get_tensor(out[0]["index"])[0]
        classes = interp.get_tensor(out[1]["index"])[0]
        scores = interp.get_tensor(out[2]["index"])[0]
        number = int(interp.get_tensor(out[3]["index"])[0])
        dets, shown, seen = [], [], []
        for i in range(min(number, len(scores))):
            label = int(classes[i])
            if label < 0 or label >= len(labels):
                continue
            ymin, xmin, ymax, xmax = (float(x) for x in boxes[i])
            w, h = xmax - xmin, ymax - ymin
            size = math.sqrt(w * h) if w > 0 and h > 0 else 0.0
            animal = bool(allowed[label])
            too_small = animal and size < min_size
            if args.debug_threshold and animal and scores[i] >= args.debug_threshold / 100.0:
                seen.append(f"{labels[label]} {scores[i]:.2f}" + (" (too small)" if too_small else ""))
            if too_small:
                continue
            if animal:
                dets.append(Det(label, float(scores[i])))
                if scores[i] >= threshold:
                    shown.append((xmin, ymin, xmax, ymax, labels[label], float(scores[i])))
                    st = stats.setdefault(label, [0, 0, 0.0])
                    st[1] += 1
                    st[2] = max(st[2], float(scores[i]))
                    if label not in best_frame or scores[i] > best_frame[label][0]:
                        best_frame[label] = (float(scores[i]), t, image.copy(), list(shown))
        if seen and int(t * 1) != int((t - 1.0 / args.fps) * 1):  # at most about once a second
            print(f"  {stamp(t)}  seen: {', '.join(seen)}")
        arr = (Det * max(len(dets), 1))(*dets)
        count = lib.animal_tracker_update(tracker, arr, len(dets), int(t * 1000), events, len(events))
        for e in range(count):
            ev = events[e]
            name = labels[ev.label]
            if ev.start:
                stats.setdefault(ev.label, [0, 0, 0.0])[0] += 1
                print(f"  {stamp(t)}  START {name} (score {ev.best_score:.2f})")
                if args.save_hits:
                    save_hit(Path(args.save_hits), video, t, name, ev.best_score, image, shown)
            else:
                print(f"  {stamp(t)}  stop  {name} (seen for {ev.duration_ms / 1000:.1f} s, "
                      f"best score {ev.best_score:.2f})")
                if args.save_hits and ev.label in best_frame:
                    s, bt, img, bx = best_frame.pop(ev.label)
                    save_hit(Path(args.save_hits), video, bt, name + "_best", s, img, bx)
            change = lib.animal_any_update(any_, ev.label, bool(ev.start))
            if change:
                active_any += change
                print(f"  {stamp(t)}  ANY ANIMAL {'present' if change > 0 else 'gone'}")
    # episodes still running at the end of the video
    for label, (s, bt, img, bx) in best_frame.items():
        if args.save_hits:
            save_hit(Path(args.save_hits), video, bt, labels[label] + "_best", s, img, bx)
    lib.animal_tracker_free(tracker)
    lib.animal_any_free(any_)
    print(f"  {n_frames} frames analysed ({stamp(last_t)}, {args.fps:g} fps)")
    if not stats:
        print("  no animal at or above the threshold")
    for label, (episodes, over, best) in sorted(stats.items(), key=lambda x: -x[1][2]):
        print(f"  {labels[label]:10s} episodes {episodes}, frames over threshold {over}, best score {best:.2f}")
    if active_any > 0:
        print("  (an animal was still present at the end of the video)")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("videos", nargs="+", help="video files (anything ffmpeg can read)")
    ap.add_argument("--model", default=str(REPO / "build/model/converted_model.tflite"))
    ap.add_argument("--labels", default=str(REPO / "build/label/labels.txt"))
    ap.add_argument("--threshold", type=int, default=50, help="Threshold in percent (app default 50)")
    ap.add_argument("--start-frames", type=int, default=3, help="StartFrames (app default 3)")
    ap.add_argument("--hold-sec", type=float, default=5, help="HoldSec (app default 5)")
    ap.add_argument("--min-animal-pct", type=int, default=0, help="MinAnimalPct (app default 0)")
    ap.add_argument("--classes", default=DEFAULT_CLASSES, help="AnimalClasses, comma separated")
    ap.add_argument("--fps", type=float, default=10, help="frames per second analysed (default 10)")
    ap.add_argument("--start", type=float, default=0, help="start at this second of the video")
    ap.add_argument("--duration", type=float, default=0, help="analyse only this many seconds")
    ap.add_argument("--debug-threshold", type=int, default=0,
                    help="like the app's DebugThreshold: also list animal detections down to this percent")
    ap.add_argument("--save-hits", metavar="DIR", help="save the frames of animal events as JPEG with boxes")
    args = ap.parse_args()

    for tool in ("ffmpeg", "cc"):
        if subprocess.run(["which", tool], capture_output=True).returncode != 0:
            sys.exit(f"{tool} not found on the PATH")
    for p in (args.model, args.labels):
        if not Path(p).is_file():
            sys.exit(f"not found: {p} (build the app or pass --model/--labels, see the top of this file)")
    from ai_edge_litert.interpreter import Interpreter
    interp = Interpreter(model_path=args.model)
    interp.allocate_tensors()
    labels = read_labels(args.labels)
    allowed = allowed_flags(labels, args.classes)
    with tempfile.TemporaryDirectory() as work:
        lib = build_tracker_lib(work)
        for video in args.videos:
            run_video(video, args, interp, labels, allowed, lib)


if __name__ == "__main__":
    main()
