# SPDX-License-Identifier: GPL-3.0-or-later
function(add_core_test name)
    qt_add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE panelcore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS core)
endfunction()

add_core_test(tst_version core/tst_version.cpp)

add_test(NAME qt_version_pin_accepts_pinned
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=6.12.0 -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
add_test(NAME qt_version_pin_rejects_other
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=6.13.0 -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
set_tests_properties(qt_version_pin_rejects_other PROPERTIES
                     PASS_REGULAR_EXPRESSION "Qt 6.12.0 is required")
set_tests_properties(qt_version_pin_accepts_pinned qt_version_pin_rejects_other PROPERTIES LABELS cmake)

function(add_gui_test name)
    qt_add_executable(${name} ${ARGN} gui/screenshot.cpp)
    target_link_libraries(${name} PRIVATE electricity_lib Qt6::Test)
    target_compile_definitions(${name} PRIVATE SCREENSHOT_DIR="${CMAKE_BINARY_DIR}/screenshots")
    qt_import_plugins(${name} INCLUDE Qt::QOffscreenIntegrationPlugin)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        LABELS gui
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gui
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen:configfile=offscreen.json")
endfunction()

add_gui_test(tst_mainwindow gui/tst_mainwindow.cpp)
