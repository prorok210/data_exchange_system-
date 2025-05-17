# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(MAVLINK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/components/mavlink)
list(APPEND EXTRA_COMPONENT_DIRS ${MAVLINK_DIR})

project(data_exchange_system)
