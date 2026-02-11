add_library(my_sphere SHARED my_sphere.cpp)

target_link_libraries(my_sphere PRIVATE mitsuba)

target_include_directories(my_sphere PRIVATE
    ${mitsuba_SOURCE_DIR}/include
    ${mitsuba_BINARY_DIR}/include
    ${mitsuba_SOURCE_DIR}/ext/drjit/include
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/include
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/lz4
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/nanothread/include
    ${mitsuba_SOURCE_DIR}/ext/nanobind/include
    ${mitsuba_SOURCE_DIR}/ext/tinyformat
    ${mitsuba_SOURCE_DIR}/ext/asmjit/src
    ${mitsuba_SOURCE_DIR}/ext/rgb2spec
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/robin_map/include
)

set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/external/mitsuba3/plugins)
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})
set_target_properties(my_sphere PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
)
set_property(TARGET my_sphere PROPERTY FOLDER "plugins")



add_library(gpis_sphere SHARED gpis_sphere.cpp)

find_package(Eigen3 REQUIRED)

target_link_libraries(gpis_sphere PUBLIC Eigen3::Eigen PRIVATE mitsuba )

target_include_directories(gpis_sphere PRIVATE
    ${mitsuba_SOURCE_DIR}/include
    ${mitsuba_BINARY_DIR}/include
    ${mitsuba_SOURCE_DIR}/ext/drjit/include
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/include
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/lz4
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/nanothread/include
    ${mitsuba_SOURCE_DIR}/ext/nanobind/include
    ${mitsuba_SOURCE_DIR}/ext/tinyformat
    ${mitsuba_SOURCE_DIR}/ext/asmjit/src
    ${mitsuba_SOURCE_DIR}/ext/rgb2spec
    ${mitsuba_SOURCE_DIR}/ext/drjit/ext/drjit-core/ext/robin_map/include
)

set(PLUGIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/external/mitsuba3/plugins)
file(MAKE_DIRECTORY ${PLUGIN_OUTPUT_DIR})
set_target_properties(gpis_sphere PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUTPUT_DIR}
)
set_property(TARGET gpis_sphere PROPERTY FOLDER "plugins")
