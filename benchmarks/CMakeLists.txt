# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

add_executable(bench_StripChart
    bench_StripChart.cpp
)

target_link_libraries(bench_StripChart PRIVATE
    QtIndustrialWidgets
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Test
)

target_include_directories(bench_StripChart PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(bench_StripChart PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
    OUTPUT_NAME bench_StripChart
)
