// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QHash>
#include <QString>
#include <any>
#include <utility>

QT_BEGIN_NAMESPACE

/**
 * @class QcwSessionData
 * @brief Process-wide in-memory key/value store, the application's equivalent of the Unix
 *        environment.
 *
 * @details Parts of the application use it to hand values to one another without linking to one
 * another: setValue() stores a copy under a string key, value() retrieves it from anywhere in
 * the process. Nothing is written to disk and nothing is restored on the next start; when the
 * process exits, the store is gone. Use QcwSettings or QcwPersistable for data that must
 * survive a restart.
 *
 * A key holds one value; setting an existing key replaces it. Types are matched exactly: a
 * value stored as @c int can only be read as @c int, not as @c qint64 or QVariant. The store
 * takes no ownership and cannot notice that a stored pointer's pointee was destroyed, so store
 * QPointer or QSharedPointer when the object may not live for the rest of the process.
 *
 * All keys share one flat namespace; prefix them per component (for example
 * @c "myplugin/settings") so that unrelated parts cannot overwrite each other's entries.
 *
 * @note Not thread-safe; use it from the GUI thread.
 * @note This is the single global instance: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because nothing connects to
 * it.
 * @see QcwSettings, QcwPersistable
 */
class QCW_EXPORT QcwSessionData
{
public:
    /// Returns the process-wide store, creating it on first call.
    static QcwSessionData* instance();

    /**
     * @brief Stores a copy of @p value under @p key, replacing any previous value.
     * @tparam T Value type, stored with references and top-level const removed. It must be
     *         copy-constructible.
     * @param key Key to write.
     * @param value Value to copy into the store.
     *
     * @note A string literal is stored as @c const char*; pass a QString when the value should
     * be read back as one.
     */
    template <typename T>
    void setValue(const QString& key, T&& value)
    {
        mData.insert(key, std::any(std::forward<T>(value)));
    }

    /**
     * @brief Returns the value stored under @p key.
     * @tparam T Type the value was stored with; no conversion is performed.
     * @param key Key to look up.
     * @param defaultValue Value returned when @p key is absent or holds another type.
     * @return A copy of the stored value, or @p defaultValue.
     */
    template <typename T>
    T value(const QString& key, T defaultValue = T()) const
    {
        const auto it = mData.constFind(key);
        if (it == mData.cend())
        {
            return defaultValue;
        }

        if (const T* stored = std::any_cast<T>(&it.value()))
        {
            return *stored;
        }

        return defaultValue;
    }

private:
    QHash<QString, std::any> mData;
};

/// Shorthand for QcwSessionData::instance().
#ifndef sessionData
#define sessionData QcwSessionData::instance()
#endif

QT_END_NAMESPACE
