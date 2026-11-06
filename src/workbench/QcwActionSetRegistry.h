// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwActionSet.h"

#include <QList>
#include <QMap>

QT_BEGIN_NAMESPACE

/**
 * @class QcwActionSetRegistry
 * @brief The process-wide catalog of action set metadata, keyed by action set id.
 *
 * @details This is the single global instance; registering action sets in a second registry
 * would be pointless, because the windows and dialogs that filter by action set look them up
 * through the actionSetRegistry macro, so anything registered elsewhere would never be seen.
 *
 * The registry is pure data: it holds copies of ids and labels, owns nothing and emits no
 * signals. A newly registered action set becomes visible the next time a window builds its
 * menus, which is driven by QcwContributionRegistry::contributionsChanged() and perspective
 * switches, not by this class.
 *
 * Registering an id that is already present replaces it and warns about it; the last
 * registration wins. A registered set never points at a contribution, so removing one only
 * forgets a copy: unregisterActionSet() exists for tests that share the process-wide registry,
 * while production code registers once and never removes, because nothing unloads.
 *
 * @note GUI thread only.
 */
class QCW_EXPORT QcwActionSetRegistry
{
public:
    /// Returns the process-wide registry, creating it on first call.
    static QcwActionSetRegistry* instance();

    /// Registers or replaces @p set; returns false when its id is invalid or empty.
    bool registerActionSet(const QcwActionSet& set);

    /// Removes the action set under @p id; returns false when it was unknown. Intended for
    /// tests: production code never removes a registered action set.
    bool unregisterActionSet(const QString& id);

    /// The action set under @p id, or nullptr when it is not registered. The pointer stays
    /// valid until the next register or unregister touches that id.
    const QcwActionSet* actionSet(const QString& id) const;

    /// True when @p id is registered.
    bool contains(const QString& id) const;

    /// Every registered action set, sorted by id.
    QList<QcwActionSet> allActionSets() const;

private:
    QMap<QString, QcwActionSet> mActionSets;
};

/// Shorthand for QcwActionSetRegistry::instance().
#ifndef actionSetRegistry
#define actionSetRegistry QcwActionSetRegistry::instance()
#endif

QT_END_NAMESPACE
