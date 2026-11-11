// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwSettingsProvider.h"
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
 * @par Settings scope
 * A page does not choose its store: it reads and writes through settings(), which asks the
 * provider installed by the QcwPreferencesWidget showing it. This is what lets one process
 * show the global pages in the usual dialog and a project's pages against the project's own
 * QSettings. Without a widget — a page read directly, or one constructed before it is shown —
 * settings() falls back to QcwSettings::get(), so existing pages keep working unchanged.
 *
 * The provider lives in the page, so one page instance serves one scope at a time. Do not put
 * the same page into two widgets with different providers: the second widget overwrites the
 * first one's provider. A provider that captures state must stay valid for as long as the page
 * lives; pages owned by QcwPreferencesManager live until the process exits, so capture a path
 * or other long-lived value rather than a project object.
 *
 * A page that edits process-wide state — QcwAppearancePreferencesPage and
 * QcwNotificationPreferencesPage are the built-in examples — keeps its settings process-wide,
 * directly or through its manager, and therefore ignores the widget's provider.
 *
 * @note Pages apply their changes as the user edits them; there is no Apply or Cancel step.
 * @note A page is a single widget, so only one preferences window can show it at a time. The
 * workbench therefore reuses one QcwPreferencesDialog.
 *
 * @see QcwPreferencesWidget, QcwPreferencesManager, QcwSettings,
 * QcwPlugin::doRegisterContributions()
 */
class QCW_EXPORT QcwPreferencesPage : public QWidget
{
    Q_OBJECT

public:
    explicit QcwPreferencesPage(QWidget* parent = nullptr);

    /**
     * @brief Installs the provider that settings() uses.
     * @param provider Factory for this page's store; an empty provider selects
     *        QcwSettings::get().
     *
     * @details QcwPreferencesWidget calls this when it takes the page for display, once per
     * page and before the page is shown. A page does not normally call it itself: replacing
     * the provider while a widget is showing the page detaches the page from that widget's
     * export/import store. The value is copied, so a temporary lambda is fine.
     */
    void setSettingsProvider(QcwSettingsProvider provider);

    /**
     * @brief A settings object for this page's scope.
     * @return A fresh QSettings on every call: from the installed provider, or from
     *         QcwSettings::get() when no provider was installed.
     *
     * @details Use it for every read and write, and do not cache the result. QSettings flushes
     * a write when the object is destroyed, and a later call sees changes made through any
     * other object for the same store. This is also why loadSettings() must not keep the
     * returned object beyond its own scope.
     */
    QSettings settings() const;

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
     * shown; the implementation updates its widgets and writes the default values through
     * settings(), like any other edit would.
     */
    virtual void restoreDefaults() = 0;

    /**
     * @brief Loads the current settings into the page's controls.
     * @details Called before the page is shown and after settings are imported. It must be
     * safe to call repeatedly and must not modify QSettings: the page reads, it does not
     * write, here.
     */
    virtual void loadSettings() = 0;

private:
    QcwSettingsProvider mSettingsProvider;
};

QT_END_NAMESPACE
