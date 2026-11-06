// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

QT_BEGIN_NAMESPACE

/**
 * @class QcwBuiltinCommands
 * @brief Registers the command metadata and contributions the workbench ships with.
 *
 * @details Each built-in command is registered in QcwCommandRegistry and placed in its menu
 * or toolbar through QcwContributionRegistry. Handlers are registered separately: menu actions
 * live in QcwWindow (per-window), while process-wide commands (New Window, Exit, Preferences,
 * Themes, ...) are registered by QcwWorkbench as global handlers.
 *
 * Calling registerCommands() twice replaces the same entries instead of adding duplicates, but
 * the registries still announce the change, so windows rebuild their menus a second time for
 * nothing; call it once during startup.
 *
 * unregisterCommands() removes exactly the entries this class registered, so a command an
 * application registered under its own ids is left alone.
 *
 * @note GUI thread only. Call it once during startup, before windows are created.
 *
 * @note Call it after the QGuiApplication exists: QKeySequence(QKeySequence::StandardKey)
 *       resolves platform shortcuts through QStyleHints and requires a GUI application, and
 *       registerCommands() refuses to run without one.
 */
class QCW_EXPORT QcwBuiltinCommands
{
public:
    /// Registers every built-in command's metadata and its menu/toolbar contribution.
    static void registerCommands();

    /// Unregisters the built-in commands and contributions. Used by tests that need a clean
    /// registry; production code never needs it.
    static void unregisterCommands();
};

QT_END_NAMESPACE
