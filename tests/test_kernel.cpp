#include "pixelforge/kernel.h"

#include <gtest/gtest.h>

using pixelforge::convolve3x3;
using pixelforge::Image;
using pixelforge::Kernel3x3;
using pixelforge::PixelFormat;

namespace {
constexpr Kernel3x3 kIdentity = {
    0, 0, 0,
    0, 1, 0,
    0, 0, 0,
};
constexpr Kernel3x3 kBoxBlur = {
    1.0f / 9, 1.0f / 9, 1.0f / 9,
    1.0f / 9, 1.0f / 9, 1.0f / 9,
    1.0f / 9, 1.0f / 9, 1.0f / 9,
};
}  // namespace

TEST(Kernel, IdentityPreservesPixels) {
    Image img(3, 3, PixelFormat::Gray);
    for (std::size_t i = 0; i < 9; ++i) {
        img.data()[i] = static_cast<std::uint8_t>(i * 20);
    }
    Image out = convolve3x3(img, kIdentity);
    for (std::size_t i = 0; i < 9; ++i) {
        EXPECT_EQ(out.data()[i], img.data()[i]);
    }
}

TEST(Kernel, BoxBlurOfUniformImageIsUnchanged) {
    Image img(5, 5, PixelFormat::Gray);
    std::fill_n(img.data(), img.byte_size(), std::uint8_t{100});
    Image out = convolve3x3(img, kBoxBlur);
    for (std::size_t i = 0; i < out.byte_size(); ++i) {
        EXPECT_NEAR(out.data()[i], 100, 1);
    }
}

TEST(Kernel, AlphaChannelIsPreserved) {
    Image img(3, 3, PixelFormat::RGBA);
    for (std::size_t i = 0; i < img.byte_size(); ++i) {
        img.data()[i] = 50;
    }
    // Stamp distinct alpha values.
    for (std::size_t p = 0; p < 9; ++p) {
        img.data()[p * 4 + 3] = static_cast<std::uint8_t>(p * 25);
    }
    Image out = convolve3x3(img, kBoxBlur);
    for (std::size_t p = 0; p < 9; ++p) {
        EXPECT_EQ(out.data()[p * 4 + 3], img.data()[p * 4 + 3])
            << "alpha changed at pixel " << p;
    }
}
