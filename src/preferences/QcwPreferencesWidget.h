// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwSettingsProvider.h"
#include <QMap>
#include <QWidget>

QT_BEGIN_NAMESPACE

class QcwPreferencesPage;
class QcwPreferencesWidgetPrivate;

/**
 * @class QcwPreferencesWidget
 * @brief The body of the preferences dialog: category tree, filter and page host.
 *
 * @details The left side lists the page map as a tree of settings groups; a filter line above
 * it matches group and page names as well as the text inside each page. Selecting a page shows
 * it on the right, with the path to it as a breadcrumb above.
 *
 * The widget does not read QcwPreferencesManager itself: the host passes the page map it wants
 * displayed, and the widget copies that map when it is constructed. That is what lets one
 * process show the global pages in the usual dialog and, for example, one project's pages
 * against the project's own QSettings. Pages registered after construction are not picked up,
 * so build the map first and pass it here.
 *
 * The provider selects the store the pages edit and the store export and import read and
 * write. An empty provider means the process-wide store, QcwSettings::get(), which is what the
 * workbench's own dialog uses. The widget and every page receive a copy of the provider; see
 * QcwPreferencesPage::settings() for what the page does with it.
 *
 * Settings are live: pages write their values as the user edits them, so this widget has no
 * Apply or Cancel step. Export writes every key of the configured store to an INI file; import
 * makes that store match the file.
 *
 * @note Pages are borrowed, never owned. A page taken from QcwPreferencesManager belongs to
 * that registry; any other page belongs to the caller. Either way it must outlive the widget,
 * and the widget does not delete it. Since one page cannot be shown in two places, do not open
 * two preferences windows on the same page at the same time; QcwWorkbench reuses a single
 * dialog for this reason.
 *
 * @note A page keeps the provider it was given for the rest of its life. Give each scope its
 * own page instances: putting one page into widgets with different providers leaves it bound
 * to the provider of the widget constructed last.
 *
 * @see QcwPreferencesDialog, QcwPreferencesPage, QcwPreferencesManager, QcwSettings,
 * QcwSettingsProvider
 */
class QCW_EXPORT QcwPreferencesWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Builds the widget over @p pages, editing the store @p settingsProvider describes.
     * @param pages Page map keyed by settings group, normally
     *        @c preferencesManager->allPages() for the global scope or a project's own pages
     *        for a project scope. The map is copied; null entries are ignored.
     * @param settingsProvider Factory for every page's store and for export/import; an empty
     *        provider selects the process-wide store, QcwSettings::get().
     * @param parent Standard QWidget parent.
     */
    explicit QcwPreferencesWidget(const QMap<QString, QcwPreferencesPage*>& pages,
                                  QcwSettingsProvider settingsProvider,
                                  QWidget* parent = nullptr);

    /**
     * @brief Destroys the widget, leaving the borrowed preference pages alive.
     * @details Pages still parented to the page area are detached first, because the caller
     *         or QcwPreferencesManager owns them.
     */
    ~QcwPreferencesWidget() override;

    /// The preferred size, 800 x 600; the dialog derives its own size from it.
    QSize sizeHint() const override;

public slots:
    /**
     * @brief Asks for a destination and writes every key of this widget's store to it as an
     *        INI file.
     * @details The destination is replaced, including keys a previous file may have held.
     * Choosing the file backing the configured store is refused. The chosen location becomes
     * the next file dialog's starting path.
     */
    void exportSettings();

    /**
     * @brief Asks for an INI file, validates it and replaces this widget's store with it.
     * @details Every key in the file is written to the store and every key it does not contain
     * is removed, so the store matches the file afterwards. Because that also removes keys the
     * file never knew about, the current configuration is snapshotted first, next to the store
     * (<tt>\<settings file\>.bak</tt>, or
     * <tt>configDirectory()/QcwSettings.ini.bak</tt> when the store has no file, as with the
     * Windows registry); if the snapshot cannot be written, the import is aborted. Choosing
     * the file backing the store is refused. On success every page of this widget reloads its
     * values and the active filter is re-applied.
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
