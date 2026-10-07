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

Each class is a stateful camera event, declared by `app/animal_output.c`:

    topic tnsaxis:AnimalDetector/<Class>     data: Detected = true | false

It shows up in the camera's event list (rules, subscriptions). To get it to MQTT, add the topic to
`MotionEvents` of [axis-scene-mqtt-bridge](https://github.com/jens62/axis-scene-mqtt-bridge)
(and `Detected` is already one of its event keys).

The log (`Apps -> Log`) shows the version and settings at start, `Animal start/stop` lines and,
every 100 frames, the average and maximum inference time. Loading the model on the first start can
take minutes.

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

## Build

    docker build --platform=linux/amd64 --tag axis-animal-detector:dev .
    docker cp $(docker create --platform=linux/amd64 axis-animal-detector:dev):/opt/app ./build

`build/*.eap` is the package (aarch64, ARTPEC-8). Unit tests of the start/stop logic: `sh tests/run.sh`.

Planned: species via a model fine-tuned on camera-trap data.

Companion project: [axis-scene-mqtt-bridge](https://github.com/jens62/axis-scene-mqtt-bridge).

## License

See `LICENSE`.
