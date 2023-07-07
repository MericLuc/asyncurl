cmake_minimum_required(VERSION 3.10)

project(asyncurl VERSION 1.0.0)

set(ASYNCURL_BRIEF "Lightweight asynchronous cpp wrapper around curl")

option(GENERATE_DOC
    "Use Doxygen to generate the project documentation" OFF
)

option(BUILD_EXAMPLES
    "Build built-in examples" OFF
)

include(${PROJECT_SOURCE_DIR}/cmake/SetEnv.cmake)

add_subdirectory(${LIBRARY_FOLDER})

