// Host-side test of the start/stop logic: see tests/run.sh
#include <assert.h>
#include <stdio.h>

#include "../app/animal_any.h"
#include "../app/animal_events.h"

enum { NONE, CAT, DOG, TABLE, N_LABELS };

static const bool allowed[N_LABELS] = {false, true, true, false};

/* One frame with at most one detection of `label` (NONE: empty frame). */
static int frame(animal_tracker_t* t, int label, float score, long ms, animal_event_t* out) {
    animal_det_t d = {label, score};
    return animal_tracker_update(t, &d, label == NONE ? 0 : 1, ms, out, 4);
}

static void test_start_stop(void) {
    animal_tracker_t* t = animal_tracker_new(allowed, N_LABELS, 0.5f, 3, 5000);
    animal_event_t e[4];

    // 10 fps: start only after 3 consecutive frames.
    assert(frame(t, CAT, 0.6f, 0, e) == 0);
    assert(frame(t, CAT, 0.7f, 100, e) == 0);
    assert(frame(t, CAT, 0.8f, 200, e) == 1);
    assert(e[0].start && e[0].label == CAT && e[0].best_score > 0.79f);
    // Staying there: nothing more, however long.
    for (int i = 3; i < 100; i++)
        assert(frame(t, CAT, 0.7f, i * 100, e) == 0);
    // Detector drop-outs inside the hold time do not end the episode.
    assert(frame(t, NONE, 0, 10000, e) == 0);
    assert(frame(t, NONE, 0, 12000, e) == 0);
    assert(frame(t, CAT, 0.7f, 13000, e) == 0);
    // Gone for hold_ms: one stop, with duration and best score.
    assert(frame(t, NONE, 0, 17000, e) == 0);
    assert(frame(t, NONE, 0, 18000, e) == 1);
    assert(!e[0].start && e[0].label == CAT);
    assert(e[0].duration_ms == 13000 - 200);
    assert(e[0].best_score > 0.79f);
    assert(frame(t, NONE, 0, 30000, e) == 0);
    // A new episode starts afresh.
    assert(frame(t, CAT, 0.6f, 40000, e) == 0);
    assert(frame(t, CAT, 0.6f, 40100, e) == 0);
    assert(frame(t, CAT, 0.6f, 40200, e) == 1 && e[0].start && e[0].best_score < 0.61f);
    animal_tracker_free(t);
}

static void test_filtering(void) {
    animal_tracker_t* t = animal_tracker_new(allowed, N_LABELS, 0.5f, 2, 1000);
    animal_event_t e[4];

    // Below threshold, not an animal, and out of range labels never count.
    for (int i = 0; i < 10; i++) {
        assert(frame(t, CAT, 0.4f, i * 100, e) == 0);
        assert(frame(t, TABLE, 0.99f, i * 100, e) == 0);
        assert(frame(t, 77, 0.99f, i * 100, e) == 0);
        assert(frame(t, -1, 0.99f, i * 100, e) == 0);
    }
    // A single frame is not enough (false positive), and the counter restarts after a gap.
    assert(frame(t, CAT, 0.9f, 2000, e) == 0);
    assert(frame(t, NONE, 0, 2100, e) == 0);
    assert(frame(t, CAT, 0.9f, 2200, e) == 0);
    assert(frame(t, NONE, 0, 2300, e) == 0);
    assert(frame(t, NONE, 0, 9000, e) == 0);  // never started: no stop either
    animal_tracker_free(t);
}

static void test_two_classes(void) {
    animal_tracker_t* t = animal_tracker_new(allowed, N_LABELS, 0.5f, 1, 1000);
    animal_event_t e[4];
    animal_det_t both[2] = {{CAT, 0.9f}, {DOG, 0.8f}};

    assert(animal_tracker_update(t, both, 2, 0, e, 4) == 2);
    assert(e[0].start && e[0].label == CAT && e[1].start && e[1].label == DOG);
    // Only the dog stays.
    animal_det_t dog[1] = {{DOG, 0.8f}};
    assert(animal_tracker_update(t, dog, 1, 500, e, 4) == 0);
    assert(animal_tracker_update(t, dog, 1, 1000, e, 4) == 1);
    assert(!e[0].start && e[0].label == CAT);
    assert(animal_tracker_update(t, NULL, 0, 3000, e, 4) == 1);
    assert(!e[0].start && e[0].label == DOG);
    // max_out is respected.
    assert(animal_tracker_update(t, both, 2, 4000, e, 1) == 1);
    animal_tracker_free(t);
}

static void test_any(void) {
    animal_any_t* a = animal_any_new(4);

    assert(animal_any_update(a, 1, true) == 1);    // the first animal: "any" starts
    assert(animal_any_update(a, 1, true) == 0);    // repeated start of the same species
    assert(animal_any_update(a, 2, true) == 0);    // a second species: still active
    assert(animal_any_update(a, 1, false) == 0);   // one is left
    assert(animal_any_update(a, 1, false) == 0);   // repeated stop
    assert(animal_any_update(a, 2, false) == -1);  // the last one is gone: "any" ends
    assert(animal_any_update(a, 3, true) == 1);    // and again
    assert(animal_any_update(a, 3, false) == -1);
    assert(animal_any_update(a, -1, true) == 0);   // out of range
    assert(animal_any_update(a, 4, true) == 0);
    animal_any_free(a);
}

int main(void) {
    test_start_stop();
    test_filtering();
    test_two_classes();
    test_any();
    puts("animal_events: all tests passed");
    return 0;
}
