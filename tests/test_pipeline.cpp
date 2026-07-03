#include "pixelforge/colorspace.h"
#include "pixelforge/pipeline.h"

#include <gtest/gtest.h>

using pixelforge::Image;
using pixelforge::Pipeline;
using pixelforge::PixelFormat;

TEST(Pipeline, EmptyPipelineReturnsInputUnchanged) {
    Image in(2, 2, PixelFormat::RGB);
    in.at(1, 1, 0) = 123;
    Pipeline p;
    EXPECT_TRUE(p.empty());
    Image out = p.run(in);
    EXPECT_EQ(out.at(1, 1, 0), 123);
}

TEST(Pipeline, AddRunsFiltersInOrder) {
    Pipeline p;
    p.add("plus_one", [](const Image& src) {
         Image dst = src;
         for (std::size_t i = 0; i < dst.byte_size(); ++i) {
             dst.data()[i] = static_cast<std::uint8_t>(dst.data()[i] + 1);
         }
         return dst;
     })
     .add("times_two", [](const Image& src) {
         Image dst = src;
         for (std::size_t i = 0; i < dst.byte_size(); ++i) {
             dst.data()[i] = static_cast<std::uint8_t>(dst.data()[i] * 2);
         }
         return dst;
     });

    EXPECT_EQ(p.size(), 2u);
    Image in(1, 1, PixelFormat::Gray);
    in.data()[0] = 5;
    Image out = p.run(in);
    EXPECT_EQ(out.data()[0], (5 + 1) * 2);
}

TEST(Pipeline, ComposesWithCoreFilter) {
    Pipeline p;
    p.add("grayscale", [](const Image& src) { return pixelforge::to_grayscale(src); });

    Image rgb(1, 1, PixelFormat::RGB);
    rgb.at(0, 0, 1) = 255;
    Image out = p.run(rgb);
    EXPECT_EQ(out.format(), PixelFormat::Gray);
    EXPECT_NEAR(out.at(0, 0, 0), 150, 1);
}
