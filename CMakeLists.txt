project(asyncurl)

cmake_minimum_required(VERSION 3.10)

set(ASYNCURL_BRIEF "Lightweight asynchronous cpp wrapper around curl")

option(GENERATE_DOC
    "Use Doxygen to generate the project documentation" OFF
)

option(BUILD_EXAMPLES
    "Build built-in examples" OFF
)

add_subdirectory(3rd/miniloop EXCLUDE_FROM_ALL)
add_subdirectory(doc)
add_subdirectory(src)
add_subdirectory(examples)
