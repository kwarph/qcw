// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwContribution.h"

#include <QList>
#include <QHash>
#include <QObject>

QT_BEGIN_NAMESPACE

/**
 * @class QcwContributionRegistry
 * @brief The process-wide catalog of menu and toolbar contributions, keyed by contribution id.
 *
 * @details This is the single global instance; registering contributions in a second registry
 * would be pointless, because windows read and listen to the one the contributionRegistry
 * macro hands out, so anything registered elsewhere would never be seen.
 *
 * Contributions are pure declarations: the registry stores them in registration
 * order and emits contributionsChanged() so windows can rebuild their menus. Ordering
 * (before/after, "@start", action set filtering, cycles) is done by QcwContributionSorter
 * against the perspective's visible action sets, not by the registry.
 *
 * @par Ownership
 * The registry stores a copy of every contribution and owns nothing. The caller may let the
 * registered object go as soon as registerContribution() returns, and a contribution never
 * points at a handler, so nothing here can dangle.
 *
 * @par Removal
 * A loaded plugin stays for the rest of the run, so production code registers once and never
 * removes. The removal methods exist for QcwBuiltinCommands::unregisterCommands() and for
 * tests that share the process-wide registry; removing an entry only forgets a copy.
 *
 * Registering an id that is already present replaces it silently; the last registration wins.
 *
 * @note GUI thread only.
 */
class QCW_EXPORT QcwContributionRegistry : public QObject
{
    Q_OBJECT
public:
    /// Returns the process-wide registry, creating it on first call.
    static QcwContributionRegistry* instance();

    /// Registers or replaces @p contribution; returns false when its id, target or commandId
    /// is invalid or empty.
    bool registerContribution(const QcwContribution& contribution);

    /// Removes the contribution under @p id; returns false when it was unknown.
    bool unregisterContribution(const QString& id);

    /// Removes every contribution whose actionSetId is @p actionSetId; returns how many were
    /// removed. Used by tests that reset the catalog; plugins never unload.
    int unregisterContributionsOf(const QString& actionSetId);

    /// The contributions for @p target, in registration order.
    QList<QcwContribution> contributionsFor(const QString& target) const;

    /// Every registered contribution, in registration order.
    QList<QcwContribution> allContributions() const;

signals:
    /// Emitted after a contribution was added, replaced or removed.
    void contributionsChanged();

private:
    QList<QcwContribution> mContributions; // insertion order
    QHash<QString, int> mIndexById;        // contribution id -> index in mContributions
};

/// Shorthand for QcwContributionRegistry::instance().
#ifndef contributionRegistry
#define contributionRegistry QcwContributionRegistry::instance()
#endif

QT_END_NAMESPACE
