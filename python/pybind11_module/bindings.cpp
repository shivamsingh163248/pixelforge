// pybind11 bindings for PixelForge.
//
// Exposes:
//   - pixelforge.Image   (with NumPy buffer protocol, zero-copy)
//   - pixelforge.Pipeline
//   - pixelforge.Plugin
//   - load_image / save_image
//   - load_plugins_from_directory
//   - version_string
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/numpy.h>
#include <pybind11/functional.h>

#include "pixelforge/colorspace.h"
#include "pixelforge/image.h"
#include "pixelforge/io.h"
#include "pixelforge/kernel.h"
#include "pixelforge/pipeline.h"
#include "pixelforge/plugin.h"
#include "pixelforge/version.h"

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace py = pybind11;
namespace pf = pixelforge;

namespace {

pf::PixelFormat format_from_channels(std::size_t c) {
    switch (c) {
        case 1: return pf::PixelFormat::Gray;
        case 3: return pf::PixelFormat::RGB;
        case 4: return pf::PixelFormat::RGBA;
        default:
            throw std::invalid_argument("channels must be 1, 3, or 4");
    }
}

// Wrap a numpy array (uint8, HxW or HxWxC, C-contiguous) as a pixelforge::Image.
// Always copies -- safer for ownership; zero-copy is exposed via .numpy() on the
// returned buffer protocol (below).
pf::Image image_from_numpy(py::array array) {
    auto buf = array.request();
    if (buf.format != py::format_descriptor<std::uint8_t>::format()) {
        throw std::invalid_argument("expected dtype=uint8");
    }
    if (buf.ndim != 2 && buf.ndim != 3) {
        throw std::invalid_argument("expected 2D (HxW) or 3D (HxWxC) array");
    }
    const std::size_t h = static_cast<std::size_t>(buf.shape[0]);
    const std::size_t w = static_cast<std::size_t>(buf.shape[1]);
    const std::size_t c = (buf.ndim == 3) ? static_cast<std::size_t>(buf.shape[2]) : 1;
    pf::PixelFormat fmt = format_from_channels(c);

    // Ensure contiguous; if not, copy.
    py::array_t<std::uint8_t, py::array::c_style | py::array::forcecast> contig(array);
    auto cbuf = contig.request();
    const auto* src = static_cast<const std::uint8_t*>(cbuf.ptr);
    std::vector<std::uint8_t> data(src, src + (w * h * c));
    return pf::Image(w, h, fmt, std::move(data));
}

// Construct a numpy view that *copies* from the Image. Cheap and safe.
py::array image_to_numpy(const pf::Image& img) {
    const auto h = static_cast<py::ssize_t>(img.height());
    const auto w = static_cast<py::ssize_t>(img.width());
    const auto c = static_cast<py::ssize_t>(img.channels());
    if (c == 1) {
        py::array_t<std::uint8_t> out({h, w});
        std::memcpy(out.mutable_data(), img.data(), img.byte_size());
        return out;
    }
    py::array_t<std::uint8_t> out({h, w, c});
    std::memcpy(out.mutable_data(), img.data(), img.byte_size());
    return out;
}

}  // namespace

PYBIND11_MODULE(_pixelforge, m) {
    m.doc() = "PixelForge Python bindings (pybind11)";
    m.attr("__version__")     = pf::version_string;
    m.attr("version_string")  = pf::version_string;

    py::enum_<pf::PixelFormat>(m, "PixelFormat")
        .value("Gray", pf::PixelFormat::Gray)
        .value("RGB",  pf::PixelFormat::RGB)
        .value("RGBA", pf::PixelFormat::RGBA);

    py::class_<pf::Image>(m, "Image", py::buffer_protocol())
        .def(py::init<std::size_t, std::size_t, pf::PixelFormat>(),
             py::arg("width"), py::arg("height"), py::arg("format"))
        .def_property_readonly("width",    &pf::Image::width)
        .def_property_readonly("height",   &pf::Image::height)
        .def_property_readonly("channels", &pf::Image::channels)
        .def_property_readonly("format",   &pf::Image::format)
        .def("to_numpy", &image_to_numpy,
             "Return the pixel data as a NumPy uint8 array (copy).")
        .def_static("from_numpy", &image_from_numpy,
                    "Build an Image from a NumPy uint8 array (copy).")
        .def_buffer([](pf::Image& img) -> py::buffer_info {
            // Zero-copy NumPy view via memoryview/np.asarray.
            const py::ssize_t h = static_cast<py::ssize_t>(img.height());
            const py::ssize_t w = static_cast<py::ssize_t>(img.width());
            const py::ssize_t c = static_cast<py::ssize_t>(img.channels());
            std::vector<py::ssize_t> shape;
            std::vector<py::ssize_t> strides;
            if (c == 1) {
                shape   = {h, w};
                strides = {static_cast<py::ssize_t>(w), 1};
            } else {
                shape   = {h, w, c};
                strides = {w * c, c, 1};
            }
            return py::buffer_info(
                img.data(), sizeof(std::uint8_t),
                py::format_descriptor<std::uint8_t>::format(),
                static_cast<py::ssize_t>(shape.size()),
                shape, strides
            );
        });

    py::class_<pf::Plugin, std::shared_ptr<pf::Plugin>>(m, "Plugin")
        .def(py::init<const std::filesystem::path&>(), py::arg("path"))
        .def_property_readonly("name",        [](const pf::Plugin& p) { return std::string(p.name()); })
        .def_property_readonly("version",     [](const pf::Plugin& p) { return std::string(p.version()); })
        .def_property_readonly("description", [](const pf::Plugin& p) { return std::string(p.description()); })
        .def_property_readonly("path",        &pf::Plugin::path)
        .def("apply", &pf::Plugin::apply,
             py::arg("image"), py::arg("params_json") = std::string{});

    py::class_<pf::Pipeline>(m, "Pipeline")
        .def(py::init<>())
        .def("add", [](pf::Pipeline& self, std::string name, pf::Filter f) -> pf::Pipeline& {
                 return self.add(std::move(name), std::move(f));
             }, py::arg("name"), py::arg("filter"),
             py::return_value_policy::reference_internal)
        .def("add_plugin", [](pf::Pipeline& self, std::shared_ptr<pf::Plugin> plugin,
                              std::string params_json) -> pf::Pipeline& {
                 // Capture shared_ptr so the .dll stays loaded.
                 std::string name(plugin->name());
                 return self.add(std::move(name),
                                 [plugin, params_json](const pf::Image& img) {
                                     return plugin->apply(img, params_json);
                                 });
             }, py::arg("plugin"), py::arg("params_json") = std::string{},
             py::return_value_policy::reference_internal)
        .def("run",  &pf::Pipeline::run, py::arg("image"))
        .def("__len__", &pf::Pipeline::size)
        .def_property_readonly("filters", [](const pf::Pipeline& self) {
            std::vector<std::string> names;
            for (const auto& f : self.filters()) names.push_back(f.name);
            return names;
        });

    m.def("load_image",
          [](const std::string& path) { return pf::load_image(path); },
          py::arg("path"));
    m.def("save_image",
          [](const pf::Image& img, const std::string& path, int q) {
              pf::save_image(img, path, q);
          },
          py::arg("image"), py::arg("path"), py::arg("jpeg_quality") = 90);

    m.def("load_plugins_from_directory",
          [](const std::filesystem::path& dir) {
              auto vec = pf::load_plugins_from_directory(dir);
              // Pipeline.add_plugin wants shared_ptr<Plugin>; convert here.
              std::vector<std::shared_ptr<pf::Plugin>> out;
              out.reserve(vec.size());
              for (auto& p : vec) {
                  out.push_back(std::make_shared<pf::Plugin>(std::move(p)));
              }
              return out;
          },
          py::arg("directory"));

    m.def("to_grayscale", &pf::to_grayscale, py::arg("image"));

    py::register_exception<pf::IoError>(m,     "IoError");
    py::register_exception<pf::PluginError>(m, "PluginError");
}
