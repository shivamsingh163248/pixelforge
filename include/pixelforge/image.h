// pixelforge/image.h -- core image container.
//
// An Image is a contiguous, row-major, 8-bit-per-channel pixel buffer.
// Channels are interleaved (e.g. RGBARGBA...). This is the fundamental
// data type passed across every PixelForge layer.
#ifndef PIXELFORGE_IMAGE_H
#define PIXELFORGE_IMAGE_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace pixelforge {

enum class PixelFormat : std::uint8_t {
    Gray = 1,
    RGB  = 3,
    RGBA = 4,
};

constexpr std::size_t channel_count(PixelFormat fmt) noexcept {
    return static_cast<std::size_t>(fmt);
}

class Image {
public:
    Image() = default;

    Image(std::size_t width, std::size_t height, PixelFormat format)
        : width_(width)
        , height_(height)
        , format_(format)
        , data_(width * height * channel_count(format), std::uint8_t{0}) {}

    Image(std::size_t width, std::size_t height, PixelFormat format,
          std::vector<std::uint8_t> data)
        : width_(width), height_(height), format_(format), data_(std::move(data)) {
        if (data_.size() != width_ * height_ * channel_count(format_)) {
            throw std::invalid_argument("Image: data size does not match dimensions/format");
        }
    }

    std::size_t  width()    const noexcept { return width_; }
    std::size_t  height()   const noexcept { return height_; }
    PixelFormat  format()   const noexcept { return format_; }
    std::size_t  channels() const noexcept { return channel_count(format_); }
    std::size_t  stride()   const noexcept { return width_ * channels(); }
    std::size_t  byte_size()const noexcept { return data_.size(); }

    std::uint8_t*       data()       noexcept { return data_.data(); }
    const std::uint8_t* data() const noexcept { return data_.data(); }

    std::uint8_t& at(std::size_t x, std::size_t y, std::size_t c) {
        return data_[index(x, y, c)];
    }
    std::uint8_t at(std::size_t x, std::size_t y, std::size_t c) const {
        return data_[index(x, y, c)];
    }

private:
    std::size_t index(std::size_t x, std::size_t y, std::size_t c) const {
        if (x >= width_ || y >= height_ || c >= channels()) {
            throw std::out_of_range("Image::at: pixel out of range");
        }
        return (y * width_ + x) * channels() + c;
    }

    std::size_t               width_  = 0;
    std::size_t               height_ = 0;
    PixelFormat               format_ = PixelFormat::RGBA;
    std::vector<std::uint8_t> data_;
};

}  // namespace pixelforge

#endif  // PIXELFORGE_IMAGE_H
