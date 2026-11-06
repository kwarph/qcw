// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPlugin.h"
#include <QIcon>
#include <QStringList>

QT_BEGIN_NAMESPACE

class QcwPerspective;
class QcwWindow;

/**
 * @class QcwPerspectiveCreator
 * @brief The interface a plugin implements to offer a type of perspective.
 *
 * @details One creator per perspective type. QcwWindow lists the creators of
 * QcwPerspectiveCreator::Category to fill the "new perspective" menu, and asks the selected
 * one for an instance; the type() the creator reports is also the value a saved perspective
 * carries, so opening one again means finding its creator again — a plugin that changes or
 * drops its type() strands every perspective saved under the old one.
 *
 * @par The template method
 * create() is the entry point and is not meant to be overridden: it calls the subclass's
 * doCreate() and stamps the result with this creator's identity (type, name, icon). A plugin
 * therefore only implements doCreate(), which has to return the perspective already parented
 * to the window it is given.
 *
 * @see QcwPerspective, QcwPlugin
 */
class QCW_EXPORT QcwPerspectiveCreator : public QcwPlugin
{
public:
    /// The category these plugins are registered under, used to list the available ones.
    static constexpr QLatin1String Category { "Perspective" };

    /**
     * @brief Build a perspective of this type for @p window.
     * @param window The window the perspective is created for; it owns the result.
     * @return The new perspective, or nullptr when doCreate() produced none.
     *
     * @details The returned perspective is empty: create() lays nothing out, because the
     * caller knows whether this is a fresh perspective (createInitialLayout()) or one being
     * restored from a document (restoreState()).
     */
    QcwPerspective* create(QcwWindow* window);

    /// @brief The category this creator's plugins belong to.
    QString category() const override
    {
        return Category;
    }

    /**
     * @brief The icon a perspective of this type starts with.
     * @details Never persisted, so every restore shows this icon again — see
     * QcwPerspective::setIcon() for changing it at runtime.
     */
    virtual QIcon defaultPerspectiveIcon() const = 0;

    /// The name a perspective of this type starts with, before the user renames or saves it.
    virtual QString defaultName() const = 0;

    /**
     * @brief The action sets this perspective starts with.
     * @details Applied by create() to the new perspective. Empty means every action set is
     * visible. An application that wants a perspective to start with a restricted set
     * overrides this.
     */
    virtual QStringList defaultActionSetIds() const
    {
        return {};
    }

protected:
    /// Makes the perspective object itself; it must be a child of @p window.
    virtual QcwPerspective* doCreate(QcwWindow* window) = 0;
};

#define QcwPerspectiveCreator_iid "org.qcw.QcwPerspectiveCreator"

Q_DECLARE_INTERFACE(QcwPerspectiveCreator, QcwPerspectiveCreator_iid)

QT_END_NAMESPACE
