// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPathUtils.h"
#include "QcwSettings.h"
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

QT_BEGIN_NAMESPACE

namespace
{

/// Settings key holding the path the last dialog started in; both QcwLastVisitPath entries use it.
const QLatin1String kLastVisitPathKey("misc/openFile/lastPath");

QString configDir;

} // namespace

bool QcwAppDirectories::setConfigDirectory(const QString& directory)
{
    static bool configDirSet = false;
    if (configDirSet)
    {
        qWarning() << "QcwAppDirectories::setConfigDirectory(): already set to" << configDir;
        return false;
    }

    const QString requested = directory.isEmpty()
                                  ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                                  : directory;
    if (requested.isEmpty() || !QDir().mkpath(requested))
    {
        qWarning() << "QcwAppDirectories::setConfigDirectory(): cannot create" << requested;
        return false;
    }

    configDir = QDir { requested }.absolutePath();
    configDirSet = true;
    return true;
}

QString QcwAppDirectories::configDirectory()
{
    return !configDir.isEmpty() ? configDir : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString QcwAppDirectories::logDirectory()
{
    return QString("%1/log").arg(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
}

QString QcwLastVisitPath::get()
{
    const QString path = QcwSettings::get().value(kLastVisitPathKey).toString();

    // Anything unusable falls back to home: no value stored yet, an empty one, or a
    // directory the user cannot reach anymore.
    if (path.isEmpty() || !QFileInfo::exists(path))
    {
        return QDir::homePath();
    }

    return path;
}

void QcwLastVisitPath::set(const QString& path)
{
    const QFileInfo info(path);

    // A path with no directory part has nothing to remember; keeping the stored value beats
    // writing an empty one, which the next get() would have to second-guess.
    const QString directory = info.isDir() ? path : info.path();
    if (directory.isEmpty())
    {
        return;
    }

    auto settings = QcwSettings::get();
    settings.setValue(kLastVisitPathKey, directory);
}

QT_END_NAMESPACE
