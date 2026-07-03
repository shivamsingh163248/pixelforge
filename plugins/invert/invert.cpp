// plugins/invert -- per-color-channel negation. Alpha is preserved.
#include "plugin_helpers.h"

namespace {

int apply_invert(const PixelforgeImageView* in,
                 PixelforgeImageBuffer*     out,
                 const char* /*params_json*/,
                 void* /*user_data*/) {
    const int rc = pf_plugin::allocate_like(in, out, in->channels);
    if (rc != PIXELFORGE_OK) return rc;

    const uint32_t pixels = in->width * in->height;
    const uint32_t ch     = in->channels;
    for (uint32_t i = 0; i < pixels; ++i) {
        for (uint32_t c = 0; c < ch; ++c) {
            const uint8_t v = in->data[i * ch + c];
            // Preserve alpha (last channel of RGBA).
            out->data[i * ch + c] =
                (ch == 4 && c == 3) ? v : static_cast<uint8_t>(255 - v);
        }
    }
    return PIXELFORGE_OK;
}

const PixelforgePlugin kPlugin = {
    PIXELFORGE_PLUGIN_ABI_VERSION,
    "invert",
    "1.0.0",
    "Invert color channels (alpha preserved)",
    &apply_invert,
    nullptr,
};

}  // namespace

extern "C" PIXELFORGE_PLUGIN_EXPORT
const PixelforgePlugin* pixelforge_plugin_entry(void) {
    return &kPlugin;
}
