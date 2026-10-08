#include "animal_output.h"

#include <axsdk/axevent.h>
#include <glib.h>
#include <stdlib.h>
#include <syslog.h>

#include "animal_any.h"

typedef enum { KIND_STATE, KIND_DETAILS } kind_t;

typedef struct {
    animal_output_t* out;
    kind_t kind;         // KIND_STATE: active; KIND_DETAILS: a pulse with Species and Score
    char* topic;         // last topic level: "Bird", "Any", "Detection"
    char* nice;          // name in the rule editor: "Bird", "Any animal"
    guint declaration;   // 0: not declared
    volatile gint ready;
} slot_t;

struct animal_output {
    AXEventHandler* handler;
    char** labels;
    size_t n;
    slot_t* slots;  // indexed by label, slots[n] is "Any", slots[n + 1] the details pulse
    animal_any_t* any;
};

typedef struct {
    slot_t* slot;
    bool detected;
    double score;
    const char* species;
} send_job_t;

static void declaration_complete(guint declaration, gpointer user_data) {
    slot_t* slot = user_data;
    syslog(LOG_INFO, "Event declaration %u complete for %s", declaration, slot->topic);
    g_atomic_int_set(&slot->ready, 1);
}

static void declare(animal_output_t* out, slot_t* slot, kind_t kind, const char* topic,
                    const char* nice) {
    slot->out   = out;
    slot->kind  = kind;
    slot->topic = g_strdup(topic);
    slot->nice  = g_strdup(nice);
    gboolean start_value = FALSE;
    gdouble start_score  = 0.0;

    AXEventKeyValueSet* set = ax_event_key_value_set_new();
    // Axis events: topic0 CameraApplicationPlatform, then the application and the event.
    ax_event_key_value_set_add_key_value(set, "topic0", "tnsaxis", "CameraApplicationPlatform",
                                         AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(set, "topic1", "tnsaxis", "AnimalDetector",
                                         AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(set, "topic2", "tnsaxis", slot->topic,
                                         AX_VALUE_TYPE_STRING, NULL);
    // An event must be sent with exactly the keys it was declared with, and every declared data
    // key becomes an input field in the rule editor. So the per-animal events declare only the
    // state; Species and Score are on a separate pulse event.
    if (kind == KIND_STATE) {
        ax_event_key_value_set_add_key_value(set, "active", NULL, &start_value,
                                             AX_VALUE_TYPE_BOOL, NULL);
        ax_event_key_value_set_mark_as_data(set, "active", NULL, NULL);
        ax_event_key_value_set_mark_as_user_defined(set, "active", NULL, "wstype:xs:boolean",
                                                    NULL);
    } else {
        ax_event_key_value_set_add_key_value(set, "Species", NULL, "", AX_VALUE_TYPE_STRING,
                                             NULL);
        ax_event_key_value_set_add_key_value(set, "Score", NULL, &start_score,
                                             AX_VALUE_TYPE_DOUBLE, NULL);
        ax_event_key_value_set_mark_as_data(set, "Species", NULL, NULL);
        ax_event_key_value_set_mark_as_user_defined(set, "Species", NULL, "wstype:xs:string",
                                                    NULL);
        ax_event_key_value_set_mark_as_data(set, "Score", NULL, NULL);
        ax_event_key_value_set_mark_as_user_defined(set, "Score", NULL, "wstype:xs:float", NULL);
    }
    // The rule editor shows the nice name of the last topic level's value, so the app name goes
    // into it: "Animal Detector - Bird", like "Image Health Analytics - Block".
    // The nice name of a topic level is the one of its VALUE (the 5th argument), not of its key.
    ax_event_key_value_set_add_nice_names(set, "topic1", "tnsaxis", NULL, "Animal Detector", NULL);
    ax_event_key_value_set_add_nice_names(set, "topic2", "tnsaxis", NULL, slot->nice, NULL);

    GError* error = NULL;
    if (!ax_event_handler_declare(out->handler, set, kind == KIND_DETAILS /* stateless */,
                                  &slot->declaration,
                                  declaration_complete, slot, &error)) {
        syslog(LOG_ERR, "Cannot declare event for %s: %s", slot->topic, error->message);
        g_clear_error(&error);
        slot->declaration = 0;
    }
    ax_event_key_value_set_free(set);
}

animal_output_t* animal_output_new(char** labels, const bool* allowed, size_t n_labels) {
    animal_output_t* out = calloc(1, sizeof(*out));
    out->handler         = ax_event_handler_new();
    out->labels          = labels;
    out->n               = n_labels;
    out->slots           = calloc(n_labels + 2, sizeof(slot_t));
    out->any             = animal_any_new((int)n_labels);
    for (size_t i = 0; i < n_labels; i++) {
        if (!allowed[i])
            continue;
        char* topic = g_strdup(labels[i]);
        if (topic[0] >= 'a' && topic[0] <= 'z')
            topic[0] = (char)(topic[0] - 'a' + 'A');  // "cat" -> "Cat"
        char* nice = g_strdup_printf("Animal Detector - %s", topic);
        declare(out, &out->slots[i], KIND_STATE, topic, nice);
        g_free(nice);
        g_free(topic);
    }
    declare(out, &out->slots[n_labels], KIND_STATE, "Any", "Animal Detector - Any animal");
    declare(out, &out->slots[n_labels + 1], KIND_DETAILS, "Detection",
            "Animal Detector - Detection (Species, Score)");
    return out;
}

static gboolean send_in_main_loop(gpointer data) {
    send_job_t* job = data;
    slot_t* slot    = job->slot;

    if (g_atomic_int_get(&slot->ready)) {
        gboolean detected = job->detected;
        gdouble score     = job->score;
        AXEventKeyValueSet* set = ax_event_key_value_set_new();
        if (slot->kind == KIND_STATE) {
            ax_event_key_value_set_add_key_value(set, "active", NULL, &detected,
                                                 AX_VALUE_TYPE_BOOL, NULL);
        } else {
            ax_event_key_value_set_add_key_value(set, "Species", NULL, job->species,
                                                 AX_VALUE_TYPE_STRING, NULL);
            ax_event_key_value_set_add_key_value(set, "Score", NULL, &score,
                                                 AX_VALUE_TYPE_DOUBLE, NULL);
        }
        AXEvent* event = ax_event_new2(set, NULL);
        ax_event_key_value_set_free(set);
        GError* error = NULL;
        if (!ax_event_handler_send_event(slot->out->handler, slot->declaration, event, &error)) {
            syslog(LOG_ERR, "Cannot send event for %s: %s", slot->topic, error->message);
            g_clear_error(&error);
        }
        ax_event_free(event);
    } else {
        syslog(LOG_WARNING, "Event for %s not sent: declaration not complete", slot->topic);
    }
    g_free(job);
    return G_SOURCE_REMOVE;
}

static void send_slot(slot_t* slot, const char* species, bool detected, double score) {
    if (slot->declaration == 0)
        return;
    send_job_t* job = g_new0(send_job_t, 1);
    job->slot       = slot;
    job->detected   = detected;
    job->score      = score;
    job->species    = species;  // a label, lives as long as the app
    g_main_context_invoke(NULL, send_in_main_loop, job);
}

void animal_output_send(animal_output_t* out, int label, bool detected, double score) {
    if (label < 0 || (size_t)label >= out->n)
        return;
    send_slot(&out->slots[label], out->labels[label], detected, score);
    int any = animal_any_update(out->any, label, detected);
    if (any != 0)
        send_slot(&out->slots[out->n], out->labels[label], any > 0, score);
    if (detected)  // one pulse per start with the species and the score
        send_slot(&out->slots[out->n + 1], out->labels[label], true, score);
}

void animal_output_free(animal_output_t* out) {
    if (out == NULL)
        return;
    for (size_t i = 0; i <= out->n + 1; i++) {
        if (out->slots[i].declaration != 0)
            ax_event_handler_undeclare(out->handler, out->slots[i].declaration, NULL);
        g_free(out->slots[i].topic);
        g_free(out->slots[i].nice);
    }
    ax_event_handler_free(out->handler);
    animal_any_free(out->any);
    free(out->slots);
    free(out);
}
