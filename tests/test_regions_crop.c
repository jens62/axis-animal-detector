// Host-side test of the region parser/store and the crop: see tests/run.sh
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../app/crop.h"
#include "../app/regions.h"

static void test_parse(void) {
    region_t out[4];
    // Shapes from a capture: a blanket (class-less), edge noise, a classified human, a head.
    const char* frame =
        "{\"channel_id\":1,\"detections\":["
        "{\"bounding_box\":{\"bottom\":0.5,\"left\":0.12,\"right\":0.21,\"top\":0.30},"
        "\"object_track_id\":\"a\"},"
        "{\"bounding_box\":{\"bottom\":0.039,\"left\":0.99,\"right\":1.0,\"top\":0.0},"
        "\"object_track_id\":\"noise\"},"
        "{\"bounding_box\":{\"bottom\":0.6,\"left\":0.1,\"right\":0.3,\"top\":0.2},"
        "\"class\":{\"type\":\"Human\",\"score\":0.9},\"object_track_id\":\"h\"},"
        "{\"bounding_box\":{\"bottom\":0.42,\"left\":0.16,\"right\":0.24,\"top\":0.16},"
        "\"object_track_id\":\"b\"}],\"timestamp\":\"t\"}";
    int n = regions_parse_frame(frame, strlen(frame), 0.03, out, 4);
    assert(n == 2);                       // noise (too small) and the human (classified) are out
    assert(fabs(out[0].left - 0.16) < 1e-9);  // larger area first: 0.08*0.26 > 0.09*0.2? 0.0208 > 0.018
    assert(fabs(out[1].left - 0.12) < 1e-9);

    // max_out keeps the largest.
    n = regions_parse_frame(frame, strlen(frame), 0.03, out, 1);
    assert(n == 1 && fabs(out[0].left - 0.16) < 1e-9);

    // Idle frames, other shapes and garbage give nothing.
    const char* idle = "{\"channel_id\":1,\"timestamp\":\"t\"}";
    assert(regions_parse_frame(idle, strlen(idle), 0.03, out, 4) == 0);
    assert(regions_parse_frame("garbage", 7, 0.03, out, 4) == 0);
    const char* old = "{\"frame\":{\"observations\":[]}}";
    assert(regions_parse_frame(old, strlen(old), 0.03, out, 4) == 0);
}

static void test_box_size(void) {
    assert(fabs(regions_box_size(0.1, 0.1, 0.3, 0.2) - sqrt(0.2 * 0.1)) < 1e-12);
    assert(regions_box_size(0.3, 0.1, 0.1, 0.2) == 0.0);   // negative width
    assert(regions_box_size(0.1, 0.2, 0.3, 0.2) == 0.0);   // no height
    assert(regions_box_size(0.5, 0.5, 0.5, 0.5) == 0.0);
}

static void test_store(void) {
    regions_t* r = regions_new();
    region_t set[2] = {{0.1, 0.1, 0.3, 0.3}, {0.5, 0.5, 0.6, 0.6}};
    region_t out[4];

    assert(regions_current(r, 0, 3000, out, 4) == 0);        // nothing yet
    regions_update(r, set, 2, 1000);
    assert(regions_current(r, 1500, 3000, out, 4) == 2);
    assert(regions_current(r, 1500, 3000, out, 1) == 1);     // max_out
    regions_update(r, set, 0, 2500);                         // flicker: no update
    assert(regions_current(r, 3500, 3000, out, 4) == 2);     // still valid, 2.5 s after the update
    assert(regions_current(r, 4100, 3000, out, 4) == 0);     // expired after hold_ms
    regions_free(r);
}

static void test_square(void) {
    // A box in the middle of a 1920x1080 image: 0.1 wide, 0.2 high -> 192 x 216 px.
    crop_square_t sq = crop_square_for_box(0.45, 0.4, 0.55, 0.6, 1920, 1080, 0.3, 64);
    assert(fabs(sq.side - 216 * 1.6) < 1e-6);                // the longer side plus 30 % each side
    assert(fabs(sq.x + sq.side / 2 - 960) < 1e-6);           // centred
    assert(fabs(sq.y + sq.side / 2 - 540) < 1e-6);

    // Small boxes get at least min_side, boxes at the edge are moved inside the image.
    sq = crop_square_for_box(0.99, 0.0, 1.0, 0.03, 1920, 1080, 0.3, 64);
    assert(sq.side >= 64 && sq.x + sq.side <= 1920 + 1e-9 && sq.y >= 0);
    // A box larger than the image gets the whole short side.
    sq = crop_square_for_box(0.0, 0.0, 1.0, 1.0, 1920, 1080, 0.3, 64);
    assert(fabs(sq.side - 1080) < 1e-9 && sq.y == 0);
}

static void test_convert(void) {
    enum { W = 64, H = 48 };
    static uint8_t y[W * H], uv[W * H / 2];

    // Mid grey: Y 126, U = V = 128 -> about 128 in all channels.
    memset(y, 126, sizeof y);
    memset(uv, 128, sizeof uv);
    uint8_t rgb[8 * 8 * 3];
    crop_nv12_to_rgb(y, W, uv, W, W, H, (crop_square_t){0, 0, 32}, rgb, 8, 8);
    for (int i = 0; i < 8 * 8 * 3; i++)
        assert(abs(rgb[i] - 128) <= 2);

    // Left half dark, right half bright: the crop picks the right part of the image.
    for (int row = 0; row < H; row++)
        for (int col = 0; col < W; col++)
            y[row * W + col] = col < W / 2 ? 30 : 220;
    crop_nv12_to_rgb(y, W, uv, W, W, H, (crop_square_t){32, 0, 16}, rgb, 8, 8);
    assert(rgb[0] > 200);                                   // inside the bright half
    crop_nv12_to_rgb(y, W, uv, W, W, H, (crop_square_t){0, 0, 16}, rgb, 8, 8);
    assert(rgb[0] < 40);                                    // inside the dark half

    // Red-ish chroma (V high, U low) gives a red pixel.
    memset(y, 90, sizeof y);
    for (int i = 0; i < W * H / 2; i += 2) {
        uv[i]     = 90;   // U
        uv[i + 1] = 240;  // V
    }
    crop_nv12_to_rgb(y, W, uv, W, W, H, (crop_square_t){0, 0, 16}, rgb, 8, 8);
    assert(rgb[0] > rgb[1] + 60 && rgb[0] > rgb[2] + 60);
}

int main(void) {
    test_parse();
    test_box_size();
    test_store();
    test_square();
    test_convert();
    puts("regions/crop: all tests passed");
    return 0;
}
