// plugin_loader.cpp -- runtime dynamic loading of plugin shared objects.
//
// Windows: LoadLibraryW / GetProcAddress / FreeLibrary
// POSIX:   dlopen      / dlsym         / dlclose
#include "pixelforge/plugin.h"

#include <algorithm>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace pixelforge {

namespace {

#if defined(_WIN32)
void* os_load(const std::filesystem::path& p) {
    return reinterpret_cast<void*>(::LoadLibraryW(p.wstring().c_str()));
}
void os_unload(void* h) noexcept {
    if (h != nullptr) ::FreeLibrary(reinterpret_cast<HMODULE>(h));
}
void* os_symbol(void* h, const char* name) {
    return reinterpret_cast<void*>(
        ::GetProcAddress(reinterpret_cast<HMODULE>(h), name));
}
std::string os_error() {
    const DWORD code = ::GetLastError();
    return "WinError " + std::to_string(code);
}
#else
void* os_load(const std::filesystem::path& p) {
    return ::dlopen(p.c_str(), RTLD_NOW | RTLD_LOCAL);
}
void os_unload(void* h) noexcept {
    if (h != nullptr) ::dlclose(h);
}
void* os_symbol(void* h, const char* name) {
    return ::dlsym(h, name);
}
std::string os_error() {
    const char* err = ::dlerror();
    return err != nullptr ? err : "unknown";
}
#endif

}  // namespace

Plugin::Plugin(const std::filesystem::path& path) : path_(path) {
    handle_ = os_load(path);
    if (handle_ == nullptr) {
        throw PluginError("failed to load plugin '" + path.string() + "': " + os_error());
    }

    void* sym = os_symbol(handle_, PIXELFORGE_PLUGIN_ENTRY_SYMBOL);
    if (sym == nullptr) {
        const std::string err = os_error();
        os_unload(handle_);
        handle_ = nullptr;
        throw PluginError("plugin '" + path.string() +
                          "' missing entry symbol '" + PIXELFORGE_PLUGIN_ENTRY_SYMBOL +
                          "': " + err);
    }

    auto entry = reinterpret_cast<PixelforgePluginEntryFn>(sym);
    const PixelforgePlugin* desc = entry();
    if (desc == nullptr) {
        os_unload(handle_);
        handle_ = nullptr;
        throw PluginError("plugin '" + path.string() + "' entry returned NULL");
    }
    if (desc->abi_version != PIXELFORGE_PLUGIN_ABI_VERSION) {
        const auto got = desc->abi_version;
        os_unload(handle_);
        handle_ = nullptr;
        throw PluginError("plugin '" + path.string() + "' ABI mismatch: host=" +
                          std::to_string(PIXELFORGE_PLUGIN_ABI_VERSION) +
                          " plugin=" + std::to_string(got));
    }
    if (desc->apply == nullptr || desc->name == nullptr) {
        os_unload(handle_);
        handle_ = nullptr;
        throw PluginError("plugin '" + path.string() + "' has incomplete descriptor");
    }

    name_        = desc->name;
    version_     = desc->version != nullptr ? desc->version : "";
    description_ = desc->description != nullptr ? desc->description : "";
    apply_       = desc->apply;
    user_data_   = desc->user_data;
}

Plugin::~Plugin() { close(); }

Plugin::Plugin(Plugin&& other) noexcept
    : handle_(other.handle_)
    , path_(std::move(other.path_))
    , name_(std::move(other.name_))
    , version_(std::move(other.version_))
    , description_(std::move(other.description_))
    , apply_(other.apply_)
    , user_data_(other.user_data_) {
    other.handle_    = nullptr;
    other.apply_     = nullptr;
    other.user_data_ = nullptr;
}

Plugin& Plugin::operator=(Plugin&& other) noexcept {
    if (this != &other) {
        close();
        handle_      = other.handle_;
        path_        = std::move(other.path_);
        name_        = std::move(other.name_);
        version_     = std::move(other.version_);
        description_ = std::move(other.description_);
        apply_       = other.apply_;
        user_data_   = other.user_data_;
        other.handle_    = nullptr;
        other.apply_     = nullptr;
        other.user_data_ = nullptr;
    }
    return *this;
}

void Plugin::close() noexcept {
    if (handle_ != nullptr) {
        os_unload(handle_);
        handle_ = nullptr;
    }
}

Image Plugin::apply(const Image& input, std::string_view params_json) const {
    if (apply_ == nullptr) {
        throw PluginError("plugin '" + name_ + "' is not loaded");
    }

    PixelforgeImageView view{};
    view.width    = static_cast<std::uint32_t>(input.width());
    view.height   = static_cast<std::uint32_t>(input.height());
    view.channels = static_cast<std::uint32_t>(input.channels());
    view.stride   = static_cast<std::uint32_t>(input.stride());
    view.data     = input.data();

    PixelforgeImageBuffer out{};
    const std::string params(params_json);
    const int rc = apply_(&view, &out, params.empty() ? nullptr : params.c_str(), user_data_);
    if (rc != PIXELFORGE_OK) {
        throw PluginError("plugin '" + name_ + "' apply() failed (code " +
                          std::to_string(rc) + ")");
    }
    if (out.data == nullptr || out.free_fn == nullptr) {
        throw PluginError("plugin '" + name_ + "' returned an invalid buffer");
    }

    // Copy plugin-owned bytes into a host-owned Image, then release the plugin buffer.
    PixelFormat fmt = PixelFormat::RGBA;
    switch (out.channels) {
        case 1: fmt = PixelFormat::Gray; break;
        case 3: fmt = PixelFormat::RGB;  break;
        case 4: fmt = PixelFormat::RGBA; break;
        default:
            out.free_fn(out.data);
            throw PluginError("plugin '" + name_ + "' returned unsupported channel count " +
                              std::to_string(out.channels));
    }
    const std::size_t bytes =
        static_cast<std::size_t>(out.width) * out.height * out.channels;
    std::vector<std::uint8_t> data(out.data, out.data + bytes);
    out.free_fn(out.data);

    return Image(out.width, out.height, fmt, std::move(data));
}

std::vector<Plugin>
load_plugins_from_directory(const std::filesystem::path& dir) {
    std::vector<Plugin> plugins;
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        return plugins;
    }
#if defined(_WIN32)
    constexpr const char* kExt = ".dll";
#elif defined(__APPLE__)
    constexpr const char* kExt = ".dylib";
#else
    constexpr const char* kExt = ".so";
#endif
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        std::string lower(ext);
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (lower != kExt) continue;
        try {
            plugins.emplace_back(entry.path());
        } catch (const PluginError&) {
            // Silently skip non-plugin shared objects.
        }
    }
    std::sort(plugins.begin(), plugins.end(),
              [](const Plugin& a, const Plugin& b) { return a.name() < b.name(); });
    return plugins;
}

}  // namespace pixelforge
