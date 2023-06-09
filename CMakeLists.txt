cmake_minimum_required(VERSION 3.10)

project(Cadical-Interpolate)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CONFIGURE_COMMAND configure --debug)
else()
    set(CONFIGURE_COMMAND configure)
endif()

include(ExternalProject)
ExternalProject_Add(radical
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/radical
    CONFIGURE_COMMAND ${CMAKE_SOURCE_DIR}/radical/${CONFIGURE_COMMAND} CXXFLAGS=-fPIC
    BUILD_COMMAND make
    INSTALL_COMMAND ""
    BUILD_IN_SOURCE 1
)

set(ABC_USE_NAMESPACE "abc")
add_subdirectory(abc EXCLUDE_FROM_ALL)

# Set the C++ Standard to C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(src)
add_subdirectory(python)
