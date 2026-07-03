#include "pixelforge/colorspace.h"

#include <algorithm>
#include <cmath>

namespace pixelforge {

namespace {
constexpr float kRedWeight   = 0.299f;
constexpr float kGreenWeight = 0.587f;
constexpr float kBlueWeight  = 0.114f;

inline std::uint8_t saturate_u8(float v) noexcept {
    if (v <= 0.0f)   return 0;
    if (v >= 255.0f) return 255;
    return static_cast<std::uint8_t>(std::lround(v));
}
}  // namespace

Image to_grayscale(const Image& src) {
    Image dst(src.width(), src.height(), PixelFormat::Gray);
    const std::size_t  in_ch  = src.channels();
    const std::uint8_t* in    = src.data();
    std::uint8_t*       out   = dst.data();
    const std::size_t   pixels = src.width() * src.height();

    if (src.format() == PixelFormat::Gray) {
        std::copy_n(in, pixels, out);
        return dst;
    }

    for (std::size_t i = 0; i < pixels; ++i) {
        const float r = static_cast<float>(in[i * in_ch + 0]);
        const float g = static_cast<float>(in[i * in_ch + 1]);
        const float b = static_cast<float>(in[i * in_ch + 2]);
        out[i] = saturate_u8(kRedWeight * r + kGreenWeight * g + kBlueWeight * b);
    }
    return dst;
}

}  // namespace pixelforge
