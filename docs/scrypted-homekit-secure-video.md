# Animal Detector with Scrypted and HomeKit Secure Video

How to let an animal that the detector sees start a HomeKit Secure Video (HKSV) recording, and what
the pieces do. Written from a real set-up (AXIS M4228-LVE, Scrypted 0.145, openHAB for notifications).
Everything marked **not verified** was not tried on that set-up.

## The picture

```
 camera                                              Scrypted                        Apple
 ------                                              --------                        -----
 animal detector                                     ONVIF Motion Mapper
   |  camera event                                     (subscribes to the event)
   |  CameraApplicationPlatform/AnimalDetector/Any     |  active=true  -> motion START
   |  data: active = true | false          ONVIF  ---> |  active=false -> motion STOP
   |                                                    v
   +--> camera rules (record on the SD card ...)      camera device + "Custom Motion Sensor"
   +--> MQTT bridge --> openHAB --> ntfy              (replaces the camera's own motion)
                                                        v
                                                      HomeKit plugin --> Apple TV (home hub)
                                                                          --> iCloud (clip)
```

- The detector reports "an animal is there" as a normal camera event with one value, `active`.
- Scrypted knows nothing about animals. The **ONVIF Motion Mapper** plugin
  ([scrypted-onvif-motion-mapper](https://github.com/jens62/scrypted-onvif-motion-mapper)) turns that
  event into a motion sensor, and the **Custom Motion Sensor** extension gives that sensor to the
  real camera.
- To HomeKit the animal is just "motion". The species and the score are **not** in Scrypted or in
  the Home app; they are in the MQTT message (`…/AnimalDetector/Detection`) and the notifications.

## What you need

- The detector (0.3.0 or newer) running on the camera, with its events visible in the camera's rule
  editor (Events > Rules > Condition > "Animal Detector - Any animal").
- Scrypted with the camera added (the ONVIF camera plugin in the example) and the HomeKit plugin
  enabled for it.
- The [scrypted-onvif-motion-mapper](https://github.com/jens62/scrypted-onvif-motion-mapper) plugin
  (0.0.5 or newer if you also want recordings on the camera's normal motion, see
  "Also record on normal motion" below).
- For HomeKit Secure Video: a home hub (Apple TV or HomePod) and an iCloud+ plan.

## How to

### 1. Make the detector strict
Every event of the detector will start a recording (see step 6), so false alarms cost iCloud
storage and notifications. In the detector's settings:

| Setting | Suggestion |
|---|---|
| Minimum score (`Threshold`) | about 65 % (low values are for experiments only) |
| Report an animal after (`StartFrames`) | 3 frames |
| Smallest animal (`MinAnimalPct`) | 0 at first, then raise it until the wind no longer triggers |
| Draw boxes (`DrawBoxes`), video channels (`OverlayChannels`) | on, `0,1`: the recording then shows the red and yellow boxes |

### 2. Check that Scrypted can see the camera event
Install the mapper plugin (build and deploy it with `npm run build` and `npx scrypted-deploy`, see
[Build and install](https://github.com/jens62/scrypted-onvif-motion-mapper#build-and-install)) and create
a device in it:

| Setting | Value |
|---|---|
| Host / Port / user / password | the camera's ONVIF address and an account of the camera |
| Event Topic | `AnimalDetector/Any` (a substring of the topic after the namespaces are stripped; the full form `CameraApplicationPlatform/AnimalDetector/Any` works too) |
| Data Item Name | `active` |
| Motion Reset (seconds) | `0` (the detector sends an explicit `active=false` when the animal is gone; the default of 30 s would end the motion while the animal is still there) |
| Log All Events | on while you test, off afterwards (it is very noisy) |

Press a simulate button on the detector's page. In the mapper's log you should see
`onvif event: topic="CameraApplicationPlatform/AnimalDetector/Any" item="active" value=1` followed by
`-> matched, motion START`, and about 5 s later `value=0` and `motion STOP`. The device's status
switches from "No motion" to "Motion".

### 3. Give the sensor to the camera
Open the **camera** device in Scrypted (not the mapper device), Extensions, switch on
**Custom Motion Sensor**, then under "Custom Motion Sensor" select the mapper device as the motion
sensor. **This replaces the camera's own motion sensor** (see the FAQ).

### Also record on normal motion (optional)
The Custom Motion Sensor **replaces** the camera's own motion sensor (see the FAQ), so with the setting
of step 2 only animals start a recording. To record on normal motion **and** on animals, put both
events into the one mapper device. Needs mapper 0.0.5 or newer, which can combine several events.
Change the device's settings in the mapper:

| Setting | Before (animals only) | Now (motion and animals) |
|---|---|---|
| Event Topic | `AnimalDetector/Any` | `/(MotionRegionDetector\/Motion\|AnimalDetector\/Any)$/` |
| Data Item Name | `active` | empty (the camera's motion item has another name, e.g. `State`; an empty name matches on the topic alone) |
| Combine Matched Topics (any active) | off | **on** |
| Motion Reset (seconds) | `0` | `0` |

Motion is then on as long as **either** the camera reports motion or an animal is there. If one event
ends while the other is still on, the motion stays on (this is what the combine setting is for;
without it the last event would win and end the motion too early). The first part of the regex is the
camera's own motion event: **check the topic your camera really sends** (switch on "Log All Events",
walk in front of the camera and read the topic in the mapper's log; on an Axis camera it is probably
`RuleEngine/MotionRegionDetector/Motion` with the item `State`). Both events must carry a single data
item, otherwise the mapper ignores them. Nothing else changes: the camera still gets the mapper device
as its Custom Motion Sensor, and step 5 ("Motion is detected") is the setting you want.

Trade-off: normal motion is much more frequent than animals (wind, shadows, people, cars), so recordings
and iCloud storage grow. Use the camera's own motion settings (areas, sensitivity) to keep it calm.

### 4. HomeKit plugin
- The camera is enabled in the HomeKit plugin (it is in the plugin's list of extended devices).
- The mapper device does not need to be exposed to HomeKit. Switch HomeKit off for it, otherwise Apple
  Home shows an extra motion sensor that nothing uses.
- Optional, for checking: switch on the plugin's debug mode for recordings. Scrypted then keeps each
  clip it records as an `.mp4` (plus a small `.json`) in the HomeKit plugin's `files/hksv` folder, and
  lists them under "HomeKit Secure Video debug mode clips".

### 5. Apple Home: record on motion
In the Home app, open the camera's settings, recording options, "More options" ("Weitere Optionen"),
"Record when" ("Aufnehmen, wenn"):

- **"Motion is detected" ("Bewegung entdeckt wird")**: HomeKit records whenever Scrypted reports
  motion, which here means whenever the detector reports an animal. **This is the setting you want.**
- "Specific motions are detected" ("Bestimmte Bewegungen werden entdeckt") with people, animals,
  vehicles, packages: the Apple TV analyses the clip itself and keeps it only if it recognises one of
  them. Apple's animal recognition misses animals, which is the reason for a detector of your own.

### 6. Test
1. Press a simulate button on the detector's page.
2. HomeKit plugin log: `motion recording starting`, `motion recording started`, several
   `motion fragment #n sent`, `motion recording closed`, `motion recording finished`.
3. With debug mode on: a new `.mp4` in `files/hksv`. Its size equals the sum of the fragment sizes.
4. Home app: the clip appears in the camera's recordings (it may take a moment).

To check what Apple makes of it, press the simulation and **walk in front of the camera** during the
next 10 to 15 seconds: the clip then contains a person, and a clip with a person is kept whatever the
setting is.

## FAQ

**Scrypted will never detect motion, because it does not know "active" as an event. Should it be
"motion"?**
No. `active` is the name of the data item of the detector's event, and the mapper's job is to translate
it into Scrypted's motion state. Scrypted never needs to know `active`. A mapper log with
`value=1` and `-> matched, motion START` shows that it works. "No motion" in the device's status
just means that no animal was active at that moment.

**The mapper log says "onvif event without a simpleItem, ignoring". Is that a problem?**
No. The mapper only handles events that carry exactly one data item. Others are ignored: the
`AnimalDetector/Detection` pulse (species and score), and events of the camera itself such as
`Device/Log/Audit` or `SoundPressureLevel/Summary`. The animal state events carry only `active` and are
handled.

**Does the Custom Motion Sensor fire on top of the camera's normal motion sensor, or replace it?**
It replaces it: the extension mirrors only the selected sensor (`ReplaceMotionSensor` in Scrypted's
Dummy Switch plugin). Once attached, the camera's own motion detection no longer reaches Scrypted or
HomeKit, and only animals start a recording. Walking in front of the camera without a simulation or an
animal does not record. That is expected with the settings of step 2. Recording on normal motion
**and** animals needs both signals in one device; the mapper (0.0.5 or newer) does that with
"Combine Matched Topics", see "Also record on normal motion" above.

**The simulation recorded a clip according to the log, but I see nothing in the Home app.**
The log (`motion recording ...`, fragments sent) and the debug clip prove that Scrypted recorded and
delivered the clip to the hub. What happens afterwards is Apple's: with "Specific motions are
detected", the hub analyses the clip, and a simulated animal in an empty scene contains nothing it
would recognise. Switch to "Motion is detected" (step 5), or walk in front of the camera during the
test.

**Why does it not record when I walk in front of the camera without a simulation?**
Because the camera's own motion sensor was replaced (see above). Only the detector's events trigger,
unless you combine both events in the mapper ("Also record on normal motion").

**Do I have to create another ONVIF media profile for the stream with the animal rectangles?**
No. The rectangles are drawn into the camera's video on the views listed in `OverlayChannels`
(default `0,1`: the whole sensor and "View Area 1"). A stream or recording shows the boxes of its own
view, whatever profile it uses. Recordings from "View Area 1" (the camera's rule, with "no stream
profile") show them. If Scrypted's stream comes from another view, add its channel number to
`OverlayChannels`. The boxes are then in everything recorded from that view; if you want a clean main
stream, use a second view area for an "animal" stream and set `OverlayChannels` to its channel only
(**not verified**: the channel number of a second view area).

**The recording from the camera's rule has no rectangles.**
The overlay is drawn on one video view per bounding-box object. A recording from "View Area 1" showed
nothing as long as the detector drew only on channel 0. Use `OverlayChannels = 0,1`. The detector's log
lists the channels it drew on (`Overlay on video channel N`).

**Can I see the species or the score in Scrypted or the Home app?**
No. The mapper provides a motion sensor only, and HomeKit sees "motion". The species and score are in
the MQTT message `…/motion/AnimalDetector/Detection` (for openHAB, notifications, and so on).

**The wind starts recordings.**
Moving foliage becomes class-less tracks, and the model reads some of them as an animal. With the
detector at a low `Threshold` and `StartFrames` 1, one gust is enough. Use `Threshold` about 65 %,
`StartFrames` 3, and `MinAnimalPct`. The red box in the recording shows what the model found. See the
detector's `IMPROVEMENTS.md` for further ideas.

**Do I need the mapper device in HomeKit?**
No. It only feeds the camera. Switch HomeKit off for it.

**Where are the logs?**
- Camera rule fired: the camera's system log (Events), and the recording itself.
- Detector: its app log (`Animal start: …`, `Overlay on video channel …`).
- ONVIF event arrived in Scrypted: the mapper device's log (with "Log All Events").
- HomeKit: the HomeKit plugin's log (`motion recording …`), and the debug clips in `files/hksv`.

## Status of this guide

| Step | Verified on the author's set-up |
|---|---|
| Detector event reaches Scrypted through ONVIF; mapper switches to motion and back | yes |
| Custom Motion Sensor attached; HomeKit records (log, fragments, debug clip) | yes |
| Recording only with an animal event, not on normal motion | yes |
| Recording with a person walking in front while a simulation runs | yes |
| Normal motion and animals in one mapper device ("Combine Matched Topics") | not verified yet (the combining logic is tested with timers, not on a camera) |
| "Motion is detected" in the Home app records every event without Apple's analysis | not verified yet |
| Boxes visible in the HKSV clip in the Home app | not verified |

## Screenshots (to add)
Useful, cleaned of addresses, names, pairing codes and QR codes, to be put into `docs/images/`:
the mapper device's settings, the camera's "Custom Motion Sensor" setting, the Home app's
"Weitere Optionen" screen, and the camera's rule editor with the "Animal Detector - …" entries.
