// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QDialog>

QT_BEGIN_NAMESPACE

class QEvent;
class QcwPreferencesDialogPrivate;

/**
 * @class QcwPreferencesDialog
 * @brief Top-level window around QcwPreferencesWidget, with the file and close buttons.
 *
 * @details The dialog adds Export, Import, Restore Defaults and Close below the widget; all
 * editing happens in the pages and is applied immediately, so there is no OK or Apply step.
 * Export writes the current settings INI elsewhere; Import validates the chosen file, backs
 * up the live one and reloads the pages.
 *
 * The dialog is non-modal and the size follows QcwPreferencesWidget's size hint.
 *
 * @note Open only one at a time: preferences pages are shared widgets owned by
 * QcwPreferencesManager, and one widget cannot be shown in two windows. QcwWorkbench reuses
 * its single dialog when the Preferences action is triggered again.
 *
 * @see QcwPreferencesWidget
 */
class QCW_EXPORT QcwPreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    /// Builds the dialog and its contents.
    explicit QcwPreferencesDialog(QWidget* parent = nullptr);
    ~QcwPreferencesDialog() override;

protected:
    /**
     * @brief Re-sets the button and title texts when the UI language changes.
     * @details The pages below retranslate themselves; these three buttons and the window
     *          title are the dialog's own strings, captured once in the constructor.
     */
    void changeEvent(QEvent* event) override;

private:
    QcwPreferencesDialogPrivate* impl;
};

QT_END_NAMESPACE
