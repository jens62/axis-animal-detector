#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static char* get_string(AXParameter* handle, const char* name) {
    GError* error = NULL;
    gchar* value  = NULL;

    if (!ax_parameter_get(handle, name, &value, &error)) {
        syslog(LOG_ERR, "Cannot read parameter %s: %s", name, error->message);
        g_clear_error(&error);
        return g_strdup("");
    }
    g_strstrip(value);
    return value;
}

static bool get_bool(AXParameter* handle, const char* name) {
    char* value = get_string(handle, name);
    bool yes    = strcmp(value, "yes") == 0;
    g_free(value);
    return yes;
}

static int get_int(AXParameter* handle, const char* name, int fallback, int min, int max) {
    char* value = get_string(handle, name);
    char* end   = NULL;
    long n      = strtol(value, &end, 10);
    int result  = (end != value && *end == '\0' && n >= min && n <= max) ? (int)n : fallback;
    g_free(value);
    return result;
}

void config_load(AXParameter* handle, config_t* cfg, int fallback_threshold_pct) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->threshold_pct  = get_int(handle, "Threshold", fallback_threshold_pct, 1, 100);
    cfg->start_frames   = get_int(handle, "StartFrames", 3, 1, 1000);
    cfg->hold_s         = get_int(handle, "HoldSec", 5, 0, 3600);
    cfg->debug_pct      = get_int(handle, "DebugThreshold", 0, 0, 100);
    cfg->draw_boxes     = get_bool(handle, "DrawBoxes");
    cfg->animal_classes = get_string(handle, "AnimalClasses");
}

void config_free(config_t* cfg) {
    g_free(cfg->animal_classes);
    memset(cfg, 0, sizeof(*cfg));
}

void config_log(const config_t* cfg) {
    syslog(LOG_INFO, "Threshold %d %%, start after %d frames, gone after %d s, boxes %s",
           cfg->threshold_pct, cfg->start_frames, cfg->hold_s, cfg->draw_boxes ? "yes" : "no");
    syslog(LOG_INFO, "Animal classes: %s", cfg->animal_classes);
    if (cfg->debug_pct > 0)
        syslog(LOG_INFO, "Troubleshooting: logging everything seen above %d %%", cfg->debug_pct);
}
