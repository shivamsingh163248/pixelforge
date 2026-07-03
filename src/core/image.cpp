// Currently the Image class is fully header-defined; this translation unit
// reserves a slot for future out-of-line members (e.g. file I/O hooks) and
// guarantees the static library has at least one object file on every
// toolchain that might otherwise warn about an empty archive.
#include "pixelforge/image.h"

namespace pixelforge {

// Anchor symbol so the archive is never empty.
const char* image_translation_unit_marker() noexcept {
    return "pixelforge::image";
}

}  // namespace pixelforge
