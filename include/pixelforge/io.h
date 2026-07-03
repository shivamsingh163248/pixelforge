// pixelforge/io.h -- image file I/O (PNG, JPEG, BMP, ...).
//
// Implementation uses vendored stb_image / stb_image_write so the
// runtime DLL has no external dependencies.
#ifndef PIXELFORGE_IO_H
#define PIXELFORGE_IO_H

#include "pixelforge/export.h"
#include "pixelforge/image.h"

#include <string>
#include <string_view>

namespace pixelforge {

// Thrown by load_image / save_image on failure.
class PIXELFORGE_API IoError : public std::runtime_error {
public:
    explicit IoError(const std::string& what) : std::runtime_error(what) {}
};

// Decode an image file. Format inferred from contents (not extension).
// Returned image is RGBA for files with alpha, otherwise RGB; pass
// `force_format` to coerce to a specific PixelFormat.
PIXELFORGE_API Image load_image(std::string_view path);
PIXELFORGE_API Image load_image(std::string_view path, PixelFormat force_format);

// Encode an image to disk. Format inferred from the file extension
// (.png, .jpg/.jpeg, .bmp). JPEG quality is 1..100.
PIXELFORGE_API void save_image(const Image& img, std::string_view path,
                               int jpeg_quality = 90);

}  // namespace pixelforge

#endif  // PIXELFORGE_IO_H
