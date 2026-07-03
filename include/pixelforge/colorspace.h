// pixelforge/colorspace.h -- color-space conversions.
#ifndef PIXELFORGE_COLORSPACE_H
#define PIXELFORGE_COLORSPACE_H

#include "pixelforge/image.h"

namespace pixelforge {

// Convert any image to single-channel grayscale using ITU-R BT.601 luma weights.
Image to_grayscale(const Image& src);

}  // namespace pixelforge

#endif  // PIXELFORGE_COLORSPACE_H
