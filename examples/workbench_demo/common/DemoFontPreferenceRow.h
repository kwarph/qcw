// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoGlobal.h"
#include <QFont>

class QFormLayout;
class QLineEdit;
class QLabel;
class QPushButton;

namespace demo
{
/**
 * @brief The Font row of a demo preferences page: the current font, Select Font and Default.
 *
 * @details It mirrors the Appearance page's font field. The read-only line edit names the
 * family and point size; Select Font opens QFontDialog, which changes the size as well as the
 * family; Default returns to defaultMonospaceFont(). The row stores nothing itself — it calls
 * the page's handler whenever the user picked a font or pressed Default, and the page writes
 * and applies it.
 */
class QCW_DEMO_EXPORT FontPreferenceRow
{
public:
    /// Called with the new font after a user change; the page writes settings and applies it.
    using ChangedHandler = std::function<void(const QFont&)>;

    FontPreferenceRow(QWidget* parent, QFormLayout* form, const QString& labelText, ChangedHandler changed);

    /**
     * @brief Re-applies every text of the row in the language now installed.
     * @param labelText The row's own label, which belongs to the calling page's context.
     * @details A QLabel keeps the string it was given, so a page that builds its rows once has to
     *          say their labels again when the language changes; QCoreApplication::translate() is
     *          what resolves the row's own texts afresh.
     */
    void retranslateUi(const QString& labelText);

    /// The font last shown: the stored one, the default, or what the user picked.
    QFont font() const
    {
        return mFont;
    }

    /// Shows @p font without reporting it; loadSettings() and restoreDefaults() use this.
    void setFont(const QFont& font);

    /// Returns to defaultMonospaceFont() and reports the change like a user's pick.
    void resetToDefault();

private:
    void chooseFont();
    void notify();

private:
    QLineEdit* mDisplay { nullptr };
    QLabel* mLabel { nullptr };
    QPushButton* mSelectButton { nullptr };
    QPushButton* mDefaultButton { nullptr };
    QFont mFont;
    ChangedHandler mChanged;
};
} // namespace demo
