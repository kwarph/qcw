// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwContributionSorter.h"

#include <QDebug>
#include <QMap>
#include <QSet>

#include <algorithm>

QT_BEGIN_NAMESPACE

namespace
{
constexpr QLatin1StringView kStartAnchor("@start");
}

bool QcwContributionSorter::isVisible(const QcwContribution& contribution, const QStringList& visibleActionSetIds)
{
    if (!contribution.visible)
    {
        return false;
    }

    if (contribution.actionSetId.isEmpty())
    {
        return true;
    }

    if (visibleActionSetIds.isEmpty())
    {
        return true;
    }

    return visibleActionSetIds.contains(contribution.actionSetId);
}

QList<QcwContribution> QcwContributionSorter::sort(const QList<QcwContribution>& contributions,
                                                   const QStringList& visibleActionSetIds,
                                                   const QString& target)
{
    // 1. Filter to what is visible under the perspective's action sets.
    QList<QcwContribution> filtered;
    filtered.reserve(contributions.size());
    for (const auto& contribution : contributions)
    {
        if (isVisible(contribution, visibleActionSetIds))
        {
            filtered.append(contribution);
        }
    }

    if (filtered.isEmpty())
    {
        return filtered;
    }

    const int count = filtered.size();

    QMap<QString, int> indexById;
    QMap<QString, int> firstIndexByCommand;
    for (int i = 0; i < count; ++i)
    {
        indexById.insert(filtered[i].id, i);
        if (!firstIndexByCommand.contains(filtered[i].commandId))
        {
            firstIndexByCommand.insert(filtered[i].commandId, i);
        }
    }

    const auto resolve = [&](const QString& ref) -> int
    {
        auto it = indexById.constFind(ref);
        if (it != indexById.constEnd())
        {
            return it.value();
        }

        it = firstIndexByCommand.constFind(ref);
        if (it != firstIndexByCommand.constEnd())
        {
            return it.value();
        }

        qWarning() << "QcwContributionSorter: unresolved reference" << ref << "in target" << target;
        return -1;
    };

    // 2. Build the dependency graph: an edge a -> b means "a before b".
    QList<QSet<int>> successors(count);
    QList<int> indegree(count, 0);
    QList<int> pinnedFirst;

    for (int i = 0; i < count; ++i)
    {
        const auto& contribution = filtered[i];

        if (contribution.before == kStartAnchor)
        {
            // @start wins over everything else; no other constraint is honoured.
            pinnedFirst.append(i);
            continue;
        }

        if (!contribution.before.isEmpty())
        {
            if (contribution.before == contribution.id)
            {
                qWarning() << "QcwContributionSorter: self before-reference in" << contribution.id;
            }
            else if (const int targetNode = resolve(contribution.before);
                     targetNode >= 0 && targetNode != i && !successors[i].contains(targetNode))
            {
                successors[i].insert(targetNode);
                ++indegree[targetNode];
            }
        }

        if (!contribution.after.isEmpty())
        {
            if (contribution.after == contribution.id)
            {
                qWarning() << "QcwContributionSorter: self after-reference in" << contribution.id;
            }
            else if (const int sourceNode = resolve(contribution.after);
                     sourceNode >= 0 && sourceNode != i && !successors[sourceNode].contains(i))
            {
                successors[sourceNode].insert(i);
                ++indegree[i];
            }
        }
    }

    // 3. Nodes participating in the graph: anything with an edge in or out. A node that
    //    carries only an unresolved reference has no edge and falls through to the
    //    unconstrained append, which keeps registration order for it.
    QSet<int> graphNodes;
    for (int i = 0; i < count; ++i)
    {
        if (!successors[i].isEmpty() || indegree[i] > 0)
        {
            graphNodes.insert(i);
        }
    }

    // 4. Kahn topological sort; ties are broken by registration order.
    QList<int> queue;
    for (int i = 0; i < count; ++i)
    {
        if (graphNodes.contains(i) && indegree[i] == 0)
        {
            queue.append(i);
        }
    }
    std::sort(queue.begin(), queue.end());

    QList<int> sorted;
    sorted.reserve(count);
    QSet<int> emitted;
    while (!queue.isEmpty())
    {
        const int node = queue.takeFirst();
        sorted.append(node);
        emitted.insert(node);

        for (int successor : successors[node])
        {
            if (--indegree[successor] == 0)
            {
                queue.append(successor);
            }
        }
        std::sort(queue.begin(), queue.end());
    }

    // 5. Anything in the graph that was never emitted is on a cycle; append in registration
    //    order.
    QList<int> cycle;
    for (int i = 0; i < count; ++i)
    {
        if (graphNodes.contains(i) && !emitted.contains(i))
        {
            cycle.append(i);
        }
    }

    if (!cycle.isEmpty())
    {
        QStringList ids;
        ids.reserve(cycle.size());
        for (int i : cycle)
        {
            ids.append(filtered[i].id);
        }

        qWarning() << "QcwContributionSorter: cycle detected in target" << target << "involving"
                   << ids.join(QStringLiteral(", ")) << "- falling back to registration order";

        std::sort(cycle.begin(), cycle.end());
        for (int i : cycle)
        {
            sorted.append(i);
            emitted.insert(i);
        }
    }

    // 6. Unconstrained, non-pinned entries follow, in registration order.
    for (int i = 0; i < count; ++i)
    {
        if (!graphNodes.contains(i) && !pinnedFirst.contains(i) && !emitted.contains(i))
        {
            sorted.append(i);
            emitted.insert(i);
        }
    }

    // 7. @start-pinned entries go first, in registration order.
    QList<int> finalOrder;
    finalOrder.reserve(count);

    std::sort(pinnedFirst.begin(), pinnedFirst.end());
    for (int i : pinnedFirst)
    {
        finalOrder.append(i);
    }
    finalOrder.append(sorted);

    QList<QcwContribution> result;
    result.reserve(count);
    for (int i : finalOrder)
    {
        result.append(filtered[i]);
    }

    return result;
}

QT_END_NAMESPACE
