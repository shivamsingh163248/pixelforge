#include "pixelforge/kernel.h"

#include <cmath>
#include <cstddef>

namespace pixelforge {

namespace {
inline std::uint8_t saturate_u8(float v) noexcept {
    if (v <= 0.0f)   return 0;
    if (v >= 255.0f) return 255;
    return static_cast<std::uint8_t>(std::lround(v));
}

inline std::size_t clamp_idx(long v, std::size_t hi) noexcept {
    if (v < 0) return 0;
    const std::size_t uv = static_cast<std::size_t>(v);
    return uv >= hi ? hi - 1 : uv;
}
}  // namespace

Image convolve3x3(const Image& src, const Kernel3x3& k, float bias) {
    Image dst(src.width(), src.height(), src.format());
    const std::size_t   ch  = src.channels();
    const std::size_t   w   = src.width();
    const std::size_t   h   = src.height();
    const std::uint8_t* in  = src.data();
    std::uint8_t*       out = dst.data();

    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            for (std::size_t c = 0; c < ch; ++c) {
                // Preserve alpha untouched.
                if (src.format() == PixelFormat::RGBA && c == 3) {
                    out[(y * w + x) * ch + c] = in[(y * w + x) * ch + c];
                    continue;
                }
                float acc = bias;
                for (int ky = -1; ky <= 1; ++ky) {
                    for (int kx = -1; kx <= 1; ++kx) {
                        const std::size_t sx = clamp_idx(static_cast<long>(x) + kx, w);
                        const std::size_t sy = clamp_idx(static_cast<long>(y) + ky, h);
                        const float       p  =
                            static_cast<float>(in[(sy * w + sx) * ch + c]);
                        const std::size_t ki =
                            static_cast<std::size_t>((ky + 1) * 3 + (kx + 1));
                        acc += p * k[ki];
                    }
                }
                out[(y * w + x) * ch + c] = saturate_u8(acc);
            }
        }
    }
    return dst;
}

}  // namespace pixelforge
