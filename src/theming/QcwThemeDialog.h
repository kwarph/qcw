// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QDialog>

QT_BEGIN_NAMESPACE

class QEvent;
class QcwThemeDialogPrivate;

/**
 * @class QcwThemeDialog
 * @brief Lists the registered themes and lets the user switch, add and remove them.
 *
 * @details Each row shows a theme name and its directory. Checking a row's box or double-
 * clicking the row makes that theme current: QcwThemeManager applies its palette and persists
 * the choice, so changes take effect immediately and there is no OK or Apply step. Only the
 * current theme's box is checked; clearing it is not possible because one theme always stays
 * current.
 *
 * Add opens a small dialog that registers a new directory containing QcwPalette.theme. Remove
 * unregisters the selected theme. The current theme cannot be removed, and neither can
 * QcwThemeManager::DefaultThemeName ("QcwLight"), the theme the manager falls back to.
 *
 * @note The dialog is non-modal: QcwWorkbench shows it with QDialog::show() and deletes it on
 * close. Themes registered while it is open appear in the list through
 * QcwThemeManager::themeAdded().
 *
 * @see QcwThemeManager
 */
class QCW_EXPORT QcwThemeDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog and lists the themes currently registered.
     * @param parent Parent widget, or @c nullptr to create a top-level dialog.
     */
    explicit QcwThemeDialog(QWidget* parent = nullptr);

    /// @brief Destroys the dialog, leaving the registered themes untouched.
    ~QcwThemeDialog() override;

protected:
    /**
     * @brief Re-sets the hint, the two buttons, the column labels and the title.
     * @details The theme names in the list are not translated: they are the names the themes
     * are registered under, which is also how QcwThemeManager stores them.
     */
    void changeEvent(QEvent* event) override;

private:
    QcwThemeDialogPrivate* impl;
};

QT_END_NAMESPACE
