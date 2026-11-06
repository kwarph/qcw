// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwPerspective.h"
#include "QcwDockingTypes.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwPerspectiveCreator.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderSite.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabPaneManager.h"
#include "QcwFastViewToolBar.h"
#include "QcwSerializeUtils.h"
#include "QcwThemeIcons.h"
#include "QcwWindow.h"
#include "QcwPluginManager.h"
#include "QcwCommandHandler.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QApplication>
#include <QCoreApplication>
#include <QPointer>
#include <QToolBar>

QT_BEGIN_NAMESPACE

class QcwPerspectivePrivate
{
public:
    explicit QcwPerspectivePrivate(QcwWindow* w, QcwPerspective* interface);

    void connectFolder(QcwTabFolder* folder);
    void trackFolder(QcwTabFolder* folder);
    void updateFolderToolBar(QcwTabFolder* folder);
    void updateCurrentFromFocus();
    void dropHandlerConnections(const QString& commandId);

public:
    QcwPerspective* owner;
    int id { 0 };
    QList<QMetaObject::Connection> connections;
    QcwWindow* window;
    QString name;
    // True while name() is the name the creator handed out, which is what lets displayName()
    // resolve it again in the current language.
    bool autoName { false };
    QIcon icon;
    QStringList visibleActionSetIds;
    // The container is parented to the window's central widget, so the window may destroy it
    // before this perspective's own destructor runs.
    QPointer<QcwTabFolderContainer> container;
    QcwTabFolder* currentFolder { nullptr };
    QSet<QcwTabFolder*> floatingFolders;
    QSet<QcwTabFolder*> trackedFolders;
    QList<QcwFastViewToolBar*> fastViewToolBars;
    QMap<QString, QcwCommandHandler*> handlers;
    // Both connections registerHandler() makes for one command id, so that unregisterHandler()
    // can take the whole pair out; see the same arrangement in QcwWindowPrivate.
    struct HandlerConnections
    {
        QMetaObject::Connection stateChanged;
        QMetaObject::Connection destroyed;
    };
    QMap<QString, HandlerConnections> handlerConnections;
};

// -----------------------------------------------------------------------------

class QcwDummyPerspective : public QcwPerspective
{
public:
    explicit QcwDummyPerspective(QcwWindow* parent);

    void createInitialLayout() override;
};

// -----------------------------------------------------------------------------

QcwPerspective::QcwPerspective(QcwWindow* parent) :
    QObject { parent }, impl { new QcwPerspectivePrivate { parent, this } }
{
    impl->container = new QcwTabFolderContainer { parent->centralWidget() };

    auto conn = connect(impl->container, &QcwTabFolderContainer::folderMinimized,
                        [this](QcwTabFolder* folder, Qt::ToolBarArea fastViewToolBarArea)
    {
        auto listAllTabs = !impl->container->isCentralFolder(folder);
        auto toolBar = new QcwFastViewToolBar(folder, impl->container, QSize(600, 400), listAllTabs, impl->window);
        impl->window->addToolBar(fastViewToolBarArea, toolBar);
        impl->fastViewToolBars.append(toolBar);

        // A tool bar can outlive this perspective (it is a child of the window), so the
        // connection is bound to this: unbound, it would run on a dead perspective later.
        connect(toolBar, &QObject::destroyed, this, [this](QObject* obj)
        {
            impl->fastViewToolBars.removeOne(static_cast<QcwFastViewToolBar*>(obj));
        });
    });
    impl->connections.append(conn);

    // Docked folders are not listened to directly here: the container owns the
    // choice between docked folders and reports it via currentFolderChanged,
    // which becomes the single activation entry point below.
    conn = connect(impl->container, &QcwTabFolderContainer::folderCreated, [this](QcwTabFolder* folder)
    {
        folder->setProperty("WindowId", impl->window->id());
        impl->trackFolder(folder);
    });
    impl->connections.append(conn);

    // A docked folder can die while it is still this perspective's current one
    // (its last tab detached, the window closed from elsewhere), and the
    // container's currentFolderChanged report is deliberately ignored while the
    // docked area does not hold focus, so track destruction directly - the same
    // way floating folders are tracked in registerFloatingFolder().
    conn = connect(impl->container, &QcwTabFolderContainer::folderDestroyed, [this](QcwTabFolder* folder)
    {
        impl->trackedFolders.remove(folder);
        if (impl->currentFolder == folder)
        {
            impl->currentFolder = nullptr;
            impl->updateCurrentFromFocus();
        }
    });
    impl->connections.append(conn);

    // The container owns the choice between docked folders and reports the
    // active one via currentFolderChanged. Adopt it only while the docked area
    // actually holds focus (or nothing is current yet); otherwise a floating
    // window owns the current view and docked housekeeping (minimize/clear)
    // must not preempt it. Genuine switches to the docked area arrive through
    // focusWindowChanged below.
    conn = connect(impl->container, &QcwTabFolderContainer::currentFolderChanged, [this](QcwTabFolder* folder)
    {
        if (!impl->currentFolder || impl->container->isActiveWindow())
        {
            activateFolder(folder);
        }
    });
    impl->connections.append(conn);

    conn = connect(qApp, &QApplication::focusWindowChanged, [this](QWindow*)
    {
        impl->updateCurrentFromFocus();
    });
    impl->connections.append(conn);
}

QcwPerspective::~QcwPerspective()
{
    for (auto& conn : impl->connections)
    {
        disconnect(conn);
    }
    impl->connections.clear();

    // A handler child of this perspective would otherwise reach the freed impl from its
    // destroyed() callback, exactly as in QcwWindow.
    for (auto it = impl->handlerConnections.begin(); it != impl->handlerConnections.end(); ++it)
    {
        disconnect(it.value().stateChanged);
        disconnect(it.value().destroyed);
    }
    impl->handlerConnections.clear();
    impl->handlers.clear();

    // The container lives in the window's central widget, but it belongs to this perspective:
    // a closed perspective that left it behind would keep following the shared folder into
    // whichever container holds it next, hiding its site or reacting to its last tab. QPointer
    // because a window teardown may have destroyed the central widget - and with it the
    // container - first.
    delete impl->container.data();

    delete impl;
}

QcwPerspective* QcwPerspective::createDummyPerspective(QcwWindow* parent)
{
    auto perspective = new QcwDummyPerspective(parent);
    perspective->setProperty("PerspectiveType", QStringLiteral("DummyPerspective"));
    perspective->setName(tr("Dummy"));
    perspective->setIcon(dummyPerspectiveIcon());

    return perspective;
}

QIcon QcwPerspective::dummyPerspectiveIcon()
{
    return qcw::themeIcon(qcw::Icon::Perspective);
}

void QcwPerspective::createInitialLayout()
{
}

void QcwPerspective::resetLayout()
{
    clear();

    // clear() only schedules the old folders for deletion, and a folder releases its tab panes
    // only when it is destroyed. Until then QcwTabPaneManager still knows their names, so
    // rebuilding the layout in this same turn would have to call every recreated view
    // "Project 1", "Log 1" and so on. A reset is the whole layout, names included: let the
    // pending deletions run first.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    createInitialLayout();

    removeTransferredSingletonTabs();

    // createInitialLayout() installs the window's shared folder into this perspective's
    // container, but the folder is one widget and only the perspective on screen may show it.
    // A reset of a hidden perspective would otherwise take it off the visible one.
    if (auto current = impl->window->currentPerspective(); current && current != this)
    {
        current->show();
    }
}

void QcwPerspective::removeTransferredSingletonTabs()
{
    // The shared folder outlives the reset, so a tab dragged into it survives a layout that
    // does not place it there. For a per-window singleton that tab is a second view of one
    // pane: the rebuilt layout already holds the view, and the shared folder's tab would
    // take the pane back whenever it is clicked. Only now, after createInitialLayout(), is
    // the view safely in the rebuilt folder, so removing these tabs cannot end its life.
    auto shared = window()->sharedFolder();
    if (!shared)
    {
        return;
    }

    for (int i = shared->count() - 1; i >= 0; --i)
    {
        auto view = qobject_cast<QcwView*>(shared->tabPane(i));
        if (!view)
        {
            continue;
        }

        auto creator = pluginManager->getPluginT<QcwViewCreator>(view->type());
        if (!creator || !creator->singleInstancePerWindow())
        {
            continue;
        }

        // Only a view the rebuilt layout places in one of this perspective's folders makes
        // the shared folder's tab a duplicate. A singleton that lives in the shared folder
        // alone is the window's, not this perspective's; removing its only tab would end it.
        QcwTabFolder* holder { nullptr };
        for (auto folder : tabPaneManager->getReferences(view))
        {
            if (folder != shared && ownsFolder(folder))
            {
                holder = folder;
                break;
            }
        }

        if (!holder)
        {
            continue;
        }

        shared->removeTab(i);

        // Removing the tab hid the pane and left it a child of the shared folder, so the tab
        // the rebuilt layout holds for it would show nothing. Re-applying the current index
        // of the folder that shows it now runs showWidget(), which re-parents and shows it.
        const int index = holder->indexOf(view);
        if (index != -1)
        {
            holder->setCurrentIndex(index);
        }
    }
}

void QcwPerspective::saveState(QXmlStreamWriter& writer, QAnyStringView elemName)
{
    writer.writeStartElement(elemName);

    writer.writeAttribute("type", type());
    writer.writeAttribute("id", QString::number(id()));
    writer.writeAttribute("name", name());
    writer.writeAttribute("autoName", QString::number(impl->autoName));
    writer.writeAttribute("extraState", extraStateToHex());
    writer.writeAttribute("visibleActionSets", impl->visibleActionSetIds.join(QLatin1Char(',')));

    impl->container->saveState(writer);

    if (!impl->floatingFolders.empty())
    {
        writer.writeStartElement("floatingFolders");
        writer.writeAttribute("count", QString::number(impl->floatingFolders.size()));

        for (auto folder : std::as_const(impl->floatingFolders))
        {
            writer.writeStartElement("floatingFolder");
            writer.writeAttribute("geometry", folder->saveGeometry().toHex(' '));

            folder->saveState(writer);

            writer.writeEndElement();
        }

        writer.writeEndElement();
    }

    writer.writeEndElement();
}

bool QcwPerspective::restoreState(QXmlStreamReader& reader,
                                  QcwTabFolder* sharedFolder,
                                  const QMap<int, QPair<QWidget*, int>>& tabPanes,
                                  int& minFloatingFolderId)
{
    const auto theId = reader.attributes().value("id").toString().toInt();
    const auto theName = reader.attributes().value("name").toString();
    // saveState() always writes the attribute, so its absence means the document predates it and
    // cannot say how the name was chosen. Assuming an automatic name then is the reading that
    // follows a later language change, and name() keeps the stored literal either way — the
    // perspective is still found under the file it was saved to. What the guess can cost is the
    // button text of a perspective a user did rename by hand, which shows the creator's default
    // from this start on, so a document that knows the answer is trusted over the inference
    // setName() just made against the language installed right now.
    const auto theAutoName =
        reader.attributes().hasAttribute("autoName") ? reader.attributes().value("autoName").toInt() != 0 : true;
    // A document written before this attribute existed cannot say which action sets were
    // visible, and the only safe reading is "all of them", which is what the empty list
    // means. A document that carries the attribute is trusted as written, including an
    // empty value that means "the user chose all".
    const auto visibleActionSets = reader.attributes().value("visibleActionSets").toString();
    const auto ba = reader.attributes().value("extraState").toLocal8Bit();
    const auto state = QcwSerializeUtils::deserialize<QMap<QString, QVariant>>(QByteArray::fromHex(ba));

    // A refused element means the document no longer describes a layout this perspective can
    // hold; the caller is expected to throw the half-filled perspective away.
    bool restored = true;

    while (reader.readNextStartElement())
    {
        if (reader.name() == "folderContainer")
        {
            restored = impl->container->restoreState(reader, sharedFolder, tabPanes) && restored;
        }
        else if (reader.name() == "floatingFolders")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() != "floatingFolder")
                {
                    reader.skipCurrentElement();
                    continue;
                }

                const auto geometry = QByteArray::fromHex(reader.attributes().value("geometry").toLocal8Bit());

                if (reader.readNextStartElement() && reader.name() == "folder")
                {
                    auto folder = new QcwTabFolder;
                    folder->setAttribute(Qt::WA_DeleteOnClose);

                    if (folder->restoreState(reader, tabPanes))
                    {
                        folder->restoreGeometry(geometry);

                        // Stamps the folder with this window's id and starts tracking it.
                        registerFloatingFolder(folder);

                        const int folderId = folder->property("FolderId").toInt();
                        if (folderId < minFloatingFolderId)
                        {
                            minFloatingFolderId = folderId;
                        }

                        // Stays hidden until the window has accepted the whole document and
                        // shows the restored current perspective.
                    }
                    else
                    {
                        // An empty top-level window is worse than a failed restore: drop the
                        // folder and fail the perspective.
                        delete folder;
                        restored = false;
                    }
                }

                reader.skipCurrentElement();
            }
        }
        else
        {
            reader.skipCurrentElement();
        }
    }

    setId(theId);
    setName(theName);
    // setName() judged the name against the creator's default in the language installed right
    // now; the document says what it was when the layout was written, and that is the truth.
    impl->autoName = theAutoName;
    restoreExtraState(state);

    if (!visibleActionSets.isEmpty())
    {
        impl->visibleActionSetIds = visibleActionSets.split(QLatin1Char(','), Qt::SkipEmptyParts);
    }

    // The folders restored above inherited the ids of the document they came from, and the
    // docked ones were stamped by the container while this window was not necessarily the
    // one written for: re-assert this window's id everywhere before anything routes a
    // floating folder by it.
    for (auto folder : impl->container->folders())
    {
        folder->setProperty("WindowId", impl->window->id());
    }

    for (auto folder : impl->floatingFolders)
    {
        folder->setProperty("WindowId", impl->window->id());
    }

    // During restore the window is typically not the active one yet, so the
    // guarded currentFolderChanged emissions may have left the wrong folder as
    // current; re-assert the container's restored selection now.
    activateFolder(impl->container->currentFolder());

    // An explicitly hidden container stays out of the window's show pass. Without this every
    // restored perspective's container would be shown together with the window, and each of
    // their show events would take the one shared folder away from the perspective the
    // document made current.
    hide();

    return restored;
}

QString QcwPerspective::type() const
{
    return property("PerspectiveType").toString();
}

int QcwPerspective::id() const
{
    return impl->id;
}

void QcwPerspective::setId(int id)
{
    impl->id = id;
}

QString QcwPerspective::name() const
{
    return impl->name;
}

void QcwPerspective::setName(const QString& name)
{
    impl->name = name;

    // A name that is not the creator's default any more is one somebody chose; it is also the
    // file name the perspective is saved under, so a language change must leave it alone.
    auto creator = pluginManager->getPluginT<QcwPerspectiveCreator>(type());
    impl->autoName = creator && !name.isEmpty() && name == creator->defaultName();
}

QString QcwPerspective::displayName() const
{
    if (!impl->autoName)
    {
        return impl->name;
    }

    // A document may claim an automatic name whose creator is not loaded any more; the stored
    // name is all there is to show then.
    auto creator = pluginManager->getPluginT<QcwPerspectiveCreator>(type());
    const auto defaultName = creator ? creator->defaultName() : QString {};
    return defaultName.isEmpty() ? impl->name : defaultName;
}

QIcon QcwPerspective::icon() const
{
    return impl->icon;
}

void QcwPerspective::setIcon(const QIcon& icon)
{
    impl->icon = icon;
}

QStringList QcwPerspective::visibleActionSetIds() const
{
    return impl->visibleActionSetIds;
}

void QcwPerspective::setVisibleActionSetIds(const QStringList& ids)
{
    if (impl->visibleActionSetIds == ids)
    {
        return;
    }

    impl->visibleActionSetIds = ids;
    emit visibleActionSetIdsChanged();
}

void QcwPerspective::registerFloatingFolder(QcwTabFolder* floatingFolder)
{
    if (!floatingFolder || impl->floatingFolders.contains(floatingFolder))
    {
        return;
    }

    // Bound to this: a floating folder is a top-level window, so it can easily outlive the
    // perspective that registered it and would otherwise call back into a dead object.
    connect(floatingFolder, &QObject::destroyed, this, [this, floatingFolder](QObject*)
    {
        const bool wasCurrent = (impl->currentFolder == floatingFolder);
        impl->floatingFolders.remove(floatingFolder);
        impl->trackedFolders.remove(floatingFolder);
        if (wasCurrent)
        {
            impl->currentFolder = nullptr;
            impl->updateCurrentFromFocus();
        }
    });

    impl->floatingFolders.insert(floatingFolder);
    impl->connectFolder(floatingFolder);
}

QSet<QcwTabFolder*> QcwPerspective::floatingFolders() const
{
    return impl->floatingFolders;
}

QcwWindow* QcwPerspective::window() const
{
    return impl->window;
}

QcwTabFolderContainer* QcwPerspective::folderContainer() const
{
    return impl->container;
}

void QcwPerspective::activateFolder(QcwTabFolder* folder)
{
    impl->currentFolder = folder;

    // The active highlight is only meaningful for the perspective currently on
    // screen. The shared folder is a single widget reused by every perspective's
    // container, so if a hidden perspective toggled it, it would corrupt the
    // visible perspective's active folder. Restrict visual updates to the
    // window's current perspective; setCurrentPerspective re-applies them on a
    // switch.
    if (impl->window && impl->window->currentPerspective() == this)
    {
        if (folder)
        {
            folder->setActive(true);
        }

        for (auto other : impl->container->folders())
        {
            if (other != folder)
            {
                other->setActive(false);
            }
        }

        for (auto other : std::as_const(impl->floatingFolders))
        {
            if (other != folder)
            {
                other->setActive(false);
            }
        }
    }

    emit currentViewChanged();
}

QcwTabFolder* QcwPerspective::currentFolder() const
{
    return impl->currentFolder;
}

static void collectViewsOfFolder(QcwTabFolder* folder, QSet<QcwView*>& views)
{
    for (int i = 0; i < folder->count(); ++i)
    {
        if (auto widget = folder->tabPane(i))
        {
            if (auto view = qobject_cast<QcwView*>(widget))
            {
                views.insert(view);
            }
        }
    }
}

QcwView* QcwPerspective::currentView() const
{
    if (impl->currentFolder == nullptr)
    {
        return nullptr;
    }

    // nullptr for an empty folder, and for a tab pane that is not a view.
    return qobject_cast<QcwView*>(impl->currentFolder->currentWidget());
}

bool QcwPerspective::ownsFolder(QcwTabFolder* folder) const
{
    if (!folder || folder == impl->window->sharedFolder())
    {
        return false;
    }

    // A minimized folder stays one of the container's folders while its fast view parks it;
    // only floating folders live outside the container.
    return impl->floatingFolders.contains(folder) || (impl->container && impl->container->folders().contains(folder));
}

void QcwPerspective::registerHandler(const QString& commandId, QcwCommandHandler* handler)
{
    if (commandId.isEmpty() || !handler)
    {
        return;
    }

    impl->handlers.insert(commandId, handler);
    impl->dropHandlerConnections(commandId);

    QcwPerspectivePrivate::HandlerConnections connections;
    connections.stateChanged = connect(handler, &QcwCommandHandler::stateChanged, this, [this, commandId]()
    {
        emit handlerStateChanged(commandId);
    });
    connections.destroyed = connect(handler, &QObject::destroyed, this, [this, commandId]()
    {
        impl->dropHandlerConnections(commandId);
        impl->handlers.remove(commandId);
    });
    impl->handlerConnections.insert(commandId, connections);
}

void QcwPerspective::unregisterHandler(const QString& commandId)
{
    if (impl->handlers.remove(commandId) == 0)
    {
        return;
    }

    impl->dropHandlerConnections(commandId);
}

void QcwPerspectivePrivate::dropHandlerConnections(const QString& commandId)
{
    auto it = handlerConnections.find(commandId);
    if (it == handlerConnections.end())
    {
        return;
    }

    const auto connections = it.value();
    handlerConnections.erase(it);

    QObject::disconnect(connections.stateChanged);
    QObject::disconnect(connections.destroyed);
}

QcwCommandHandler* QcwPerspective::handler(const QString& commandId) const
{
    return impl->handlers.value(commandId, nullptr);
}

void QcwPerspective::collectViews(QSet<QcwView*>& views) const
{
    for (auto folder : impl->container->folders())
    {
        collectViewsOfFolder(folder, views);
    }

    for (auto folder : std::as_const(impl->floatingFolders))
    {
        collectViewsOfFolder(folder, views);
    }

    // A minimized folder is parked in a fast view tool bar, outside the container, and goes
    // with it when the perspective is cleared.
    for (auto toolBar : std::as_const(impl->fastViewToolBars))
    {
        if (auto* folder = toolBar->folder())
        {
            collectViewsOfFolder(folder, views);
        }
    }
}

void QcwPerspective::hide()
{
    // A minimized shared folder is parked in this perspective's fast view tool bar, and the
    // folder is one widget: the perspective shown next would take it into its own layout,
    // leaving this tool bar empty and the folder minimized where it cannot be minimized again.
    // Leaving the perspective therefore restores it, exactly as the tool bar's Restore action
    // does, while this container is still the one the folder belongs back in.
    if (auto shared = impl->window->sharedFolder())
    {
        for (auto toolBar : std::as_const(impl->fastViewToolBars))
        {
            if (toolBar->parksFolder(shared) && toolBar->releaseFolder())
            {
                emit shared->restoreRequested();
            }
        }
    }

    for (auto toolBar : std::as_const(impl->fastViewToolBars))
    {
        toolBar->hide();
    }
    for (auto folder : std::as_const(impl->floatingFolders))
    {
        folder->hide();
    }

    impl->container->hide();
}

void QcwPerspective::show()
{
    for (auto toolBar : std::as_const(impl->fastViewToolBars))
    {
        toolBar->show();
    }
    for (auto folder : std::as_const(impl->floatingFolders))
    {
        folder->show();
    }

    // The shared folder is one widget used by every perspective's container, so the
    // perspective shown before this one currently holds it. Re-installing the folder a site
    // already names is how QcwTabFolderSite takes it back.
    if (auto shared = impl->window->sharedFolder())
    {
        for (auto site : impl->container->findChildren<QcwTabFolderSite*>())
        {
            if (site->folder() == shared)
            {
                site->setFolder(shared);
            }
        }
    }

    impl->container->show();
}

void QcwPerspective::clear()
{
    for (auto toolBar : std::as_const(impl->fastViewToolBars))
    {
        // A tool bar destroys a folder still parked in it. The window's shared folder is not
        // this perspective's to destroy, so hand it back first; container->clear() below then
        // detaches it like any other hosted shared folder. Its Minimized state would otherwise
        // outlive the fast view, which the rebuilt layout no longer has.
        if (auto shared = impl->window->sharedFolder();
            shared && toolBar->folder() == shared && toolBar->releaseFolder())
        {
            shared->setState(qcw::FolderState::Normal);
        }

        toolBar->hide();
        toolBar->deleteLater();
    }
    impl->fastViewToolBars.clear();

    for (auto folder : std::as_const(impl->floatingFolders))
    {
        folder->close();
    }

    // close() only schedules the folders for deletion, so their destroyed() handlers - the
    // ones that would drop currentFolder - run later. Clear the pointer now: between this
    // call and that event, currentFolder() must not name a folder that is already gone.
    if (impl->floatingFolders.contains(impl->currentFolder))
    {
        impl->currentFolder = nullptr;
    }

    impl->floatingFolders.clear();

    impl->container->clear();
}

QPair<QcwView*, QIcon>
QcwPerspective::getOrCreateView(const QString& viewType, const QString& viewName, const QList<QVariant>& args)
{
    QPair<QcwView*, QIcon> viewInfo {};

    auto creator = pluginManager->getPluginT<QcwViewCreator>(viewType);
    if (!creator)
    {
        return viewInfo;
    }

    const bool singleInstance = creator->singleInstancePerWindow();
    const auto wantedName = viewName.isEmpty() ? creator->defaultWindowTitle() : viewName;

    QSet<QcwView*> myViews;
    collectViews(myViews);

    for (auto existingView : window()->allViews())
    {
        // A view of this perspective already sits in a tab of it; handing it out again would
        // take it out of that tab, so only the other perspectives are sharing candidates. A
        // singleton has no alternative: returning it is what keeps a second instance out.
        if (!singleInstance && myViews.contains(existingView))
        {
            continue;
        }

        if (existingView->type() != viewType)
        {
            continue;
        }

        if (singleInstance || existingView->windowTitle() == wantedName)
        {
            viewInfo.first = existingView;
            break;
        }
    }

    if (!viewInfo.first)
    {
        // (type, title) is the key a saved perspective restores one view by, so a second view
        // with an already used pair would come back as a single one.
        const auto newName = QcwWindow::makeNewViewName(wantedName);

        if (auto newView = creator->create(nullptr, true, args))
        {
            newView->setWindowTitle(newName);

            // The creator stamped the baseline it asked for; a name the caller chose has no
            // creator default to re-resolve to, so it leaves the view without one.
            if (!viewName.isEmpty())
            {
                newView->setDefaultTitle({});
            }

            viewInfo.first = newView;
        }
    }

    if (viewInfo.first)
    {
        viewInfo.second = creator->defaultViewIcon();
    }

    return viewInfo;
}

// -----------------------------------------------------------------------------

QcwPerspectivePrivate::QcwPerspectivePrivate(QcwWindow* w, QcwPerspective* interface) :
    owner { interface }, window { w }
{
}

void QcwPerspectivePrivate::connectFolder(QcwTabFolder* folder)
{
    folder->setProperty("WindowId", window->id());
    trackFolder(folder);

    auto conn = QObject::connect(folder, &QcwTabFolder::tabBarClicked, [this, folder](int)
    {
        owner->activateFolder(folder);
    });
    connections.append(conn);
}

void QcwPerspectivePrivate::trackFolder(QcwTabFolder* folder)
{
    updateFolderToolBar(folder);

    // The shared folder is hosted again after a reset; tracking it once is enough.
    if (trackedFolders.contains(folder))
    {
        return;
    }

    trackedFolders.insert(folder);

    auto conn = QObject::connect(folder, &QcwTabFolder::currentChanged, owner, [this, folder](int)
    {
        // The toolbar follows the folder's own current widget, so it is also correct
        // for a folder this perspective is not showing right now.
        updateFolderToolBar(folder);

        // Tab switches that do not come through the tab bar (shortcuts, removals) do
        // not reach activateFolder(), but the window still has to re-read the view.
        if (currentFolder == folder)
        {
            emit owner->currentViewChanged();
        }
    });
    connections.append(conn);
}

void QcwPerspectivePrivate::updateFolderToolBar(QcwTabFolder* folder)
{
    if (!folder)
    {
        return;
    }

    auto view = qobject_cast<QcwView*>(folder->currentWidget());

    // Name the view as the tool bar's owner: the same view can be current in two perspectives'
    // folders, so a folder must not treat the other folder it took the widget from as owner.
    folder->setRightWidget(view ? view->topRightToolBar() : nullptr, view);
}

void QcwPerspectivePrivate::updateCurrentFromFocus()
{
    QcwTabFolder* target { nullptr };

    if (container->isActiveWindow())
    {
        target = container->currentFolder();
    }
    else
    {
        for (auto folder : std::as_const(floatingFolders))
        {
            if (folder->isActiveWindow())
            {
                target = folder;
                break;
            }
        }
    }

    if (target)
    {
        owner->activateFolder(target);
    }
}

// -----------------------------------------------------------------------------

QcwDummyPerspective::QcwDummyPerspective(QcwWindow* parent) :
    QcwPerspective { parent }
{
}

void QcwDummyPerspective::createInitialLayout()
{
    auto container = folderContainer();
    QcwTabFolder* centralFolder = nullptr;
    if (window()->sharedFolder())
    {
        container->addSharedFolder(window()->sharedFolder(), nullptr, qcw::LeftDockArea);
        centralFolder = window()->sharedFolder();
    }
    else
    {
        centralFolder = container->createFolder(nullptr, qcw::LeftDockArea);
    }
    container->setCentralFolder(centralFolder);
}

QT_END_NAMESPACE
