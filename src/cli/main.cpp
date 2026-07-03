// pixelforge CLI -- the user-facing executable.
//
// Usage:
//   pixelforge IN.png --filter NAME [--filter NAME ...] -o OUT.png
//   pixelforge list-filters
//   pixelforge --version
//   pixelforge --help
//
// The CLI auto-discovers plugins from a directory next to the executable:
//   <exe_dir>/../plugins      (installed / build layout)
//   <exe_dir>/plugins         (fallback)
// You can override with --plugin-dir <path>.

#include "pixelforge/io.h"
#include "pixelforge/pipeline.h"
#include "pixelforge/plugin.h"
#include "pixelforge/version.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#elif defined(__APPLE__)
#  include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* kProgName = "pixelforge";

void print_usage(std::ostream& os) {
    os << "pixelforge " << pixelforge::version_string << "\n"
       << "Cross-platform image processing CLI.\n\n"
       << "USAGE\n"
       << "  " << kProgName << " <input> --filter NAME[:JSON] [--filter ...] -o <output>\n"
       << "  " << kProgName << " list-filters [--plugin-dir DIR]\n"
       << "  " << kProgName << " --version | -V\n"
       << "  " << kProgName << " --help    | -h\n\n"
       << "OPTIONS\n"
       << "  -o, --output PATH      Output image path (.png, .jpg, .bmp, .tga)\n"
       << "  -f, --filter NAME[:J]  Filter to apply, optionally with JSON params\n"
       << "                         (may be repeated; filters run in order)\n"
       << "      --plugin-dir DIR   Override plugin discovery directory\n"
       << "      --jpeg-quality N   JPEG output quality 1-100 (default 90)\n"
       << "      --quiet            Suppress informational messages\n";
}

// Resolve the path of the running executable.
fs::path exe_path() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return {};
    return fs::path(buf);
#elif defined(__APPLE__)
    char     buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    return fs::canonical(fs::path(buf));
#else
    std::error_code ec;
    auto p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::path{} : p;
#endif
}

// Returns the first existing candidate directory that holds plugins.
fs::path default_plugin_dir() {
    const fs::path exe = exe_path();
    if (exe.empty()) return {};
    const fs::path dir = exe.parent_path();
    for (const fs::path& candidate : {dir / ".." / "lib" / "pixelforge" / "plugins",
                                       dir / ".." / "plugins",
                                       dir / "plugins"}) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
            return fs::weakly_canonical(candidate, ec);
        }
    }
    return {};
}

struct FilterSpec {
    std::string name;
    std::string params_json;  // empty if none
};

struct Args {
    std::optional<fs::path>   input;
    std::optional<fs::path>   output;
    std::vector<FilterSpec>   filters;
    std::optional<fs::path>   plugin_dir;
    int                       jpeg_quality = 90;
    bool                      quiet        = false;
    enum class Mode { Pipeline, ListFilters, Help, Version } mode = Mode::Pipeline;
};

// Split "name:json" into {name, json}; ":" inside json is preserved.
FilterSpec parse_filter(std::string_view raw) {
    const auto pos = raw.find(':');
    if (pos == std::string_view::npos) {
        return FilterSpec{std::string(raw), {}};
    }
    return FilterSpec{std::string(raw.substr(0, pos)),
                      std::string(raw.substr(pos + 1))};
}

[[noreturn]] void die(const std::string& msg) {
    std::cerr << kProgName << ": error: " << msg << "\n";
    std::cerr << "Run '" << kProgName << " --help' for usage.\n";
    std::exit(2);
}

Args parse_args(int argc, char** argv) {
    Args a;
    if (argc < 2) {
        a.mode = Args::Mode::Help;
        return a;
    }

    std::vector<std::string> tokens;
    tokens.reserve(static_cast<std::size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) tokens.emplace_back(argv[i]);

    auto next_value = [&](std::size_t& i, std::string_view flag) -> std::string {
        if (i + 1 >= tokens.size()) die(std::string(flag) + " requires a value");
        return tokens[++i];
    };

    // Subcommands first.
    if (tokens[0] == "list-filters") {
        a.mode = Args::Mode::ListFilters;
        for (std::size_t i = 1; i < tokens.size(); ++i) {
            if (tokens[i] == "--plugin-dir") {
                a.plugin_dir = next_value(i, "--plugin-dir");
            } else if (tokens[i] == "--quiet") {
                a.quiet = true;
            } else {
                die("unknown argument: " + tokens[i]);
            }
        }
        return a;
    }

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        if (t == "-h" || t == "--help") {
            a.mode = Args::Mode::Help;  return a;
        }
        if (t == "-V" || t == "--version") {
            a.mode = Args::Mode::Version;  return a;
        }
        if (t == "-o" || t == "--output") {
            a.output = next_value(i, t);
        } else if (t == "-f" || t == "--filter") {
            a.filters.push_back(parse_filter(next_value(i, t)));
        } else if (t == "--plugin-dir") {
            a.plugin_dir = next_value(i, t);
        } else if (t == "--jpeg-quality") {
            a.jpeg_quality = std::atoi(next_value(i, t).c_str());
        } else if (t == "--quiet") {
            a.quiet = true;
        } else if (!t.empty() && t.front() == '-') {
            die("unknown option: " + t);
        } else if (!a.input.has_value()) {
            a.input = t;
        } else {
            die("unexpected positional argument: " + t);
        }
    }
    return a;
}

int cmd_list_filters(const Args& a) {
    fs::path dir = a.plugin_dir.value_or(default_plugin_dir());
    if (dir.empty()) {
        std::cerr << kProgName << ": no plugin directory found\n";
        return 1;
    }
    auto plugins = pixelforge::load_plugins_from_directory(dir);
    if (!a.quiet) {
        std::cout << "Plugins in " << dir.string() << ":\n";
    }
    if (plugins.empty()) {
        std::cout << "  (none)\n";
        return 0;
    }
    std::size_t name_w = 0;
    for (const auto& p : plugins) name_w = std::max(name_w, p.name().size());
    for (const auto& p : plugins) {
        std::cout << "  " << std::string(p.name())
                  << std::string(name_w - p.name().size() + 2, ' ')
                  << p.version() << "  " << p.description() << "\n";
    }
    return 0;
}

int cmd_pipeline(const Args& a) {
    if (!a.input.has_value())  die("missing input image");
    if (!a.output.has_value()) die("missing -o/--output");
    if (a.filters.empty())     die("at least one --filter is required");

    fs::path dir = a.plugin_dir.value_or(default_plugin_dir());
    if (dir.empty()) die("no plugin directory found; pass --plugin-dir");

    auto plugins = pixelforge::load_plugins_from_directory(dir);
    if (plugins.empty()) die("no plugins discovered in " + dir.string());

    // Build pipeline by name lookup. We move the matched Plugin into
    // a heap-allocated holder captured by the Filter lambda so the
    // shared library stays loaded for the lifetime of the pipeline.
    pixelforge::Pipeline pipe;
    for (const auto& spec : a.filters) {
        auto it = std::find_if(plugins.begin(), plugins.end(),
                               [&](const pixelforge::Plugin& p) { return p.name() == spec.name; });
        if (it == plugins.end()) die("filter not found: " + spec.name);

        auto holder = std::make_shared<pixelforge::Plugin>(std::move(*it));
        plugins.erase(it);
        const std::string params = spec.params_json;
        pipe.add(std::string(holder->name()),
                 [holder, params](const pixelforge::Image& img) {
                     return holder->apply(img, params);
                 });
    }

    if (!a.quiet) {
        std::cout << "Loading " << a.input->string() << " ... ";
        std::cout.flush();
    }
    pixelforge::Image img = pixelforge::load_image(a.input->string());
    if (!a.quiet) {
        std::cout << img.width() << "x" << img.height() << "x" << img.channels() << "\n";
        std::cout << "Applying " << pipe.size() << " filter(s):";
        for (const auto& nf : pipe.filters()) std::cout << " " << nf.name;
        std::cout << "\n";
    }
    pixelforge::Image out = pipe.run(img);
    if (!a.quiet) {
        std::cout << "Writing " << a.output->string() << " ("
                  << out.width() << "x" << out.height() << "x" << out.channels() << ")\n";
    }
    pixelforge::save_image(out, a.output->string(), a.jpeg_quality);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Args a = parse_args(argc, argv);
        switch (a.mode) {
            case Args::Mode::Help:        print_usage(std::cout); return 0;
            case Args::Mode::Version:     std::cout << pixelforge::version_string << "\n"; return 0;
            case Args::Mode::ListFilters: return cmd_list_filters(a);
            case Args::Mode::Pipeline:    return cmd_pipeline(a);
        }
    } catch (const std::exception& e) {
        std::cerr << kProgName << ": " << e.what() << "\n";
        return 1;
    }
    return 0;
}
