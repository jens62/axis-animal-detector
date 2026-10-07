#ifndef REGIONS_H
#define REGIONS_H

#include <stddef.h>
#include <stdint.h>

/**
 * Regions of interest from the camera's scene metadata (com.axis.scene.frame.v1): the boxes of
 * moving objects the camera could not classify (no "class"). The tracker only follows moving
 * things, so these are the places where a moving animal would show up.
 */
typedef struct {
    double left;
    double top;
    double right;
    double bottom;  // normalized 0..1
} region_t;

/**
 * Class-less detections of one frame whose size sqrt(width*height) is at least `min_size`
 * (normalized). Larger boxes first, at most max_out. Returns how many were written; frames in any
 * other shape give 0.
 */
int regions_parse_frame(const char* json, size_t len, double min_size, region_t* out, int max_out);

/**
 * Keeps the regions of the latest frame that had any. A frame without any (the object flickers
 * out for a moment) does not replace them: they stay valid for hold_ms.
 */
typedef struct regions regions_t;

regions_t* regions_new(void);
void regions_free(regions_t* r);

/** Call with the result of regions_parse_frame() when it found something. Thread safe. */
void regions_update(regions_t* r, const region_t* set, int n, int64_t now_ms);

/** The current regions, or 0 if the last update is older than hold_ms. Thread safe. */
int regions_current(regions_t* r, int64_t now_ms, int64_t hold_ms, region_t* out, int max_out);

#endif
