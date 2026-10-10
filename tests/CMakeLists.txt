# SPDX-FileCopyrightText: 2026 Paolo Sereno <paolomsereno@gmail.com>
#
# SPDX-License-Identifier: MIT

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
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

add_industrial_widget_test(tst_RadialGauge)
add_industrial_widget_test(tst_LinearGauge)
add_industrial_widget_test(tst_SevenSegmentDisplay)
add_industrial_widget_test(tst_LedIndicator)
add_industrial_widget_test(tst_IndustrialKnob)
add_industrial_widget_test(tst_StripChart)
add_industrial_widget_test(tst_IndustrialSwitch)
add_industrial_widget_test(tst_LevelMeter)
add_industrial_widget_test(tst_AnnunciatorPanel)
add_industrial_widget_test(tst_Compass)

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
add_test(NAME tst_M4Decimator COMMAND tst_M4Decimator)
