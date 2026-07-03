// plugins/blur -- 3x3 box blur.
#include "kernel_common.h"

namespace {

constexpr float kBoxBlur[9] = {
    1.0f / 9, 1.0f / 9, 1.0f / 9,
    1.0f / 9, 1.0f / 9, 1.0f / 9,
    1.0f / 9, 1.0f / 9, 1.0f / 9,
};

int apply_blur(const PixelforgeImageView* in,
               PixelforgeImageBuffer*     out,
               const char* /*params_json*/,
               void* /*user_data*/) {
    return pf_plugin::convolve3x3(in, out, kBoxBlur, 0.0f);
}

const PixelforgePlugin kPlugin = {
    PIXELFORGE_PLUGIN_ABI_VERSION,
    "blur",
    "1.0.0",
    "3x3 box blur",
    &apply_blur,
    nullptr,
};

}  // namespace

extern "C" PIXELFORGE_PLUGIN_EXPORT
const PixelforgePlugin* pixelforge_plugin_entry(void) { return &kPlugin; }
