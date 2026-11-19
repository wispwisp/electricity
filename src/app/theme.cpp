// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/theme.h"

#include <QApplication>
#include <QStyleFactory>

namespace Theme {

namespace {
const QColor windowColor(45, 45, 48);
const QColor baseColor(30, 30, 32);
const QColor textColor(224, 224, 224);
const QColor buttonColor(58, 58, 62);
const QColor accentColor(42, 130, 218);
const QColor disabledTextColor(128, 128, 128);
} // namespace

QPalette darkPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, windowColor);
    palette.setColor(QPalette::WindowText, textColor);
    palette.setColor(QPalette::Base, baseColor);
    palette.setColor(QPalette::AlternateBase, windowColor);
    palette.setColor(QPalette::ToolTipBase, buttonColor);
    palette.setColor(QPalette::ToolTipText, textColor);
    palette.setColor(QPalette::Text, textColor);
    palette.setColor(QPalette::Button, buttonColor);
    palette.setColor(QPalette::ButtonText, textColor);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, accentColor);
    palette.setColor(QPalette::Highlight, accentColor);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, disabledTextColor);
    palette.setColor(QPalette::Light, buttonColor.lighter(130));
    palette.setColor(QPalette::Midlight, buttonColor.lighter(115));
    palette.setColor(QPalette::Mid, buttonColor.darker(130));
    palette.setColor(QPalette::Dark, baseColor.darker(130));
    palette.setColor(QPalette::Shadow, Qt::black);

    for (QPalette::ColorRole role :
         { QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText })
        palette.setColor(QPalette::Disabled, role, disabledTextColor);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, buttonColor);
    return palette;
}

void apply()
{
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(darkPalette());
}

} // namespace Theme
