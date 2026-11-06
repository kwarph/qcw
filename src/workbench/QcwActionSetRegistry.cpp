// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwActionSetRegistry.h"

#include "QcwIdUtils.h"

#include <QDebug>
#include <QGlobalStatic>

#include <algorithm>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwActionSetRegistry, actionSetRegistryInstance);

QcwActionSetRegistry* QcwActionSetRegistry::instance()
{
    return actionSetRegistryInstance;
}

bool QcwActionSetRegistry::registerActionSet(const QcwActionSet& set)
{
    if (!qcw::isValidId(set.id))
    {
        qWarning() << "QcwActionSetRegistry: rejecting action set with invalid id" << set.id;
        return false;
    }

    if (set.label.isEmpty())
    {
        qWarning() << "QcwActionSetRegistry: rejecting action set" << set.id << "with empty label";
        return false;
    }

    if (mActionSets.contains(set.id))
    {
        qWarning() << "QcwActionSetRegistry: replacing existing action set" << set.id;
    }

    mActionSets.insert(set.id, set);
    return true;
}

bool QcwActionSetRegistry::unregisterActionSet(const QString& id)
{
    return mActionSets.remove(id) > 0;
}

const QcwActionSet* QcwActionSetRegistry::actionSet(const QString& id) const
{
    auto it = mActionSets.constFind(id);
    return it == mActionSets.constEnd() ? nullptr : &it.value();
}

bool QcwActionSetRegistry::contains(const QString& id) const
{
    return mActionSets.contains(id);
}

QList<QcwActionSet> QcwActionSetRegistry::allActionSets() const
{
    auto sets = mActionSets.values();
    std::sort(sets.begin(), sets.end(), [](const QcwActionSet& a, const QcwActionSet& b)
    {
        return a.id < b.id;
    });
    return sets;
}

QT_END_NAMESPACE
