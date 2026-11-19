# SPDX-License-Identifier: GPL-3.0-or-later
add_library(electricity_lib STATIC mainwindow.cpp mainwindow.h)
target_link_libraries(electricity_lib PUBLIC panelcore Qt6::Widgets)

qt_add_executable(electricity WIN32 main.cpp)
target_link_libraries(electricity PRIVATE electricity_lib)
# Release binary carries its own C++ runtime (ADR 0019).
target_link_options(electricity PRIVATE
    "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-static-libstdc++;-static-libgcc>")
# Qt links its Wayland helper plugins by default; without the Wayland platform
# plugin they are dead weight that adds libwayland-* runtime dependencies.
qt_import_plugins(electricity EXCLUDE_BY_TYPE
    wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration)
