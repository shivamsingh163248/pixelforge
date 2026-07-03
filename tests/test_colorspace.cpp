#include "pixelforge/colorspace.h"

#include <gtest/gtest.h>

using pixelforge::Image;
using pixelforge::PixelFormat;
using pixelforge::to_grayscale;

TEST(Colorspace, GrayscaleOfPureRed) {
    Image rgb(1, 1, PixelFormat::RGB);
    rgb.at(0, 0, 0) = 255;
    Image gray = to_grayscale(rgb);
    ASSERT_EQ(gray.format(), PixelFormat::Gray);
    EXPECT_NEAR(gray.at(0, 0, 0), 76, 1);  // 0.299 * 255
}

TEST(Colorspace, GrayscaleOfPureGreen) {
    Image rgb(1, 1, PixelFormat::RGB);
    rgb.at(0, 0, 1) = 255;
    Image gray = to_grayscale(rgb);
    EXPECT_NEAR(gray.at(0, 0, 0), 150, 1);  // 0.587 * 255
}

TEST(Colorspace, GrayscaleOfPureBlue) {
    Image rgb(1, 1, PixelFormat::RGB);
    rgb.at(0, 0, 2) = 255;
    Image gray = to_grayscale(rgb);
    EXPECT_NEAR(gray.at(0, 0, 0), 29, 1);  // 0.114 * 255
}

TEST(Colorspace, GrayscaleIsIdempotent) {
    Image gray(2, 2, PixelFormat::Gray);
    gray.at(0, 0, 0) = 42;
    gray.at(1, 1, 0) = 200;
    Image again = to_grayscale(gray);
    EXPECT_EQ(again.at(0, 0, 0), 42);
    EXPECT_EQ(again.at(1, 1, 0), 200);
}
