cmake_minimum_required(VERSION 3.10)

# Find Pybind11
find_package(pybind11 CONFIG)

if(pybind11_FOUND)
    # Build the Python bindings module
    pybind11_add_module(annotatingsolver_module annotatingsolver_bindings.cpp)
    # Add the "src" directory to the include path
    target_include_directories(annotatingsolver_module PRIVATE ${CMAKE_SOURCE_DIR}/src/)
    # Link the Python bindings module to the "annotatingsolver" library
    target_link_libraries(annotatingsolver_module PRIVATE annotatingsolver)
else()
    message(WARNING "Pybind11 not found - Python bindings will not be built.")
endif()
