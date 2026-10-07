#ifndef CROP_H
#define CROP_H

#include <stdint.h>

/** A square region of an image in pixels. */
typedef struct {
    double x;     // left
    double y;     // top
    double side;  // width and height
} crop_square_t;

/**
 * The square to look at for a box given in normalized image coordinates: centred on the box, as
 * large as its longer side plus `margin` (0.3 = 30 %) on each side, at least `min_side` pixels, and
 * moved inside the image.
 */
crop_square_t crop_square_for_box(double left, double top, double right, double bottom,
                                  unsigned img_w, unsigned img_h, double margin, unsigned min_side);

/**
 * Copies the square of an NV12 image (Y plane, then interleaved UV at half resolution) into an
 * interleaved RGB image of out_w x out_h, scaling with bilinear sampling of the luma.
 */
void crop_nv12_to_rgb(const uint8_t* y_plane, unsigned y_pitch, const uint8_t* uv_plane,
                      unsigned uv_pitch, unsigned img_w, unsigned img_h, crop_square_t square,
                      uint8_t* rgb, unsigned out_w, unsigned out_h);

#endif
