// nanobind bindings for PixelForge.
//
// Same surface area as the pybind11 module, but compiled with nanobind
// (smaller binaries, faster compile times, requires C++17). Useful to
// compare developer ergonomics and runtime behavior side-by-side.
#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/function.h>

#include "pixelforge/colorspace.h"
#include "pixelforge/image.h"
#include "pixelforge/io.h"
#include "pixelforge/pipeline.h"
#include "pixelforge/plugin.h"
#include "pixelforge/version.h"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace nb = nanobind;
namespace pf = pixelforge;
using namespace nb::literals;

namespace {

pf::PixelFormat format_from_channels(std::size_t c) {
    switch (c) {
        case 1: return pf::PixelFormat::Gray;
        case 3: return pf::PixelFormat::RGB;
        case 4: return pf::PixelFormat::RGBA;
        default: throw std::invalid_argument("channels must be 1, 3, or 4");
    }
}

pf::Image image_from_ndarray(nb::ndarray<std::uint8_t, nb::c_contig, nb::device::cpu> arr) {
    if (arr.ndim() != 2 && arr.ndim() != 3) {
        throw std::invalid_argument("expected 2D (HxW) or 3D (HxWxC) array");
    }
    const std::size_t h = arr.shape(0);
    const std::size_t w = arr.shape(1);
    const std::size_t c = (arr.ndim() == 3) ? arr.shape(2) : 1;
    pf::PixelFormat fmt = format_from_channels(c);
    const auto* src = arr.data();
    std::vector<std::uint8_t> data(src, src + (w * h * c));
    return pf::Image(w, h, fmt, std::move(data));
}

}  // namespace

NB_MODULE(_pixelforge_nb, m) {
    m.doc() = "PixelForge Python bindings (nanobind)";
    m.attr("__version__")    = pf::version_string;
    m.attr("version_string") = pf::version_string;

    nb::enum_<pf::PixelFormat>(m, "PixelFormat")
        .value("Gray", pf::PixelFormat::Gray)
        .value("RGB",  pf::PixelFormat::RGB)
        .value("RGBA", pf::PixelFormat::RGBA);

    nb::class_<pf::Image>(m, "Image")
        .def(nb::init<std::size_t, std::size_t, pf::PixelFormat>(),
             "width"_a, "height"_a, "format"_a)
        .def_prop_ro("width",    &pf::Image::width)
        .def_prop_ro("height",   &pf::Image::height)
        .def_prop_ro("channels", &pf::Image::channels)
        .def_prop_ro("format",   &pf::Image::format)
        .def("to_numpy", [](pf::Image& img) {
            std::size_t shape[3] = {img.height(), img.width(), img.channels()};
            std::size_t ndim     = img.channels() == 1 ? 2u : 3u;
            // Copy into a Python-owned buffer.
            std::size_t bytes    = img.byte_size();
            auto* buf = new std::uint8_t[bytes];
            std::memcpy(buf, img.data(), bytes);
            nb::capsule owner(buf, [](void* p) noexcept { delete[] static_cast<std::uint8_t*>(p); });
            return nb::ndarray<nb::numpy, std::uint8_t>(buf, ndim, shape, owner);
        })
        .def_static("from_numpy", &image_from_ndarray);

    nb::class_<pf::Plugin>(m, "Plugin")
        .def(nb::init<const std::filesystem::path&>(), "path"_a)
        .def_prop_ro("name",        [](const pf::Plugin& p) { return std::string(p.name()); })
        .def_prop_ro("version",     [](const pf::Plugin& p) { return std::string(p.version()); })
        .def_prop_ro("description", [](const pf::Plugin& p) { return std::string(p.description()); })
        .def("apply", &pf::Plugin::apply, "image"_a, "params_json"_a = std::string{});

    nb::class_<pf::Pipeline>(m, "Pipeline")
        .def(nb::init<>())
        .def("add", [](pf::Pipeline& self, std::string name,
                       std::function<pf::Image(const pf::Image&)> f) -> pf::Pipeline& {
                 return self.add(std::move(name), std::move(f));
             }, "name"_a, "filter"_a, nb::rv_policy::reference_internal)
        .def("add_plugin", [](pf::Pipeline& self, std::shared_ptr<pf::Plugin> plugin,
                              std::string params_json) -> pf::Pipeline& {
                 std::string name(plugin->name());
                 return self.add(std::move(name),
                                 [plugin, params_json](const pf::Image& img) {
                                     return plugin->apply(img, params_json);
                                 });
             }, "plugin"_a, "params_json"_a = std::string{},
             nb::rv_policy::reference_internal)
        .def("run",     &pf::Pipeline::run, "image"_a)
        .def("__len__", &pf::Pipeline::size);

    m.def("load_image",
          [](const std::string& path) { return pf::load_image(path); }, "path"_a);
    m.def("save_image",
          [](const pf::Image& img, const std::string& path, int q) {
              pf::save_image(img, path, q);
          }, "image"_a, "path"_a, "jpeg_quality"_a = 90);

    m.def("load_plugins_from_directory",
          [](const std::filesystem::path& dir) {
              auto vec = pf::load_plugins_from_directory(dir);
              std::vector<std::shared_ptr<pf::Plugin>> out;
              out.reserve(vec.size());
              for (auto& p : vec) out.push_back(std::make_shared<pf::Plugin>(std::move(p)));
              return out;
          }, "directory"_a);

    m.def("to_grayscale", &pf::to_grayscale, "image"_a);

    nb::exception<pf::IoError>(m,     "IoError");
    nb::exception<pf::PluginError>(m, "PluginError");
}
