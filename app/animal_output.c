#include "animal_output.h"

#include <axsdk/axevent.h>
#include <glib.h>
#include <stdlib.h>
#include <syslog.h>

typedef struct {
    animal_output_t* out;
    int label;
    guint declaration;
    volatile gint ready;
} slot_t;

struct animal_output {
    AXEventHandler* handler;
    char** labels;
    size_t n;
    slot_t* slots;  // indexed by label; declaration == 0: not an animal
};

typedef struct {
    slot_t* slot;
    bool detected;
} send_job_t;

static void declaration_complete(guint declaration, gpointer user_data) {
    slot_t* slot = user_data;
    syslog(LOG_INFO, "Event declaration %u complete for %s", declaration,
           slot->out->labels[slot->label]);
    g_atomic_int_set(&slot->ready, 1);
}

static void declare(animal_output_t* out, int label) {
    slot_t* slot  = &out->slots[label];
    slot->out     = out;
    slot->label   = label;
    char* topic   = g_strdup(out->labels[label]);
    if (topic[0] >= 'a' && topic[0] <= 'z')
        topic[0] = (char)(topic[0] - 'a' + 'A');  // "cat" -> "Cat"
    gboolean start_value = FALSE;

    AXEventKeyValueSet* set = ax_event_key_value_set_new();
    ax_event_key_value_set_add_key_value(set, "topic0", "tnsaxis", "AnimalDetector",
                                         AX_VALUE_TYPE_STRING, NULL);
    ax_event_key_value_set_add_key_value(set, "topic1", "tnsaxis", topic, AX_VALUE_TYPE_STRING,
                                         NULL);
    ax_event_key_value_set_add_key_value(set, "Detected", NULL, &start_value, AX_VALUE_TYPE_BOOL,
                                         NULL);
    ax_event_key_value_set_mark_as_data(set, "Detected", NULL, NULL);
    ax_event_key_value_set_mark_as_user_defined(set, "Detected", NULL, "wstype:xs:boolean", NULL);
    ax_event_key_value_set_add_nice_names(set, "topic0", "tnsaxis", "Animal Detector", NULL, NULL);
    ax_event_key_value_set_add_nice_names(set, "topic1", "tnsaxis", topic, NULL, NULL);
    ax_event_key_value_set_add_nice_names(set, "Detected", NULL, "Detected", NULL, NULL);

    GError* error = NULL;
    if (!ax_event_handler_declare(out->handler, set, FALSE /* stateful */, &slot->declaration,
                                  declaration_complete, slot, &error)) {
        syslog(LOG_ERR, "Cannot declare event for %s: %s", out->labels[label], error->message);
        g_clear_error(&error);
        slot->declaration = 0;
    }
    ax_event_key_value_set_free(set);
    g_free(topic);
}

animal_output_t* animal_output_new(char** labels, const bool* allowed, size_t n_labels) {
    animal_output_t* out = calloc(1, sizeof(*out));
    out->handler         = ax_event_handler_new();
    out->labels          = labels;
    out->n               = n_labels;
    out->slots           = calloc(n_labels, sizeof(slot_t));
    for (size_t i = 0; i < n_labels; i++)
        if (allowed[i])
            declare(out, (int)i);
    return out;
}

static gboolean send_in_main_loop(gpointer data) {
    send_job_t* job = data;
    slot_t* slot    = job->slot;

    if (g_atomic_int_get(&slot->ready)) {
        gboolean detected = job->detected;
        AXEventKeyValueSet* set = ax_event_key_value_set_new();
        ax_event_key_value_set_add_key_value(set, "Detected", NULL, &detected, AX_VALUE_TYPE_BOOL,
                                             NULL);
        AXEvent* event = ax_event_new2(set, NULL);
        ax_event_key_value_set_free(set);
        GError* error = NULL;
        if (!ax_event_handler_send_event(slot->out->handler, slot->declaration, event, &error)) {
            syslog(LOG_ERR, "Cannot send event for %s: %s", slot->out->labels[slot->label],
                   error->message);
            g_clear_error(&error);
        }
        ax_event_free(event);
    } else {
        syslog(LOG_WARNING, "Event for %s not sent: declaration not complete",
               slot->out->labels[slot->label]);
    }
    g_free(job);
    return G_SOURCE_REMOVE;
}

void animal_output_send(animal_output_t* out, int label, bool detected) {
    if (label < 0 || (size_t)label >= out->n || out->slots[label].declaration == 0)
        return;
    send_job_t* job = g_new0(send_job_t, 1);
    job->slot       = &out->slots[label];
    job->detected   = detected;
    g_main_context_invoke(NULL, send_in_main_loop, job);
}

void animal_output_free(animal_output_t* out) {
    if (out == NULL)
        return;
    for (size_t i = 0; i < out->n; i++)
        if (out->slots[i].declaration != 0)
            ax_event_handler_undeclare(out->handler, out->slots[i].declaration, NULL);
    ax_event_handler_free(out->handler);
    free(out->slots);
    free(out);
}
