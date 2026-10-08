#ifndef ANIMAL_OUTPUT_H
#define ANIMAL_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Camera events for the animals, one stateful event per species plus one for "any animal":
 *   topic tnsaxis:CameraApplicationPlatform/AnimalDetector/<Species>   (e.g. .../Bird)
 *   topic tnsaxis:CameraApplicationPlatform/AnimalDetector/Any
 *   data  active (true/false), nothing else: an event must carry exactly the keys it was declared
 *         with, and every declared data key becomes an input field in the camera's rule editor
 *   topic tnsaxis:AnimalDetector/Detection   (a pulse, not a state; deliberately NOT under
 *         CameraApplicationPlatform, so that the rule editor does not list it)
 *   data  Species (the class, e.g. "bird"), Score (0..1), sent once when a species starts
 * so the details for MQTT and notifications are on one topic for all species. To get it to MQTT
 * add tnsaxis:AnimalDetector to the bridge's MotionEvents.
 * The topic starts with CameraApplicationPlatform because only such Axis events show up in the
 * camera's rule editor (Axis' send_event example). "Any" is true while at least one species is
 * active; its Species and Score are those of the species that started or ended it.
 * They can be forwarded to MQTT with axis-scene-mqtt-bridge (add
 * tnsaxis:CameraApplicationPlatform/AnimalDetector to its MotionEvents).
 * Needs a running GLib main loop on the default context (the declarations complete there).
 */
typedef struct animal_output animal_output_t;

animal_output_t* animal_output_new(char** labels, const bool* allowed, size_t n_labels);
void animal_output_free(animal_output_t* out);

/** Thread safe: the event is sent from the main loop. Call from one thread only. */
void animal_output_send(animal_output_t* out, int label, bool detected, double score);

#endif
