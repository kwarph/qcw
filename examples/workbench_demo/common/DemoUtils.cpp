// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "DemoUtils.h"
#include "DemoServices.h"

#include <QString>
#include <QFileInfo>

namespace demo
{

QString normalizedPath(const QString& path)
{
    if (path.isEmpty())
    {
        return {};
    }

    const QFileInfo info { path };
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

bool pathIsUnder(const QString& path, const QString& prefix)
{
    if (prefix.isEmpty() || path.isEmpty())
    {
        return false;
    }

    const QString normalPath = normalizedPath(path);
    const QString normalPrefix = normalizedPath(prefix);
    if (normalPath == normalPrefix)
    {
        return true;
    }

    return normalPath.startsWith(normalPrefix.endsWith(QLatin1Char('/')) ? normalPrefix :
                                                                           normalPrefix + QLatin1Char('/'));
}

QStringList rememberedProjectRoots()
{
    auto settings = QcwSettings::get();
    QStringList roots = settings.value(QString(ProjectRootsKey)).toStringList();

    if (roots.isEmpty())
    {
        // Before several projects were supported, one folder lived under the legacy key.
        const QString legacy = settings.value(QString(ProjectRootKey)).toString();
        if (!legacy.isEmpty())
        {
            roots.append(legacy);
        }
    }

    return roots;
}

void rememberProjectRoots(const QStringList& roots)
{
    auto settings = QcwSettings::get();
    if (roots.isEmpty())
    {
        settings.remove(QString(ProjectRootsKey));
    }
    else
    {
        settings.setValue(QString(ProjectRootsKey), roots);
    }

    settings.remove(QString(ProjectRootKey));
}

QString rememberedCurrentProject()
{
    const QString current = QcwSettings::get().value(QString(CurrentProjectKey)).toString();
    return current.isEmpty() ? rememberedProjectRoots().value(0) : current;
}

void rememberCurrentProject(const QString& root)
{
    auto settings = QcwSettings::get();
    if (root.isEmpty())
    {
        settings.remove(QString(CurrentProjectKey));
    }
    else
    {
        settings.setValue(QString(CurrentProjectKey), root);
    }
}

} // namespace demo
