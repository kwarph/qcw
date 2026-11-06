// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QHash>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <functional>

QT_BEGIN_NAMESPACE

class QcwWindow;

/**
 * @class QcwTopicService
 * @brief A scoped, stateful publish-subscribe hub for context and events.
 *
 * @details A topic is a namespaced string; its payload is a QVariantMap. One service instance
 * holds the cache and the subscribers of one scope:
 * - QcwWindow::topicService() for topics that belong to a single window;
 * - QcwWorkbench::topicService() for topics shared by the whole process, also reachable through
 *   forApplication().
 *
 * The scopes are isolated: the same topic id published through two window services does not
 * travel between them, and a topic published on the application service reaches only that
 * service's subscribers. publish() does not route on the id — the caller chooses the service.
 *
 * @par Naming
 * Use an "app/" prefix for application-scope topics and "win/" for window-scope topics.
 * QcwTopicRegistry warns when a topic registered there uses neither prefix. The prefix records
 * the intended scope; it is not enforced at publish time.
 *
 * @par State
 * publish() stores the payload as the topic's current state and invokes the subscribers
 * synchronously, in subscription order. subscribe() replays the cached payload immediately when
 * there is one, so a consumer created after the publisher still sees the current value.
 * current() reads the cache without subscribing. clear() drops it, for example when the subject
 * a topic described is gone; it does not notify subscribers, so they learn about the empty
 * value by reading current() or when the publisher announces the change.
 *
 * The implementation currently caches every topic. QcwTopicInfo::isStateful is catalog
 * metadata for QcwTopicRegistry and is not consulted yet, so an announcement-style topic also
 * replays its last payload to a late subscriber. Consumers of announcements should use them to
 * refresh derived state rather than to reconstruct state from the payload.
 *
 * @par Subscription lifetime
 * A subscription belongs to @c receiver. The service removes it automatically when the receiver
 * is destroyed; call unsubscribe() to stop earlier. A handler runs synchronously in the thread
 * that published. It may unsubscribe, or destroy receivers, safely: publish() iterates over a
 * copy of the subscriber list and skips receivers that are already gone.
 *
 * @par Threads
 * Not thread-safe. publish(), subscribe(), unsubscribe(), current() and clear() must be called
 * on the GUI thread, where windows, views and their receivers live. Delivery is synchronous in
 * the calling thread; nothing is queued. A worker thread that has data to publish should queue
 * the call to the GUI thread with QMetaObject::invokeMethod().
 *
 * @par Contracts
 * Topic ids and payload keys are the contract between publisher and subscribers. Keep them in a
 * shared header, as the workbench demo does in plugins/common/DemoServices.h, and register them
 * in QcwTopicRegistry when tooling or other plugins should discover them. A view that publishes
 * its current subject should publish only while QcwWindow::currentView() returns the view
 * itself, and clear the topic when it stops being current; the window neither publishes nor
 * clears topics on the view's behalf. A view that subscribes to a window-scope topic should call
 * QcwView::subscribeWindowTopic() rather than subscribe() here and cache the window or this
 * service: the view then follows the window it is in through attach, moves, detach and teardown,
 * which is exactly the lifetime a cached pointer cannot track safely.
 *
 * @see QcwTopicRegistry, QcwWindow::topicService(), QcwWorkbench::topicService()
 */
class QCW_EXPORT QcwTopicService : public QObject
{
    Q_OBJECT
public:
    explicit QcwTopicService(QObject* parent = nullptr);
    ~QcwTopicService() override;

    /**
     * @brief The window-scope service of @p window.
     * @param window The window whose service to return.
     * @return The service, or nullptr when @p window is null.
     * @details Equivalent to window->topicService().
     * @see QcwWindow::topicService()
     */
    static QcwTopicService* forWindow(QcwWindow* window);

    /**
     * @brief The application-scope service.
     * @return The service owned by the QcwWorkbench attached to the QCoreApplication, or a
     *         process-wide fallback when the application has no workbench.
     * @details The fallback lets tests and small applications use application-scope topics
     *          without creating a workbench. It is created on first use and lives until the
     *          process exits. Requires a QCoreApplication instance.
     * @see QcwWorkbench::topicService()
     */
    static QcwTopicService* forApplication();

    /**
     * @brief Publishes @p payload as the current state of @p topicId and notifies subscribers.
     * @param topicId The topic id, e.g. "win/selection/editor" or "app/themeChanged".
     * @param payload The data; use the keys agreed in the topic's contract.
     * @details The payload replaces the cached state, and every subscriber of @p topicId is
     *          invoked synchronously, in subscription order. An empty @p topicId is ignored.
     *          Publishing does not require a prior registration in QcwTopicRegistry.
     * @note All topics are cached in the current implementation; see the class notes.
     * @see subscribe(), clear(), current()
     */
    void publish(const QString& topicId, const QVariantMap& payload = {});

    /**
     * @brief Drops the cached state of @p topicId.
     * @param topicId The topic to clear.
     * @details Subscribers are not notified. current() returns an empty map afterwards, and a
     *          later subscribe() replays nothing until the topic is published again. The
     *          operation affects only this service's scope.
     * @see publish(), current()
     */
    void clear(const QString& topicId);

    /**
     * @brief The cached state of @p topicId.
     * @param topicId The topic to read.
     * @return The last published payload, or an empty map when the topic was never published,
     *         was cleared, or belongs to another scope.
     * @details Returns a copy; modifying it does not change the cache.
     * @see publish(), clear()
     */
    QVariantMap current(const QString& topicId) const;

    /**
     * @brief Subscribes @p receiver to @p topicId.
     * @param topicId The topic id to follow.
     * @param receiver The object whose lifetime bounds the subscription; must not be null.
     * @param handler Invoked with each payload; must not be empty.
     * @return A connection owned by the subscription, or an invalid connection when @p topicId
     *         is empty, @p receiver is null, or @p handler is empty.
     *
     * @details The subscription is active immediately. If the topic has a cached payload, the
     *          handler runs once before subscribe() returns, so a consumer can rely on being in
     *          sync without a separate current() call. A receiver subscribed to the same topic
     *          twice gets two callbacks. The subscription is removed when @p receiver is
     *          destroyed.
     *
     * @note The returned connection is the internal receiver-destroyed connection. Use
     *       unsubscribe() to stop receiving; disconnecting the returned connection only stops
     *       the automatic cleanup, not the handler.
     * @see unsubscribe(), current(), publish()
     */
    QMetaObject::Connection
    subscribe(const QString& topicId, QObject* receiver, std::function<void(const QVariantMap&)> handler);

    /**
     * @brief Removes every subscription @p receiver has to @p topicId.
     * @param topicId The topic to unsubscribe from.
     * @param receiver The receiver whose subscriptions to remove.
     * @details Safe to call from inside a handler. Calling it for a topic or receiver without a
     *          subscription does nothing. Subscriptions whose receiver was already destroyed
     *          are removed as well.
     * @see subscribe()
     */
    void unsubscribe(const QString& topicId, QObject* receiver);

private:
    struct Subscription
    {
        QPointer<QObject> receiver;
        std::function<void(const QVariantMap&)> handler;
        QMetaObject::Connection destroyedConnection;
    };

    QHash<QString, QVariantMap> mState;
    QHash<QString, QList<Subscription>> mSubscribers;
};

QT_END_NAMESPACE
