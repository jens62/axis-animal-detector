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
    bool draw_boxes;       // draw boxes of detected animals on the video stream
    char* animal_classes;  // comma separated label names that count as animals
} config_t;

/** Reads all parameters; `fallback_threshold_pct` is used if Threshold can't be read. */
void config_load(AXParameter* handle, config_t* cfg, int fallback_threshold_pct);
void config_free(config_t* cfg);

/** Writes the effective settings to syslog. */
void config_log(const config_t* cfg);

#endif
