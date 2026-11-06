// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPluginManager.h"
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwPluginManager, pluginManagerInstance);

QcwPluginManager* QcwPluginManager::instance()
{
    return pluginManagerInstance;
}

bool QcwPluginManager::registerPlugin(QcwPlugin* plugin)
{
    if (plugin == nullptr)
    {
        return false;
    }

    // First registration wins, so the pointers already handed out keep meaning the same
    // plugin. Reporting the clash is the caller's business; the loader names the file.
    if (mPlugins.contains(plugin->type()))
    {
        return false;
    }

    mPlugins.insert(plugin->type(), plugin);
    return true;
}

QcwPlugin* QcwPluginManager::removePlugin(const QString& type)
{
    auto it = mPlugins.find(type);
    if (it == mPlugins.end())
    {
        return nullptr;
    }

    QcwPlugin* plugin = it.value();
    mPlugins.erase(it);
    return plugin;
}

QcwPlugin* QcwPluginManager::getPlugin(const QString& type) const
{
    auto it = mPlugins.find(type);
    return it != mPlugins.end() ? it.value() : nullptr;
}

bool QcwPluginManager::contains(const QString& type) const
{
    return mPlugins.contains(type);
}

QList<QcwPlugin*> QcwPluginManager::getPluginsByCategory(const QString& category) const
{
    QList<QcwPlugin*> result;

    for (auto* plugin : mPlugins)
    {
        if (plugin->category() == category)
        {
            result.append(plugin);
        }
    }

    return result;
}

QList<QcwPlugin*> QcwPluginManager::allPlugins() const
{
    return mPlugins.values();
}

QList<QString> QcwPluginManager::allTypes() const
{
    return mPlugins.keys();
}

QSet<QString> QcwPluginManager::allCategories() const
{
    QSet<QString> result;

    for (auto* plugin : mPlugins)
    {
        result.insert(plugin->category());
    }

    return result;
}

bool QcwPluginManager::empty() const
{
    return mPlugins.empty();
}

int QcwPluginManager::size() const
{
    return mPlugins.size();
}

QT_END_NAMESPACE
