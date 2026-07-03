// plugins/sharpen -- classic 3x3 sharpening kernel.
#include "kernel_common.h"

namespace {

constexpr float kSharpen[9] = {
     0.0f, -1.0f,  0.0f,
    -1.0f,  5.0f, -1.0f,
     0.0f, -1.0f,  0.0f,
};

int apply_sharpen(const PixelforgeImageView* in,
                  PixelforgeImageBuffer*     out,
                  const char* /*params_json*/,
                  void* /*user_data*/) {
    return pf_plugin::convolve3x3(in, out, kSharpen, 0.0f);
}

const PixelforgePlugin kPlugin = {
    PIXELFORGE_PLUGIN_ABI_VERSION,
    "sharpen",
    "1.0.0",
    "3x3 unsharp mask",
    &apply_sharpen,
    nullptr,
};

}  // namespace

extern "C" PIXELFORGE_PLUGIN_EXPORT
const PixelforgePlugin* pixelforge_plugin_entry(void) { return &kPlugin; }
