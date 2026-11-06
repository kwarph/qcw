// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "Workbench.h"
#include "MainWindow.h"

#include "QcwPluginLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

QcwWindow* Workbench::createWindow()
{
    auto window = new MainWindow;
    window->resize(1280, 800);
    return window;
}

bool Workbench::preStart()
{
    const QString appPlugins = QCoreApplication::applicationDirPath() + QStringLiteral("/plugins");
    QStringList directories { appPlugins + QStringLiteral("/workbench_demo") };

    // The app-local plugins directory is shared with the other examples, which keep their own
    // plugins there. An installed workbench uses the private library directory instead.
    for (const QString& directory : QcwPluginLoader::defaultPluginDirectories())
    {
        if (QDir::cleanPath(directory) != QDir::cleanPath(appPlugins))
        {
            directories.append(directory);
        }
    }

    if (!QcwPluginLoader::loadPlugins(directories))
    {
        qWarning() << "Errors occurred while loading plugins";
    }

    return true;
}
