// pixelforge/kernel.h -- 3x3 convolution primitive used by filters.
#ifndef PIXELFORGE_KERNEL_H
#define PIXELFORGE_KERNEL_H

#include "pixelforge/image.h"

#include <array>

namespace pixelforge {

using Kernel3x3 = std::array<float, 9>;

// Apply a 3x3 convolution kernel to every channel of the image, with
// "edge clamp" border handling. Returns a new image with the same format.
Image convolve3x3(const Image& src, const Kernel3x3& kernel, float bias = 0.0f);

}  // namespace pixelforge

#endif  // PIXELFORGE_KERNEL_H
