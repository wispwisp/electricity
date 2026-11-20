// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QTreeWidget>

struct Catalog;
struct Preset;

// The list of catalog presets, grouped; shown in the Components dock.
class PaletteWidget : public QTreeWidget
{
    Q_OBJECT
public:
    static constexpr int PresetIdRole = Qt::UserRole;

    explicit PaletteWidget(const Catalog &catalog, QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void retranslate();
    [[nodiscard]] QString toolTipFor(const Preset &preset) const;

    const Catalog &m_catalog;
};
