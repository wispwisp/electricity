// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app/language.h"

#include <QDialog>

// Shown on first launch; the heading is fixed trilingual copy because no language is chosen yet.
class LanguageDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LanguageDialog(Language preselected, QWidget *parent = nullptr);
    [[nodiscard]] Language chosenLanguage() const { return m_chosen; }

private:
    Language m_chosen;
};
