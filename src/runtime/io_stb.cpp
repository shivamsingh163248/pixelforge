// io_stb.cpp -- image file I/O backed by stb_image / stb_image_write.
//
// We instantiate the stb implementations exactly once, in this TU only.
// All other code uses the thin pixelforge::load_image / save_image API.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// Disable a few stb features we don't ship to keep the binary smaller.
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PIC
#define STBI_NO_PNM

#include <stb_image.h>
#include <stb_image_write.h>

#include "pixelforge/io.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace pixelforge {

namespace {

std::string lower_extension(std::string_view path) {
    auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return {};
    std::string ext(path.substr(dot + 1));
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

PixelFormat infer_format(int channels) {
    switch (channels) {
        case 1:  return PixelFormat::Gray;
        case 3:  return PixelFormat::RGB;
        case 4:  return PixelFormat::RGBA;
        default: throw IoError("unsupported channel count: " + std::to_string(channels));
    }
}

Image load_internal(std::string_view path, int desired_channels) {
    int width = 0;
    int height = 0;
    int channels_in_file = 0;
    const std::string path_str(path);
    unsigned char* raw = stbi_load(path_str.c_str(), &width, &height,
                                   &channels_in_file, desired_channels);
    if (raw == nullptr) {
        throw IoError(std::string("failed to load '") + path_str +
                      "': " + (stbi_failure_reason() ? stbi_failure_reason() : "unknown"));
    }
    const int        out_ch  = desired_channels != 0 ? desired_channels : channels_in_file;
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> data(raw, raw + pixels * static_cast<std::size_t>(out_ch));
    stbi_image_free(raw);
    return Image(static_cast<std::size_t>(width),
                 static_cast<std::size_t>(height),
                 infer_format(out_ch),
                 std::move(data));
}

}  // namespace

Image load_image(std::string_view path) {
    return load_internal(path, 0);
}

Image load_image(std::string_view path, PixelFormat force_format) {
    return load_internal(path, static_cast<int>(channel_count(force_format)));
}

void save_image(const Image& img, std::string_view path, int jpeg_quality) {
    const std::string ext      = lower_extension(path);
    const std::string path_str(path);
    const int         w        = static_cast<int>(img.width());
    const int         h        = static_cast<int>(img.height());
    const int         c        = static_cast<int>(img.channels());
    const void*       data     = img.data();
    int               result   = 0;

    if (ext == "png") {
        result = stbi_write_png(path_str.c_str(), w, h, c, data, w * c);
    } else if (ext == "jpg" || ext == "jpeg") {
        const int q = std::clamp(jpeg_quality, 1, 100);
        result = stbi_write_jpg(path_str.c_str(), w, h, c, data, q);
    } else if (ext == "bmp") {
        result = stbi_write_bmp(path_str.c_str(), w, h, c, data);
    } else if (ext == "tga") {
        result = stbi_write_tga(path_str.c_str(), w, h, c, data);
    } else {
        throw IoError("unsupported output extension: '" + ext + "'");
    }
    if (result == 0) {
        throw IoError("failed to write '" + path_str + "'");
    }
}

}  // namespace pixelforge
