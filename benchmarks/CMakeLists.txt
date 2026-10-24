# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

add_executable(bench_StripChart
    bench_StripChart.cpp
)

target_link_libraries(bench_StripChart PRIVATE
    QtIndustrialWidgets
    Qt6::Widgets
    Qt6::Test
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

add_executable(bench_AnalysisChart
    bench_AnalysisChart.cpp
)

target_link_libraries(bench_AnalysisChart PRIVATE
    QtIndustrialWidgets
    Qt6::Widgets
)

target_include_directories(bench_AnalysisChart PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(bench_AnalysisChart PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
    OUTPUT_NAME bench_AnalysisChart
)
