// plugins/common/plugin_helpers.h -- tiny conveniences shared by built-in plugins.
//
// Plugins are independent shared libraries; they don't link against
// libpixelforge. They include only <pixelforge/plugin_api.h> (pure C)
// plus this small header for boilerplate they all need.
#ifndef PIXELFORGE_PLUGIN_HELPERS_H
#define PIXELFORGE_PLUGIN_HELPERS_H

#include <pixelforge/plugin_api.h>

#include <cstdlib>
#include <cstring>
#include <new>

namespace pf_plugin {

// Standard "operator new[] / delete[]" pair the host can call back into.
inline void free_buffer(uint8_t* data) {
    delete[] data;
}

// Allocate an output buffer that mirrors the input dimensions.
inline int allocate_like(const PixelforgeImageView* in,
                         PixelforgeImageBuffer*     out,
                         uint32_t                   out_channels) {
    if (in == nullptr || out == nullptr) return PIXELFORGE_ERR_GENERIC;
    const size_t bytes =
        static_cast<size_t>(in->width) * in->height * out_channels;
    auto* buf = new (std::nothrow) uint8_t[bytes];
    if (buf == nullptr) return PIXELFORGE_ERR_NOMEM;
    std::memset(buf, 0, bytes);
    out->width    = in->width;
    out->height   = in->height;
    out->channels = out_channels;
    out->stride   = in->width * out_channels;
    out->data     = buf;
    out->free_fn  = &free_buffer;
    return PIXELFORGE_OK;
}

}  // namespace pf_plugin

#endif  // PIXELFORGE_PLUGIN_HELPERS_H
