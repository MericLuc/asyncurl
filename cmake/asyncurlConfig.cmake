add_library(${LIBRARY_NAME} SHARED ${SOURCES})

add_library(${PROJECT_NAME}::${LIBRARY_NAME} 
  ALIAS ${LIBRARY_NAME})

find_package(OpenSSL      REQUIRED)
find_package(CURL         REQUIRED)
find_package(miniLoop 1.0 REQUIRED)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS      "-Wl,-rpath,${CMAKE_INSTALL_PREFIX}/lib" )
set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX                                      )
set_target_properties(${LIBRARY_NAME} PROPERTIES PUBLIC_HEADER   "${HEADERS_PUBLIC}"                      )
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME     "${LIBRARY_NAME}"                        )
set_target_properties(${LIBRARY_NAME} PROPERTIES SUFFIX          ".so"                                    )
set_target_properties(${LIBRARY_NAME} PROPERTIES PREFIX          "lib"                                    )

target_compile_definitions(
    ${LIBRARY_NAME} 
    PUBLIC
        "${PROJECT_NAME_UPPERCASE}_DEBUG=$<CONFIG:Debug>")

target_link_directories(${LIBRARY_NAME}
    PUBLIC
        ${CMAKE_INSTALL_PREFIX}/lib
)

target_include_directories(
    ${LIBRARY_NAME} 
    PUBLIC
        $<BUILD_INTERFACE:${INC_DIR}>
        $<BUILD_INTERFACE:${GENERATED_HEADERS_DIR}>
        $<INSTALL_INTERFACE:.>
    PRIVATE
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
        ${CURL_INCLUDE_DIR}
)

target_link_libraries(
    ${LIBRARY_NAME}
    PUBLIC
        miniLoop
        ${CURL_LIBRARIES}
)

install(
    TARGETS                       "${LIBRARY_NAME}"
    EXPORT                        "${TARGETS_EXPORT_NAME}"
    LIBRARY         DESTINATION   "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE         DESTINATION   "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME         DESTINATION   "${CMAKE_INSTALL_BINDIR}"
    INCLUDES        DESTINATION   "${CMAKE_INSTALL_INCLUDEDIR}"
    PUBLIC_HEADER   DESTINATION   "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_FOLDER}"
)

install(
    FILES       "${GENERATED_HEADERS_DIR}/${LIBRARY_FOLDER}/version.h"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${LIBRARY_FOLDER}"
)

install(
    FILES       "${PROJECT_CONFIG_FILE}"
                "${VERSION_CONFIG_FILE}"
    DESTINATION "${CONFIG_INSTALL_DIR}"
)

install(
  EXPORT      "${TARGETS_EXPORT_NAME}"
  FILE        "${PROJECT_NAME}Targets.cmake"
  DESTINATION "${CONFIG_INSTALL_DIR}"
  NAMESPACE   "${PROJECT_NAME}::"
)
