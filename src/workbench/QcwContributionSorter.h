// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwContribution.h"

#include <QList>
#include <QStringList>

QT_BEGIN_NAMESPACE

/**
 * @class QcwContributionSorter
 * @brief Turns a target's registered contributions into the order a menu or toolbar shows.
 *
 * @details Given contributions in registration order and the perspective's visible action
 * sets, the sorter:
 * 1. drops entries with visible == false, and entries whose action set is not visible;
 * 2. applies before/after as a dependency graph, matching contribution ids first and command
 *    ids second;
 * 3. topological-sorts the constrained entries, in registration order when there is a tie;
 * 4. appends cycle entries in registration order and logs a warning;
 * 5. appends unconstrained entries in registration order;
 * 6. prepends entries pinned with before == "@start".
 *
 * The function is pure: it does not read the registry, take ownership or cache anything. Checking
 * that an action set id a contribution names actually exists is therefore the caller's job, which
 * is how QcwWindow applies Section 7.3's rule that an unregistered set hides the entry.
 */
class QCW_EXPORT QcwContributionSorter
{
public:
    /**
     * @brief Sorts @p contributions for display in @p target.
     * @param contributions Contributions in registration order.
     * @param visibleActionSetIds The perspective's visible action sets; an empty list means
     *                            every action set is visible.
     * @param target The menu or toolbar being built, used only in warnings.
     * @return The sorted, filtered contributions.
     */
    static QList<QcwContribution>
    sort(const QList<QcwContribution>& contributions, const QStringList& visibleActionSetIds, const QString& target);

    /// Whether @p contribution is visible under @p visibleActionSetIds. An action set id that no
    /// registry knows still counts as listed here — dropping such a contribution is the caller's
    /// rule, because it needs the registry this class deliberately never reads.
    static bool isVisible(const QcwContribution& contribution, const QStringList& visibleActionSetIds);
};

QT_END_NAMESPACE
