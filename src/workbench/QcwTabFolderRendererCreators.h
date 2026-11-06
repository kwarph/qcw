// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QMap>
#include <QString>
#include <functional>

QT_BEGIN_NAMESPACE

class QcwTabFolder;
class QcwTabFolderRenderer;

/// Factory that builds a renderer for one folder; ownership of the result goes to the caller.
using QcwTabFolderRendererCreator = std::function<QcwTabFolderRenderer*(QcwTabFolder*)>;

/**
 * @class QcwTabFolderRendererCreators
 * @brief Process-wide registry that maps renderer names to factory functions.
 *
 * @details QcwWindowStyler reads the @c folderRenderer name of a QcwWindow.<i>style</i>.theme
 * file, looks it up here and calls the factory for every folder of every window.
 * QcwWorkbench registers the built-in renderers during start(); applications and plugins add
 * their own with registerCreator(), using the name their theme files will refer to.
 *
 * The registry stores copies of the factory functions and owns neither them nor the renderers
 * they create: a renderer returns with ownership at the caller, and QcwTabFolder takes it over
 * when the renderer is installed through QcwTabFolder::setRenderer(). Nothing unloads a plugin,
 * so there is no unregistration; registering a name again replaces its creator.
 *
 * The registry does no synchronization: register and query it on the GUI thread only.
 *
 * @note This is the single global registry: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because nothing connects to
 * it.
 * @see QcwWindowStyler, QcwTabFolderRenderer
 */
class QCW_EXPORT QcwTabFolderRendererCreators
{
public:
    /// Returns the process-wide registry, creating it on first call.
    static QcwTabFolderRendererCreators* instance();

    /**
     * @brief Registers @p creator under @p name.
     * @details The name is matched verbatim against the @c folderRenderer value of a window
     * style file. Registering an existing name replaces that creator; the last registration
     * wins.
     */
    void registerCreator(const QString& name, QcwTabFolderRendererCreator creator)
    {
        mCreators.insert(name, creator);
    }

    /**
     * @brief The creator registered under @p name.
     * @return The stored factory, or an empty std::function when @p name is unknown.
     */
    QcwTabFolderRendererCreator getCreator(const QString& name) const
    {
        return mCreators.value(name);
    }

    /// Number of registered creators.
    auto size() const
    {
        return mCreators.size();
    }

    /// Whether no creator is registered.
    bool empty() const
    {
        return mCreators.empty();
    }

private:
    QMap<QString, QcwTabFolderRendererCreator> mCreators;
};

/// Shorthand for QcwTabFolderRendererCreators::instance().
#ifndef folderRendererCreators
#define folderRendererCreators QcwTabFolderRendererCreators::instance()
#endif

QT_END_NAMESPACE
