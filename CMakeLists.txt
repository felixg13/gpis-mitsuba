cmake_minimum_required(VERSION 3.21)
project(gpis_mitusba LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TRACKED_MITSUBA_CONF "${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf")
set(DEST_MITSUBA_CONF "${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba.conf")
message(STATUS "Copying tracked mitsuba.conf to build directory")
configure_file("${TRACKED_MITSUBA_CONF}" "${DEST_MITSUBA_CONF}" COPYONLY)

set(DRJIT_ENABLE_PYTHON OFF) 
set(MI_ENABLE_EMBREE OFF)
add_subdirectory(external/mitsuba3)

add_subdirectory(src)

find_program(MAGICK_EXECUTABLE magick REQUIRED)

set(RENDER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/render")
file(MAKE_DIRECTORY ${RENDER_OUTPUT_DIR})

add_custom_target(render
    COMMAND ${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba 
            ${CMAKE_SOURCE_DIR}/scenes/gpis_scene.xml
            -o ${RENDER_OUTPUT_DIR}/render.exr
    COMMAND ${MAGICK_EXECUTABLE} 
            ${RENDER_OUTPUT_DIR}/render.exr 
            ${RENDER_OUTPUT_DIR}/render.png
    DEPENDS mitsuba gpis_sphere
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Rendering scene and converting to PNG..."
    VERBATIM
)

add_custom_target(view
    COMMAND open ${RENDER_OUTPUT_DIR}/render.png
    DEPENDS render
    COMMENT "Opening rendered image..."
    VERBATIM
)
