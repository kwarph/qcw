// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QWidget>

QT_BEGIN_NAMESPACE

class QcwPreferencesWidgetPrivate;

/**
 * @class QcwPreferencesWidget
 * @brief The body of the preferences dialog: category tree, filter and page host.
 *
 * @details The left side lists the registered pages as a tree of settings groups; a filter
 * line above it matches group and page names as well as the text inside each page. Selecting
 * a page shows it on the right, with the path to it as a breadcrumb above.
 *
 * Settings are live: pages write their values as the user edits them, so this widget has no
 * Apply or Cancel step. It also exports the whole configuration to an INI file and imports
 * one in return; the conversion does not depend on the store the settings provider uses.
 *
 * The tree is built the first time the widget is shown, so pages registered after
 * construction but before that are included. Register all pages before showing the widget.
 *
 * @note A page belongs to QcwPreferencesManager and is only borrowed for display; never
 * delete it. Since one page cannot be shown in two places, do not open two preferences
 * windows at the same time; QcwWorkbench reuses a single dialog for this reason.
 *
 * @see QcwPreferencesDialog, QcwPreferencesPage, QcwPreferencesManager, QcwSettings
 */
class QCW_EXPORT QcwPreferencesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QcwPreferencesWidget(QWidget* parent = nullptr);

    /**
     * @brief Destroys the widget, leaving the borrowed preference pages alive.
     * @details Pages still parented to the page area are detached first, because
     *         QcwPreferencesManager owns them.
     */
    ~QcwPreferencesWidget() override;

    /// The preferred size, 800 x 600; the dialog derives its own size from it.
    QSize sizeHint() const override;

public slots:
    /**
     * @brief Asks for a destination and writes every live setting to it as an INI file.
     * @details The destination is replaced, including keys a previous file may have held.
     * Choosing the file backing the live settings is refused. The chosen location becomes
     * the next file dialog's starting path.
     */
    void exportSettings();

    /**
     * @brief Asks for an INI file, validates it and replaces the live settings with it.
     * @details Every key in the file is written to the live store and every key it does not
     * contain is removed, so the store matches the file afterwards. Because that also removes
     * keys the file never knew about, the current configuration is snapshotted to
     * <tt>configDirectory()/QcwSettings.ini.bak</tt> first; if the snapshot cannot be written,
     * the import is aborted. Choosing the file backing the live settings is refused. On
     * success every registered page reloads its values and the active filter is re-applied.
     */
    void importSettings();

    /// Restores the defaults of the page currently shown; no-op when a category is selected.
    void restoreDefaults();

protected:
    /// Builds the tree on first display, sizes the panes and selects the first page.
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Re-sets the filter placeholder, the window title, the page labels and the path.
     * @details The page names come from QcwPreferencesPage::name(), which a page builds as a
     * string, so the tree rows have to be re-labelled here; a page that translates its name()
     * follows the new language.
     */
    void changeEvent(QEvent* event) override;

private:
    QcwPreferencesWidgetPrivate* impl;
};

QT_END_NAMESPACE
