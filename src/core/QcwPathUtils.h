// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwAppDirectories
 * @brief The two directories the workbench keeps its files in.
 *
 * @details Both are lookups into QStandardPaths, so on Linux they sit under the usual
 * @c ~/.config and @c ~/.local/share and on Windows in the per-user directories. Neither is
 * created here: QcwWorkbench::initialize() makes both on the way up, before anything reads
 * or writes a file.
 *
 * @note The paths are derived from the application and organization names, so they are only
 * correct once QcwWorkbench::initialize() has set them. Asking earlier returns a path built
 * from the executable name; callers must ask again afterwards rather than cache that answer.
 */
struct QCW_EXPORT QcwAppDirectories
{
    /**
     * @brief Sets the configuration directory for this process, at most once.
     *
     * @details QcwWorkbench calls this from initialize() before it installs the QcwSettings
     * INI provider and before any window or perspective state is read. A command-line
     * application can therefore choose a workspace-specific directory first; everything that
     * uses configDirectory() afterwards (QcwSettings, QcwWorkbench.xml, QcwWindow-*.xml and
     * perspectives/) lands in that directory.
     *
     * The directory is created when it does not exist. An empty path selects the platform
     * default. A second call is refused: changing the directory while a QcwSettings provider
     * is installed would split one process across two configuration trees.
     *
     * @return True when this call selected the directory; false when it was already set or
     *         could not be created.
     */
    static bool setConfigDirectory(const QString& directory);

    /// Directory of the persisted user data: window and perspective state, and the INI file
    /// that QcwWorkbench installs as its settings provider.
    static QString configDirectory();

    /**
     * @brief Directory of the log files, one per run.
     * @see QcwLogger
     */
    static QString logDirectory();
};

/**
 * @struct QcwLastVisitPath
 * @brief The path the user last dealt with, so file dialogs continue from there.
 *
 * @details The workbench's file dialogs (importing and exporting settings, in
 * QcwPreferencesWidget) start at this path instead of always opening the home directory.
 * The value is kept in QcwSettings under @c misc/openFile/lastPath and therefore survives a
 * restart; get() and set() are the only places that read and write that key.
 */
struct QCW_EXPORT QcwLastVisitPath
{
    /**
     * @brief The path to hand a file dialog as its starting point.
     * @return The stored path, or QDir::homePath() when nothing usable was stored — no value
     *         yet, an empty value, or a directory that no longer exists (a removable or
     *         renamed location from an earlier session).
     */
    static QString get();

    /**
     * @brief Stores @p path as the next dialog's starting point.
     * @param path A directory, or a file — in which case its directory is stored, since a
     *             dialog wants to open somewhere rather than on a file.
     * @note Ignored when @p path carries no directory at all (a bare "Settings.ini", or an
     *       empty string from a dialog the user cancelled), which keeps the previous value
     *       instead of overwriting it with nothing.
     */
    static void set(const QString& path);
};

QT_END_NAMESPACE
