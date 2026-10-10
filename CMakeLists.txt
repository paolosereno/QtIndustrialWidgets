# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

project(QtIndustrialWidgets
    VERSION 2.0.1
    DESCRIPTION "Modern, high-performance industrial instruments and SCADA widgets for Qt"
    HOMEPAGE_URL "https://github.com/paolosereno/QtIndustrialWidgets"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Central binary and library output directories (ensures DLL discovery on Windows)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable compiler warnings for highest code quality
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# Qt Setup: Supports Qt 6 (primary) and Qt 5.15
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

message(STATUS "QtIndustrialWidgets: Using Qt ${QT_VERSION_MAJOR} (version ${Qt${QT_VERSION_MAJOR}_VERSION})")

# Options
option(BUILD_DESIGNER_PLUGIN "Build the Qt Designer plugin" ON)
option(BUILD_EXAMPLES "Build the gallery showcase application" ON)
option(BUILD_TESTS "Build the unit test suite" ON)
option(BUILD_DOCS "Build API documentation using Doxygen" ON)

# Check for Qt Designer module if plugin requested
if(BUILD_DESIGNER_PLUGIN)
    find_package(Qt${QT_VERSION_MAJOR}Designer QUIET)
    if(NOT Qt${QT_VERSION_MAJOR}Designer_FOUND)
        # Try alternate component name
        find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS Designer)
    endif()

    if(TARGET Qt${QT_VERSION_MAJOR}::Designer)
        message(STATUS "QtIndustrialWidgets: Qt Designer found, building designer plugin")
    else()
        message(STATUS "QtIndustrialWidgets: Qt Designer NOT found, disabling designer plugin")
        set(BUILD_DESIGNER_PLUGIN OFF)
    endif()
endif()

# Include directories helper for CMake packages
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Main library target (Separated from Designer)
add_subdirectory(src)

# Qt Designer plugin target
if(BUILD_DESIGNER_PLUGIN)
    add_subdirectory(plugin)
endif()

# Showcase Gallery example application
if(BUILD_EXAMPLES)
    add_subdirectory(examples/gallery)
endif()

# Unit tests suite
if(BUILD_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks suite (independent of ctest)
option(BUILD_BENCHMARKS "Build benchmark targets" OFF)
if(BUILD_BENCHMARKS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_subdirectory(benchmarks)
endif()

# Doxygen documentation target
if(BUILD_DOCS)
    find_package(Doxygen OPTIONAL_COMPONENTS dot)
    if(DOXYGEN_FOUND)
        message(STATUS "QtIndustrialWidgets: Doxygen found, enabling 'docs' target")
        add_subdirectory(docs)
    else()
        message(STATUS "QtIndustrialWidgets: Doxygen not found, 'docs' target disabled (install doxygen and graphviz to build docs)")
    endif()
endif()

# CMake package configuration for consumers (find_package(QtIndustrialWidgets))
set(CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/QtIndustrialWidgets")

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/QtIndustrialWidgetsConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QtIndustrialWidgetsConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/QtIndustrialWidgetsConfig.cmake"
    INSTALL_DESTINATION "${CONFIG_INSTALL_DIR}"
)

install(EXPORT QtIndustrialWidgetsTargets
    FILE QtIndustrialWidgetsTargets.cmake
    NAMESPACE QtIndustrialWidgets::
    DESTINATION "${CONFIG_INSTALL_DIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/QtIndustrialWidgetsConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/QtIndustrialWidgetsConfigVersion.cmake"
    DESTINATION "${CONFIG_INSTALL_DIR}"
)

install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/MIT.txt"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/QtIndustrialWidgets"
    RENAME "LICENSE"
)

# ========================================================================
# CPack Packaging Configuration
# ========================================================================
set(CPACK_PACKAGE_NAME "QtIndustrialWidgets")
set(CPACK_PACKAGE_VENDOR "Paolo Sereno")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern, high-performance industrial instruments and SCADA widgets for Qt")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/paolosereno/QtIndustrialWidgets")
set(CPACK_PACKAGE_CONTACT "Paolo Sereno <paolomsereno@gmail.com>")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/MIT.txt")

# Package file naming: e.g. QtIndustrialWidgets-1.0.0-Linux-x86_64
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# Debian (.deb) specific generator settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Paolo Sereno <paolomsereno@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/paolosereno/QtIndustrialWidgets")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# RPM specific generator settings
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_URL "https://github.com/paolosereno/QtIndustrialWidgets")
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)

# Default binary generators per platform
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
endif()

include(CPack)

