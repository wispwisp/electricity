// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/palettewidget.h"

#include "app/language.h"
#include "app/svgpixmap.h"
#include "core/catalog.h"

#include <QCoreApplication>
#include <QEvent>
#include <QHash>
#include <QLocale>
#include <QPainter>

namespace {
QString catalogText(const QString &source)
{
    return QCoreApplication::translate("Catalog", source.toUtf8().constData());
}

// Every picture sits centred in the same cell, so the text of all rows starts at the same x.
QPixmap iconCell(const QString &path, qreal ratio, const QSize &cell)
{
    const QPixmap picture = svgPixmap(path, cell.height(), ratio);
    QPixmap result(cell * ratio);
    result.setDevicePixelRatio(ratio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawPixmap(QPointF((cell.width() - picture.width() / ratio) / 2, 0), picture);
    return result;
}
} // namespace

PaletteWidget::PaletteWidget(const Catalog &catalog, QWidget *parent)
    : QTreeWidget(parent)
    , m_catalog(catalog)
{
    setObjectName(QStringLiteral("paletteTree"));
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setIconSize(QSize(24, 48));
    setIndentation(8);

    QHash<QString, QTreeWidgetItem *> groups;
    QFont bold = font();
    bold.setBold(true);
    for (const Preset &preset : m_catalog.presets) {
        QTreeWidgetItem *&group = groups[preset.group];
        if (!group) {
            group = new QTreeWidgetItem(this);
            group->setFlags(Qt::ItemIsEnabled);
            group->setFont(0, bold);
            group->setData(0, Qt::UserRole + 1, preset.group);
            group->setExpanded(true);
        }
        auto *item = new QTreeWidgetItem(group);
        item->setFlags(Qt::ItemIsEnabled);
        item->setData(0, PresetIdRole, preset.id);
        const DeviceType *type = m_catalog.deviceType(preset.type);
        const DeviceVariant *variant = type ? type->variant(preset.poles) : nullptr;
        if (variant)
            item->setIcon(0,
                          QIcon(iconCell(picturePath(variant->picture), devicePixelRatioF(),
                                         iconSize())));
    }
    retranslate();
}

QString PaletteWidget::toolTipFor(const Preset &preset) const
{
    const DeviceType *type = m_catalog.deviceType(preset.type);
    QString tip;
    if (type) {
        tip = QStringLiteral("<b>") + catalogText(type->name).toHtmlEscaped()
                + QStringLiteral("</b><br>") + catalogText(type->description).toHtmlEscaped();
    }
    if (preset.wireSection) {
        const QString wire = QLocale(languageCode(currentLanguage())).toString(*preset.wireSection);
        tip += QStringLiteral("<br>") + tr("Usual wire: %1 mm²").arg(wire);
    }
    return tip;
}

void PaletteWidget::retranslate()
{
    for (int g = 0; g < topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = topLevelItem(g);
        group->setText(0, catalogText(group->data(0, Qt::UserRole + 1).toString()));
        for (int i = 0; i < group->childCount(); ++i) {
            QTreeWidgetItem *item = group->child(i);
            const QString id = item->data(0, PresetIdRole).toString();
            for (const Preset &preset : m_catalog.presets) {
                if (preset.id != id)
                    continue;
                item->setText(0,
                              catalogText(preset.name) + QLatin1Char('\n')
                                      + catalogText(preset.purpose));
                item->setToolTip(0, toolTipFor(preset));
            }
        }
    }
}

void PaletteWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QTreeWidget::changeEvent(event);
}
