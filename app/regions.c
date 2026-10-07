#include "regions.h"

#include <jansson.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STORED 16

int regions_parse_frame(const char* data, size_t len, double min_size, region_t* out, int max_out) {
    json_t* root = json_loadb(data, len, 0, NULL);
    if (root == NULL)
        return 0;

    int n               = 0;
    json_t* detections = json_object_get(root, "detections");
    size_t i;
    json_t* det;
    json_array_foreach(detections, i, det) {
        if (json_object_get(det, "class") != NULL)
            continue;  // the camera classified it (Human, Head, vehicle ...)
        json_t* box = json_object_get(det, "bounding_box");
        if (!json_is_object(box))
            continue;
        region_t r;
        r.left   = json_number_value(json_object_get(box, "left"));
        r.top    = json_number_value(json_object_get(box, "top"));
        r.right  = json_number_value(json_object_get(box, "right"));
        r.bottom = json_number_value(json_object_get(box, "bottom"));
        double w = r.right - r.left;
        double h = r.bottom - r.top;
        if (w <= 0.0 || h <= 0.0 || sqrt(w * h) < min_size)
            continue;

        // Insert sorted by area, largest first.
        int pos = n < max_out ? n : max_out - 1;
        if (n >= max_out) {
            region_t* last = &out[max_out - 1];
            if ((last->right - last->left) * (last->bottom - last->top) >= w * h)
                continue;
        } else {
            n++;
        }
        while (pos > 0 && (out[pos - 1].right - out[pos - 1].left) *
                                  (out[pos - 1].bottom - out[pos - 1].top) < w * h) {
            out[pos] = out[pos - 1];
            pos--;
        }
        out[pos] = r;
    }
    json_decref(root);
    return n;
}

struct regions {
    pthread_mutex_t lock;
    region_t set[MAX_STORED];
    int n;
    int64_t updated_ms;
};

regions_t* regions_new(void) {
    regions_t* r = calloc(1, sizeof(*r));
    pthread_mutex_init(&r->lock, NULL);
    return r;
}

void regions_free(regions_t* r) {
    if (r == NULL)
        return;
    pthread_mutex_destroy(&r->lock);
    free(r);
}

void regions_update(regions_t* r, const region_t* set, int n, int64_t now_ms) {
    if (n <= 0)
        return;
    if (n > MAX_STORED)
        n = MAX_STORED;
    pthread_mutex_lock(&r->lock);
    memcpy(r->set, set, (size_t)n * sizeof(region_t));
    r->n          = n;
    r->updated_ms = now_ms;
    pthread_mutex_unlock(&r->lock);
}

int regions_current(regions_t* r, int64_t now_ms, int64_t hold_ms, region_t* out, int max_out) {
    int n = 0;
    pthread_mutex_lock(&r->lock);
    if (r->n > 0 && now_ms - r->updated_ms <= hold_ms) {
        n = r->n < max_out ? r->n : max_out;
        memcpy(out, r->set, (size_t)n * sizeof(region_t));
    }
    pthread_mutex_unlock(&r->lock);
    return n;
}
