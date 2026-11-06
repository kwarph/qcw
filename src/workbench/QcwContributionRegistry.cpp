// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwContributionRegistry.h"

#include "QcwIdUtils.h"

#include <QDebug>
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwContributionRegistry, contributionRegistryInstance);

QcwContributionRegistry* QcwContributionRegistry::instance()
{
    return contributionRegistryInstance;
}

bool QcwContributionRegistry::registerContribution(const QcwContribution& contribution)
{
    if (!qcw::isValidId(contribution.id))
    {
        qWarning() << "QcwContributionRegistry: rejecting contribution with invalid id" << contribution.id;
        return false;
    }

    if (contribution.target.isEmpty())
    {
        qWarning() << "QcwContributionRegistry: rejecting contribution" << contribution.id << "with empty target";
        return false;
    }

    if (contribution.commandId.isEmpty())
    {
        qWarning() << "QcwContributionRegistry: rejecting contribution" << contribution.id << "with empty commandId";
        return false;
    }

    if (contribution.before == contribution.id || contribution.after == contribution.id)
    {
        qWarning() << "QcwContributionRegistry: ignoring self-reference in contribution" << contribution.id;
    }

    if (const auto it = mIndexById.constFind(contribution.id); it != mIndexById.constEnd())
    {
        // A repeated id is normal: a language change re-declares the built-in contributions with
        // the same fields, and an application may register the same one twice. Only a duplicate
        // that says something different is worth reporting, because that is the one that quietly
        // moves an entry to another menu, changes its order, or hides it behind another action set.
        if (mContributions[it.value()] != contribution)
        {
            qWarning() << "QcwContributionRegistry: replacing existing contribution" << contribution.id;
        }

        mContributions[it.value()] = contribution;
    }
    else
    {
        mIndexById.insert(contribution.id, mContributions.size());
        mContributions.append(contribution);
    }

    emit contributionsChanged();
    return true;
}

bool QcwContributionRegistry::unregisterContribution(const QString& id)
{
    const auto it = mIndexById.constFind(id);
    if (it == mIndexById.constEnd())
    {
        return false;
    }

    const int index = it.value();
    mContributions.removeAt(index);
    mIndexById.remove(id);

    for (auto i = mIndexById.begin(); i != mIndexById.end(); ++i)
    {
        if (i.value() > index)
        {
            --i.value();
        }
    }

    emit contributionsChanged();
    return true;
}

int QcwContributionRegistry::unregisterContributionsOf(const QString& actionSetId)
{
    int removed = 0;
    for (int i = mContributions.size() - 1; i >= 0; --i)
    {
        if (mContributions.at(i).actionSetId == actionSetId)
        {
            mIndexById.remove(mContributions.at(i).id);
            mContributions.removeAt(i);
            ++removed;
        }
    }

    if (removed > 0)
    {
        mIndexById.clear();
        for (int i = 0; i < mContributions.size(); ++i)
        {
            mIndexById.insert(mContributions.at(i).id, i);
        }

        emit contributionsChanged();
    }

    return removed;
}

QList<QcwContribution> QcwContributionRegistry::contributionsFor(const QString& target) const
{
    QList<QcwContribution> result;
    for (const auto& contribution : mContributions)
    {
        if (contribution.target == target)
        {
            result.append(contribution);
        }
    }
    return result;
}

QList<QcwContribution> QcwContributionRegistry::allContributions() const
{
    return mContributions;
}

QT_END_NAMESPACE
