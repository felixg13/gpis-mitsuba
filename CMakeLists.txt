cmake_minimum_required(VERSION 3.21)
project(gpis_mitusba LANGUAGES CXX)

# Path to your tracked config
set(TRACKED_MITSUBA_CONF "${CMAKE_CURRENT_SOURCE_DIR}/resources/mitsuba.conf")

# Destination in build dir (same place as plugins)
set(DEST_MITSUBA_CONF "${CMAKE_BINARY_DIR}/external/mitsuba3/mitsuba.conf")

# Copy the tracked config over
message(STATUS "Copying tracked mitsuba.conf to build directory")
configure_file("${TRACKED_MITSUBA_CONF}" "${DEST_MITSUBA_CONF}" COPYONLY)
set(MI_ENABLE_EMBREE OFF)
add_subdirectory(external/mitsuba3)

add_subdirectory(src)
