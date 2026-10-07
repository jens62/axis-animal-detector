# axis-animal-detector

An ACAP application for Axis cameras that detects animals **on the camera** (DLPU, no other
machine involved) and reports them as events.

Developed against an AXIS M4228-LVE (ARTPEC-8, aarch64, AXIS OS 12.x).

## Status

Early work. First milestone: run a pretrained COCO model (SSD MobileNet v2, based on the
`object-detection` example of
[acap-native-sdk-examples](https://github.com/AxisCommunications/acap-native-sdk-examples))
on the DLPU and check how well it recognises animals (bird, cat, dog, bear, ...), by day and
with IR at night.

Planned: keep only animal classes, publish start/stop events (like a motion detector) instead of
a stream of detections, settings page, later species via a model fine-tuned on camera-trap data.

Companion project: [axis-scene-mqtt-bridge](https://github.com/jens62/axis-scene-mqtt-bridge).

## License

See `LICENSE`.
