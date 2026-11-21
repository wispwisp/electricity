# SPDX-License-Identifier: GPL-3.0-or-later
add_library(electricity_lib STATIC mainwindow.cpp mainwindow.h theme.cpp theme.h appsetup.cpp appsetup.h
    language.cpp language.h settings.cpp settings.h
    languagedialog.cpp languagedialog.h svgpixmap.cpp svgpixmap.h palettewidget.cpp palettewidget.h
    editcommands.cpp editcommands.h)
target_link_libraries(electricity_lib PUBLIC panelcore Qt6::Widgets Qt6::Svg)
qt_add_resources(electricity_lib "fonts" PREFIX "/fonts"
    BASE "${PROJECT_SOURCE_DIR}/resources/fonts"
    FILES "${PROJECT_SOURCE_DIR}/resources/fonts/DejaVuSans.ttf"
          "${PROJECT_SOURCE_DIR}/resources/fonts/DejaVuSans-Bold.ttf")
qt_add_resources(electricity_lib "pictures" PREFIX "/" BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES "${PROJECT_SOURCE_DIR}/resources/svg/devices/breaker-1p.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/breaker-2p.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/rcd-2p.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/rcbo-1pn.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/enclosure-12.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/enclosure-24.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/enclosure-36.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/nbar.svg"
          "${PROJECT_SOURCE_DIR}/resources/svg/devices/pebar.svg")

qt_add_executable(electricity WIN32 main.cpp)
target_link_libraries(electricity PRIVATE electricity_lib)
# Release binary carries its own C++ runtime (ADR 0019).
target_link_options(electricity PRIVATE
    "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-static-libstdc++;-static-libgcc>")
# Qt links its Wayland helper plugins by default; without the Wayland platform
# plugin they are dead weight that adds libwayland-* runtime dependencies.
# The default image format, icon engine and EglFS plugins are unused in F01 and
# would add codecs and their license notices; pictures are rendered with QSvgRenderer,
# so qsvg/qsvgicon are not needed either.
qt_import_plugins(electricity EXCLUDE_BY_TYPE
    wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration
    imageformats iconengines egldeviceintegrations)
set_target_properties(electricity PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

qt_add_translations(TARGETS electricity_lib
    SOURCE_TARGETS electricity_lib electricity panelcore
    TS_FILES ${PROJECT_SOURCE_DIR}/translations/electricity_ru.ts
             ${PROJECT_SOURCE_DIR}/translations/electricity_kk.ts
    PLURALS_TS_FILE ${PROJECT_SOURCE_DIR}/translations/electricity_en.ts)
