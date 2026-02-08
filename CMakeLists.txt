cmake_minimum_required(VERSION 3.15)
project(MitsubaCustomShapes)

set(CMAKE_DISABLE_FIND_PACKAGE_nanobind TRUE)

add_subdirectory(external/mitsuba3)

add_library(my_shape MODULE src/my_shape.cpp)

target_link_libraries(my_shape PRIVATE mitsuba-core mitsuba-render)

set_target_properties(my_shape PROPERTIES PREFIX "")

set_target_properties(
  my_shape PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                      "${CMAKE_BINARY_DIR}/external/mitsuba3/plugins")
