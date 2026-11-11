// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwSettingsProvider.h"
#include "QcwNavigationContentDialog.h"
#include <QMap>

QT_BEGIN_NAMESPACE

class QEvent;
class QcwPreferencesPage;
class QcwPreferencesDialogPrivate;

/**
 * @class QcwPreferencesDialog
 * @brief Top-level window around QcwPreferencesWidget, with the file and close buttons.
 *
 * @details The navigation frame and the button row come from QcwNavigationContentDialog; this
 * class fills the frame with QcwPreferencesWidget and adds Restore Defaults, Export, Import and
 * Close below it. All editing happens in the pages and is applied immediately, so there is no
 * OK or Apply step. Export writes every key of the configured store elsewhere; Import validates
 * the chosen file, backs up the store and reloads the pages.
 *
 * The dialog owns no pages and no settings store: it forwards the page map and the provider to
 * QcwPreferencesWidget. The workbench passes the registry's pages and an empty provider, which
 * selects the process-wide store; an application can pass a project's pages and a provider for
 * that project's QSettings instead.
 *
 * The dialog is non-modal and the size follows QcwPreferencesWidget's size hint.
 *
 * @note Open only one dialog per page at a time: preferences pages are shared widgets, usually
 * owned by QcwPreferencesManager, and one widget cannot be shown in two windows. QcwWorkbench
 * reuses its single dialog when the Preferences action is triggered again.
 *
 * @see QcwNavigationContentDialog, QcwPreferencesWidget, QcwPreferencesPage
 */
class QCW_EXPORT QcwPreferencesDialog : public QcwNavigationContentDialog
{
    Q_OBJECT

public:
    /**
     * @brief Builds the dialog and its contents.
     * @param pages Pages to show, keyed by settings group; copied and borrowed, not owned.
     * @param settingsProvider Store the pages and Export/Import use; an empty provider
     *        selects the process-wide store, QcwSettings::get().
     * @param parent Standard QDialog parent.
     */
    explicit QcwPreferencesDialog(const QMap<QString, QcwPreferencesPage*>& pages,
                                  QcwSettingsProvider settingsProvider,
                                  QWidget* parent = nullptr);
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
