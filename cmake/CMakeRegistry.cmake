###
# CMake Registry
#
#   Export package to CMake registry
#   CMake will be able to find it even if it has not been installed to a standard directory.
###

if( NOT (DEFINED CMAKE_REGISTRY_FOLDER) )
  set(CMAKE_REGISTRY_FOLDER "ON"
    CACHE STRING "Choose CMake registry folder." FORCE)
endif()

set_property(CACHE CMAKE_REGISTRY_FOLDER PROPERTY STRINGS
  "BUILD_FOLDER" "INSTALL_FOLDER" "OFF")
message(STATUS "CMAKE_REGISTRY_FOLDER: ${CMAKE_REGISTRY_FOLDER}")

if(CMAKE_REGISTRY_FOLDER STREQUAL "BUILD_FOLDER")
  export(PACKAGE ${PROJECT_NAME})
endif()


###
# register_package
#
# Adds an entry to the CMake registry for packages with the path of the directory
# where the package configuration file of the installed package is located in order 
# to help CMake find the package in a custom nstallation prefix.
###
function (register_package PACKAGE_FOLDER)
  if (NOT IS_ABSOLUTE "${PACKAGE_FOLDER}")
    set (PACKAGE_FOLDER "${CMAKE_INSTALL_PREFIX}/${PACKAGE_FOLDER}")
  endif ()

  string (MD5 REGISTRY_ENTRY "${PACKAGE_FOLDER}")

  if (IS_DIRECTORY "$ENV{HOME}")
    file (WRITE "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-registry-entry" "${PACKAGE_FOLDER}")
    install (
      FILES       "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-registry-entry"
      DESTINATION "$ENV{HOME}/.cmake/packages/${PROJECT_NAME}"
      RENAME      "${REGISTRY_ENTRY}"
    )
    message (STATUS "CMake registry: $ENV{HOME}/.cmake/packages/${PROJECT_NAME}")
  endif ()
endfunction ()

if(CMAKE_REGISTRY_FOLDER STREQUAL "INSTALL_FOLDER")
  register_package(${CONFIG_INSTALL_DIR})
endif ()