cmake_minimum_required(VERSION 3.21)
project(gpis_mitusba LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(TRACKED_MITSUBA_CONF "${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf")
set(DEST_MITSUBA_CONF "${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba.conf")
message(STATUS "Copying tracked mitsuba.conf to build directory")
configure_file("${TRACKED_MITSUBA_CONF}" "${DEST_MITSUBA_CONF}" COPYONLY)

set(MI_ENABLE_JIT ON) 
set(MI_ENABLE_EMBREE OFF) 
add_subdirectory(external/mitsuba3)

add_subdirectory(src)

find_program(MAGICK_EXECUTABLE magick REQUIRED)

set(RENDER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/render")
file(MAKE_DIRECTORY ${RENDER_OUTPUT_DIR})

set(SCENE "gpis_cbox.xml" CACHE STRING "Scene file to render")
set(RENDER_EXR ${RENDER_OUTPUT_DIR}/render.exr)
set(RENDER_PNG ${RENDER_OUTPUT_DIR}/render.png)

add_custom_command(
    OUTPUT ${RENDER_PNG}
    COMMAND ${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba
            ${CMAKE_SOURCE_DIR}/scenes/${SCENE}
            -o ${RENDER_EXR}
    COMMAND ${MAGICK_EXECUTABLE}
            ${RENDER_EXR}
            ${RENDER_PNG}
    DEPENDS
        ${CMAKE_SOURCE_DIR}/scenes/${SCENE}
        mitsuba
        gpis_sphere
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Rendering ${SCENE}..."
    VERBATIM
)

add_custom_target(render
    DEPENDS ${RENDER_PNG}
)

add_custom_target(view
    COMMAND open ${RENDER_PNG}
    DEPENDS ${RENDER_PNG}
    COMMENT "Opening rendered image..."
    VERBATIM
)
