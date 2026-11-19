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
