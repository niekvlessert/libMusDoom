cmake_minimum_required(VERSION 3.10)
project(libmusdoom VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(BUILD_SHARED_LIBS "Build shared library" ON)
option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_TESTS "Build test programs" ON)

# Source files
set(MUSDOOM_SOURCES
    src/libmusdoom.c
    src/opl3.c
    src/mus_player.c
    src/mus2mid.c
    src/memio.c
)

set(MUSDOOM_HEADERS
    src/libmusdoom.h
    src/opl3.h
    src/doom_music.h
    src/internal/types.h
    src/mus2mid.h
    src/memio.h
)

# Create library
add_library(musdoom ${MUSDOOM_SOURCES})

# Set library properties
set_target_properties(musdoom PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "src/libmusdoom.h"
    OUTPUT_NAME "musdoom"
)

# Include directories
target_include_directories(musdoom
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

# Compiler flags
if(MSVC)
    target_compile_definitions(musdoom PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(musdoom PRIVATE -Wall -Wextra -pedantic)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS musdoom
    EXPORT musdoom-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${MUSDOOM_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/musdoom
)

# Export targets
install(EXPORT musdoom-targets
    FILE musdoom-targets.cmake
    NAMESPACE musdoom::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/musdoom
)

# Package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/musdoom-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/musdoom-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/musdoom
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/musdoom-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/musdoom-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/musdoom-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/musdoom
)

# Tools
add_executable(wadextract tools/wadextract.c)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# pkg-config file
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig/musdoom.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/musdoom.pc"
    @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/musdoom.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
