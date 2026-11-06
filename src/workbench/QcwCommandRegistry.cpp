// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwCommandRegistry.h"
#include "QcwIdUtils.h"

#include <QDebug>
#include <algorithm>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwCommandRegistry, commandRegistryInstance)

QcwCommandRegistry* QcwCommandRegistry::instance()
{
    return commandRegistryInstance;
}

bool QcwCommandRegistry::registerCommand(const QcwCommandInfo& info)
{
    if (!qcw::isValidId(info.id))
    {
        qWarning() << "QcwCommandRegistry: rejecting command with invalid id" << info.id;
        return false;
    }

    auto it = mCommands.find(info.id);
    if (it != mCommands.end() && it.value() == info)
    {
        return false;
    }

    if (it != mCommands.end())
    {
        qWarning() << "QcwCommandRegistry: replacing existing command" << info.id;
    }

    mCommands.insert(info.id, info);
    emit commandsChanged();
    return true;
}

bool QcwCommandRegistry::unregisterCommand(const QString& id)
{
    if (mCommands.remove(id) == 0)
    {
        return false;
    }

    emit commandsChanged();
    return true;
}

std::optional<QcwCommandInfo> QcwCommandRegistry::command(const QString& id) const
{
    auto it = mCommands.constFind(id);
    if (it == mCommands.constEnd())
    {
        return std::nullopt;
    }

    return it.value();
}

bool QcwCommandRegistry::contains(const QString& id) const
{
    return mCommands.contains(id);
}

QList<QcwCommandInfo> QcwCommandRegistry::commands() const
{
    auto entries = mCommands.values();

    std::sort(entries.begin(), entries.end(), [](const QcwCommandInfo& left, const QcwCommandInfo& right)
    {
        const auto compareCategory = QString::compare(left.category, right.category, Qt::CaseInsensitive);
        if (compareCategory != 0)
        {
            return compareCategory < 0;
        }

        const auto compareLabel = QString::compare(left.label, right.label, Qt::CaseInsensitive);
        if (compareLabel != 0)
        {
            return compareLabel < 0;
        }

        return QString::compare(left.id, right.id) < 0;
    });

    return entries;
}

QT_END_NAMESPACE
