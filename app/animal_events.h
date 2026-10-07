#ifndef ANIMAL_EVENTS_H
#define ANIMAL_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Turns the per-frame detections of an object detector into motion-detector style events:
 * one "start" when a class of animal appears and one "stop" when it is gone, instead of a
 * message per frame.
 *
 * Per class (label index):
 *  - start: seen with score >= threshold in start_frames consecutive frames. Needing a few frames
 *    suppresses single-frame false positives.
 *  - stop: not seen for hold_ms. A short drop-out of the detector therefore does not end the
 *    episode.
 * Classes not marked in `allowed` are ignored.
 */
typedef struct animal_tracker animal_tracker_t;

typedef struct {
    int label;
    float score;  // 0..1
} animal_det_t;

typedef struct {
    bool start;           // false: stop
    int label;
    float best_score;     // highest score seen during the episode
    int64_t duration_ms;  // stop only: from start to the last frame the class was seen
} animal_event_t;

/** allowed[i] tells whether label i is an animal worth reporting; it is copied. */
animal_tracker_t* animal_tracker_new(const bool* allowed, int n_labels, float threshold,
                                     int start_frames, int hold_ms);
void animal_tracker_free(animal_tracker_t* t);

/**
 * Feed one frame (n may be 0, it must be called for every frame so that "stop" is noticed).
 * now_ms is a monotonic clock. Writes up to max_out events to out and returns their number.
 */
int animal_tracker_update(animal_tracker_t* t, const animal_det_t* dets, int n, int64_t now_ms,
                          animal_event_t* out, int max_out);

#endif
