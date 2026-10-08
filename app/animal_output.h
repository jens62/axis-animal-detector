#ifndef ANIMAL_OUTPUT_H
#define ANIMAL_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Camera events for the animals, one stateful event per species plus one for "any animal":
 *   topic tnsaxis:CameraApplicationPlatform/AnimalDetector/<Species>   (e.g. .../Bird)
 *   topic tnsaxis:CameraApplicationPlatform/AnimalDetector/Any
 *   declared data: active (true/false); only the state is declared, because every declared data
 *         key becomes an input field in the camera's rule editor
 *   sent with each event: active, Detected (same value), Species (the class, e.g. "bird"),
 *         Score (0..1: the score that started the episode on "true", the best score of the
 *         episode on "false")
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
