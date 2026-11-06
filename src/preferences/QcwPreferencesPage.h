// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QWidget>

QT_BEGIN_NAMESPACE

/**
 * @class QcwPreferencesPage
 * @brief One page of the preferences dialog, editing the settings of one group.
 *
 * @details A page is both the widget shown on the right of QcwPreferencesWidget and the
 * definition of one QSettings group: settingsGroup() names the group, name() labels the
 * matching tree entry, and the page reads and writes the keys below that group itself.
 *
 * Pages are usually contributed by a plugin from QcwPlugin::doRegisterContributions() and
 * registered with @c preferencesManager. Registration hands the page to the registry: an
 * accepted page is owned by QcwPreferencesManager and lives until the process exits, so it
 * must never be deleted afterwards. A page that registerPage() refuses stays with its
 * creator, who must delete it. QcwPreferencesWidget only parents the page temporarily.
 *
 * @par Filtering
 * The preferences filter first matches the page's tree label, that of its ancestors and of
 * its descendants. matchesFilter() adds the page's own content to that search, so text that
 * only exists inside the page still finds it.
 *
 * @par Lifecycle
 * QcwPreferencesWidget calls loadSettings() every time it is about to show the page, and
 * again after a settings file has been imported. Implementations must be repeatable and must
 * not write to QSettings. restoreDefaults() backs the dialog's Restore Defaults button.
 *
 * @note Pages apply their changes as the user edits them; there is no Apply or Cancel step.
 * @note A page is a single widget, so only one preferences window can show it at a time. The
 * workbench therefore reuses one QcwPreferencesDialog.
 *
 * @see QcwPreferencesWidget, QcwPreferencesManager, QcwPlugin::doRegisterContributions()
 */
class QCW_EXPORT QcwPreferencesPage : public QWidget
{
    Q_OBJECT

public:
    explicit QcwPreferencesPage(QWidget* parent = nullptr);

    /**
     * @brief The QSettings group this page edits, e.g. @c "appearance/themes".
     * @return A group path using @c '/' separators; never empty.
     *
     * @details The value is the page's identity in @c preferencesManager, so it must be
     * unique and stable: saved settings, the preferences tree and page lookup all key on it.
     * Missing intermediate groups are added to the tree automatically, so a page registered
     * only for @c "appearance/fonts" still appears below an @c "appearance" category.
     */
    virtual QString settingsGroup() const = 0;

    /// The translated label shown for the page in the preferences tree, e.g. tr("Themes").
    virtual QString name() const = 0;

    /**
     * @brief Whether the page content contains @p filterText.
     * @param filterText Literal text from the preferences filter; never empty.
     * @return True when the page should remain reachable for the current filter.
     * @note Compare case-insensitively to agree with the tree filter above the pages.
     */
    virtual bool matchesFilter(const QString& filterText) const = 0;

    /**
     * @brief Restores the page's controls and stored values to their defaults.
     * @details Invoked by Restore Defaults in the preferences dialog for the page currently
     * shown; the implementation updates its widgets and writes the default values to
     * QSettings, like any other edit would.
     */
    virtual void restoreDefaults() = 0;

    /**
     * @brief Loads the current settings into the page's controls.
     * @details Called before the page is shown and after settings are imported. It must be
     * safe to call repeatedly and must not modify QSettings: the page reads, it does not
     * write, here.
     */
    virtual void loadSettings() = 0;
};

QT_END_NAMESPACE
