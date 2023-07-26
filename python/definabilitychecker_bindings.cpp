#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "definabilitychecker.hpp"

namespace py = pybind11;

PYBIND11_MODULE(definabilitychecker_module, m) {
    py::class_<Definabilitychecker>(m, "Definabilitychecker")
        .def(py::init<>())  // Default constructor
        .def("add_clause", &Definabilitychecker::add_clause)
        .def("append_formula", &Definabilitychecker::append_formula)
        .def("has_definition", &Definabilitychecker::has_definition)
        .def("get_definition", &Definabilitychecker::get_definition);
}

