#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <axsdk/axparameter.h>

#define APP_NAME "animal_detector"

typedef struct {
    int threshold_pct;     // minimum score in percent
    int start_frames;      // consecutive frames before an animal is reported
    int hold_s;            // seconds without the animal until it is reported as gone
    int debug_pct;         // troubleshooting: log and draw everything seen above this score, 0 = off
    bool region_mode;      // only look at regions where the camera sees unclassified movement
    int min_animal_pct;    // smallest animal: sqrt(width*height) of its box in percent of the image, 0 = off
    int min_box_pct;       // smallest region, sqrt(width*height) in percent of the image
    int region_hold_s;     // keep looking this long after the last region
    int max_regions;       // regions looked at per frame
    bool draw_boxes;       // draw boxes of detected animals on the video stream
    char* overlay_channels;  // video channels (views) the boxes are drawn on, e.g. "0,1"
    char* animal_classes;  // comma separated label names that count as animals
} config_t;

/** Reads all parameters; `fallback_threshold_pct` is used if Threshold can't be read. */
void config_load(AXParameter* handle, config_t* cfg, int fallback_threshold_pct);
void config_free(config_t* cfg);

/** Writes the effective settings to syslog. */
void config_log(const config_t* cfg);

#endif
