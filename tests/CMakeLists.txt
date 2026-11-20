# SPDX-License-Identifier: GPL-3.0-or-later
function(add_core_test name)
    qt_add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE panelcore Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS core)
endfunction()

add_core_test(tst_version core/tst_version.cpp)
add_core_test(tst_catalog core/tst_catalog.cpp)

add_test(NAME qt_version_pin_accepts_pinned
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=6.12.0 -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
add_test(NAME qt_version_pin_rejects_other
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=6.13.0 -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
set_tests_properties(qt_version_pin_rejects_other PROPERTIES
                     PASS_REGULAR_EXPRESSION "Qt 6.12.0 is required")
set_tests_properties(qt_version_pin_accepts_pinned qt_version_pin_rejects_other PROPERTIES LABELS cmake)

# The CI Qt build (scripts/build_qt.cmake) must build the version QtVersion.cmake requires.
set(ci_qt_script ${PROJECT_SOURCE_DIR}/scripts/build_qt.cmake)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ci_qt_script})
file(STRINGS ${ci_qt_script} ci_qt_line REGEX "^set\\(QT_VERSION ")
string(REGEX REPLACE "^set\\(QT_VERSION ([^)]*)\\)$" "\\1" ci_qt_version "${ci_qt_line}")
add_test(NAME qt_version_pin_matches_ci
         COMMAND ${CMAKE_COMMAND} -DQt6_VERSION=${ci_qt_version} -P ${PROJECT_SOURCE_DIR}/cmake/QtVersion.cmake)
set_tests_properties(qt_version_pin_matches_ci PROPERTIES LABELS cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # tst_version links libstdc++ dynamically (only the app gets -static-libstdc++),
    # so the release dependency check must reject it.
    add_test(NAME linux_deps_check_rejects_dynamic_cxx
             COMMAND ${PROJECT_SOURCE_DIR}/scripts/check_linux_deps.sh $<TARGET_FILE:tst_version>)
    set_tests_properties(linux_deps_check_rejects_dynamic_cxx PROPERTIES
        LABELS scripts PASS_REGULAR_EXPRESSION "not allowed: libstdc\\+\\+\\.so\\.6")
endif()

function(add_gui_test name)
    qt_add_executable(${name} ${ARGN} gui/screenshot.cpp gui/testsettings.cpp
        gui/menuclicks.cpp)
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
add_gui_test(tst_preferences gui/tst_preferences.cpp)
add_gui_test(tst_catalogart gui/tst_catalogart.cpp)
add_gui_test(tst_palette gui/tst_palette.cpp)

# Strings must be extracted before scanning: the fixture .ts has no unfinished entries,
# only the fixture source has a new one.
add_test(NAME translations_check_extracts_new_text
         COMMAND ${CMAKE_COMMAND} -DLUPDATE=$<TARGET_FILE:Qt6::lupdate>
                 -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/translations
                 -DTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/translations
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/translations_check_fixture
                 -P ${PROJECT_SOURCE_DIR}/cmake/CheckTranslations.cmake)
set_tests_properties(translations_check_extracts_new_text PROPERTIES
    LABELS translations PASS_REGULAR_EXPRESSION "untranslated_ru\\.ts: 1 unfinished")

add_test(NAME translations_complete
         COMMAND ${CMAKE_COMMAND} -DLUPDATE=$<TARGET_FILE:Qt6::lupdate>
                 -DSOURCE_DIR=${PROJECT_SOURCE_DIR}/src
                 -DTS_DIR=${PROJECT_SOURCE_DIR}/translations
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/translations_check
                 -P ${PROJECT_SOURCE_DIR}/cmake/CheckTranslations.cmake)
set_tests_properties(translations_complete PROPERTIES LABELS translations)
