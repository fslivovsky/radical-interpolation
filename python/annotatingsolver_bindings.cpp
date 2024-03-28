#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "annotatingsolver.hpp"

namespace py = pybind11;

using namespace cadical_annotations;

PYBIND11_MODULE(annotatingsolver_module, m) {
    py::class_<Annotatingsolver>(m, "Annotatingsolver")
        .def(py::init<>())  // Default constructor
        .def("add_universal", &Annotatingsolver::add_clause)
        .def("create_arbiter_var", &Annotatingsolver::create_arbiter_var)
        .def("add_clause", &Annotatingsolver::add_clause)
        .def("append_formula", &Annotatingsolver::append_formula)
        .def("solve", &Annotatingsolver::solve)
        .def("get_model", &Annotatingsolver::get_model)
        .def("get_values", &Annotatingsolver::get_values)
        .def("get_annotation_conflicts", &Annotatingsolver::get_annotation_conflicts);
}

