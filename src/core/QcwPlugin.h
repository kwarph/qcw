// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @class QcwPlugin
 * @brief Base of every object a plugin .so hands to the workbench.
 *
 * @details A plugin binary exports one QObject whose class implements QcwPlugin, usually
 * through a specialised interface such as @ref QcwViewCreator or @ref QcwPerspectiveCreator.
 * QcwPluginLoader opens the binary, qobject_casts the root component to QcwPlugin* and hands
 * the object to @c pluginManager, where the seven accessors below are what identifies it.
 *
 * Because QObject casting — not the file — decides what a plugin is, the binary must declare
 * the interface with @c Q_INTERFACES and @c Q_PLUGIN_METADATA; see QCW_PLUGIN_INTERFACE and
 * QCW_PLUGIN_IMPLEMENT, the macros that generate the boilerplate for these accessors.
 *
 * @par Identity
 * type() is the plugin's key: it is what QcwPluginManager indexes on, what callers pass back
 * to create something and what QcwWindow writes into the saved state, so it has to be unique
 * within the process and stable across runs — renaming it invalidates every saved perspective
 * that refers to it. category() is the much looser grouping the workbench lists by (all views
 * under @c "View", all perspectives under @c "Perspective"); it names a family, not one plugin.
 *
 * @note The remaining accessors — version(), provider(), updateTime(), description() and
 * details() — are pure metadata for a plugin browser; the workbench itself never reads them.
 * Their text is free-form, and updateTime() is no exception: it is a display string, not a
 * value to parse.
 *
 * @par Build contract
 * A plugin has to be built and shipped together with the QCW library it is loaded into. The
 * framework passes views, perspectives and other derived objects back and forth by pointer
 * and shares the library's class layouts, so no binary compatibility exists across library
 * builds and none is promised. Nothing in this interface detects a mismatch either: version()
 * is a display string, not a check the loader performs. Keeping an application and its
 * plugins in step is the job of the application's update mechanism, which distributes them as
 * one set and replaces whatever differs.
 *
 * @par Contributions
 * Once the object is registered, QcwPluginLoader calls registerContributions(), which
 * forwards to the protected doRegisterContributions() hook. That hook is where a plugin
 * registers what it adds to the framework — for example a preferences page with
 * @c preferencesManager or a service with @c serviceRegistry. The default does nothing,
 * which is right for a plugin that contributes nothing beyond itself. An object whose
 * type() was already registered is left unused and is not called.
 *
 * @see QcwPluginManager, QcwPluginLoader
 */
class QCW_EXPORT QcwPlugin : public QObject
{
    Q_OBJECT
public:
    /// The family this plugin belongs to, e.g. @c "View"; used to group plugins for listing.
    virtual QString category() const = 0;

    /**
     * @brief The unique id this plugin is registered and looked up under, e.g. @c "SampleView".
     * @details Must not change once a version has been shipped, because saved state refers
     * to plugins by this string.
     */
    virtual QString type() const = 0;

    /// Version string for display, e.g. @c "1.0.0".
    virtual QString version() const = 0;

    /// Who wrote the plugin, for display.
    virtual QString provider() const = 0;

    /// When the plugin was last updated, as a display string; not meant to be parsed.
    virtual QString updateTime() const = 0;

    /// One-line summary for display.
    virtual QString description() const = 0;

    /// Longer, multi-line information for display; empty when there is nothing to add.
    virtual QString details() const = 0;

protected:
    /**
     * @brief Registers this plugin's contributions to the framework.
     *
     * @details Override this to register what the plugin adds — a preferences page with
     * @c preferencesManager, a service with @c serviceRegistry, and so on. A derived class
     * may call the implementation of its base class before adding its own, which is why the
     * hook is protected rather than private.
     *
     * QcwPluginLoader does not call this directly: it calls registerContributions(), which
     * forwards here exactly once, right after the object has been registered with
     * @c pluginManager. It is not called at all when registration was refused because type()
     * was already taken. Loading is synchronous, so the call happens before loadPlugin()
     * returns.
     *
     * The default does nothing, which suits a plugin that contributes nothing beyond itself.
     * There is no counterpart at shutdown — a loaded plugin stays for the rest of the run.
     */
    virtual void doRegisterContributions()
    {
    }

private:
    friend struct QcwPluginLoader;

    /// Framework entry point: QcwPluginLoader calls this once after registration.
    void registerContributions()
    {
        doRegisterContributions();
    }
};

QT_END_NAMESPACE
