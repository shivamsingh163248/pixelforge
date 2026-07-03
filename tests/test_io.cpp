#include "pixelforge/io.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

using pixelforge::Image;
using pixelforge::IoError;
using pixelforge::PixelFormat;

namespace {
std::string tmp_path(const char* name) {
    std::filesystem::path p = PIXELFORGE_TEST_TMP_DIR;
    p /= name;
    return p.string();
}

Image make_gradient(std::size_t w, std::size_t h, PixelFormat fmt) {
    Image img(w, h, fmt);
    const std::size_t ch = img.channels();
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            for (std::size_t c = 0; c < ch; ++c) {
                img.at(x, y, c) = static_cast<std::uint8_t>((x * 7 + y * 11 + c * 31) & 0xFF);
            }
        }
    }
    return img;
}
}  // namespace

TEST(Io, PngRoundTripRgbPreservesPixels) {
    Image src = make_gradient(16, 12, PixelFormat::RGB);
    const std::string path = tmp_path("roundtrip_rgb.png");
    pixelforge::save_image(src, path);

    Image loaded = pixelforge::load_image(path);
    ASSERT_EQ(loaded.format(), PixelFormat::RGB);
    ASSERT_EQ(loaded.width(),  src.width());
    ASSERT_EQ(loaded.height(), src.height());
    for (std::size_t i = 0; i < src.byte_size(); ++i) {
        ASSERT_EQ(loaded.data()[i], src.data()[i]) << "byte " << i;
    }
}

TEST(Io, PngRoundTripRgbaPreservesPixels) {
    Image src = make_gradient(8, 8, PixelFormat::RGBA);
    const std::string path = tmp_path("roundtrip_rgba.png");
    pixelforge::save_image(src, path);

    Image loaded = pixelforge::load_image(path);
    ASSERT_EQ(loaded.format(), PixelFormat::RGBA);
    for (std::size_t i = 0; i < src.byte_size(); ++i) {
        ASSERT_EQ(loaded.data()[i], src.data()[i]);
    }
}

TEST(Io, BmpRoundTripPreservesPixels) {
    Image src = make_gradient(5, 4, PixelFormat::RGB);
    const std::string path = tmp_path("roundtrip.bmp");
    pixelforge::save_image(src, path);

    Image loaded = pixelforge::load_image(path);
    EXPECT_EQ(loaded.width(), src.width());
    EXPECT_EQ(loaded.height(), src.height());
}

TEST(Io, ForceFormatGray) {
    Image src = make_gradient(4, 4, PixelFormat::RGB);
    const std::string path = tmp_path("force_gray.png");
    pixelforge::save_image(src, path);

    Image loaded = pixelforge::load_image(path, PixelFormat::Gray);
    EXPECT_EQ(loaded.format(), PixelFormat::Gray);
    EXPECT_EQ(loaded.byte_size(), src.width() * src.height());
}

TEST(Io, LoadMissingFileThrows) {
    EXPECT_THROW(pixelforge::load_image(tmp_path("does_not_exist.png")), IoError);
}

TEST(Io, SaveUnknownExtensionThrows) {
    Image src = make_gradient(2, 2, PixelFormat::RGB);
    EXPECT_THROW(pixelforge::save_image(src, tmp_path("out.xyz")), IoError);
}
