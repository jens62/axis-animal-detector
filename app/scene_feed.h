#ifndef SCENE_FEED_H
#define SCENE_FEED_H

#include <stdbool.h>

#include "regions.h"

/**
 * Subscribes to the camera's scene metadata (com.axis.scene.frame.v1, Device Data Hub) and keeps
 * the boxes of unclassified moving objects in `regions`. Needs no manifest resource, it only reads
 * a public topic. `on_lost` is called (from another thread) if the connection to Device Data Hub
 * is lost.
 */
bool scene_feed_start(regions_t* regions, int channel_id, double min_size, void (*on_lost)(void));
void scene_feed_stop(void);

#endif
