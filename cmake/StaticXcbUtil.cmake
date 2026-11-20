# SPDX-License-Identifier: GPL-3.0-or-later
# Qt's xcb plugin needs the xcb-util libraries, which many desktops do not install
# (libxcb-cursor0 above all), so they are linked statically (ADR 0021). Qt's FindXCB
# has already created the XCB:: targets for the shared libraries; point them at the
# static archives instead.
foreach(component CURSOR ICCCM IMAGE KEYSYMS RENDERUTIL UTIL)
    string(REPLACE "RENDERUTIL" "RENDER-UTIL" name "${component}")
    string(TOLOWER "libxcb-${name}.a" archive)
    find_library(XCB_${component}_STATIC_LIBRARY NAMES ${archive} REQUIRED)
    set_target_properties(XCB::${component} PROPERTIES
        IMPORTED_LOCATION "${XCB_${component}_STATIC_LIBRARY}")
endforeach()

# Static archives must come before the libraries they use on the link line. Qt lists
# XCB::UTIL before XCB::IMAGE, so the dependencies are declared here.
set_target_properties(XCB::CURSOR PROPERTIES
    INTERFACE_LINK_LIBRARIES "XCB::RENDERUTIL;XCB::IMAGE;XCB::RENDER;XCB::XCB")
set_target_properties(XCB::IMAGE PROPERTIES INTERFACE_LINK_LIBRARIES "XCB::UTIL;XCB::SHM;XCB::XCB")
set_target_properties(XCB::RENDERUTIL PROPERTIES INTERFACE_LINK_LIBRARIES "XCB::RENDER;XCB::XCB")
set_target_properties(XCB::ICCCM XCB::KEYSYMS XCB::UTIL PROPERTIES INTERFACE_LINK_LIBRARIES "XCB::XCB")
