# axis-animal-detector

An ACAP application for Axis cameras that detects animals **on the camera** (DLPU, no other
machine involved) and reports them as events.

Developed against an AXIS M4228-LVE (ARTPEC-8, aarch64, AXIS OS 12.x).

## Status

Early work, **not yet verified on a camera**. Based on the `object-detection` example of
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
    declared data: active = true | false
    sent with each event: active, Detected (same), Species = "bird" ..., Score = 0..1

Only `active` is declared, so that the rule editor shows no input fields for the other values.
`Species` and `Score` are sent with every event nevertheless (they are for MQTT, the editor could
not use them as filters anyway).

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
With `RegionMode` on, the app instead reads the camera's scene metadata (`com.axis.scene.frame.v1`,
through Device Data Hub, no manifest resource needed) and takes the boxes of moving objects that the
camera could not classify (no `class`: not a person, not a vehicle). The tracker only follows moving
things, so these are where a moving animal shows up. For each of the largest few boxes
(`MaxRegions`) the app crops a square with some margin out of a 1920x1080 stream, scales it to
300x300 in software, and runs the model on it. The existing start/stop logic does the rest.

* Boxes smaller than `MinBoxPct` (sqrt(width x height), percent of the image) are ignored: the
  tracker reports tiny boxes at the image edge all the time.
* The last regions stay valid for `RegionHoldSec`, because the camera's tracks flicker.
* With nothing moving the model does not run at all and the stream drops to about 5 fps.
* Animals that do not move are not detected in this mode, and neither are animals the camera does
  not track. The stream shows the examined regions (and the detections) when `DrawBoxes` is on.
* To see what the model makes of each region, set `DebugThreshold` (e.g. 20): `Seen: [r1] person
  0.31, [r2] couch 0.24` lists the results per region.

## Settings and test page

Open the app's settings page (Apps -> Animal Detector -> Open). It has the settings and one
**test button per animal class**: it fires the real event (start now, stop after `HoldSec`), like
the test buttons of Axis' audio detection.

| Parameter | Default | Meaning |
|---|---|---|
| `Threshold` | 50 | Minimum score in percent |
| `StartFrames` | 3 | Frames in a row before an animal is reported |
| `HoldSec` | 5 | Seconds without the animal until it is reported as gone |
| `AnimalClasses` | bird, cat, dog, horse, sheep, cow, elephant, bear, zebra, giraffe | Label names of the COCO label file that count as animals |
| `DrawBoxes` | yes | Draw boxes around animals in the video stream |
| `RegionMode` | no | Only analyse regions of unclassified movement (see above) |
| `MinBoxPct` | 3 | Smallest region, sqrt(width x height) in percent of the image |
| `RegionHoldSec` | 3 | Keep looking this long after the last region |
| `MaxRegions` | 3 | Regions per frame, largest first |
| `DebugThreshold` | 0 | Troubleshooting, 0 = off. Logs `Seen: …` (at most once a second) with everything the model sees at this score (percent) or more, animals or not, and draws boxes for all of it |

## Build

    docker build --platform=linux/amd64 --tag axis-animal-detector:dev .
    docker cp $(docker create --platform=linux/amd64 axis-animal-detector:dev):/opt/app ./build

`build/*.eap` is the package (aarch64, ARTPEC-8). Unit tests of the start/stop logic: `sh tests/run.sh`.

Planned: species via a model fine-tuned on camera-trap data.

Companion project: [axis-scene-mqtt-bridge](https://github.com/jens62/axis-scene-mqtt-bridge).

## License

See `LICENSE`.
