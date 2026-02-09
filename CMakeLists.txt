cmake_minimum_required(VERSION 3.21)
project(gpis_mitusba LANGUAGES CXX)

set(TRACKED_MITSUBA_CONF "${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf")
set(DEST_MITSUBA_CONF "${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba.conf")
message(STATUS "Copying tracked mitsuba.conf to build directory")
configure_file("${TRACKED_MITSUBA_CONF}" "${DEST_MITSUBA_CONF}" COPYONLY)

set(MI_ENABLE_EMBREE OFF)
add_subdirectory(external/mitsuba3)

add_subdirectory(src)

find_program(MAGICK_EXECUTABLE magick REQUIRED)

set(RENDER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/render")
file(MAKE_DIRECTORY ${RENDER_OUTPUT_DIR})

add_custom_target(render
    COMMAND ${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba 
            ${CMAKE_SOURCE_DIR}/scenes/scene.xml
            -o ${RENDER_OUTPUT_DIR}/scene.exr
    COMMAND ${MAGICK_EXECUTABLE} 
            ${RENDER_OUTPUT_DIR}/scene.exr 
            ${RENDER_OUTPUT_DIR}/scene.png
    DEPENDS mitsuba my_sphere
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Rendering scene and converting to PNG..."
    VERBATIM
)

add_custom_target(view
    COMMAND open ${RENDER_OUTPUT_DIR}/scene.png
    DEPENDS render
    COMMENT "Opening rendered image..."
    VERBATIM
)
