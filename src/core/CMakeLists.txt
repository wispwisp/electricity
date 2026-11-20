# SPDX-License-Identifier: GPL-3.0-or-later
add_library(panelcore STATIC catalog.cpp catalog.h version.cpp version.h)
target_link_libraries(panelcore PUBLIC Qt6::Core)
target_include_directories(panelcore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(panelcore PRIVATE ELECTRICITY_VERSION="${PROJECT_VERSION}")
