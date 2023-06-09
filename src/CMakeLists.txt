cmake_minimum_required(VERSION 3.10)

add_library(interrupt interrupt.cpp interrupt.hpp)

add_library(cadical_solver cadical_solver.cpp cadical_solver.hpp)
target_include_directories(cadical_solver PUBLIC ${CMAKE_SOURCE_DIR}/radical/src/)
add_dependencies(cadical_solver radical)

target_link_libraries(cadical_solver PUBLIC ${CMAKE_SOURCE_DIR}/radical/build/libcadical.a interrupt)

add_library(interpolator interpolator.cpp interpolator.hpp)
target_link_libraries(interpolator cadical_solver libabc-pic)
target_include_directories(interpolator PRIVATE ${CMAKE_SOURCE_DIR}/abc/src/abc/)

add_library(definabilitychecker definabilitychecker.cpp definabilitychecker.hpp)
target_link_libraries(definabilitychecker interpolator)

add_executable(test main.cpp)
target_link_libraries(test definabilitychecker libabc-pic)
target_include_directories(test PRIVATE ${CMAKE_SOURCE_DIR}/abc/src/abc/)

set_target_properties(test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)