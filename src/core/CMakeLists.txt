# SPDX-License-Identifier: GPL-3.0-or-later
add_library(panelcore STATIC catalog.cpp catalog.h catalogstrings.cpp catalogstrings.h project.cpp project.h version.cpp version.h wirerouter.cpp wirerouter.h)
target_link_libraries(panelcore PUBLIC Qt6::Core)
target_include_directories(panelcore PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(panelcore PRIVATE ELECTRICITY_VERSION="${PROJECT_VERSION}")
qt_add_resources(panelcore "catalog" PREFIX "/" BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES "${PROJECT_SOURCE_DIR}/resources/catalog/devices.json"
          "${PROJECT_SOURCE_DIR}/resources/catalog/enclosures.json"
          "${PROJECT_SOURCE_DIR}/resources/catalog/presets.json"
          "${PROJECT_SOURCE_DIR}/resources/catalog/products.json"
          "${PROJECT_SOURCE_DIR}/resources/catalog/loads.json")
