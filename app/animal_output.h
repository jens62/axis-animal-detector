#ifndef ANIMAL_OUTPUT_H
#define ANIMAL_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

/**
 * Camera events for the animals: one stateful event per animal class,
 *   topic tnsaxis:AnimalDetector/<Class>
 *   data  Detected (true/false), Species (the class, e.g. "bird"), Score (0..1: the score that
 *         started the episode on "true", the best score of the episode on "false")
 * They show up in the camera's event list (rules, event subscriptions) and can be forwarded to
 * MQTT with axis-scene-mqtt-bridge (add the topic to its MotionEvents or AudioEvents).
 * Needs a running GLib main loop on the default context (the declarations complete there).
 */
typedef struct animal_output animal_output_t;

animal_output_t* animal_output_new(char** labels, const bool* allowed, size_t n_labels);
void animal_output_free(animal_output_t* out);

/** Thread safe: the event is sent from the main loop. */
void animal_output_send(animal_output_t* out, int label, bool detected, double score);

#endif
