#include "pixelforge/image.h"

#include <gtest/gtest.h>

using pixelforge::Image;
using pixelforge::PixelFormat;

TEST(Image, ConstructsZeroFilled) {
    Image img(4, 3, PixelFormat::RGBA);
    EXPECT_EQ(img.width(), 4u);
    EXPECT_EQ(img.height(), 3u);
    EXPECT_EQ(img.channels(), 4u);
    EXPECT_EQ(img.byte_size(), 4u * 3u * 4u);
    for (std::size_t i = 0; i < img.byte_size(); ++i) {
        EXPECT_EQ(img.data()[i], 0);
    }
}

TEST(Image, AcceptsBufferOfMatchingSize) {
    std::vector<std::uint8_t> buf(2 * 2 * 3, 7);
    Image img(2, 2, PixelFormat::RGB, std::move(buf));
    EXPECT_EQ(img.at(0, 0, 0), 7);
    EXPECT_EQ(img.at(1, 1, 2), 7);
}

TEST(Image, RejectsBufferOfWrongSize) {
    std::vector<std::uint8_t> buf(5);
    EXPECT_THROW(Image(2, 2, PixelFormat::RGB, std::move(buf)), std::invalid_argument);
}

TEST(Image, AtThrowsOnOutOfRange) {
    Image img(2, 2, PixelFormat::Gray);
    EXPECT_THROW(img.at(2, 0, 0), std::out_of_range);
    EXPECT_THROW(img.at(0, 2, 0), std::out_of_range);
    EXPECT_THROW(img.at(0, 0, 1), std::out_of_range);
}

TEST(Image, StrideMatchesWidthTimesChannels) {
    Image img(10, 1, PixelFormat::RGB);
    EXPECT_EQ(img.stride(), 30u);
}
