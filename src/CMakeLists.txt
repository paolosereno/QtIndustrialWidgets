add_library(QtIndustrialWidgets SHARED
    QRadialGauge.cpp
    QLinearGauge.cpp
    QSevenSegmentDisplay.cpp
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/qtindustrialwidgets_global.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/QRadialGauge.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/QLinearGauge.h
    ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets/QSevenSegmentDisplay.h
)

add_library(QtIndustrialWidgets::QtIndustrialWidgets ALIAS QtIndustrialWidgets)

target_compile_definitions(QtIndustrialWidgets
    PRIVATE
        QTINDUSTRIALWIDGETS_LIBRARY
)

target_include_directories(QtIndustrialWidgets
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(QtIndustrialWidgets
    PUBLIC
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
)

set_target_properties(QtIndustrialWidgets PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME QtIndustrialWidgets
    AUTOMOC ON
)

include(GNUInstallDirs)

install(TARGETS QtIndustrialWidgets
    EXPORT QtIndustrialWidgetsTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/QtIndustrialWidgets
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
