# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
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

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE QtIndustrialWidgets::QtIndustrialWidgets Qt6::Widgets)
