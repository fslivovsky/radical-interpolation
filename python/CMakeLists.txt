cmake_minimum_required(VERSION 3.10)

# Find Pybind11
find_package(pybind11 CONFIG)

if(pybind11_FOUND)
    # Build the Python bindings module
    pybind11_add_module(interpolator_module interpolator_bindings.cpp)
    # Add the "src" directory to the include path
    target_include_directories(interpolator_module PRIVATE ${CMAKE_SOURCE_DIR}/src/)
    # Link the Python bindings module to the "interpolator" library
    target_link_libraries(interpolator_module PRIVATE interpolator)
    # Same for the Definabilitychecker class.
    pybind11_add_module(definabilitychecker_module definabilitychecker_bindings.cpp)
    target_include_directories(definabilitychecker_module PRIVATE ${CMAKE_SOURCE_DIR}/src/)
    target_link_libraries(definabilitychecker_module PRIVATE definabilitychecker)
else()
    message(WARNING "Pybind11 not found - Python bindings will not be built.")
endif()
