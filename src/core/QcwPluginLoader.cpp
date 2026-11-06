// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPluginLoader.h"
#include "QcwPlugin.h"
#include "QcwPluginManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMessageBox>
#include <QPluginLoader>

QT_BEGIN_NAMESPACE

namespace
{
#ifdef Q_OS_UNIX
// An installation prefix is recognisable by the bin directory the executable sits in. The
// executable may be one level down from it (PREFIX/bin/examples), so check every ancestor.
QStringList installationPrefixes(const QString& applicationDirPath)
{
    QStringList prefixes;
    QDir dir(applicationDirPath);

    for (;;)
    {
        if (dir.dirName() == QStringLiteral("bin"))
        {
            prefixes.append(QDir(dir.absoluteFilePath(QStringLiteral(".."))).absolutePath());
        }

        const QString path = dir.absolutePath();
        if (!dir.cdUp() || dir.absolutePath() == path)
        {
            break;
        }
    }

    return prefixes;
}

// Installed layouts are named after the CMake project, which is usually the executable's
// name, but the workbench is initialized with a separate application name that may differ.
QStringList applicationDirectoryNames()
{
    QStringList names;
    const QString executableName = QFileInfo(QCoreApplication::applicationFilePath()).completeBaseName();
    if (!executableName.isEmpty())
    {
        names.append(executableName);
    }

    const QString applicationName = QCoreApplication::applicationName();
    if (!applicationName.isEmpty() && !names.contains(applicationName))
    {
        names.append(applicationName);
    }

    return names;
}
#endif
} // namespace

bool QcwPluginLoader::loadPlugin(const QString& filePath)
{
    if (!QLibrary::isLibrary(filePath))
    {
        qWarning() << filePath << "is not a library file";
        return false;
    }

    // A local loader: its destructor leaves the plugin in memory, which is what lets the
    // manager keep the instance below. See the note about unload() in the header.
    QPluginLoader pluginLoader(filePath);

    QObject* plugin = pluginLoader.instance();
    if (!plugin)
    {
        QMessageBox::warning(nullptr, QObject::tr("Loading Plugin Failed"), pluginLoader.errorString());
        return false;
    }

    auto* creator = qobject_cast<QcwPlugin*>(plugin);
    if (!creator)
    {
        QMessageBox::warning(nullptr, QObject::tr("Loading Plugin Failed"),
                             QObject::tr("The type of the loaded plugin is not \"QcwPlugin\""));
        return false;
    }

    if (!pluginManager->registerPlugin(creator))
    {
        // Some other file already claimed this type. The object stays as it is: it cannot be
        // deleted here, and the file may well be the very library the registered instance came
        // from, so unloading would take that one with it.
        qWarning() << "Plugin" << creator->type() << "is already registered, ignoring" << filePath;
        return true;
    }

    creator->registerContributions();

    qInfo().noquote()
        << QStringLiteral("Loaded plugin \"%1\" %2 from %3").arg(creator->type(), creator->version(), filePath);
    return true;
}

QStringList QcwPluginLoader::defaultPluginDirectories()
{
    const QDir applicationDir(QCoreApplication::applicationDirPath());

    QStringList directories;
    directories.append(applicationDir.absoluteFilePath(QStringLiteral("plugins")));

#ifdef Q_OS_MACOS
    // A bundle keeps its plugins in Contents/PlugIns. In a build tree the bundle sits in bin/
    // with the plugin copies beside it, which is where the example projects deploy them.
    QDir contentsDir(applicationDir.absolutePath());
    if (contentsDir.dirName() == QStringLiteral("MacOS") && contentsDir.cdUp() &&
        contentsDir.dirName() == QStringLiteral("Contents"))
    {
        directories.append(contentsDir.absoluteFilePath(QStringLiteral("PlugIns")));

        QDir bundleDir(contentsDir.absolutePath());
        if (bundleDir.cdUp())
        {
            directories.append(bundleDir.absoluteFilePath(QStringLiteral("plugins")));
        }
    }
#endif

#ifdef Q_OS_UNIX
    const QStringList names = applicationDirectoryNames();
    for (const QString& prefix : installationPrefixes(applicationDir.absolutePath()))
    {
        for (const QString& name : names)
        {
            directories.append(QDir(prefix).absoluteFilePath(QStringLiteral("lib/%1/plugins").arg(name)));

#ifdef Q_OS_LINUX
            directories.append(QDir(prefix).absoluteFilePath(QStringLiteral("lib64/%1/plugins").arg(name)));

            // Multiarch distributions install into PREFIX/lib/TRIPLET/NAME/plugins. The
            // triplet is chosen by the distribution, so use the directories that exist
            // instead of hard-coding one.
            const QDir libDir(QDir(prefix).absoluteFilePath(QStringLiteral("lib")));
            const auto triplets = libDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString& triplet : triplets)
            {
                const QString pluginDir = libDir.absoluteFilePath(QStringLiteral("%1/%2/plugins").arg(triplet, name));
                if (QFileInfo(pluginDir).isDir())
                {
                    directories.append(pluginDir);
                }
            }
#endif
        }
    }
#endif

    directories.removeDuplicates();
    return directories;
}

bool QcwPluginLoader::loadPlugins(const QString& pluginsDir)
{
    QDir dir(pluginsDir);
    if (!dir.exists())
    {
        QMessageBox::critical(nullptr, QObject::tr("Loading Plugins Failed"),
                              QObject::tr("No such directory '%1'.").arg(pluginsDir));
        return false;
    }

    int loaded = 0;
    int failed = 0;

    for (const auto& fileName : dir.entryList(QDir::Files))
    {
        // The directory also holds whatever the build dropped there, so a file that cannot be
        // a library is passed over instead of reported.
        if (!QLibrary::isLibrary(fileName))
        {
            continue;
        }

        if (loadPlugin(dir.absoluteFilePath(fileName)))
        {
            ++loaded;
        }
        else
        {
            ++failed;
        }
    }

    // One line rather than the item-per-argument form, so the counts read as a sentence in
    // the log file.
    qInfo().noquote()
        << QStringLiteral("Loaded %1 plugin(s) from %2, %3 failed").arg(loaded).arg(pluginsDir).arg(failed);

    // At least one failure is what the caller wants to hear about; loading nothing is not.
    return failed == 0;
}

bool QcwPluginLoader::loadPlugins(const QStringList& pluginsDirectories)
{
    bool success = true;

    for (const QString& pluginsDir : pluginsDirectories)
    {
        // The default list names layouts that most applications do not use, so a missing
        // directory is expected and not reported.
        if (!QDir(pluginsDir).exists())
        {
            continue;
        }

        if (!loadPlugins(pluginsDir))
        {
            success = false;
        }
    }

    return success;
}

QT_END_NAMESPACE
