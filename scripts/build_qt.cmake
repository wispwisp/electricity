# SPDX-License-Identifier: GPL-3.0-or-later
# Builds the static Qt used by CI (same configuration as the container, see ADR 0004).
# Usage: cmake -DQT_PREFIX=<install dir> -DQT_WORK_DIR=<new dir> -P scripts/build_qt.cmake
# On Windows run it from a shell with the MSVC environment loaded.
set(QT_VERSION 6.12.0)

if(NOT DEFINED QT_PREFIX OR NOT DEFINED QT_WORK_DIR)
    message(FATAL_ERROR "Pass -DQT_PREFIX=<install dir> and -DQT_WORK_DIR=<new work dir>")
endif()
if(EXISTS "${QT_WORK_DIR}")
    message(FATAL_ERROR "QT_WORK_DIR already exists: ${QT_WORK_DIR}")
endif()

set(QT_HASH_qtbase a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307)
set(QT_HASH_qtsvg e4ab39534ec97987b1b9b60ef7f4d3253d5912a69f8c713a01753174a4029331)
set(QT_HASH_qttools 8dab8f3611496486a470ad5f115ceea584f36bc22a2b8b6f6ebdbafbb8160693)

set(common_flags -static -release -qt-zlib -qt-libpng -qt-libjpeg -qt-pcre -qt-harfbuzz
    -nomake examples -nomake tests)
if(CMAKE_HOST_WIN32)
    set(script_ext .bat)
    set(run_prefix cmd /c)
    list(APPEND common_flags -static-runtime)
else()
    set(script_ext "")
    set(run_prefix "")
    list(APPEND common_flags -feature-xcb -no-feature-wayland -no-feature-glib -no-feature-icu
         -no-feature-zstd -no-feature-libudev -no-feature-cups -no-feature-gtk3)
endif()

file(MAKE_DIRECTORY "${QT_WORK_DIR}")

foreach(module qtbase qtsvg qttools)
    set(name ${module}-everywhere-src-${QT_VERSION})
    set(archive ${QT_WORK_DIR}/${name}.tar.xz)
    file(DOWNLOAD
         https://download.qt.io/official_releases/qt/6.12/${QT_VERSION}/submodules/${name}.tar.xz
         ${archive}
         EXPECTED_HASH SHA256=${QT_HASH_${module}}
         SHOW_PROGRESS)
    # qttools has a non-ASCII file name, which libarchive can only extract under a UTF-8 locale.
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env LC_ALL=C.UTF-8 ${CMAKE_COMMAND} -E tar xf ${archive}
        WORKING_DIRECTORY ${QT_WORK_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

    set(build_dir ${QT_WORK_DIR}/build-${module})
    file(MAKE_DIRECTORY ${build_dir})
    if(module STREQUAL qtbase)
        set(configure_cmd ${run_prefix} ${QT_WORK_DIR}/${name}/configure${script_ext}
            -prefix ${QT_PREFIX} ${common_flags})
    else()
        set(configure_cmd ${run_prefix} ${QT_PREFIX}/bin/qt-configure-module${script_ext}
            ${QT_WORK_DIR}/${name})
        if(module STREQUAL qttools)
            list(APPEND configure_cmd -no-feature-clang)
        endif()
    endif()
    execute_process(COMMAND ${configure_cmd}
                    WORKING_DIRECTORY ${build_dir} COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND ${CMAKE_COMMAND} --build . --parallel
                    WORKING_DIRECTORY ${build_dir} COMMAND_ERROR_IS_FATAL ANY)
    execute_process(COMMAND ${CMAKE_COMMAND} --install .
                    WORKING_DIRECTORY ${build_dir} COMMAND_ERROR_IS_FATAL ANY)
endforeach()
