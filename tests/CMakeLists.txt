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

add_industrial_widget_test(tst_QRadialGauge)
add_industrial_widget_test(tst_QLinearGauge)
add_industrial_widget_test(tst_QSevenSegmentDisplay)
add_industrial_widget_test(tst_QLedIndicator)
add_industrial_widget_test(tst_QIndustrialKnob)
add_industrial_widget_test(tst_QStripChart)
add_industrial_widget_test(tst_QIndustrialSwitch)
add_industrial_widget_test(tst_QLevelMeter)
add_industrial_widget_test(tst_QAnnunciatorPanel)
add_industrial_widget_test(tst_QCompass)
