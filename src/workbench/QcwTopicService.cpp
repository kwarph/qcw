// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTopicService.h"
#include "QcwWindow.h"
#include "QcwWorkbench.h"

#include <QCoreApplication>

QT_BEGIN_NAMESPACE

QcwTopicService::QcwTopicService(QObject* parent) :
    QObject { parent }
{
}

QcwTopicService::~QcwTopicService() = default;

QcwTopicService* QcwTopicService::forWindow(QcwWindow* window)
{
    return window ? window->topicService() : nullptr;
}

QcwTopicService* QcwTopicService::forApplication()
{
    if (auto* workbench = qobject_cast<QcwWorkbench*>(QCoreApplication::instance()->findChild<QcwWorkbench*>()))
    {
        return workbench->topicService();
    }

    // Fallback for unit tests or applications without a QcwWorkbench
    static QcwTopicService fallback;
    return &fallback;
}

void QcwTopicService::publish(const QString& topicId, const QVariantMap& payload)
{
    if (topicId.isEmpty())
    {
        return;
    }

    // Every topic is cached today: QcwTopicInfo::isStateful is catalog metadata only, so an
    // announcement-style topic also replays its last payload to a late subscriber.
    mState.insert(topicId, payload);

    const auto it = mSubscribers.constFind(topicId);
    if (it != mSubscribers.constEnd())
    {
        // Copy the list in case a handler unsubscribes during iteration
        const auto subscribers = it.value();
        for (const auto& sub : subscribers)
        {
            if (sub.receiver && sub.handler)
            {
                sub.handler(payload);
            }
        }
    }
}

void QcwTopicService::clear(const QString& topicId)
{
    mState.remove(topicId);
}

QVariantMap QcwTopicService::current(const QString& topicId) const
{
    return mState.value(topicId);
}

QMetaObject::Connection
QcwTopicService::subscribe(const QString& topicId, QObject* receiver, std::function<void(const QVariantMap&)> handler)
{
    if (topicId.isEmpty() || !receiver || !handler)
    {
        return QMetaObject::Connection();
    }

    Subscription sub;
    sub.receiver = receiver;
    sub.handler = std::move(handler);

    sub.destroyedConnection = connect(receiver, &QObject::destroyed, this, [this, topicId, receiver]()
    {
        unsubscribe(topicId, receiver);
    });

    mSubscribers[topicId].append(sub);

    // Replay current state if it exists
    if (const auto it = mState.constFind(topicId); it != mState.constEnd())
    {
        sub.handler(it.value());
    }

    return sub.destroyedConnection;
}

void QcwTopicService::unsubscribe(const QString& topicId, QObject* receiver)
{
    auto it = mSubscribers.find(topicId);
    if (it == mSubscribers.end())
    {
        return;
    }

    auto& list = it.value();
    for (int i = list.size() - 1; i >= 0; --i)
    {
        if (list[i].receiver == receiver || !list[i].receiver)
        {
            disconnect(list[i].destroyedConnection);
            list.removeAt(i);
        }
    }

    if (list.isEmpty())
    {
        mSubscribers.erase(it);
    }
}

QT_END_NAMESPACE
