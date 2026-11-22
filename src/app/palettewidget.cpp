// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/palettewidget.h"

#include "app/language.h"
#include "app/svgpixmap.h"
#include "core/catalog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDrag>
#include <QEvent>
#include <QHash>
#include <QLocale>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>

#include <utility>

namespace {
QString catalogText(const QString &source)
{
    return QCoreApplication::translate("Catalog", source.toUtf8().constData());
}

// Every picture sits centred in the same cell, so the text of all rows starts at the same x. A
// picture wider than the cell, as the loads are, shrinks to its width.
QPixmap iconCell(const QString &path, qreal ratio, const QSize &cell)
{
    const QSizeF box = svgRenderer(path).viewBoxF().size();
    const int height = box.isEmpty()
            ? cell.height()
            : qMin(cell.height(), qFloor(cell.width() * box.height() / box.width()));
    const QPixmap picture = svgPixmap(path, height, ratio);
    QPixmap result(cell * ratio);
    result.setDevicePixelRatio(ratio);
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.drawPixmap(QPointF((cell.width() - picture.width() / ratio) / 2,
                               (cell.height() - picture.height() / ratio) / 2),
                       picture);
    return result;
}

QString loadToolTip(const LoadType &type)
{
    return QStringLiteral("<b>") + catalogText(type.name).toHtmlEscaped()
            + QStringLiteral("</b><br>") + catalogText(type.description).toHtmlEscaped();
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
    const auto addGroup = [this, &bold](const QString &name) {
        auto *group = new QTreeWidgetItem(this);
        group->setFlags(Qt::ItemIsEnabled);
        group->setFont(0, bold);
        group->setData(0, Qt::UserRole + 1, name);
        group->setExpanded(true);
        return group;
    };
    for (const Preset &preset : m_catalog.presets) {
        QTreeWidgetItem *&group = groups[preset.group];
        if (!group)
            group = addGroup(preset.group);
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
    if (!m_catalog.loads.isEmpty()) {
        QTreeWidgetItem *group = addGroup(QStringLiteral("Loads"));
        for (const LoadType &type : m_catalog.loads) {
            auto *item = new QTreeWidgetItem(group);
            item->setFlags(Qt::ItemIsEnabled);
            item->setData(0, LoadTypeRole, type.id);
            item->setIcon(0,
                          QIcon(iconCell(loadPicturePath(type.picture), devicePixelRatioF(),
                                         iconSize())));
        }
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
    QLocale locale(languageCode(currentLanguage()));
    locale.setNumberOptions(QLocale::OmitGroupSeparator); // "2000 W", as on appliance labels
    for (int g = 0; g < topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = topLevelItem(g);
        group->setText(0, catalogText(group->data(0, Qt::UserRole + 1).toString()));
        for (int i = 0; i < group->childCount(); ++i) {
            QTreeWidgetItem *item = group->child(i);
            if (const LoadType *type = m_catalog.loadType(item->data(0, LoadTypeRole).toString())) {
                QString text = catalogText(type->name);
                if (type->power > 0) // a socket has no power of its own
                    text += QLatin1Char('\n') + tr("%1 W").arg(locale.toString(type->power));
                item->setText(0, text);
                item->setToolTip(0, loadToolTip(*type));
                continue;
            }
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

QStringList PaletteWidget::mimeTypes() const
{
    return { QString::fromLatin1(PresetMimeType), QString::fromLatin1(LoadMimeType) };
}

QMimeData *PaletteWidget::mimeData(const QList<QTreeWidgetItem *> &items) const
{
    for (const QTreeWidgetItem *item : items) {
        QString format = QString::fromLatin1(PresetMimeType);
        QString id = item->data(0, PresetIdRole).toString();
        if (id.isEmpty()) {
            format = QString::fromLatin1(LoadMimeType);
            id = item->data(0, LoadTypeRole).toString();
        }
        if (id.isEmpty())
            continue;
        auto *data = new QMimeData;
        data->setData(format, id.toUtf8());
        return data;
    }
    return nullptr;
}

// Presets and loads aren't selectable and QAbstractItemView only drags selected items, so the
// palette starts the drag itself (ADR 0025).
void PaletteWidget::mousePressEvent(QMouseEvent *event)
{
    m_pressedItem = nullptr;
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->position().toPoint();
        QTreeWidgetItem *item = itemAt(m_pressPos);
        if (item
            && (!item->data(0, PresetIdRole).toString().isEmpty()
                || !item->data(0, LoadTypeRole).toString().isEmpty()))
            m_pressedItem = item;
    }
    QTreeWidget::mousePressEvent(event);
}

void PaletteWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressedItem && (event->buttons() & Qt::LeftButton)
        && (event->position().toPoint() - m_pressPos).manhattanLength()
                >= QApplication::startDragDistance()) {
        dragItem(std::exchange(m_pressedItem, nullptr));
        event->accept();
        return;
    }
    QTreeWidget::mouseMoveEvent(event);
}

void PaletteWidget::dragItem(QTreeWidgetItem *item)
{
    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData({ item }));
    drag->setPixmap(item->icon(0).pixmap(iconSize(), devicePixelRatioF()));
    drag->setHotSpot(QPoint(iconSize().width(), iconSize().height()) / 2);
    drag->exec(Qt::CopyAction);
}
