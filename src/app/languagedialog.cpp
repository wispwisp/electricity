// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/languagedialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LanguageDialog::LanguageDialog(Language preselected, QWidget *parent)
    : QDialog(parent)
    , m_chosen(preselected)
{
    setObjectName(QStringLiteral("languageDialog"));
    setWindowTitle(QStringLiteral("Electricity"));

    auto *heading = new QLabel(QStringLiteral("Выберите язык · Тілді таңдаңыз · Language"), this);
    heading->setObjectName(QStringLiteral("languageHeading"));
    heading->setAlignment(Qt::AlignCenter);

    auto *buttons = new QHBoxLayout;
    for (const Language language : kAllLanguages) {
        auto *button = new QPushButton(nativeLanguageName(language), this);
        button->setObjectName(QStringLiteral("languageButton_") + languageCode(language));
        button->setMinimumSize(140, 48);
        connect(button, &QPushButton::clicked, this, [this, language] {
            m_chosen = language;
            accept();
        });
        buttons->addWidget(button);
        if (language == preselected) {
            button->setDefault(true);
            button->setFocus();
        }
    }

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(heading);
    layout->addLayout(buttons);
}
