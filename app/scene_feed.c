#include "scene_feed.h"

#include <datahub/client.h>
#include <datahub/subscriber.h>
#include <glib.h>
#include <string.h>
#include <syslog.h>

#define TOPIC "com.axis.scene.frame.v1"
#define MAX_PER_FRAME 8

static DHClient* client;
static DHSubscriber* subscriber;
static regions_t* store;
static double min_box_size;
static void (*lost_cb)(void);
static volatile gint connected;

static bool failed(DHError* err, const char* context) {
    if (err == NULL)
        return false;
    syslog(LOG_ERR, "Device Data Hub: %s failed: %s", context, dh_error_to_string(err));
    dh_error_destroy(err);
    return true;
}

static void on_sample(const DHTopicSample* sample, void* user_data) {
    (void)user_data;
    const char* json = dh_topic_data_get_json_data(dh_topic_sample_get_data(sample));
    if (json == NULL)
        return;
    region_t found[MAX_PER_FRAME];
    int n = regions_parse_frame(json, strlen(json), min_box_size, found, MAX_PER_FRAME);
    regions_update(store, found, n, g_get_monotonic_time() / 1000);
}

static void on_connection(DHConnectionState state, void* user_data) {
    (void)user_data;
    if (state == DH_CONN_CONNECTED) {
        g_atomic_int_set(&connected, 1);
    } else if (state == DH_CONN_DISCONNECTED && g_atomic_int_get(&connected)) {
        syslog(LOG_ERR, "Device Data Hub connection lost");
        if (lost_cb != NULL)
            lost_cb();
    }
}

bool scene_feed_start(regions_t* regions, int channel_id, double min_size, void (*on_lost)(void)) {
    DHError* err = NULL;
    store        = regions;
    min_box_size = min_size;
    lost_cb      = on_lost;

    client = dh_client_create("axis-animal-detector", &err);
    if (client == NULL) {
        failed(err, "create client");
        return false;
    }
    dh_client_set_logging(client, DH_LOG_WARNING, DH_LOG_TARGET_SYSLOG, &err);
    failed(err, "set logging");
    err = NULL;
    dh_client_set_connection_update_callback(client, on_connection, NULL, &err);
    if (failed(err, "set connection callback"))
        return false;
    err = NULL;
    dh_client_connect(client, &err);
    if (failed(err, "connect"))
        return false;

    err        = NULL;
    subscriber = dh_client_create_subscriber(client, "scene regions", &err);
    if (failed(err, "create subscriber"))
        return false;
    err = NULL;
    dh_subscriber_set_data_callback(subscriber, on_sample, NULL, &err);
    if (failed(err, "set data callback"))
        return false;

    err              = NULL;
    DHFilter* filter = dh_filter_create();
    dh_filter_add_topic_name(filter, TOPIC, &err);
    if (failed(err, "add topic")) {
        dh_filter_destroy(filter);
        return false;
    }
    err                  = NULL;
    DHInstanceKeys* keys = dh_instance_keys_create();
    bool bad             = false;
    dh_instance_keys_add_integer(keys, "channel_id", channel_id, &err);
    if (failed(err, "add channel_id")) {
        bad = true;
    } else {
        err = NULL;
        dh_filter_add_instance(filter, keys, &err);
        bad = failed(err, "add instance filter");
    }
    dh_instance_keys_destroy(keys);
    if (bad) {
        dh_filter_destroy(filter);
        return false;
    }

    err                         = NULL;
    DHSubscribeOptions* options = dh_subscribe_options_create();
    dh_subscribe_options_add_filter(options, filter, &err);
    dh_filter_destroy(filter);
    if (failed(err, "add filter")) {
        dh_subscribe_options_destroy(options);
        return false;
    }
    dh_subscribe_options_set_enable_data_updates(options, true);
    err = NULL;
    dh_subscriber_subscribe(subscriber, options, &err);
    dh_subscribe_options_destroy(options);
    if (failed(err, "subscribe"))
        return false;

    syslog(LOG_INFO, "Subscribed to %s (channel %d) through Device Data Hub", TOPIC, channel_id);
    return true;
}

void scene_feed_stop(void) {
    if (subscriber != NULL) {
        dh_subscriber_destroy(subscriber);
        subscriber = NULL;
    }
    if (client != NULL) {
        g_atomic_int_set(&connected, 0);
        DHError* err = NULL;
        dh_client_disconnect(client, &err);
        failed(err, "disconnect");
        dh_client_destroy(client);
        client = NULL;
    }
}
