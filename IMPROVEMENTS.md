# Improvements

Ideas that came up while building and testing the app. Not scheduled.

## Per-class thresholds
One `Threshold` applies to all animals. A bird on a screen scores about 0.28, a horse can be sure at
0.8. Idea: a list like `bird=20, cat=40, horse=60` with the general `Threshold` as the default, so
that hard classes can be lowered without letting every class through at that level.

## Richer output: boxes and scores on a topic of its own
The camera event only carries `Detected`, `Species` and `Score`. Idea: write every detection with its
bounding box to a Device Data Hub topic (in the format of `com.axis.scene.frame.v1`
`detections`), so that the bridge can publish animals next to people in `bridge/objects`. Needs the
`deviceDataHub` resource in the manifest (creating and writing a topic), and the install of such a
package on the camera has to be tried.

## Region mode
- A region around a large object (a tablet, a person) contains a small animal again. Idea: tile large
  regions, or analyse them at two scales.
- Class-less boxes next to a classified person (feet, bags) produce clutter at low scores
  (`oven 0.28`, `car 0.33`). Idea: ignore regions that touch a classified object, as an option.
- Animals that do not move are not seen in region mode. Idea: a slow full-frame pass (every few
  seconds) as an option.
- The scene boxes and our video frame are matched by "latest". Idea: use the timestamps to pick the
  frame that belongs to the box.

## Model
- Species beyond the COCO animals (raccoon, fox, deer, wild boar ...): fine-tune on camera-trap data
  (COCO Camera Traps), also for IR/night images, and convert it for the DLPU.
- Check the DLPU load together with Axis Object Analytics.
