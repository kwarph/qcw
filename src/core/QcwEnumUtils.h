// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QByteArray>
#include <QMetaEnum>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwEnumUtils
 * @brief Converts a registered enum to and from its string keys.
 *
 * @tparam EnumType Enum type declared with @c Q_ENUM or @c Q_ENUM_NS. For an enum registered
 *         with @c Q_FLAG, pass the @c QFlags type named by the macro, for example
 *         @c QcwEnumUtils<QStandardPaths::LocateOptions>; keys of a combination are then
 *         separated by @c '|'.
 *
 * @details The functions delegate to QMetaEnum and need no QObject instance. Keys are the
 * enumerator names exactly as declared, and matching is case-sensitive. If @c EnumType is not
 * registered with the meta-object system (passing the bare enum type of a Q_FLAG enum, for
 * example), the conversions fail: key() returns an empty string, value() returns -1 and sets
 * @p ok to @c false, and keys() returns an empty list.
 *
 * @note All functions are static and only read the meta-object data of @c EnumType.
 * @see QMetaEnum
 */
template <typename EnumType>
struct QcwEnumUtils
{
    /**
     * @brief Returns the key of @p value.
     * @param value Numeric enumerator value, or a combination of values for a @c Q_FLAG enum.
     * @return The enumerator name, the names separated by @c '|' for a flag combination, or an
     *         empty string when no key matches.
     */
    static QString key(int value)
    {
        const QMetaEnum metaEnum = QMetaEnum::fromType<EnumType>();
        if (metaEnum.isFlag())
        {
            return QString::fromLatin1(metaEnum.valueToKeys(value));
        }

        const char* name = metaEnum.valueToKey(value);
        return name ? QString::fromLatin1(name) : QString {};
    }

    /**
     * @brief Returns the value of @p key.
     * @param key Enumerator name, case-sensitive. For a @c Q_FLAG enum this may be one name or
     *        several names separated by @c '|'.
     * @param ok Set to @c true when @p key was recognized, @c false otherwise; may be null.
     * @return The numeric value, or -1 when @p key is unknown.
     *
     * @note -1 can also be a valid enumerator value; use @p ok to distinguish the two cases.
     */
    static int value(const QString& key, bool* ok = nullptr)
    {
        const QMetaEnum metaEnum = QMetaEnum::fromType<EnumType>();
        const QByteArray encodedKey = key.toLatin1();
        if (metaEnum.isFlag())
        {
            return metaEnum.keysToValue(encodedKey.constData(), ok);
        }

        return metaEnum.keyToValue(encodedKey.constData(), ok);
    }

    /**
     * @brief Returns every enumerator key in declaration order.
     * @return The keys, including aliases; empty if @c EnumType is not registered.
     */
    static QStringList keys()
    {
        const QMetaEnum metaEnum = QMetaEnum::fromType<EnumType>();
        QStringList result;
        result.reserve(metaEnum.keyCount());

        for (int i = 0; i < metaEnum.keyCount(); ++i)
        {
            result.append(QString::fromLatin1(metaEnum.key(i)));
        }

        return result;
    }
};

QT_END_NAMESPACE
