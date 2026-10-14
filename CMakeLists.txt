# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

#if(NOT DEFINED ENV{IDF_PATH})
#    set(ENV{IDF_PATH} "C:/esp/v6.0.2/esp-idf") # <-- Adjust this path to match your PC setup
#endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMMON_REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/esp32_common")

execute_process(
    COMMAND git -C "${COMMON_REPO_DIR}" describe --tags --long --dirty --always
    OUTPUT_VARIABLE COMMON_REPO_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_DESCRIBE_RESULT
)

if(NOT GIT_DESCRIBE_RESULT EQUAL 0 OR COMMON_REPO_VERSION STREQUAL "")
    set(COMMON_REPO_VERSION "unknown")
endif()

project(esp32-prov)
