// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "DemoFontPreferenceRow.h"
#include "DemoFonts.h"

#include <QCoreApplication>
#include <QFont>
#include <QFontDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>

#include <functional>
#include <utility>

namespace demo
{

FontPreferenceRow::FontPreferenceRow(QWidget* parent,
                                     QFormLayout* form,
                                     const QString& labelText,
                                     ChangedHandler changed) :
    mChanged(std::move(changed))
{
    mLabel = new QLabel { parent };
    mLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);

    auto* row = new QHBoxLayout;

    mDisplay = new QLineEdit { parent };
    mDisplay->setReadOnly(true);
    mDisplay->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mLabel->setBuddy(mDisplay);
    row->addWidget(mDisplay);

    mSelectButton = new QPushButton { parent };
    row->addWidget(mSelectButton);

    mDefaultButton = new QPushButton { parent };
    row->addWidget(mDefaultButton);

    form->addRow(mLabel, row);

    QObject::connect(mSelectButton, &QPushButton::clicked, parent, [this]()
    {
        chooseFont();
    });
    QObject::connect(mDefaultButton, &QPushButton::clicked, parent, [this]()
    {
        resetToDefault();
    });

    retranslateUi(labelText);
}

/**
 * @brief Re-applies every text of the row in the language now installed.
 * @param labelText The row's own label, which belongs to the calling page's context.
 * @details A QLabel keeps the string it was given, so a page that builds its rows once has to
 *          say their labels again when the language changes; QCoreApplication::translate() is
 *          what resolves the row's own texts afresh.
 */
void FontPreferenceRow::retranslateUi(const QString& labelText)
{
    mLabel->setText(labelText);
    mSelectButton->setText(QCoreApplication::translate("demo::FontPreferenceRow", "Select &Font..."));
    mDefaultButton->setText(QCoreApplication::translate("demo::FontPreferenceRow", "&Default"));
    mDefaultButton->setToolTip(QCoreApplication::translate("demo::FontPreferenceRow", "Reset to the default font"));
}

/// Shows @p font without reporting it; loadSettings() and restoreDefaults() use this.
void FontPreferenceRow::setFont(const QFont& font)
{
    mFont = font;

    const QFontInfo info { font };
    QString text = info.family();
    if (info.pointSize() > 0)
    {
        text += QStringLiteral(" %1").arg(info.pointSize());
    }

    mDisplay->setText(text);
    mDisplay->setCursorPosition(0);
}

/// Returns to defaultMonospaceFont() and reports the change like a user's pick.
void FontPreferenceRow::resetToDefault()
{
    setFont(defaultMonospaceFont());
    notify();
}

void FontPreferenceRow::chooseFont()
{
    bool accepted = false;
    const QFont selected = QFontDialog::getFont(&accepted, mFont, mDisplay->parentWidget(),
                                                QCoreApplication::translate("demo::FontPreferenceRow", "Select Font"));
    if (!accepted)
    {
        return;
    }

    setFont(selected);
    notify();
}

void FontPreferenceRow::notify()
{
    if (mChanged)
    {
        mChanged(mFont);
    }
}

} // namespace demo
