// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwView.h"
#include "QcwDockingTypes.h"
#include "QcwTabFolder.h"
#include "QcwTopicService.h"
#include "QcwWindow.h"
#include <QApplication>
#include <QPointer>

QT_BEGIN_NAMESPACE

class QcwViewPrivate
{
public:
    explicit QcwViewPrivate(QcwView* interface);

    /// (Un)subscribes the handlers of the window the view now belongs to.
    void updateTopicWindow();
    void topicWindowTransition();
    void resubscribe(const QString& topicId);
    void forgetTopicWindow();

public:
    QcwView* owner;
    // What the view asked for, kept so a move to another window can subscribe it again.
    QHash<QString, std::function<void(const QVariantMap&)>> handlers;
    // The window the handlers currently follow and its service. Both are set and cleared
    // together: leaving the service cached means a detach can unsubscribe and a read can answer
    // without going through the window — whose private is the first thing its destructor frees.
    QcwWindow* topicWindow { nullptr };
    QcwTopicService* topicService { nullptr };
    // Made with the window the handlers follow and dropped when they move off it, so a handler
    // can never be entered through a window that is gone. ~QcwView disconnects it before the
    // private goes, because the automatic cleanup only comes with ~QObject, which is later.
    QMetaObject::Connection destroyedConnection;
    // The window that announced its own destruction, remembered only to recognize it: the parent
    // chain still names it while the folder destructors release the panes, and re-subscribing to
    // a window that is dying would undo what forgetTopicWindow() just did.
    QPointer<QcwWindow> topicWindowGone;
    // A handler may move the view while a transition is being delivered, and the event that
    // follows would enter this code again. One transition runs at a time: the nested call asks
    // for another one afterwards, when the view's place in the world is settled.
    bool inTransition { false };
    bool againRequested { false };
};

QcwViewPrivate::QcwViewPrivate(QcwView* interface) :
    owner { interface }
{
}

void QcwViewPrivate::updateTopicWindow()
{
    if (inTransition)
    {
        againRequested = true;
        return;
    }

    inTransition = true;
    topicWindowTransition();
    inTransition = false;

    if (againRequested)
    {
        againRequested = false;
        updateTopicWindow();
    }
}

void QcwViewPrivate::topicWindowTransition()
{
    auto* window = owner->owningWindow();

    if (window == topicWindowGone)
    {
        window = nullptr;
    }

    if (window == topicWindow)
    {
        return;
    }

    auto* oldService = topicService;

    // The pair is assigned before anything is delivered, so a handler that subscribes again
    // while it is being told about the detach binds to the window it is moving to — or to none —
    // rather than to the service that is being abandoned. A window whose service cannot be
    // reached is not one the view can follow, so the two stay set and cleared together.
    auto* service = window ? window->topicService() : nullptr;
    topicWindow = service ? window : nullptr;
    topicService = service;

    if (oldService)
    {
        QObject::disconnect(destroyedConnection);

        // A handler may subscribe or unsubscribe while it runs, so the ids are walked from a
        // snapshot; resubscribe() and the erase in unsubscribeWindowTopic() re-read the table.
        const auto ids = handlers.keys();

        for (const auto& id : ids)
        {
            oldService->unsubscribe(id, owner);

            if (const auto handler = handlers.value(id))
            {
                handler({});
            }
        }
    }

    if (topicWindow)
    {
        destroyedConnection = QObject::connect(topicWindow,
                                               &QcwWindow::aboutToBeDestroyed,
                                               owner,
                                               [this]()
                                               {
                                                   forgetTopicWindow();
                                               });

        for (const auto& id : handlers.keys())
        {
            resubscribe(id);
        }
    }
}

void QcwViewPrivate::resubscribe(const QString& topicId)
{
    if (!topicService)
    {
        return;
    }

    const auto handler = handlers.value(topicId);

    if (!handler)
    {
        return;
    }

    // subscribe() replays a cached payload, so the previous subscription has to be gone first:
    // a receiver subscribed twice would be called twice per publication.
    topicService->unsubscribe(topicId, owner);

    auto replayed = std::make_shared<bool>(false);

    topicService->subscribe(topicId,
                            owner,
                            [replayed, handler](const QVariantMap& payload)
                            {
                                *replayed = true;
                                handler(payload);
                            });

    if (!*replayed)
    {
        // Nothing cached: the view is attached to an empty topic, which is the state it has to
        // learn about just as a replayed payload would have told it.
        handler({});
    }
}

void QcwViewPrivate::forgetTopicWindow()
{
    // The window is being destroyed and its service is its child, so the subscriptions go with
    // them; nothing here reads through the window. A later ParentChange finds no window and the
    // view keeps its handlers, ready for whatever window it ends up in.
    topicWindowGone = topicWindow;
    topicWindow = nullptr;
    topicService = nullptr;
}

QcwView::QcwView(QWidget* parent) :
    QWidget { parent },
    impl { new QcwViewPrivate { this } }
{
}

QcwView::~QcwView()
{
    // The window may still live and still emit aboutToBeDestroyed() after this private is gone,
    // so that connection goes first. The topic subscriptions outlive the view only as copies of
    // the handler and are dropped when ~QObject reports the view destroyed.
    QObject::disconnect(impl->destroyedConnection);
    delete impl;
}

QString QcwView::type() const
{
    return property("ViewType").toString();
}

void QcwView::setType(const QString& type)
{
    setProperty("ViewType", type);
}

QString QcwView::defaultTitle() const
{
    return property(qcw::ViewDefaultTitlePropertyName).toString();
}

void QcwView::setDefaultTitle(const QString& title)
{
    setProperty(qcw::ViewDefaultTitlePropertyName, title);
}

int QcwView::id() const
{
    return property(qcw::TabPaneIdPropertyName).toInt();
}

void QcwView::setId(int id)
{
    // Write-once: the tab this view sits in keeps its own copy of the id, so changing it later
    // would leave the two apart. Restores impose their saved id before adding the view anywhere.
    if (!dynamicPropertyNames().contains(qcw::TabPaneIdPropertyName))
    {
        setProperty(qcw::TabPaneIdPropertyName, id);
    }
}

QList<QVariant> QcwView::creatingArguments() const
{
    return property("CreatingArguments").toList();
}

void QcwView::setCreatingArguments(const QList<QVariant>& args)
{
    // Write-once, for the same reason as the id: what is saved has to stay what the view was made
    // with, whoever asks first.
    if (!dynamicPropertyNames().contains("CreatingArguments"))
    {
        setProperty("CreatingArguments", args);
    }
}

QWidget* QcwView::statusBarWidget() const
{
    return nullptr;
}

QToolBar* QcwView::topRightToolBar() const
{
    return nullptr;
}

QcwWindow* QcwView::owningWindow() const
{
    QString folderWindowId;

    for (QWidget* widget = parentWidget(); widget; widget = widget->parentWidget())
    {
        if (auto* window = qobject_cast<QcwWindow*>(widget))
        {
            return window;
        }

        if (folderWindowId.isEmpty())
        {
            if (auto* folder = qobject_cast<QcwTabFolder*>(widget))
            {
                folderWindowId = folder->property("WindowId").toString();
            }
        }
    }

    // A floating folder is top-level, so its id names the window the view belongs to.
    if (!folderWindowId.isEmpty())
    {
        for (auto* widget : QApplication::topLevelWidgets())
        {
            if (auto* window = qobject_cast<QcwWindow*>(widget))
            {
                if (window->id() == folderWindowId)
                {
                    return window;
                }
            }
        }
    }

    return nullptr;
}

void QcwView::canClose(const std::function<void(bool)>& callback)
{
    if (callback)
    {
        callback(true);
    }
}

void QcwView::subscribeWindowTopic(const QString& topicId,
                                   std::function<void(const QVariantMap&)> handler)
{
    if (topicId.isEmpty() || !handler)
    {
        return;
    }

    // Asking for a topic again replaces the handler rather than adding a second callback, so the
    // old subscription goes before the new handler is remembered.
    if (impl->topicService)
    {
        impl->topicService->unsubscribe(topicId, this);
    }

    impl->handlers.insert(topicId, std::move(handler));
    impl->resubscribe(topicId);
}

void QcwView::unsubscribeWindowTopic(const QString& topicId)
{
    impl->handlers.remove(topicId);

    if (impl->topicService)
    {
        impl->topicService->unsubscribe(topicId, this);
    }
}

QVariantMap QcwView::currentWindowTopic(const QString& topicId) const
{
    return impl->topicService ? impl->topicService->current(topicId) : QVariantMap {};
}

void QcwView::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    else if (event->type() == QEvent::ParentChange)
    {
        impl->updateTopicWindow();
    }

    QWidget::changeEvent(event);
}

void QcwView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // A restore reparents the panes before the last link of the chain is made, so the ParentChange
    // it already answered named no window; being shown is the first event that sees the real one.
    impl->updateTopicWindow();
}

void QcwView::retranslateUi()
{
}

QT_END_NAMESPACE
