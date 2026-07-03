// plugins/grayscale -- BT.601 luma conversion to single-channel image.
#include "plugin_helpers.h"

#include <cmath>

namespace {

int apply_grayscale(const PixelforgeImageView* in,
                    PixelforgeImageBuffer*     out,
                    const char* /*params_json*/,
                    void* /*user_data*/) {
    if (in->channels != 1 && in->channels != 3 && in->channels != 4) {
        return PIXELFORGE_ERR_FORMAT;
    }
    const int rc = pf_plugin::allocate_like(in, out, 1u);
    if (rc != PIXELFORGE_OK) return rc;

    const uint32_t pixels = in->width * in->height;
    if (in->channels == 1) {
        std::memcpy(out->data, in->data, pixels);
        return PIXELFORGE_OK;
    }
    for (uint32_t i = 0; i < pixels; ++i) {
        const float r = static_cast<float>(in->data[i * in->channels + 0]);
        const float g = static_cast<float>(in->data[i * in->channels + 1]);
        const float b = static_cast<float>(in->data[i * in->channels + 2]);
        const float y = 0.299f * r + 0.587f * g + 0.114f * b;
        const long  v = std::lround(y);
        out->data[i] = static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
    }
    return PIXELFORGE_OK;
}

const PixelforgePlugin kPlugin = {
    PIXELFORGE_PLUGIN_ABI_VERSION,
    "grayscale",
    "1.0.0",
    "Convert image to single-channel BT.601 luma",
    &apply_grayscale,
    nullptr,
};

}  // namespace

extern "C" PIXELFORGE_PLUGIN_EXPORT
const PixelforgePlugin* pixelforge_plugin_entry(void) {
    return &kPlugin;
}
