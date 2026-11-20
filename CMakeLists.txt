# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.22)
project(electricity VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test LinguistTools)
include(cmake/QtVersion.cmake)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(cmake/StaticXcbUtil.cmake)
endif()
qt_standard_project_setup()

if(MSVC)
    add_compile_options(/W4 /WX /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tests)
