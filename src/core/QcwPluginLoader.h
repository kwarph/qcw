// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwPluginLoader
 * @brief Opens plugin binaries and registers what they export.
 *
 * @details The two entries differ in how many files they take: loadPlugins() scans one
 * directory, its overload scans a list, and loadPlugin() handles one file. All are one-way —
 * a plugin that loads is registered with @c pluginManager and stays for the rest of the run.
 * Once registration succeeds, QcwPlugin::registerContributions() lets the object register
 * what it contributes; the loader itself knows nothing about those contributions.
 *
 * @par Where plugins come from
 * defaultPluginDirectories() reports the conventional locations for an application: its
 * own @c plugins directory for a build tree or portable install, then the platform's
 * installed location — @c Contents/PlugIns inside a macOS bundle, a private library
 * directory such as @c /usr/lib/myapp/plugins on Unix. Windows installers put the plugin
 * DLLs next to the executable, so the app-local directory is the only one there. Pass the
 * result to loadPlugins(); directories that do not exist are simply skipped.
 *
 * @note Loading needs a QApplication, because a plugin that cannot be opened is reported with
 * a message box rather than only a log line. That is why QcwWorkbench calls this from
 * initialize(), once the application object exists.
 *
 * @note Nothing here ever calls QPluginLoader::unload(). The registered objects live in those
 * libraries, and unloading one deletes its root component — every pointer @c pluginManager
 * holds to it would dangle. Qt leaves a loaded plugin in memory until the application
 * terminates even after its QPluginLoader is destroyed, which is what makes handing the
 * instance to the manager safe.
 *
 * @see QcwPlugin, QcwPluginManager
 */
struct QCW_EXPORT QcwPluginLoader
{
    /**
     * @brief Load and register the plugin in @p filePath.
     * @param filePath Full path of one plugin binary; anything without a library suffix is
     *                 rejected (the directory scan in loadPlugins() filters those out first).
     * @return @c true when the plugin is available afterwards — including the case where an
     *         object of this type() is already registered from another file, which is logged
     *         and left alone rather than registered twice (it does not get the contribution
     *         callback). @c false when the file could not be opened or does not export a
     *         @ref QcwPlugin; both are reported to the user.
     */
    static bool loadPlugin(const QString& filePath);

    /**
     * @brief The plugin directories an application normally uses, most specific first.
     * @return Candidate directories, in lookup order.
     *
     * @details The list starts with <tt>applicationDirPath()/plugins</tt>, which covers a
     * build tree and a portable install on every platform. Platform-specific locations
     * follow:
     * - Windows: none beyond that directory — plugins are DLLs installed next to the
     *   executable.
     * - macOS: for an application bundle, <tt>Contents/PlugIns</tt> and the @c plugins
     *   directory beside the @c .app, where a build tree keeps its copies. A non-bundle
     *   executable uses the Unix layout below.
     * - Unix: <tt>PREFIX/lib/NAME/plugins</tt>, plus <tt>PREFIX/lib64/NAME/plugins</tt> and
     *   the multiarch <tt>PREFIX/lib/TRIPLET/NAME/plugins</tt> variants that exist on Linux.
     *   PREFIX is found by walking up from the executable directory to the nearest @c bin
     *   directory and taking its parent; NAME is the executable's base name and, when it
     *   differs, the application name.
     *
     * @note Directories may not exist; loadPlugins() skips the missing ones. The order is
     *       also the precedence: when two directories hold a plugin of the same type(), the
     *       earlier one is registered and the later copy is ignored.
     */
    static QStringList defaultPluginDirectories();

    /**
     * @brief Load every plugin binary in @p pluginsDir.
     * @param pluginsDir Directory holding the plugin files; files without a library suffix are
     *                   skipped quietly, since build leftovers and notes live here too.
     * @return @c false when the directory does not exist or at least one library failed.
     *
     * @details An empty or plugin-free directory is not a failure: a build with nothing to
     * load is normal, so this reports @c true there and leaves the count in the log.
     */
    static bool loadPlugins(const QString& pluginsDir);

    /**
     * @brief Load every plugin binary in each directory of @p pluginsDirectories.
     * @param pluginsDirectories Directories to scan in lookup order, e.g. the list from
     *                           defaultPluginDirectories().
     * @return @c false when at least one library failed; @c true otherwise.
     *
     * @details Directories that do not exist are skipped, so passing a list of candidates is
     * safe. Each library is loaded as soon as its directory is reached, which makes the
     * order the precedence: a plugin type() registered from an earlier directory is kept, and
     * a copy of it in a later one is logged and left unregistered.
     */
    static bool loadPlugins(const QStringList& pluginsDirectories);
};

QT_END_NAMESPACE
