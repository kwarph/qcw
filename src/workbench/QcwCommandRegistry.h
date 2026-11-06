// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwCommand.h"
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <optional>

QT_BEGIN_NAMESPACE

/**
 * @class QcwCommandRegistry
 * @brief The process-wide catalog of command metadata, keyed by command id.
 *
 * @details QcwBuiltinCommands publishes the File, Edit, Window and Help commands here and an
 * application or plugin adds its own, so the command palette can list every command the process
 * knows, whether or not a menu shows it. An entry says what a command is; the object that runs it
 * is a QcwCommandHandler provided by a view, a perspective, a window or the workbench, and
 * QcwWindow::resolveHandler() finds it by id. Where a command appears is a QcwContribution's
 * business, not this catalog's.
 *
 * Entries are display metadata only (QcwCommandInfo); an entry does not keep the command
 * runnable and outlives every window that could run it. Registering an id that is already
 * present replaces its info; commandsChanged() is emitted only when the stored info actually
 * changes, so a second window publishing the same built-in commands is silent. Every window
 * listens: a change refreshes its command QActions, and unregistering removes the action the
 * id no longer deserves.
 *
 * @par Ownership
 * The registry stores copies of the metadata and owns nothing; neither a handler nor a QAction
 * is registered here. Unregistering only forgets a copy: it exists for
 * QcwBuiltinCommands::unregisterCommands() and tests, while production code registers once and
 * never removes, because nothing unloads.
 *
 * @note The registry is not thread-safe and belongs to the GUI thread.
 * @note This is the single global instance: instance() creates it on first use and always
 * returns the same object. A second one is pointless because the palette and every window
 * publish to the same global through the @c commandRegistry macro.
 * @see QcwCommandInfo, QcwContributionRegistry, QcwWindow::resolveHandler(), QcwCommandPalette
 */
class QCW_EXPORT QcwCommandRegistry : public QObject
{
    Q_OBJECT

public:
    /// Returns the process-wide registry, creating it on first call.
    static QcwCommandRegistry* instance();

    /**
     * @brief Publishes @p info under its id.
     * @param info The catalog entry; its id must be non-empty and follow the id naming rules.
     * @return @c true when the entry was added or changed, @c false when the id is rejected or
     *         the stored entry already equals @p info.
     *
     * @details Registering an id that is already present replaces the whole entry. The handler is
     * resolved when the command runs, so a stale entry never decides what executes; it decides
     * only what the palette shows. commandsChanged() is emitted only when the change is real.
     */
    bool registerCommand(const QcwCommandInfo& info);

    /**
     * @brief Removes the entry under @p id.
     * @return @c true when an entry was removed, @c false when the id is unknown.
     * @details Intended for QcwBuiltinCommands::unregisterCommands() and tests; production
     *          code registers once and never removes.
     */
    bool unregisterCommand(const QString& id);

    /// The entry under @p id, or @c std::nullopt when the id is not catalogued.
    std::optional<QcwCommandInfo> command(const QString& id) const;

    /// True when @p id is catalogued.
    bool contains(const QString& id) const;

    /**
     * @brief Every catalogued command.
     * @return The entries sorted by category (case-insensitively), then label, then id.
     */
    QList<QcwCommandInfo> commands() const;

    /// Emitted after an entry was added, replaced or removed.
    Q_SIGNAL void commandsChanged();

private:
    QMap<QString, QcwCommandInfo> mCommands;
};

/// Shorthand for QcwCommandRegistry::instance().
#ifndef commandRegistry
#define commandRegistry QcwCommandRegistry::instance()
#endif

QT_END_NAMESPACE
