# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

# Registers a test executable at the device pixel ratios the library supports. The DPR is set
# with QT_SCALE_FACTOR (also honoured by the offscreen QPA): <name> runs at 1, <name>_dpr125 at
# 1.25, <name>_dpr150 at 1.5 and <name>_hidpi at 2.
function(add_dpr_tests test_name)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1")
    add_test(NAME ${test_name}_dpr125 COMMAND ${test_name})
    set_tests_properties(${test_name}_dpr125 PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1.25")
    add_test(NAME ${test_name}_dpr150 COMMAND ${test_name})
    set_tests_properties(${test_name}_dpr150 PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=1.5")
    add_test(NAME ${test_name}_hidpi COMMAND ${test_name})
    set_tests_properties(${test_name}_hidpi PROPERTIES ENVIRONMENT "QT_SCALE_FACTOR=2")
endfunction()

function(add_industrial_widget_test test_name)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name}
        PRIVATE
            QtIndustrialWidgets::QtIndustrialWidgets
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::Gui
            Qt${QT_VERSION_MAJOR}::Widgets
            Qt${QT_VERSION_MAJOR}::Test
    )
    target_include_directories(${test_name}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    set_target_properties(${test_name} PROPERTIES
        AUTOMOC ON
    )
    add_dpr_tests(${test_name})
endfunction()

add_industrial_widget_test(tst_RadialGauge)
add_executable(tst_LinearGauge
    tst_LinearGauge.cpp
    $<TARGET_OBJECTS:LinearGaugeScale>
)
target_link_libraries(tst_LinearGauge
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
)
target_include_directories(tst_LinearGauge
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_LinearGauge PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_LinearGauge)
add_industrial_widget_test(tst_SevenSegmentDisplay)
add_industrial_widget_test(tst_LedIndicator)
add_industrial_widget_test(tst_IndustrialKnob)
add_executable(tst_StripChart
    tst_StripChart.cpp
    $<TARGET_OBJECTS:M4Decimator>
)
target_link_libraries(tst_StripChart
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
)
target_include_directories(tst_StripChart
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_StripChart PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_StripChart)

add_industrial_widget_test(tst_IndustrialSwitch)
add_industrial_widget_test(tst_LevelMeter)
add_industrial_widget_test(tst_AnnunciatorPanel)
add_industrial_widget_test(tst_Compass)
add_executable(tst_CircularBargraph
    tst_CircularBargraph.cpp
    $<TARGET_OBJECTS:CircularBargraphSegments>
)
target_link_libraries(tst_CircularBargraph
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
)
target_include_directories(tst_CircularBargraph
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_CircularBargraph PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_CircularBargraph)

add_executable(tst_M4Decimator
    tst_M4Decimator.cpp
    $<TARGET_OBJECTS:M4Decimator>
)
target_link_libraries(tst_M4Decimator
    PRIVATE
        QtIndustrialWidgets::QtIndustrialWidgets
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
)
target_include_directories(tst_M4Decimator
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
set_target_properties(tst_M4Decimator PROPERTIES
    AUTOMOC ON
)
add_dpr_tests(tst_M4Decimator)

# TEMP (#36): deliberately failing test for the Windows CI output check. Remove before merging.
add_industrial_widget_test(tst_TempCiOutput)
