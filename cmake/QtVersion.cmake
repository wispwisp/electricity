# SPDX-License-Identifier: GPL-3.0-or-later
# Pinned together with the container and scripts/build_qt.cmake (CI); change all three at once.
set(ELECTRICITY_QT_VERSION 6.12.0)
if(NOT Qt6_VERSION VERSION_EQUAL ELECTRICITY_QT_VERSION)
    message(FATAL_ERROR "Qt ${ELECTRICITY_QT_VERSION} is required (ADR 0003), found ${Qt6_VERSION}")
endif()
