// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QLatin1String>
#include <QString>

QT_BEGIN_NAMESPACE

namespace qcw
{

/**
 * @brief The prefix that makes QcwContribution::target a tool bar rather than a menu.
 * @details "ToolBar/Main" names the tool bar whose logical name is "Main"; what follows the
 * prefix is the name of the QToolBar the window creates for that target.
 */
inline constexpr QLatin1StringView kToolBarTargetPrefix { "ToolBar/" };

} // namespace qcw

/**
 * @struct QcwContribution
 * @brief One declarative menu or toolbar entry: which command, in which target, where.
 *
 * @details A contribution names the command it displays but never the command's handler; the
 * window places the command's QAction in the menu, and that action follows whichever handler
 * QcwWindow::resolveHandler() finds. A contribution whose command the catalog does not list cannot
 * be placed and is left out. before and after may name another contribution id or a command id in
 * the same target; the sorter matches contribution ids first and command ids second. See
 * QcwContributionSorter.
 *
 * @par target
 * A menu target is the menu id ("File", "Edit", "Window/Perspective"); a toolbar target is
 * the toolbar name prefixed with "ToolBar/" ("ToolBar/Main"). Both are placed by the window
 * and both go through the same sorter: the menu a window owns is filled from the contributions
 * naming its id, and a QToolBar is created for a "ToolBar/" target the first time a
 * contribution names it — see QcwWindow::getToolBar() and QcwWindow::registeredToolBars().
 *
 * @par id
 * The contribution id is stable and namespaced, and is what other contributions use to
 * position themselves before or after this one.
 *
 * @par groupId
 * Consecutive contributions with different groupId get a separator between them. An empty
 * groupId is "no group", and separates from any non-empty group.
 */
struct QCW_EXPORT QcwContribution
{
    /// Stable, namespaced id; used to unregister and to reference from other contributions.
    QString id;

    /// The action set this contribution belongs to; empty means always visible.
    QString actionSetId;

    /// Target menu or toolbar, e.g. "File", "Edit", "ToolBar/Main".
    QString target;

    /// The command id to place.
    QString commandId;

    /// Place before this contribution id or command id; empty means unconstrained.
    QString before;

    /// Place after this contribution id or command id; empty means unconstrained.
    QString after;

    /// Group id; a separator appears whenever this changes between adjacent entries.
    QString groupId;

    /// Programmatic visibility switch; when false the contribution is left out.
    bool visible { true };

    /// True when every field matches, which is what lets QcwContributionRegistry tell a
    /// re-registration of the same entry — a language change re-declares the built-ins — from a
    /// replacement that quietly moves or hides something.
    bool operator==(const QcwContribution&) const = default;
};

QT_END_NAMESPACE
