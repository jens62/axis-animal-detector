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
- Wind: moving foliage becomes class-less tracks, the model reads some of them as `bear` or `horse`
  at 0.5 to 0.6 (seen on a windy day with `Threshold` 20 and `StartFrames` 1). Ideas: ignore regions
  that stay in the same small area for a long time, require that a region really travels, a mask of
  image areas to ignore (vegetation), a higher default `StartFrames`, and a check of the model result
  against a second look (the same region on the next frames).
- The overlay API draws no text, so the video cannot show species and score. Idea: put the box
  (`Left`, `Top`, `Right`, `Bottom`) into the `Detection` pulse so that other tools can draw it.
- Animals that do not move are not seen in region mode. Idea: a slow full-frame pass (every few
  seconds) as an option.
- The scene boxes and our video frame are matched by "latest". Idea: use the timestamps to pick the
  frame that belongs to the box.

## Animals the model does not know
The model only knows the ten COCO animals. A snake or a crocodile is either not recognised at all (no
event, no message) or recognised as the wrong animal (a crocodile as `dog`, with a notification that
names the wrong species), which is more likely at a low `Threshold`.
- **Generic "something moves" event:** an event such as `AnimalDetector/Unknown` for a class-less
  moving object that persists and that the model could not name, throttled by a long cooldown. Needs
  strong filtering: shadows, leaves, insects and the tracker's tiny edge boxes also show up as
  class-less objects.
- **Any-animal detection:** a model that only decides "animal or not" (camera-trap detectors such as
  MegaDetector work like this), combined with the species classifier for the known ones. Then an
  unknown species is still reported as "animal". Needs a model that runs on the DLPU.
- **Fine-tuning on camera-trap data** (see Model) adds species such as deer, raccoon and fox, but not
  reptiles; those are not typical for such datasets.

## Model
- Species beyond the COCO animals (raccoon, fox, deer, wild boar ...): fine-tune on camera-trap data
  (COCO Camera Traps), also for IR/night images, and convert it for the DLPU.
- Check the DLPU load together with Axis Object Analytics.
