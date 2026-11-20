// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/theme.h"

#include <QApplication>
#include <QStyle>
#include <QStyleFactory>

namespace Theme {

namespace {
const QColor windowColor(45, 45, 48);
const QColor baseColor(30, 30, 32);
const QColor textColor(224, 224, 224);
const QColor buttonColor(58, 58, 62);
const QColor accentColor(42, 130, 218);
const QColor disabledTextColor(128, 128, 128);
Mode appliedMode = Mode::Dark;
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

QPalette lightPalette()
{
    const QColor window(240, 240, 240);
    const QColor text(30, 30, 30);
    const QColor button(225, 225, 225);
    const QColor disabledText(150, 150, 150);

    QPalette palette;
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Link, accentColor);
    palette.setColor(QPalette::Highlight, accentColor);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, disabledText);
    palette.setColor(QPalette::Light, Qt::white);
    palette.setColor(QPalette::Midlight, button.lighter(105));
    palette.setColor(QPalette::Mid, button.darker(130));
    palette.setColor(QPalette::Dark, button.darker(150));
    palette.setColor(QPalette::Shadow, QColor(100, 100, 100));

    for (QPalette::ColorRole role :
         { QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText })
        palette.setColor(QPalette::Disabled, role, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, button);
    return palette;
}

void apply(Mode mode)
{
    if (QApplication::style()->name().compare(QStringLiteral("fusion"), Qt::CaseInsensitive) != 0)
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(mode == Mode::Light ? lightPalette() : darkPalette());
    appliedMode = mode;
}

Mode currentMode()
{
    return appliedMode;
}

} // namespace Theme
