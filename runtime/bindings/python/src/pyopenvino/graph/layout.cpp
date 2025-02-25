#include "openvino/core/layout.hpp"

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>

#include "pyopenvino/graph/layout.hpp"

namespace py = pybind11;

void regclass_graph_Layout(py::module m) {
    py::class_<ov::Layout, std::shared_ptr<ov::Layout>> layout(m, "Layout");
    layout.doc() = "openvino.impl.Layout wraps ov::Layout";

    layout.def(py::init<>());
    layout.def(py::init<const std::string&>(), py::arg("layout_str"));

    // operator overloading
    layout.def(py::self == py::self);
    layout.def(py::self != py::self);
    layout.def(
        "__eq__",
        [](ov::Layout& self, std::string& dims) {
            ov::Layout other(dims);
            return self == other;
        },
        py::is_operator());
    layout.def(
        "__ne__",
        [](ov::Layout& self, std::string& dims) {
            ov::Layout other(dims);
            return self != other;
        },
        py::is_operator());

    layout.def("scalar", &ov::Layout::scalar);
    layout.def("has_name", &ov::Layout::has_name, py::arg("dimension_name"));
    layout.def("get_index_by_name", &ov::Layout::get_index_by_name, py::arg("dimension_name"));
    layout.def("to_string", &ov::Layout::to_string);
    layout.def("__str__", [](const ov::Layout& self) {
        return self.to_string();
    });
}
