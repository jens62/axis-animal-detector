#include "animal_events.h"

#include <stdlib.h>

typedef struct {
    bool allowed;
    bool active;
    int consecutive;  // frames in a row in which the class was seen
    float best_score;
    int64_t started_ms;
    int64_t last_seen_ms;
} cls_t;

struct animal_tracker {
    int n_labels;
    float threshold;
    int start_frames;
    int64_t hold_ms;
    cls_t* cls;
    float* frame_best;  // scratch: best score per class in the current frame
};

animal_tracker_t* animal_tracker_new(const bool* allowed, int n_labels, float threshold,
                                     int start_frames, int hold_ms) {
    animal_tracker_t* t = calloc(1, sizeof(*t));
    t->n_labels         = n_labels;
    t->threshold        = threshold;
    t->start_frames     = start_frames < 1 ? 1 : start_frames;
    t->hold_ms          = hold_ms;
    t->cls              = calloc((size_t)n_labels, sizeof(cls_t));
    t->frame_best       = calloc((size_t)n_labels, sizeof(float));
    for (int i = 0; i < n_labels; i++)
        t->cls[i].allowed = allowed[i];
    return t;
}

void animal_tracker_free(animal_tracker_t* t) {
    if (t == NULL)
        return;
    free(t->cls);
    free(t->frame_best);
    free(t);
}

int animal_tracker_update(animal_tracker_t* t, const animal_det_t* dets, int n, int64_t now_ms,
                          animal_event_t* out, int max_out) {
    for (int i = 0; i < t->n_labels; i++)
        t->frame_best[i] = 0.0f;
    for (int i = 0; i < n; i++) {
        int l = dets[i].label;
        if (l < 0 || l >= t->n_labels || !t->cls[l].allowed || dets[i].score < t->threshold)
            continue;
        if (dets[i].score > t->frame_best[l])
            t->frame_best[l] = dets[i].score;
    }

    int events = 0;
    for (int l = 0; l < t->n_labels; l++) {
        cls_t* c = &t->cls[l];
        if (!c->allowed)
            continue;

        if (t->frame_best[l] > 0.0f) {
            c->consecutive++;
            c->last_seen_ms = now_ms;
            if (t->frame_best[l] > c->best_score)
                c->best_score = t->frame_best[l];
            if (!c->active && c->consecutive >= t->start_frames) {
                c->active     = true;
                c->started_ms = now_ms;
                if (events < max_out)
                    out[events++] = (animal_event_t){true, l, c->best_score, 0};
            }
        } else {
            c->consecutive = 0;
            if (c->active && now_ms - c->last_seen_ms >= t->hold_ms) {
                c->active = false;
                if (events < max_out)
                    out[events++] = (animal_event_t){false, l, c->best_score,
                                                     c->last_seen_ms - c->started_ms};
                c->best_score = 0.0f;
            } else if (!c->active) {
                c->best_score = 0.0f;
            }
        }
    }
    return events;
}
