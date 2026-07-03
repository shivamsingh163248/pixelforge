#include "pixelforge/colorspace.h"
#include "pixelforge/plugin.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

using pixelforge::Image;
using pixelforge::Plugin;
using pixelforge::PluginError;
using pixelforge::PixelFormat;

namespace {
std::filesystem::path plugin_dir() { return PIXELFORGE_TEST_PLUGIN_DIR; }

#if defined(_WIN32)
constexpr const char* kExt = ".dll";
#elif defined(__APPLE__)
constexpr const char* kExt = ".dylib";
#else
constexpr const char* kExt = ".so";
#endif

std::filesystem::path plugin_path(const char* name) {
    return plugin_dir() / (std::string("pf_") + name + kExt);
}
}  // namespace

TEST(PluginLoader, DiscoversAllBuiltInPlugins) {
    auto plugins = pixelforge::load_plugins_from_directory(plugin_dir());
    std::set<std::string> names;
    for (const auto& p : plugins) names.emplace(p.name());

    EXPECT_TRUE(names.count("blur"));
    EXPECT_TRUE(names.count("sharpen"));
    EXPECT_TRUE(names.count("edge"));
    EXPECT_TRUE(names.count("grayscale"));
    EXPECT_TRUE(names.count("invert"));
}

TEST(PluginLoader, GrayscalePluginMatchesCoreImpl) {
    Plugin gs(plugin_path("grayscale"));
    EXPECT_EQ(gs.name(), "grayscale");
    EXPECT_EQ(gs.version(), "1.0.0");

    Image rgb(4, 3, PixelFormat::RGB);
    for (std::size_t y = 0; y < rgb.height(); ++y) {
        for (std::size_t x = 0; x < rgb.width(); ++x) {
            rgb.at(x, y, 0) = static_cast<std::uint8_t>(x * 60);
            rgb.at(x, y, 1) = static_cast<std::uint8_t>(y * 80);
            rgb.at(x, y, 2) = 100;
        }
    }
    Image plug = gs.apply(rgb);
    Image core = pixelforge::to_grayscale(rgb);

    ASSERT_EQ(plug.format(), PixelFormat::Gray);
    ASSERT_EQ(plug.width(),  core.width());
    ASSERT_EQ(plug.height(), core.height());
    for (std::size_t i = 0; i < core.byte_size(); ++i) {
        EXPECT_EQ(plug.data()[i], core.data()[i]) << "mismatch at byte " << i;
    }
}

TEST(PluginLoader, InvertPluginNegatesAndPreservesAlpha) {
    Plugin inv(plugin_path("invert"));
    Image rgba(2, 1, PixelFormat::RGBA);
    rgba.at(0, 0, 0) = 10;  rgba.at(0, 0, 1) = 20;  rgba.at(0, 0, 2) = 30;  rgba.at(0, 0, 3) = 200;
    rgba.at(1, 0, 0) = 250; rgba.at(1, 0, 1) = 100; rgba.at(1, 0, 2) = 0;   rgba.at(1, 0, 3) = 50;
    Image out = inv.apply(rgba);
    EXPECT_EQ(out.at(0, 0, 0), 245);
    EXPECT_EQ(out.at(0, 0, 1), 235);
    EXPECT_EQ(out.at(0, 0, 2), 225);
    EXPECT_EQ(out.at(0, 0, 3), 200);  // alpha preserved
    EXPECT_EQ(out.at(1, 0, 3),  50);
}

TEST(PluginLoader, BlurOfUniformImageIsStableWithinOneCount) {
    Plugin blur(plugin_path("blur"));
    Image flat(5, 5, PixelFormat::Gray);
    std::fill_n(flat.data(), flat.byte_size(), std::uint8_t{120});
    Image out = blur.apply(flat);
    for (std::size_t i = 0; i < out.byte_size(); ++i) {
        EXPECT_NEAR(out.data()[i], 120, 1);
    }
}

TEST(PluginLoader, LoadingMissingFileThrows) {
    const auto missing = plugin_dir() / ("pf_nope" + std::string(kExt));
    EXPECT_THROW({ Plugin p(missing); (void)p; }, PluginError);
}

TEST(PluginLoader, LoadingNonPluginSharedObjectThrows) {
    // libpixelforge itself is a shared object that does not export the
    // plugin entry symbol -- loading it via Plugin must fail cleanly.
#if defined(_WIN32)
    auto runtime = std::filesystem::path(PIXELFORGE_TEST_PLUGIN_DIR) /
                   ".." / "bin" / "libpixelforge.dll";
#else
    auto runtime = std::filesystem::path(PIXELFORGE_TEST_PLUGIN_DIR) /
                   ".." / "bin" / "libpixelforge.so";
#endif
    if (std::filesystem::exists(runtime)) {
        EXPECT_THROW({ Plugin p(runtime); (void)p; }, PluginError);
    }
}
