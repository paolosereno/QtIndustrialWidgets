# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Consumer of an installed QtIndustrialWidgets package (#66): built by the vcpkg job of packaging.yml and by the Conan
# test_package (conanfile.py next to this file). It finds the package, links a widget and checks it draws.
cmake_minimum_required(VERSION 3.16)
project(QtIndustrialWidgetsConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QtIndustrialWidgets CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# The check runs on Qt's offscreen platform plugin. A static Qt does not load plugins at run time, so the plugin is
# linked into the executable: with Qt's own CMake packages (vcpkg) through qt_import_plugins(), otherwise from the
# files given in QIW_CONSUMER_STATIC_PLUGIN (Conan: test_package/conanfile.py), before the Qt libraries it needs.
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ${QIW_CONSUMER_STATIC_PLUGIN} QtIndustrialWidgets::QtIndustrialWidgets Qt6::Widgets)
if(TARGET Qt6::QOffscreenIntegrationPlugin AND COMMAND qt_import_plugins)
    qt_import_plugins(consumer INCLUDE Qt6::QOffscreenIntegrationPlugin)
else()
    # Without the plugin, a static Qt stops at run time ("no Qt platform plugin could be initialized"; on Windows in a
    # message box that never closes): stop here instead. Qt builds the offscreen plugin only with its freetype feature.
    get_target_property(qiw_qt_core_type Qt6::Core TYPE)
    if(qiw_qt_core_type STREQUAL "STATIC_LIBRARY" AND NOT QIW_CONSUMER_STATIC_PLUGIN)
        message(FATAL_ERROR "Static Qt without the offscreen platform plugin (Qt6::QOffscreenIntegrationPlugin)")
    endif()
endif()

# The version the package must have (the vcpkg registry job of packaging.yml passes the latest release)
set(QIW_EXPECTED_VERSION "" CACHE STRING "Version of QtIndustrialWidgets the consumer expects (empty: not checked)")
if(QIW_EXPECTED_VERSION)
    target_compile_definitions(consumer PRIVATE QIW_EXPECTED_VERSION="${QIW_EXPECTED_VERSION}")
endif()
