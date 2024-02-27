cmake_minimum_required(VERSION 3.10)

add_library(interrupt_ann interrupt.cpp interrupt.hpp)

add_library(cadical_solver_ann cadical_solver.cpp cadical_solver.hpp)
target_include_directories(cadical_solver_ann PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../radical/src/)
add_dependencies(cadical_solver_ann radical)

target_link_libraries(cadical_solver_ann PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../radical/build/libcadical.a interrupt_ann)

add_library(annotatingsolver annotatingsolver.cpp annotatingsolver.hpp)
target_link_libraries(annotatingsolver PRIVATE cadical_solver_ann)

#add_executable(test test.cpp)
#target_link_libraries(test annotatingsolver)

#set_target_properties(test PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
#)