// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QIcon>
#include <QKeySequence>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwCommandInfo
 * @brief A command as the process-wide catalog lists it: what it is called, not where it lives.
 *
 * @details QcwCommandRegistry holds one entry per command id so the command palette can list
 * and search commands. The handler itself is an ordinary QcwCommandHandler provided by a
 * view, perspective, window or the workbench; the registry never holds handlers.
 *
 * Which handler answers is decided by the scope order QcwWindow::resolveHandler() walks, not
 * by anything stored here, so a command needs no scope flag of its own.
 */
struct QCW_EXPORT QcwCommandInfo
{
    /// The command id, e.g. QcwWindowActionIds::ActionSave.
    QString id;

    /// The palette group the command is listed under, e.g. "File"; empty means "Application".
    QString category;

    /// The command name shown in the palette and on the command QAction.
    QString label;

    /// One line of help shown beside the name; may be empty.
    QString description;

    /// The shortcut the command QAction carries; UnknownKey for none.
    QKeySequence defaultShortcut;

    /// Icon reused by the command QAction, menu items, toolbar buttons and the palette.
    QIcon icon;

    /// Whether the command is checkable; the command QAction calls setCheckable() accordingly.
    bool checkable { false };

    /// Compares every field; the registry uses this to detect a real change.
    bool operator==(const QcwCommandInfo&) const;
};

QT_END_NAMESPACE
