// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPlugin.h"
#include <QMap>
#include <QSet>

QT_BEGIN_NAMESPACE

/**
 * @class QcwPluginManager
 * @brief The process-wide registry of the loaded plugins, keyed by QcwPlugin::type().
 *
 * @details QcwPluginLoader puts plugins in here as it opens them; everything that later needs
 * a plugin — QcwWindow creating a view or a perspective, the menus listing the available
 * ones — reads it back from here. Use the @c pluginManager shorthand for the one instance,
 * which is always valid.
 *
 * @note This is the single global instance: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because lookups go through
 * the @c pluginManager macro, so a second manager would never be seen.
 *
 * @par Ownership
 * The map holds raw pointers and owns nothing. A plugin object lives in its library, and Qt
 * keeps that library mapped — and the object with it — until the application terminates, so
 * a registered pointer stays good for the whole run. Nothing here deletes a plugin, and
 * QcwPluginLoader never calls unload(), because that would delete the objects registered
 * here.
 *
 * @see QcwPlugin, QcwPluginLoader
 */
class QCW_EXPORT QcwPluginManager
{
public:
    static QcwPluginManager* instance();

    /**
     * @brief Registers @p plugin under its QcwPlugin::type().
     * @return @c true when it was added, @c false when a plugin of that type is already
     *         registered — or when @p plugin is null.
     *
     * @details Refusing to replace is deliberate: two objects claiming one type would leave
     * every pointer to the first one in hand while lookups started returning the second. The
     * object that lost the race is left alone, not deleted.
     */
    bool registerPlugin(QcwPlugin* plugin);

    /**
     * @brief Forgets the plugin registered under @p type.
     * @return The plugin that was registered, or @c nullptr when @p type is unknown.
     *
     * @details The pointer is returned rather than deleted, because it cannot be deleted here:
     * the object's code belongs to a library that stays mapped, and the caller has no way to
     * unload it either (see the ownership note above). A host that wants to stop offering a
     * plugin removes it here and keeps the pointer; tests use it to release plugins they
     * created themselves. Production code registers once and never removes.
     */
    QcwPlugin* removePlugin(const QString& type);

    /**
     * @brief The plugin registered under @p type, cast to @c PluginType.
     * @return The cast plugin, or @c nullptr when @p type is unknown or is not a @c
     *         PluginType.
     *
     * @details Use this rather than getPlugin() plus a hand-written cast whenever the expected
     * class is known:
     * @code
     * auto creator = pluginManager->getPluginT<QcwViewCreator>("SampleView");
     * @endcode
     */
    template <typename PluginType>
    PluginType* getPluginT(const QString& type) const
    {
        if (auto plugin = this->getPlugin(type))
        {
            return qobject_cast<PluginType*>(plugin);
        }

        return nullptr;
    }

    /// The plugin registered under @p type, or @c nullptr when there is none.
    QcwPlugin* getPlugin(const QString& type) const;

    /// True when @p type is registered.
    bool contains(const QString& type) const;

    /// All plugins whose QcwPlugin::category() equals @p category.
    QList<QcwPlugin*> getPluginsByCategory(const QString& category) const;

    /// Every registered plugin.
    QList<QcwPlugin*> allPlugins() const;

    /// The type() of every registered plugin.
    QList<QString> allTypes() const;

    /// The distinct category() values among the registered plugins.
    QSet<QString> allCategories() const;

    /// True when no plugin is registered.
    bool empty() const;

    /// How many plugins are registered.
    int size() const;

private:
    QMap<QString, QcwPlugin*> mPlugins;
};

/// Shorthand for the process-wide manager, see QcwPluginManager::instance().
/// Guarded so a project that already defines it keeps its own definition.
#ifndef pluginManager
#define pluginManager QcwPluginManager::instance()
#endif

QT_END_NAMESPACE
