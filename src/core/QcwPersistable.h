// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QByteArray>
#include <QVariant>

QT_BEGIN_NAMESPACE

/**
 * @class QcwPersistable
 * @brief Interface for objects that add their own state to saved sessions.
 *
 * @details The workbench saves window geometry, views, folders and perspectives itself. An
 * object that must persist more, such as a plugin view's user settings, derives from
 * QcwPersistable and overrides saveExtraState() and restoreExtraState(). While saving,
 * extraStateToHex() serializes the map produced by saveExtraState() into the @c extraState XML
 * attribute; while restoring, the attribute is deserialized and passed to
 * restoreExtraState().
 *
 * The map is serialized with QDataStream, so every value must be of a type QDataStream can
 * write. Raw pointers cannot be persisted meaningfully; serialize the object they refer to
 * instead. Implementations should ignore keys they do not recognize so that state written by
 * another version still loads.
 *
 * This class neither saves nor restores anything by itself, and it is not a QObject: QcwView,
 * QcwWindow, QcwWorkbench and QcwPerspective inherit it alongside their QObject base. The
 * workbench decides when state is saved and restored.
 *
 * @see QcwSerializeUtils, QcwView, QcwWindow, QcwPerspective
 */
class QCW_EXPORT QcwPersistable
{
public:
    virtual ~QcwPersistable();

    /**
     * @brief Adds the object's state to @p state.
     * @param state Map filled by the implementation; it is serialized as a whole.
     *
     * Keys only have to be unique within this object's own state. The default implementation
     * adds nothing.
     * @see restoreExtraState()
     */
    virtual void saveExtraState(QVariantMap& state) const;

    /**
     * @brief Applies state previously produced by saveExtraState().
     * @param state Deserialized state map.
     *
     * Called while the saved session is applied to the object. Values are read with
     * QVariant::value<T>(); missing and unknown keys should be ignored. The default
     * implementation does nothing.
     * @see saveExtraState()
     */
    virtual void restoreExtraState(const QVariantMap& state);

    /**
     * @brief Serializes the object's state for the @c extraState XML attribute.
     * @return The QDataStream-serialized map, hex-encoded with spaces between bytes.
     *
     * Calls saveExtraState(). The owning window, workbench or perspective calls this; there is
     * normally no reason to call it directly.
     */
    QByteArray extraStateToHex() const;
};

QT_END_NAMESPACE
