// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QMap>
#include <QString>

QT_BEGIN_NAMESPACE

class QcwPreferencesPage;

/**
 * @class QcwPreferencesManager
 * @brief Process-wide registry of the pages available to the preferences dialog.
 *
 * @details Plugins register their pages here from QcwPlugin::doRegisterContributions(),
 * which QcwPluginLoader calls once the plugin is registered. Applications may also call
 * registerPage() directly. The registry owns pages but does not display them: a host passes
 * allPages() (or a subset) to QcwPreferencesWidget, which copies the map when it is
 * constructed. Register pages before building that widget; typically that means before any
 * preferences window is opened.
 *
 * The registry is for the process-wide preferences. A project that keeps its own settings can
 * build its pages directly and hand them, with its own provider, to QcwPreferencesWidget
 * without registering them here; those pages then belong to that caller, not to the registry.
 *
 * The registry owns the pages it accepts. Registration transfers ownership: a page that
 * registerPage() accepts belongs to the registry from then on and lives until the process
 * exits. The registry deletes nothing, not even at shutdown, so an accepted page is never
 * freed and stays valid for the whole run. The registrant — usually the plugin — must not
 * keep the page for lifetime management or delete it: the registry holds a raw pointer, so
 * deleting an accepted page leaves it dangling and the next tree build or lookup crashes.
 * A page that registerPage() refuses stays with the caller, who must release it. The groups
 * are the page identity: they must be unique, and the first page registered for a group wins
 * (a later duplicate is refused rather than silently replacing a page that may already be
 * shown).
 *
 * @note The registry is created on first use and destroyed at process exit. It is meant for
 * the GUI thread; it performs no locking.
 * @note This is the single global instance: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because lookups go through
 * the @c preferencesManager macro, so a second registry would never be seen.
 *
 * @see QcwPreferencesPage, QcwPlugin::doRegisterContributions(), QcwPreferencesWidget
 */
class QCW_EXPORT QcwPreferencesManager
{
public:
    /// The registry singleton; created on first call.
    static QcwPreferencesManager* instance();

    /**
     * @brief Adds @p page under its settingsGroup() and takes ownership of it.
     * @param page Page to register; may be nullptr, in which case nothing happens.
     * @return True when the page was registered and the registry now owns it; false when
     *         @p page is null, its group is empty, or the group already has a page. A refused
     *         page stays with the caller, who must delete it.
     *
     * @details A null page, an empty group or a group that already has a page is ignored
     * (with a warning for the latter two). An accepted page is detached from any parent and
     * kept until the process exits; never delete it. Registration order does not matter for
     * display: the tree is ordered by group path.
     */
    bool registerPage(QcwPreferencesPage* page);

    /**
     * @brief Returns the page registered under @p settingsGroup, or nullptr.
     * @see allPages()
     */
    QcwPreferencesPage* getPage(const QString& settingsGroup) const;

    /**
     * @brief All registered pages, keyed and ordered by settings group.
     * @return The live map; do not modify it through this reference.
     * @see getPage()
     */
    const QMap<QString, QcwPreferencesPage*>& allPages() const;

private:
    QMap<QString, QcwPreferencesPage*> mPages;
};

/// Shorthand for QcwPreferencesManager::instance().
#ifndef preferencesManager
#define preferencesManager QcwPreferencesManager::instance()
#endif

QT_END_NAMESPACE
