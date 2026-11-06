// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTopicRegistry.h"

#include <QDebug>
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwTopicRegistry, topicRegistryInstance)

QcwTopicRegistry* QcwTopicRegistry::instance()
{
    return topicRegistryInstance;
}

bool QcwTopicRegistry::registerTopic(const QcwTopicInfo& info)
{
    if (info.id.isEmpty())
    {
        qWarning() << "QcwTopicRegistry: rejecting topic with empty id";
        return false;
    }

    if (!info.id.startsWith(QLatin1String("app/")) && !info.id.startsWith(QLatin1String("win/")))
    {
        qWarning() << "QcwTopicRegistry: topic" << info.id
                   << "does not follow the recommended naming convention ('app/...' or 'win/...')";
    }

    mTopics.insert(info.id, info);
    return true;
}

QList<QcwTopicInfo> QcwTopicRegistry::allTopics() const
{
    return mTopics.values();
}

QList<QcwTopicInfo> QcwTopicRegistry::topicsByPublisher(const QString& publisher) const
{
    QList<QcwTopicInfo> result;
    for (const auto& info : mTopics)
    {
        if (info.publisher == publisher)
        {
            result.append(info);
        }
    }
    return result;
}

QT_END_NAMESPACE
