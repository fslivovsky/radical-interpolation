cmake_minimum_required(VERSION 3.10)

project(Cadical-Interpolate)

find_package(Boost 1.7 REQUIRED)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CONFIGURE_COMMAND configure --debug)
else()
    set(CONFIGURE_COMMAND configure)
endif()

include(ExternalProject)

# Check if the target already exists
if(NOT TARGET radical)
    ExternalProject_Add(radical
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/radical
        CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/radical/${CONFIGURE_COMMAND} CXXFLAGS=-fPIC
        BUILD_COMMAND make
        INSTALL_COMMAND ""
        BUILD_IN_SOURCE 1
    )
endif()


# Set the C++ Standard to C++20.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(src)
