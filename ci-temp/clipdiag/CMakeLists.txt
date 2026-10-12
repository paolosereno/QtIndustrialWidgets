# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

# TEMP (#31): diagnostic for LinearGauge label clipping on macOS. Remove before merging.
cmake_minimum_required(VERSION 3.16)
project(clipdiag CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(clipdiag clipdiag.cpp)
target_link_libraries(clipdiag PRIVATE Qt6::Widgets)
