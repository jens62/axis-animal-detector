#include "crop.h"

#include <math.h>
#include <stddef.h>

static double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint8_t clamp8(int v) {
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

crop_square_t crop_square_for_box(double left, double top, double right, double bottom,
                                  unsigned img_w, unsigned img_h, double margin, unsigned min_side) {
    double w    = (right - left) * img_w;
    double h    = (bottom - top) * img_h;
    double cx   = (left + right) / 2.0 * img_w;
    double cy   = (top + bottom) / 2.0 * img_h;
    double side = (w > h ? w : h) * (1.0 + 2.0 * margin);
    double max_side = img_w < img_h ? img_w : img_h;

    if (side < min_side)
        side = min_side;
    if (side > max_side)
        side = max_side;

    crop_square_t sq;
    sq.side = side;
    sq.x    = clampd(cx - side / 2.0, 0.0, img_w - side);
    sq.y    = clampd(cy - side / 2.0, 0.0, img_h - side);
    return sq;
}

void crop_nv12_to_rgb(const uint8_t* y_plane, unsigned y_pitch, const uint8_t* uv_plane,
                      unsigned uv_pitch, unsigned img_w, unsigned img_h, crop_square_t sq,
                      uint8_t* rgb, unsigned out_w, unsigned out_h) {
    for (unsigned oy = 0; oy < out_h; oy++) {
        double sy = clampd(sq.y + (oy + 0.5) * sq.side / out_h - 0.5, 0.0, img_h - 1.0);
        int y0    = (int)sy;
        int y1    = y0 + 1 < (int)img_h ? y0 + 1 : y0;
        double fy = sy - y0;

        for (unsigned ox = 0; ox < out_w; ox++) {
            double sx = clampd(sq.x + (ox + 0.5) * sq.side / out_w - 0.5, 0.0, img_w - 1.0);
            int x0    = (int)sx;
            int x1    = x0 + 1 < (int)img_w ? x0 + 1 : x0;
            double fx = sx - x0;

            double top    = y_plane[y0 * y_pitch + x0] * (1.0 - fx) + y_plane[y0 * y_pitch + x1] * fx;
            double bottom = y_plane[y1 * y_pitch + x0] * (1.0 - fx) + y_plane[y1 * y_pitch + x1] * fx;
            double luma   = top * (1.0 - fy) + bottom * fy;

            // Chroma has half the resolution: nearest sample.
            const uint8_t* uv = uv_plane + (size_t)(y0 / 2) * uv_pitch + (size_t)(x0 / 2) * 2;
            int c = (int)(luma + 0.5) - 16;
            int d = uv[0] - 128;
            int e = uv[1] - 128;
            uint8_t* px = rgb + ((size_t)oy * out_w + ox) * 3;
            px[0]       = clamp8((298 * c + 409 * e + 128) >> 8);
            px[1]       = clamp8((298 * c - 100 * d - 208 * e + 128) >> 8);
            px[2]       = clamp8((298 * c + 516 * d + 128) >> 8);
        }
    }
}
