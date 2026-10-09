cmake_minimum_required(VERSION 3.16)

project(QtIndustrialWidgets
    VERSION 1.0.0
    DESCRIPTION "Modern, high-performance industrial instruments and SCADA widgets for Qt"
    HOMEPAGE_URL "https://github.com/paolo/QtIndustrialWidgets"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
