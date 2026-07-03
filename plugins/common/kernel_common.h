// plugins/_kernel_common -- shared 3x3 convolution helper for filter plugins.
#ifndef PF_PLUGIN_KERNEL_COMMON_H
#define PF_PLUGIN_KERNEL_COMMON_H

#include "plugin_helpers.h"

#include <cmath>

namespace pf_plugin {

inline uint8_t saturate_u8(float v) {
    if (v <= 0.0f)   return 0;
    if (v >= 255.0f) return 255;
    const long r = std::lround(v);
    return static_cast<uint8_t>(r);
}

inline uint32_t clamp_idx(long v, uint32_t hi) {
    if (v < 0) return 0;
    const uint32_t uv = static_cast<uint32_t>(v);
    return uv >= hi ? hi - 1u : uv;
}

// Apply a 3x3 kernel. Alpha (channel 3 of RGBA) is copied verbatim.
inline int convolve3x3(const PixelforgeImageView* in,
                       PixelforgeImageBuffer*     out,
                       const float                kernel[9],
                       float                      bias) {
    const int rc = allocate_like(in, out, in->channels);
    if (rc != PIXELFORGE_OK) return rc;

    const uint32_t w  = in->width;
    const uint32_t h  = in->height;
    const uint32_t ch = in->channels;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            for (uint32_t c = 0; c < ch; ++c) {
                if (ch == 4u && c == 3u) {
                    out->data[(y * w + x) * ch + c] = in->data[(y * w + x) * ch + c];
                    continue;
                }
                float acc = bias;
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        const uint32_t sx = clamp_idx(static_cast<long>(x) + kx, w);
                        const uint32_t sy = clamp_idx(static_cast<long>(y) + ky, h);
                        const float    p  =
                            static_cast<float>(in->data[(sy * w + sx) * ch + c]);
                        acc += p * kernel[(ky + 1) * 3 + (kx + 1)];
                    }
                }
                out->data[(y * w + x) * ch + c] = saturate_u8(acc);
            }
        }
    }
    return PIXELFORGE_OK;
}

}  // namespace pf_plugin

#endif  // PF_PLUGIN_KERNEL_COMMON_H
