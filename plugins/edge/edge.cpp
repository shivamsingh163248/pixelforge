// plugins/edge -- 3x3 Laplacian edge detector.
#include "kernel_common.h"

namespace {

constexpr float kLaplacian[9] = {
    -1.0f, -1.0f, -1.0f,
    -1.0f,  8.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
};

int apply_edge(const PixelforgeImageView* in,
               PixelforgeImageBuffer*     out,
               const char* /*params_json*/,
               void* /*user_data*/) {
    return pf_plugin::convolve3x3(in, out, kLaplacian, 0.0f);
}

const PixelforgePlugin kPlugin = {
    PIXELFORGE_PLUGIN_ABI_VERSION,
    "edge",
    "1.0.0",
    "3x3 Laplacian edge detector",
    &apply_edge,
    nullptr,
};

}  // namespace

extern "C" PIXELFORGE_PLUGIN_EXPORT
const PixelforgePlugin* pixelforge_plugin_entry(void) { return &kPlugin; }
