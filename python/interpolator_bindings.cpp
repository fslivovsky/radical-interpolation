#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "interpolator.hpp"

namespace py = pybind11;

using namespace cadical_itp;

PYBIND11_MODULE(interpolator_module, m) {
    py::class_<Interpolator>(m, "Interpolator")
        .def(py::init<>())  // Default constructor
        .def("add_clause", &Interpolator::add_clause)
        .def("append_formula", &Interpolator::append_formula)
        .def("solve", &Interpolator::solve)
        .def("get_model", &Interpolator::get_model)
        .def("get_interpolant", &Interpolator::get_interpolant);
}

