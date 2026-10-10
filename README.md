# axis-animal-detector

An ACAP application for Axis cameras that detects animals **on the camera** (DLPU, no other
machine involved) and reports them as events.

Developed against an AXIS M4228-LVE (ARTPEC-8, aarch64, AXIS OS 12.x).

## Status

Early work, but in use on an AXIS M4228-LVE: the events show up in the camera's rule editor, a rule with
the *Record video* action records on them, and they reach Scrypted and HomeKit Secure Video (the
[Scrypted guide](docs/scrypted-homekit-secure-video.md) lists what has been verified and what has not).
Other cameras have not been tried. Based on the `object-detection` example of
[acap-native-sdk-examples](https://github.com/AxisCommunications/acap-native-sdk-examples):
SSD MobileNet v2 (COCO) on the ARTPEC-8 DLPU.

## How it works

Every frame is run through the model. Detections of the configured animal classes are turned into
motion-detector style events by `app/animal_events.c`:

* **start**: a class is seen with at least `Threshold` % in `StartFrames` consecutive frames
* **stop**: it was not seen for `HoldSec` seconds (short drop-outs do not end the episode)

Each species is a stateful camera event, declared by `app/animal_output.c`, plus one for "any
animal":

    topic tnsaxis:CameraApplicationPlatform/AnimalDetector/<Species>   (e.g. .../Bird)
    topic tnsaxis:CameraApplicationPlatform/AnimalDetector/Any
    data: active = true | false

    topic tnsaxis:AnimalDetector/Detection   (a pulse; not offered in the rule editor)
    data: Species = "bird" ..., Score = 0..1

The camera requires an event to carry exactly the keys it was declared with, and the rule editor
makes an input field of every declared data key. So the state events carry only `active` (the rule
editor shows one field, `active`, prefilled with 1). `Species` and `Score` are on the separate
`Detection` pulse, sent once when a species starts, which gives MQTT and notifications one topic for
all species. It is deliberately declared outside `CameraApplicationPlatform`, so that the rule editor
does not list it. Add `tnsaxis:AnimalDetector` to the bridge's `MotionEvents` to get it on MQTT (as
`<prefix>/motion/AnimalDetector/Detection`).

`Any` is true while at least one species is active; its `Species` and `Score` are those of the
species that started or ended it. The topics start with `CameraApplicationPlatform` because only such
Axis events are offered as conditions in the camera's rule editor (Events -> Rules -> Condition,
under "Anwendung": "Animal Detector - Bird", "Animal Detector - Any animal"). Before 0.3.0 they were
`tnsaxis:AnimalDetector/<Class>`, which exist for ONVIF but are not offered in the rule editor.

To get them to MQTT, add `tnsaxis:CameraApplicationPlatform/AnimalDetector` to `MotionEvents` of
[axis-scene-mqtt-bridge](https://github.com/jens62/axis-scene-mqtt-bridge); the MQTT topics are then
`<prefix>/motion/CameraApplicationPlatform/AnimalDetector/<Species>`
(`Detected` is already one of its event keys; add `Species` and `Score` to its `EventKeys` to get them on MQTT too).
`Score` is the score that started the episode on "true" and the best score of the episode on "false".

The log (`Apps -> Log`) shows the version and settings at start, `Animal start/stop` lines and,
every 100 frames, the average and maximum inference time. Loading the model on the first start can
take minutes.

## Region mode (moving animals only)

The model sees the whole image squeezed to 300x300 pixels, so a small animal is only a few pixels.
With `RegionMode` ("Only look where the camera sees unclassified movement") on, the app instead reads the camera's scene metadata (`com.axis.scene.frame.v1`,
through Device Data Hub, no manifest resource needed) and takes the boxes of moving objects that the
camera could not classify (no `class`: not a person, not a vehicle). The tracker only follows moving
things, so these are where a moving animal shows up. For each of the largest few boxes
(`MaxRegions`, "Regions per frame") the app crops a square with some margin out of a 1920x1080 stream, scales it to
300x300 in software, and runs the model on it. The existing start/stop logic does the rest.

* Boxes smaller than `MinBoxPct` ("Smallest region (% of the image)", sqrt(width x height) in percent of the image) are ignored: the
  tracker reports tiny boxes at the image edge all the time.
* The last regions stay valid for `RegionHoldSec` ("Keep looking for (s) after the last region"), because the camera's tracks flicker.
* With nothing moving the model does not run at all and the stream drops to about 5 fps.
* Animals that do not move are not detected in this mode, and neither are animals the camera does
  not track. The stream shows the examined regions (and the detections) when `DrawBoxes` is on.
* To see what the model makes of each region, set `DebugThreshold` (e.g. 20): `Seen: [r1] person
  0.31, [r2] couch 0.24` lists the results per region.

## Scrypted and HomeKit Secure Video
The detector's "any animal" event can start a HomeKit Secure Video recording through Scrypted and the
[scrypted-onvif-motion-mapper](https://github.com/jens62/scrypted-onvif-motion-mapper) plugin: set-up,
test and an FAQ are in [docs/scrypted-homekit-secure-video.md](docs/scrypted-homekit-secure-video.md).
In short: the plugin turns the event `AnimalDetector/Any` (item `active`) into a motion sensor, the
Custom Motion Sensor extension gives it to the camera (replacing the camera's own motion), and in the
Home app "Record when motion is detected" makes HomeKit keep every clip instead of letting Apple's own
animal recognition filter it.

## Settings and test page

Open the app's settings page (Apps -> Animal Detector -> Open). It has the settings and one
**test button per animal class**: it fires the real event (start now, stop after `HoldSec`), like
the test buttons of Axis' audio detection. The page shows the labels in the first column below; the
parameter names are what the log and the camera's parameter list use.

| Label on the settings page | Parameter | Default | Meaning |
|---|---|---|---|
| Minimum score (%) | `Threshold` | 50 | Minimum score in percent |
| Report an animal after (frames in a row) | `StartFrames` | 3 | Frames in a row before an animal is reported |
| Animal is gone after (s) | `HoldSec` | 5 | Seconds without the animal until it is reported as gone |
| Animal classes (comma separated, names from the COCO label file) | `AnimalClasses` | bird, cat, dog, horse, sheep, cow, elephant, bear, zebra, giraffe | Label names of the COCO label file that count as animals |
| Draw the boxes on video channels (comma separated) | `OverlayChannels` | 0,1 | Video channels (views) the boxes are drawn on: a stream or recording only shows the boxes of its own view (0 = whole sensor, 1 = "View Area 1"). Channels that do not exist are skipped |
| Draw boxes around animals in the video stream (checkbox) | `DrawBoxes` | yes | Draw boxes on the video stream: yellow = the region that is analysed (region mode), red = an animal at or above `Threshold` (kept for `HoldSec` after the last sighting), green = other objects (only with `DebugThreshold`). The overlay cannot show text, species and score are in the log and the events |
| Only look where the camera sees unclassified movement (checkbox) | `RegionMode` | no | Only analyse regions of unclassified movement (see above) |
| Smallest animal (% of the image) | `MinAnimalPct` | 0 | Smallest animal, sqrt(width x height) of the detected box in percent of the image; 0 = off. Smaller detections are ignored (with `DebugThreshold` the log marks them "(animal, too small)"). Against leaves and spots moved by the wind |
| Smallest region (% of the image) | `MinBoxPct` | 3 | Smallest region, sqrt(width x height) in percent of the image |
| Keep looking for (s) after the last region | `RegionHoldSec` | 3 | Keep looking this long after the last region |
| Regions per frame (largest first) | `MaxRegions` | 3 | Regions per frame, largest first |
| Troubleshooting: log everything seen above (%) | `DebugThreshold` | 0 | Troubleshooting, 0 = off. Logs `Seen: …` (at most once a second) with everything the model sees at this score (percent) or more, animals or not, and draws boxes for all of it |

## Build

    docker build --platform=linux/amd64 --tag axis-animal-detector:dev .
    docker cp $(docker create --platform=linux/amd64 axis-animal-detector:dev):/opt/app ./build

`build/*.eap` is the package (aarch64, ARTPEC-8). Unit tests of the start/stop logic: `sh tests/run.sh`.

## Testing with videos, without a camera
`tools/video_test.py` runs the app's model and its decision logic on video files on a Mac or a Linux
machine. It decodes the frames with `ffmpeg`, squashes them to the model's 300x300 input like the app does in its
default mode, runs the same SSD MobileNet v2 (COCO) model on the CPU, applies `AnimalClasses`, `MinAnimalPct`
and then the app's own tracker (`app/animal_events.c` and `app/animal_any.c`, compiled from this repository and
called from Python) with `Threshold`, `StartFrames` and `HoldSec`. It prints the events the camera would send.

    python3 -m venv ~/.venvs/animal-video-test
    ~/.venvs/animal-video-test/bin/pip install ai-edge-litert numpy pillow
    ~/.venvs/animal-video-test/bin/python tools/video_test.py my_video.mp4 --threshold 50 --save-hits hits/

Example output:

    00:00.6  START bird (score 0.87)
    00:00.6  ANY ANIMAL present
    00:09.9  stop  bird (seen for 4.3 s, best score 1.00)
    00:09.9  ANY ANIMAL gone

Options mirror the settings (`--threshold`, `--start-frames`, `--hold-sec`, `--min-animal-pct`, `--classes`);
`--fps` is the number of frames analysed per second (`StartFrames` counts frames), `--debug-threshold` also lists
animal detections below the threshold (like `DebugThreshold`), `--save-hits DIR` saves the frames of the events
with boxes, `--start`/`--duration` select a part of the video. It needs the model and the label file from a build
(`build/model/converted_model.tflite`, `build/label/labels.txt`), or pass `--model`/`--labels`.

The results are close to the camera's, not identical: the camera's DLPU runs the same quantised model but not bit
for bit, the camera analyses its raw sensor stream (a video file is compressed), its frame rate depends on the
speed of its analysis, and **region mode is not reproduced**. Good test material: a clip with an animal (does it
start, and when?), a long clip without one (does the wind or a shadow start an event at your `Threshold`?).

Planned: species via a model fine-tuned on camera-trap data.

Companion project: [axis-scene-mqtt-bridge](https://github.com/jens62/axis-scene-mqtt-bridge).

## License

The code is under the Apache License 2.0, see `LICENSE`.

**The model is not part of this repository.** The `Dockerfile` downloads the pretrained SSD MobileNet v2
(COCO) model (`ssd_mobilenet_v2_coco_quant_postprocess.tflite`) and the COCO label file from Google Coral's
[test_data](https://github.com/google-coral/test_data) repository, as the `object-detection` example of
Axis does, and packs them into the `.eap` package; the packages in the releases contain them. The model and
the labels are not covered by this repository's licence and keep their own terms. This repository does not state
them: check the Coral repository before you redistribute a package.
