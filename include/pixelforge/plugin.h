// pixelforge/plugin.h -- C++ wrapper around the C plugin ABI.
//
// One Plugin object owns one loaded shared library. The destructor
// unloads it (LoadLibrary/FreeLibrary or dlopen/dlclose). Plugins are
// move-only because the OS handle is unique.
#ifndef PIXELFORGE_PLUGIN_H
#define PIXELFORGE_PLUGIN_H

#include "pixelforge/export.h"
#include "pixelforge/image.h"
#include "pixelforge/plugin_api.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pixelforge {

class PIXELFORGE_API PluginError : public std::runtime_error {
public:
    explicit PluginError(const std::string& what) : std::runtime_error(what) {}
};

class PIXELFORGE_API Plugin {
public:
    explicit Plugin(const std::filesystem::path& path);
    ~Plugin();

    Plugin(Plugin&& other) noexcept;
    Plugin& operator=(Plugin&& other) noexcept;

    Plugin(const Plugin&)            = delete;
    Plugin& operator=(const Plugin&) = delete;

    std::string_view name()        const noexcept { return name_; }
    std::string_view version()     const noexcept { return version_; }
    std::string_view description() const noexcept { return description_; }
    const std::filesystem::path& path() const noexcept { return path_; }

    // Run the plugin on an input image.
    Image apply(const Image& input, std::string_view params_json = {}) const;

private:
    void close() noexcept;

    void*                      handle_ = nullptr;  // HMODULE on Win, void* on POSIX
    std::filesystem::path      path_;
    std::string                name_;
    std::string                version_;
    std::string                description_;
    PixelforgeApplyFn          apply_  = nullptr;
    void*                      user_data_ = nullptr;
};

// Discover and load every shared library under `dir` that exposes the
// PixelForge plugin entry symbol. Files that don't are silently skipped.
PIXELFORGE_API std::vector<Plugin>
load_plugins_from_directory(const std::filesystem::path& dir);

}  // namespace pixelforge

#endif  // PIXELFORGE_PLUGIN_H
