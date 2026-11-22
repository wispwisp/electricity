// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTreeWidget>

struct Catalog;
struct Preset;

// The catalog presets, grouped, and the load types; shown in the Components dock.
class PaletteWidget : public QTreeWidget
{
    Q_OBJECT
public:
    static constexpr int PresetIdRole = Qt::UserRole;
    // drag data: the preset id as UTF-8
    static constexpr char PresetMimeType[] = // NOLINT(modernize-avoid-c-arrays)
            "application/x-electricity-preset";
    static constexpr int LoadTypeRole = Qt::UserRole + 2;
    // drag data: the load type id as UTF-8
    static constexpr char LoadMimeType[] = // NOLINT(modernize-avoid-c-arrays)
            "application/x-electricity-load";

    explicit PaletteWidget(const Catalog &catalog, QWidget *parent = nullptr);
    [[nodiscard]] QStringList mimeTypes() const override;
    // the first preset or load item's id; nullptr if there is none
    [[nodiscard]] QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override;

protected:
    void changeEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void retranslate();
    [[nodiscard]] QString toolTipFor(const Preset &preset) const;
    void dragItem(QTreeWidgetItem *item);

    const Catalog &m_catalog;
    QTreeWidgetItem *m_pressedItem = nullptr; // under a left press that may become a drag
    QPoint m_pressPos;
};
