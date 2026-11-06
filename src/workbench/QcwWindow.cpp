// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwWindow.h"
#include "QcwCommandHandler.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwWorkbench.h"
#include "QcwDockingTypes.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabFolderRenderer.h"
#include "QcwFastViewToolBar.h"
#include "QcwFloatingFolderMonitor.h"
#include "QcwPerspective.h"
#include "QcwPerspectiveCreator.h"
#include "QcwSavedPerspectives.h"
#include "QcwTopicService.h"
#include "QcwSerializeUtils.h"
#include "QcwTabPaneManager.h"
#include "QcwCommandPalette.h"
#include "QcwActionSetRegistry.h"
#include "QcwCommandRegistry.h"
#include "QcwContributionRegistry.h"
#include "QcwContributionSorter.h"
#include "QcwWindowActionIds.h"
#include "QcwBuiltinCommands.h"
#include "QcwPathUtils.h"
#include "QcwThemeIcons.h"
#include "QcwFixedRowHeightItemDelegate.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QPointer>
#include <QCloseEvent>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QApplication>
#include <QDesktopServices>
#include <QActionGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListView>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QKeySequence>
#include <QHash>
#include <QQueue>
#include <QSet>
#include <QStyle>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

QT_BEGIN_NAMESPACE

namespace
{
constexpr int kWindowStateVersion = 1;

/// The ordinal QcwWindow::makeNewViewName() appended to @p base to make @p text.
int autoTitleOrdinal(const QString& text, const QString& base)
{
    if (text == base)
    {
        return 0;
    }

    if (!text.startsWith(base + QLatin1Char(' ')))
    {
        return -1;
    }

    bool ok = false;
    const int ordinal = QStringView(text).mid(base.size() + 1).toInt(&ok);
    return ok && ordinal > 0 ? ordinal : -1;
}

} // namespace

struct QcwPerspectiveInfo
{
    QString type;
    QString name;
    bool hasSavedState { false };
};

static QMap<QString, QcwPerspectiveInfo> availablePerspectiveInfos();
static QString perspectiveNameError(const QString& name);

// -----------------------------------------------------------------------------

class QcwPerspectiveListView : public QListView
{
public:
    explicit QcwPerspectiveListView(QWidget* parent = nullptr);
    void initialize();

    QcwPerspectiveInfo currentInfo() const;
};

// -----------------------------------------------------------------------------

class QcwOpenPerspectiveDialog : public QDialog
{
public:
    explicit QcwOpenPerspectiveDialog(QWidget* parent = nullptr);

    QcwPerspectiveInfo selectedPerspective() const
    {
        return info;
    }

private:
    QcwPerspectiveInfo info;
};

// -----------------------------------------------------------------------------

class QcwSavePerspectiveDialog : public QDialog
{
public:
    explicit QcwSavePerspectiveDialog(const QString& perspectiveName, QWidget* parent = nullptr);

    QString newPerspectiveName() const
    {
        return lineEdit->text();
    }

    QPushButton* saveButton() const
    {
        return buttonBox->button(QDialogButtonBox::Save);
    }

private:
    QLineEdit* lineEdit { nullptr };
    QcwPerspectiveListView* listView { nullptr };
    QDialogButtonBox* buttonBox { nullptr };
};

// -----------------------------------------------------------------------------

/// The views of @p folder, in tab order; panes that are not QcwViews are left out, since the
/// close protocol is a QcwView contract.
QList<QcwView*> viewsInFolder(QcwTabFolder* folder)
{
    QList<QcwView*> views;

    if (!folder)
    {
        return views;
    }

    for (int i = 0; i < folder->count(); ++i)
    {
        if (auto* view = qobject_cast<QcwView*>(folder->tabPane(i)))
        {
            views.append(view);
        }
    }

    return views;
}

// -----------------------------------------------------------------------------

/**
 * Asks views for their consent one at a time, in request order.
 *
 * A request holds the views of one close (a tab batch, a perspective, a window, a folder) and
 * a callback for its outcome. QcwView::canClose() may answer synchronously or later; while an
 * answer is outstanding the pump waits, and a view destroyed meanwhile no longer blocks. A
 * refusal ends its own request only — independent closes keep their place in the queue.
 *
 * The object is owned by QcwWindowPrivate. Callbacks handed to views check @c lifetime so one
 * that fires after the window is gone does nothing instead of reaching freed memory.
 */
class QcwViewCloseCoordinator : public QObject
{
public:
    using Callback = std::function<void(bool)>;

    ~QcwViewCloseCoordinator() override
    {
        lifetime.reset();
    }

    /// @return true when @p callback was called before this returned, i.e. every view
    ///         answered without deferring; false when at least one answer is outstanding.
    bool canClose(const QList<QcwView*>& views, Callback callback);

private:
    struct Answer
    {
        bool reported { false };
        bool allowed { false };
    };

    struct Request
    {
        QList<QPointer<QcwView>> views;
        Callback callback;
    };

    void pump();
    void onAnswer(const std::shared_ptr<Answer>& answer, bool allowed);
    void finishRequest(bool allowed);

    std::shared_ptr<int> lifetime { std::make_shared<int>(0) };
    QQueue<Request> requests;
    std::unique_ptr<Request> current;
    std::shared_ptr<Answer> awaiting;
    bool pumping { false };
};

bool QcwViewCloseCoordinator::canClose(const QList<QcwView*>& views, Callback callback)
{
    if (!callback)
    {
        return true;
    }

    auto answered = std::make_shared<bool>(false);

    Request request;
    request.views.reserve(views.size());
    for (auto* view : views)
    {
        request.views.append(QPointer<QcwView> { view });
    }
    request.callback = [answered, callback = std::move(callback)](bool allowed)
    {
        *answered = true;
        callback(allowed);
    };

    requests.enqueue(std::move(request));
    pump();
    return *answered;
}

void QcwViewCloseCoordinator::pump()
{
    if (pumping)
    {
        return;
    }

    pumping = true;

    while (true)
    {
        if (!current)
        {
            if (requests.isEmpty())
            {
                break;
            }

            current = std::make_unique<Request>(requests.dequeue());
        }

        while (!current->views.isEmpty() && current->views.first().isNull())
        {
            current->views.removeFirst();
        }

        if (current->views.isEmpty())
        {
            QPointer<QcwViewCloseCoordinator> self { this };
            finishRequest(true);
            if (!self)
            {
                return;
            }
            continue;
        }

        auto view = current->views.takeFirst();
        auto answer = std::make_shared<Answer>();
        awaiting = answer;
        std::weak_ptr<int> guard = lifetime;
        QPointer<QcwViewCloseCoordinator> self { this };

        // A view destroyed while its answer is outstanding cannot keep anything open.
        QObject::connect(view, &QObject::destroyed, this, [this, answer, guard, self]()
        {
            if (guard.expired() || !self)
            {
                return;
            }

            onAnswer(answer, true);
        });

        view->canClose([this, answer, guard, self](bool allowed)
        {
            if (guard.expired() || !self)
            {
                return;
            }

            onAnswer(answer, allowed);
        });

        if (!answer->reported)
        {
            // The view answers later; onAnswer() restarts the pump then.
            pumping = false;
            return;
        }

        awaiting.reset();

        if (!answer->allowed)
        {
            QPointer<QcwViewCloseCoordinator> selfAfterRefusal { this };
            finishRequest(false);
            if (!selfAfterRefusal)
            {
                return;
            }
        }
    }

    pumping = false;
}

void QcwViewCloseCoordinator::onAnswer(const std::shared_ptr<Answer>& answer, bool allowed)
{
    if (!answer || answer->reported)
    {
        return;
    }

    answer->reported = true;
    answer->allowed = allowed;

    if (pumping)
    {
        // The pump is waiting right behind canClose() and reads the answer itself.
        return;
    }

    if (answer != awaiting)
    {
        return;
    }

    awaiting.reset();

    QPointer<QcwViewCloseCoordinator> self { this };

    if (!allowed)
    {
        finishRequest(false);
        if (!self)
        {
            return;
        }
    }

    pump();
}

void QcwViewCloseCoordinator::finishRequest(bool allowed)
{
    auto callback = std::move(current->callback);
    current.reset();

    // The callback may start another request or delete the coordinator; callers hold a
    // QPointer if they touch this object afterwards.
    callback(allowed);
}

// -----------------------------------------------------------------------------

class QcwWindowPrivate
{
public:
    explicit QcwWindowPrivate(QcwWindow* interface);
    ~QcwWindowPrivate();

    void createWidgets();
    void prepareActions();
    void createPerspectiveToolBar();
    void addPerspective(QcwPerspective* perspective);
    void addPerspectiveButton(QcwPerspective* perspective);
    void removePerspectiveButton(int perspectiveId);
    void connectPerspective(QcwPerspective* perspective);

    void setTopToolBarVisible(bool visible);
    void closeFloatingFolders();
    QString stateFilePath() const;

    QcwPerspective* createPerspective(const QString& type);

    QcwPerspective* restorePerspectiveState(QXmlStreamReader& reader,
                                            QcwTabFolder* sharedFolder,
                                            const QMap<int, QPair<QWidget*, int>>& tabPanes,
                                            int& minFloatingFolderId);

    void openPerspectiveFromInfo(const QcwPerspectiveInfo& info);

    QSet<QcwView*> collectViews() const;
    QList<QcwView*> windowViews() const;
    void removePerspective(int perspectiveId);
    void installFolderCloseHandler(QcwTabFolder* folder);
    void requestTabClose(QcwTabFolder* folder, int index);
    void flushTabCloses();

    void handleCurrentViewChanged(QcwPerspective* perspective);
    void installStatusWidget(QWidget* widget);
    void releaseStatusWidget();
    void retranslateViewTitles();
    void retranslateUi();

    void refreshCommandAction(const QString& commandId);
    void refreshAllCommandActions();
    void dropHandlerConnections(const QString& commandId);
    void connectViewHandlers(QcwView* view);
    void disconnectViewHandlers();

    /// One entry a contribution placed, and whether a separator belongs in front of it.
    struct PlacedEntry
    {
        QAction* action;
        bool separatorBefore;
    };

    void rebuildTargets();
    void buildMenus();
    void buildMenuContent(const QString& menuId, QMenu* menu);
    void buildToolBars();
    void buildToolBarContent(const QString& target, QToolBar* toolBar);
    QToolBar* createPlacedToolBar(const QString& target);
    QList<PlacedEntry> placedEntries(const QString& target);
    QStringList currentVisibleActionSets() const;
    /// True the first time @p actionSetId is reported, so one bad id cannot print a line per rebuild.
    bool firstWarningAbout(const QString& actionSetId);
    void registerBuiltinWindowHandlers();
    void createShortcutCommandActions();
    void pruneCommandActions();
    void syncCommandActionLabels();
    void scheduleRebuild();

public:
    QcwWindow* owner;
    QString windowId;
    QVBoxLayout* mainLayout;
    QMap<QString, QMenu*> menus;
    QMap<QString, QToolBar*> toolBars; // contribution target -> the tool bar built for it
    QPointer<QcwCommandPalette> commandPalette;
    QToolBar* perspectiveToolBar;
    QActionGroup* perspectiveActionGroup;
    QHBoxLayout* statusBarPlaceholderLayout;
    QWidget* statusBarSpacer;
    QPointer<QWidget> statusWidget;      // Installed in statusBarPlaceholderLayout.
    QPointer<QWidget> statusWidgetOwner; // Parent statusWidget had when it was installed.
    QMetaObject::Connection statusWidgetOwnerConnection;
    QcwPerspective* dummyPerspective { nullptr };
    QcwPerspective* currentPerspective { nullptr };
    QcwTopicService* topicService { nullptr };
    QMap<int, QcwPerspective*> perspectives;
    QPointer<QcwTabFolder> sharedFolder;
    int lastFloatingFolderId { -1 }; // First floating folder is -1, the second -2, and so on.
    int lastPerspectiveId { 0 };
    bool uiInitialized { false };
    bool restoredFromState { false };
    bool topToolBarsVisible { true };
    bool saveStateOnClose { true };

    QcwViewCloseCoordinator* closeCoordinator { nullptr };
    // Views an enclosing close (window or perspective) has already asked; the folder close
    // handlers consume them instead of asking a second time.
    QSet<QcwView*> confirmedViews;
    QList<QPair<QPointer<QcwTabFolder>, QPointer<QWidget>>> pendingTabCloses;
    QList<QPointer<QcwTabFolder>> pendingFolderCloses;
    bool tabCloseFlushScheduled { false };
    bool closeConfirmationPending { false };
    bool closeApproved { false };
    bool skipViewPrompts { false };
    QMap<QString, QcwCommandHandler*> handlers;
    // Both connections registerHandler() makes for one command id. Keeping the destroyed() one
    // here as well is what lets unregisterHandler() take the whole pair out: leaving it behind
    // meant a handler that died later — in particular one parented to this window, which
    // ~QObject destroys after the destructor body has already freed impl — called into freed
    // memory.
    struct HandlerConnections
    {
        QMetaObject::Connection stateChanged;
        QMetaObject::Connection destroyed;
    };
    QMap<QString, HandlerConnections> handlerConnections;
    QList<QMetaObject::Connection> viewHandlerConnections;
    QMap<QString, QAction*> commandQactions;
    bool rebuildScheduled { false };
    // The action set ids this window has already reported as unregistered, so one typo is one
    // warning rather than one per rebuild. See QcwContributionSorter's note on purity for why the
    // registry side of the visibility rules is checked by the window instead of the sorter.
    QSet<QString> warnedActionSets;
    QcwWorkbench* workbench { nullptr };
    QMetaObject::Connection globalHandlerConnection;
};

// -----------------------------------------------------------------------------

QcwWindow::QcwWindow(QWidget* parent) :
    QMainWindow { parent }, impl { new QcwWindowPrivate { this } }
{
    impl->createWidgets();
    impl->prepareActions();
    impl->createPerspectiveToolBar();

    // The monitor is a process-wide singleton, so the connection has to be bound to this
    // window: unbound, it would keep calling into a destroyed QcwWindow.
    connect(floatingFolderMonitor, &QcwFloatingFolderMonitor::floatingFolderCreated, this, [this](QcwTabFolder* folder)
    {
        if (!folder || folder->property("WindowId").toString() != id())
        {
            return;
        }

        impl->installFolderCloseHandler(folder);

        if (impl->currentPerspective)
        {
            folder->setProperty("FolderId", impl->lastFloatingFolderId--);
            impl->currentPerspective->registerFloatingFolder(folder);
        }
    });
}

QcwWindow::~QcwWindow()
{
    // Views drop their cached pointers to this window while the accessors still work: the
    // widget teardown below deletes the folders, and the ParentChange their destructors send
    // while releasing the panes reaches those views.
    emit aboutToBeDestroyed();

    // Hand an installed status widget back before the children go: its owner may outlive
    // the window.
    impl->releaseStatusWidget();

    // The built-in handlers are children of this window. QWidget::~QWidget() destroys them
    // after this body runs, and each destruction emits destroyed(), whose callback reaches
    // into impl - which is gone by then. Disconnecting every connection from a handler back
    // to this window is what stops the callback from firing.
    for (auto it = impl->handlerConnections.begin(); it != impl->handlerConnections.end(); ++it)
    {
        disconnect(it.value().stateChanged);
        disconnect(it.value().destroyed);
    }
    impl->handlerConnections.clear();
    impl->handlers.clear();

    for (const auto& conn : impl->viewHandlerConnections)
    {
        disconnect(conn);
    }
    impl->viewHandlerConnections.clear();

    if (impl->globalHandlerConnection)
    {
        disconnect(impl->globalHandlerConnection);
        impl->globalHandlerConnection = {};
    }

    // Floating folders are top-level widgets with no parent among the window's children, so
    // deleting the window would leave them - and their views - running with no window behind
    // them. Close them without asking: the window is already going away, so no answer could
    // keep a folder open that nothing can show any more.
    impl->skipViewPrompts = true;
    impl->closeFloatingFolders();
    impl->skipViewPrompts = false;

    // The container detaches the shared folder instead of deleting it, so by the time the
    // window goes it can be parentless and would otherwise survive to QApplication's own
    // teardown. The window owns it, so it is deleted here, while the application is still up;
    // the containers hold it through QPointers and go null with it.
    delete impl->sharedFolder.data();

    delete impl;
    // The widget teardown that follows frees the folders, and their destructors send events into
    // the views they release. A view that still names this window can therefore reach an accessor
    // here; the few that answer from below report an empty value instead of reading freed memory.
    impl = nullptr;
}

void QcwWindow::setupUi()
{
    if (impl->uiInitialized)
    {
        return;
    }

    impl->sharedFolder = createSharedFolder();
    if (impl->sharedFolder)
    {
        impl->sharedFolder->setProperty("FolderId", 0);
        impl->sharedFolder->setProperty("WindowId", id());
    }

    impl->dummyPerspective = QcwPerspective::createDummyPerspective(this);
    impl->dummyPerspective->setId(++impl->lastPerspectiveId);
    impl->dummyPerspective->createInitialLayout();

    impl->mainLayout->addWidget(impl->dummyPerspective->folderContainer());

    impl->currentPerspective = impl->dummyPerspective;
    impl->connectPerspective(impl->dummyPerspective);

    // The first target build, deferred from the constructor so the subclass that owns this window
    // has registered its own menus and tool bars by now; see QcwWindowPrivate::prepareActions().
    impl->rebuildTargets();

    postSetupUi();

    impl->dummyPerspective->activateFolder(impl->dummyPerspective->currentFolder());

    impl->uiInitialized = true;
}

QString QcwWindow::id() const
{
    return impl ? impl->windowId : QString {};
}

void QcwWindow::setId(const QString& id)
{
    impl->windowId = id;
}

void QcwWindow::openPerspective(QcwPerspective* perspective)
{
    // Before setupUi() there is no dummy perspective, and installing a real one would put a
    // second container into the central layout once setupUi() adds its own.
    if (!impl->dummyPerspective || !perspective || perspective == impl->dummyPerspective)
    {
        return;
    }

    // A perspective straight from its creator still carries the default id 0. Without an
    // id of its own it would look "already registered" as soon as the first such perspective
    // is open, and every later one would silently re-show that first perspective. An explicit
    // id has to move the counter forward as well, so the next automatic one cannot collide.
    if (perspective->id() == 0)
    {
        perspective->setId(++impl->lastPerspectiveId);
    }
    else if (perspective->id() > impl->lastPerspectiveId)
    {
        impl->lastPerspectiveId = perspective->id();
    }

    const int perspectiveId = perspective->id();
    if (impl->perspectives.contains(perspectiveId))
    {
        setCurrentPerspective(perspectiveId);
        return;
    }

    impl->addPerspective(perspective);
    setCurrentPerspective(perspectiveId);
}

void QcwWindow::closePerspective(int perspectiveId)
{
    auto it = impl->perspectives.find(perspectiveId);
    if (it == impl->perspectives.end() || it.value() == impl->dummyPerspective)
    {
        return;
    }

    auto perspective = it.value();

    QSet<QcwView*> viewSet;
    perspective->collectViews(viewSet);

    QList<QcwView*> views;
    QList<QPointer<QcwView>> perspectiveViews;
    views.reserve(viewSet.size());
    perspectiveViews.reserve(viewSet.size());
    for (auto view : viewSet)
    {
        perspectiveViews.append(view);

        // Only a view whose every tab belongs to a folder this close disposes of is actually
        // closed here. A view another perspective still shows, and one the shared folder holds,
        // stays open with its work; asking it would let a refusal block a close that does not
        // end it.
        bool closesWithPerspective = true;
        for (auto folder : tabPaneManager->getReferences(view))
        {
            if (!perspective->ownsFolder(folder))
            {
                closesWithPerspective = false;
                break;
            }
        }

        if (closesWithPerspective)
        {
            views.append(view);
        }
    }

    QPointer<QcwPerspective> perspectiveGuard { perspective };
    impl->closeCoordinator->canClose(views, [this, perspectiveId, perspectiveGuard, perspectiveViews](bool allowed)
    {
        if (!allowed || !perspectiveGuard)
        {
            return;
        }

        // The approved views let the floating folders go without asking again, and so do the
        // ones this close left open elsewhere: their folder here releases a view that another
        // folder keeps, which is not a close the view has to answer.
        for (auto& view : perspectiveViews)
        {
            if (view)
            {
                impl->confirmedViews.insert(view);
            }
        }

        impl->removePerspective(perspectiveId);
        impl->confirmedViews.clear();
    });
}

void QcwWindow::setCurrentPerspective(int perspectiveId)
{
    auto perspective = impl->perspectives.value(perspectiveId);
    if (!perspective)
    {
        perspective = impl->dummyPerspective;
    }

    if (!perspective)
    {
        return;
    }

    const int resolvedId = perspective->id();
    if (perspective == impl->currentPerspective)
    {
        // Re-activating the perspective already current. Its visible action sets are not
        // compared here because a change to them reaches the window through the perspective's
        // own visibleActionSetIdsChanged(), which is connected for every perspective.
        perspective->activateFolder(perspective->currentFolder());
        return;
    }

    const QStringList oldSets =
        impl->currentPerspective ? impl->currentPerspective->visibleActionSetIds() : QStringList {};

    auto folderContainer = perspective->folderContainer();
    if (impl->currentPerspective)
    {
        impl->currentPerspective->hide();
        if (auto item = impl->mainLayout->replaceWidget(impl->currentPerspective->folderContainer(), folderContainer))
        {
            delete item;
        }
    }
    else
    {
        impl->mainLayout->addWidget(folderContainer);
    }

    for (auto action : impl->perspectiveActionGroup->actions())
    {
        action->setChecked(action->data().toInt() == resolvedId);
    }

    perspective->show();
    impl->currentPerspective = perspective;

    perspective->activateFolder(perspective->currentFolder());

    const QStringList newSets = perspective->visibleActionSetIds();
    if (oldSets != newSets)
    {
        impl->rebuildTargets();
    }
}

QcwPerspective* QcwWindow::currentPerspective() const
{
    return impl->currentPerspective;
}

QcwView* QcwWindow::currentView() const
{
    if (!impl)
    {
        return nullptr;
    }

    return impl->currentPerspective ? impl->currentPerspective->currentView() : nullptr;
}

QcwTopicService* QcwWindow::topicService() const
{
    return impl ? impl->topicService : nullptr;
}

QList<QcwPerspective*> QcwWindow::allPerspectives() const
{
    auto perspectives = impl->perspectives.values();
    if (impl->dummyPerspective)
    {
        perspectives.append(impl->dummyPerspective);
    }

    return perspectives;
}

QcwTabFolder* QcwWindow::sharedFolder() const
{
    return impl->sharedFolder;
}

QSet<QcwView*> QcwWindow::allViews() const
{
    return impl->collectViews();
}

QString QcwWindow::makeNewViewName(const QString& defaultViewName)
{
    QSet<QString> usedViewNames;
    for (auto widget : tabPaneManager->allTabPanes().keys())
    {
        usedViewNames.insert(widget->windowTitle());
    }

    int i = 1;
    auto viewName = defaultViewName;
    while (usedViewNames.contains(viewName))
    {
        viewName = QString("%1 %2").arg(defaultViewName).arg(i++);
    }

    return viewName;
}

QcwTabFolder* QcwWindow::createDetachedView(const QString& type, const QString& title, const QList<QVariant>& args)
{
    if (!impl->currentPerspective)
    {
        return nullptr;
    }

    auto viewCreator = pluginManager->getPluginT<QcwViewCreator>(type);
    if (!viewCreator)
    {
        return nullptr;
    }

    // A per-window singleton that is already laid out cannot be moved into a floating folder,
    // and making a second one is what the creator forbids.
    if (viewCreator->singleInstancePerWindow())
    {
        for (auto view : impl->collectViews())
        {
            if (view->type() == type)
            {
                return nullptr;
            }
        }
    }

    auto view = viewCreator->create(nullptr, true, args);
    if (!view)
    {
        return nullptr;
    }

    auto folder = new QcwTabFolder;
    folder->setAttribute(Qt::WA_DeleteOnClose);
    folder->setFloating(true);
    folder->setProperty("FolderId", impl->lastFloatingFolderId--);
    folder->setProperty("WindowId", id());
    folder->setWindowTitle(title);

    // The folder is created here, not detached by a drag, so the monitor never announces it
    // and nothing else would route its close button - or its tabs' - through the protocol.
    impl->installFolderCloseHandler(folder);

    // A floating folder belongs to no perspective's container, so it is not among the folders
    // QcwWindow::folderAdded() announces and QcwWindowStyler never styles it. Adopting the
    // renderer of a folder that already wears the style is what keeps the two looking alike.
    QcwTabFolder* styled = impl->currentPerspective->currentFolder();
    if (!styled)
    {
        styled = impl->sharedFolder;
    }
    if (!styled)
    {
        const auto folders = impl->currentPerspective->folderContainer()->folders();
        styled = folders.isEmpty() ? nullptr : *folders.cbegin();
    }

    if (styled)
    {
        folder->setRenderer(styled->renderer()->clone(folder));
    }

    folder->addTab(view, title, viewCreator->defaultViewIcon());

    // The caller named the tab, normally out of this window's New View menu, which resolves
    // defaultWindowTitle(); recording that default is what lets a language change tell an
    // untouched tab from one the user renamed. A caller-supplied title that is not the default
    // simply never matches it, so the tab is left alone like any rename.
    view->setDefaultTitle(viewCreator->defaultWindowTitle());

    impl->currentPerspective->registerFloatingFolder(folder);

    return folder;
}

QMenu* QcwWindow::getMenu(const QString& menuId) const
{
    return impl->menus.value(menuId);
}

QToolBar* QcwWindow::getToolBar(const QString& target) const
{
    return impl->toolBars.value(target);
}

QAction* QcwWindow::resolveCommand(const QString& commandId)
{
    if (commandId.isEmpty() || !commandRegistry->contains(commandId))
    {
        return nullptr;
    }

    return commandAction(commandId);
}

void QcwWindow::registerHandler(const QString& commandId, QcwCommandHandler* handler)
{
    if (commandId.isEmpty() || !handler)
    {
        return;
    }

    impl->handlers.insert(commandId, handler);
    impl->dropHandlerConnections(commandId);

    QcwWindowPrivate::HandlerConnections connections;
    connections.stateChanged = connect(handler, &QcwCommandHandler::stateChanged, this, [this, commandId]()
    {
        impl->refreshCommandAction(commandId);
    });
    connections.destroyed = connect(handler, &QObject::destroyed, this, [this, commandId]()
    {
        impl->dropHandlerConnections(commandId);
        impl->handlers.remove(commandId);
        impl->refreshCommandAction(commandId);
    });
    impl->handlerConnections.insert(commandId, connections);

    impl->refreshCommandAction(commandId);
}

void QcwWindow::unregisterHandler(const QString& commandId)
{
    if (impl->handlers.remove(commandId) == 0)
    {
        return;
    }

    impl->dropHandlerConnections(commandId);
    impl->refreshCommandAction(commandId);
}

QcwCommandHandler* QcwWindow::resolveHandler(const QString& commandId) const
{
    if (commandId.isEmpty())
    {
        return nullptr;
    }

    if (auto* view = currentView())
    {
        if (auto* handler = view->commandHandlers().value(commandId, nullptr))
        {
            return handler;
        }
    }

    if (auto* perspective = currentPerspective())
    {
        if (auto* handler = perspective->handler(commandId))
        {
            return handler;
        }
    }

    if (auto* handler = impl->handlers.value(commandId, nullptr))
    {
        return handler;
    }

    if (impl->workbench)
    {
        return impl->workbench->globalHandler(commandId);
    }

    return nullptr;
}

bool QcwWindow::executeCommand(const QString& commandId, const QVariantMap& params)
{
    auto* handler = resolveHandler(commandId);
    if (!handler || !handler->isEnabled() || !handler->accepts(params))
    {
        return false;
    }

    const auto info = commandRegistry->command(commandId);
    if (info && info->checkable)
    {
        if (auto* action = impl->commandQactions.value(commandId, nullptr))
        {
            handler->setChecked(action->isChecked());
        }
    }

    handler->execute(params);
    return true;
}

bool QcwWindow::isCommandEnabled(const QString& commandId) const
{
    auto* handler = resolveHandler(commandId);
    return handler != nullptr && handler->isEnabled();
}

QAction* QcwWindow::commandAction(const QString& commandId)
{
    if (commandId.isEmpty())
    {
        return nullptr;
    }

    if (auto it = impl->commandQactions.find(commandId); it != impl->commandQactions.end())
    {
        return it.value();
    }

    const auto info = commandRegistry->command(commandId);
    if (!info)
    {
        return nullptr;
    }

    auto* action = new QAction(this);
    action->setText(info->label);
    action->setIcon(info->icon);
    action->setCheckable(info->checkable);
    action->setShortcutContext(Qt::WindowShortcut);

    if (info->defaultShortcut != QKeySequence())
    {
        action->setShortcut(info->defaultShortcut);
        addAction(action);
    }

    connect(action, &QAction::triggered, this, [this, commandId]()
    {
        // A command this window's own UI runs addresses this window: the id travels along so a
        // process-wide handler can tell which window the user meant. Handlers that need no
        // target ignore the extra key.
        executeCommand(commandId, { { QString(QcwWindowActionIds::ParamWindowId), id() } });
    });

    impl->commandQactions.insert(commandId, action);
    impl->refreshCommandAction(commandId);
    return action;
}

void QcwWindow::setWorkbench(QcwWorkbench* workbench)
{
    if (impl->globalHandlerConnection)
    {
        disconnect(impl->globalHandlerConnection);
        impl->globalHandlerConnection = {};
    }

    impl->workbench = workbench;

    if (workbench)
    {
        impl->globalHandlerConnection =
            connect(workbench, &QcwWorkbench::globalHandlerStateChanged, this, [this](const QString& commandId)
        {
            impl->refreshCommandAction(commandId);
        });
    }

    // The workbench attaches after setupUi(), so the actions built there saw no global handler and
    // came out greyed out. The handlers exist already; only their observer was missing.
    impl->refreshAllCommandActions();
}

QcwWorkbench* QcwWindow::workbench() const
{
    return impl->workbench;
}

void QcwWindow::openCommandPalette()
{
    if (impl->commandPalette)
    {
        impl->commandPalette->raise();
        impl->commandPalette->activateWindow();
        return;
    }

    auto* palette = new QcwCommandPalette { this };
    palette->resize(640, 480);
    impl->commandPalette = palette;
    palette->show();
}

bool QcwWindow::saveStateOnClose() const
{
    return impl->saveStateOnClose;
}

void QcwWindow::setSaveStateOnClose(bool save)
{
    impl->saveStateOnClose = save;
}

bool QcwWindow::saveStateToFile(const QString& stateFile)
{
    if (stateFile.isEmpty() || id().isEmpty())
    {
        return false;
    }

    auto perspective = impl->currentPerspective ? impl->currentPerspective : impl->dummyPerspective;
    if (!perspective || !impl->dummyPerspective)
    {
        return false;
    }

    QByteArray xmlData;
    QXmlStreamWriter writer(&xmlData);
    writer.writeDTD(R"(<?xml version="1.0" encoding="UTF-8"?>)");

    writer.writeStartElement("window"); // --> window begin
    writer.writeAttribute("version", QString::number(kWindowStateVersion));
    writer.writeAttribute("id", id());
    writer.writeAttribute("currentPerspective", QString::number(perspective->id()));
    writer.writeAttribute("geometry", saveGeometry().toHex(' '));
    writer.writeAttribute("extraState", extraStateToHex());

    const auto views = impl->collectViews();
    if (!views.empty())
    {
        // --> views begin
        writer.writeStartElement("views");
        writer.writeAttribute("count", QString::number(views.size()));

        for (auto view : views)
        {
            writer.writeStartElement("view"); // --> view begin

            writer.writeAttribute("type", view->type());
            writer.writeAttribute("id", QString::number(view->id()));

            auto ba = QcwSerializeUtils::serialize(view->creatingArguments());
            writer.writeAttribute("creatingArguments", ba.toHex(' '));

            writer.writeAttribute("extraState", view->extraStateToHex());

            writer.writeEndElement(); // --> view end
        }
        writer.writeEndElement();
        // --> views end
    }

    // sharedFolder
    if (impl->sharedFolder)
    {
        writer.writeStartElement("sharedFolder");
        impl->sharedFolder->saveState(writer);
        writer.writeEndElement();
    }

    // dummyPerspective
    impl->dummyPerspective->saveState(writer, "dummyPerspective");

    // perspectives
    if (!impl->perspectives.empty())
    {
        writer.writeStartElement("perspectives");
        writer.writeAttribute("count", QString::number(impl->perspectives.size()));

        for (auto perspective : std::as_const(impl->perspectives))
        {
            perspective->saveState(writer, "perspective");
        }

        writer.writeEndElement();
    }

    // save more state data
    saveMoreState(writer);

    writer.writeEndElement(); // --> window end
    writer.writeEndDocument();

    QFile file(stateFile);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "QcwWindow: cannot write state file" << stateFile << ":" << file.errorString();
        return false;
    }

    if (file.write(xmlData) != xmlData.size())
    {
        qWarning() << "QcwWindow: incomplete state file write" << stateFile << ":" << file.errorString();
        return false;
    }

    return true;
}

bool QcwWindow::restoreStateFromFile(const QString& stateFile)
{
    if (impl->uiInitialized)
    {
        qWarning() << "QcwWindow::restoreStateFromFile: the window is already initialized";
        return false;
    }

    // Parsing stamps the folders and new perspectives with the document's window id and
    // advances the perspective counter, so a failed attempt has to put both back: the caller
    // may fall back to setupUi() and would otherwise inherit the identity of a broken file.
    const auto previousWindowId = impl->windowId;
    const int previousLastPerspectiveId = impl->lastPerspectiveId;

    QFile file(stateFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "QcwWindow::restoreStateFromFile: cannot open" << stateFile << ":" << file.errorString();
        return false;
    }

    QXmlStreamReader reader(&file);

    reader.readNextStartElement();
    if (reader.name() != "window")
    {
        qWarning() << "QcwWindow::restoreStateFromFile:" << stateFile << "does not start with a window element";
        return false;
    }

    const int version = reader.attributes().value("version").toInt();
    if (version > kWindowStateVersion)
    {
        qWarning() << "QcwWindow::restoreStateFromFile:" << stateFile << "uses state version" << version
                   << "which is newer than" << kWindowStateVersion;
        return false;
    }

    const auto winId = reader.attributes().value("id").toString();
    if (!winId.isEmpty())
    {
        setId(winId);
    }

    const int savedPerspectiveId = reader.attributes().value("currentPerspective").toInt();

    const auto geometryData = QByteArray::fromHex(reader.attributes().value("geometry").toLocal8Bit());
    const auto winStateData = QcwSerializeUtils::deserialize<QMap<QString, QVariant>>(
        QByteArray::fromHex(reader.attributes().value("extraState").toLocal8Bit()));

    QcwTabFolder* tmpSharedFolder { nullptr };
    QcwPerspective* tmpDummyPerspective { nullptr };
    QMap<int, QcwPerspective*> tmpPerspectives;
    QMap<int, QPair<QWidget*, int>> tabPanes;
    QSet<QString> singletonViewTypes;
    int minFloatingFolderId { int(qcw::FolderStartingIds::Floating) };
    bool ok = true;

    while (ok && reader.readNextStartElement())
    {
        const auto elemName = reader.name();
        if (elemName == "views")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() != "view")
                {
                    reader.skipCurrentElement();
                    continue;
                }

                const auto type = reader.attributes().value("type").toString();
                auto creator = pluginManager->getPluginT<QcwViewCreator>(type);
                if (!creator)
                {
                    qWarning() << "Failed to create view: unknown type:" << type;
                    reader.skipCurrentElement();
                    continue;
                }

                if (creator->singleInstancePerWindow())
                {
                    if (singletonViewTypes.contains(type))
                    {
                        qWarning() << "QcwWindow: dropping a saved duplicate of the per-window singleton view" << type;
                        reader.skipCurrentElement();
                        continue;
                    }

                    singletonViewTypes.insert(type);
                }

                const auto args = QcwSerializeUtils::deserialize<QList<QVariant>>(
                    QByteArray::fromHex(reader.attributes().value("creatingArguments").toLocal8Bit()));
                if (auto view = creator->create(this, false, args))
                {
                    const int viewId = reader.attributes().value("id").toInt();
                    const auto state = QcwSerializeUtils::deserialize<QMap<QString, QVariant>>(
                        QByteArray::fromHex(reader.attributes().value("extraState").toLocal8Bit()));

                    view->setId(viewId);
                    view->setWindowIcon(creator->defaultViewIcon());
                    view->restoreExtraState(state);

                    tabPanes.insert(viewId, { view, viewId });
                }
                reader.skipCurrentElement();
            }
        }
        else if (elemName == "sharedFolder")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "folder" && !tmpSharedFolder)
                {
                    tmpSharedFolder = new QcwTabFolder;
                    if (!tmpSharedFolder->restoreState(reader, tabPanes))
                    {
                        delete tmpSharedFolder;
                        tmpSharedFolder = nullptr;
                        ok = false;
                        break;
                    }
                }
                else
                {
                    reader.skipCurrentElement();
                }
            }
        }
        else if (elemName == "dummyPerspective")
        {
            tmpDummyPerspective = impl->restorePerspectiveState(reader, tmpSharedFolder, tabPanes, minFloatingFolderId);
        }
        else if (elemName == "perspectives")
        {
            while (reader.readNextStartElement())
            {
                if (reader.name() == "perspective")
                {
                    if (auto perspective =
                            impl->restorePerspectiveState(reader, tmpSharedFolder, tabPanes, minFloatingFolderId))
                    {
                        tmpPerspectives.insert(perspective->id(), perspective);
                    }
                }
                else
                {
                    reader.skipCurrentElement();
                }
            }
        }
        else
        {
            ok = restoreMoreState(reader);
        }
    }

    if (reader.hasError() || !ok || !tmpDummyPerspective)
    {
        if (reader.hasError())
        {
            qWarning() << "QcwWindow::restoreStateFromFile: cannot parse" << stateFile << ":" << reader.errorString();
        }
        else if (!ok)
        {
            qWarning() << "QcwWindow::restoreStateFromFile: a state element was refused in" << stateFile;
        }
        else
        {
            qWarning() << "QcwWindow::restoreStateFromFile: no dummy perspective in" << stateFile;
        }

        for (auto perspective : std::as_const(tmpPerspectives))
        {
            perspective->clear();
            perspective->deleteLater();
        }
        if (tmpDummyPerspective)
        {
            tmpDummyPerspective->clear();
            tmpDummyPerspective->deleteLater();
        }
        if (tmpSharedFolder)
        {
            tmpSharedFolder->deleteLater();
        }
        for (const auto& view : std::as_const(tabPanes))
        {
            if (view.first)
            {
                view.first->deleteLater();
            }
        }
        impl->windowId = previousWindowId;
        impl->lastPerspectiveId = previousLastPerspectiveId;
        return false;
    }

    // A session that lost the window's shared folder - older builds deleted it once a closed
    // perspective's container saw it emptied - saved a document without that element. Restoring
    // it would leave the window without one, and every editor would then fall back to the folder
    // of the view that opened it. setupUi() would have created it, so do the same here.
    if (!tmpSharedFolder)
    {
        tmpSharedFolder = createSharedFolder();
        if (tmpSharedFolder)
        {
            qWarning() << "QcwWindow::restoreStateFromFile:" << stateFile
                       << "has no shared folder; created an empty one";
        }
    }

    impl->dummyPerspective = tmpDummyPerspective;
    impl->mainLayout->addWidget(impl->dummyPerspective->folderContainer());
    impl->currentPerspective = impl->dummyPerspective;
    impl->connectPerspective(impl->dummyPerspective);

    impl->sharedFolder = tmpSharedFolder;
    if (impl->sharedFolder)
    {
        impl->sharedFolder->setProperty("FolderId", 0);
        impl->sharedFolder->setProperty("WindowId", id());

        // A document saved while the shared folder was minimized restores it parked in a fast
        // view of the perspective that minimized it. The folder is one widget shared by every
        // perspective, and that tool bar is about to be hidden with its perspective, so the
        // restored session starts with the folder back in its site; the current perspective's
        // show() below then installs it as if it had never been minimized.
        for (auto toolBar : findChildren<QcwFastViewToolBar*>())
        {
            if (toolBar->parksFolder(tmpSharedFolder) && toolBar->releaseFolder())
            {
                emit tmpSharedFolder->restoreRequested();
            }
        }
    }

    for (const auto& perspective : std::as_const(tmpPerspectives))
    {
        impl->addPerspective(perspective);
    }

    if (savedPerspectiveId != impl->dummyPerspective->id() && !impl->perspectives.contains(savedPerspectiveId))
    {
        qWarning() << "QcwWindow::restoreStateFromFile: saved current perspective" << savedPerspectiveId
                   << "was not restored; activating the dummy perspective";
    }

    setCurrentPerspective(savedPerspectiveId);

    // Floating folders are restored hidden; show those of the current perspective now that
    // the document has been accepted. Already showing them is harmless.
    impl->currentPerspective->show();

    impl->lastFloatingFolderId = minFloatingFolderId - 1;

    if (!tabPanes.empty())
    {
        tabPaneManager->resetStartingPaneId(tabPanes.lastKey() + 1);
    }

    if (!geometryData.isEmpty() && !restoreGeometry(geometryData))
    {
        qWarning() << "QcwWindow::restoreStateFromFile: cannot restore the window geometry from" << stateFile;
    }
    restoreExtraState(winStateData);

    // postSetupUi() runs on both paths and is where an application tells them apart.
    impl->restoredFromState = true;
    impl->rebuildTargets();
    postSetupUi();

    // The document carries the titles as they were written, which may be another language's;
    // every tab still wearing its creator's default is re-resolved for the one now installed.
    impl->retranslateViewTitles();

    impl->uiInitialized = true;

    return true;
}

bool QcwWindow::isRestoredFromState() const
{
    return impl->restoredFromState;
}

void QcwWindow::canClose(const std::function<void(bool)>& callback)
{
    if (!callback)
    {
        return;
    }

    impl->closeCoordinator->canClose(impl->windowViews(), callback);
}

void QcwWindow::forceClose()
{
    impl->skipViewPrompts = true;
    impl->closeApproved = true;

    close();

    impl->skipViewPrompts = false;
    impl->closeApproved = false;
    impl->confirmedViews.clear();
}

void QcwWindow::closeEvent(QCloseEvent* event)
{
    if (!impl->closeApproved && !impl->skipViewPrompts)
    {
        if (impl->closeConfirmationPending)
        {
            event->ignore();
            return;
        }

        impl->closeConfirmationPending = true;

        QList<QPointer<QcwView>> asked;
        const auto views = impl->windowViews();
        asked.reserve(views.size());
        for (auto* view : views)
        {
            asked.append(view);
        }

        // Views that answer on the spot keep close() synchronous, which QApplication's quit
        // handling relies on; the close event below carries on with their approval.
        auto synchronous = std::make_shared<bool>(true);
        const bool decided = impl->closeCoordinator->canClose(views, [this, asked, synchronous](bool allowed)
        {
            impl->closeConfirmationPending = false;

            if (!allowed)
            {
                return;
            }

            // The approved views cover the floating folders the close closes below.
            impl->closeApproved = true;
            for (auto& view : asked)
            {
                if (view)
                {
                    impl->confirmedViews.insert(view);
                }
            }

            if (*synchronous)
            {
                return;
            }

            // A late approval cannot resume this close event, and QWidget refuses a close()
            // re-issued from inside a close event, so the window closes on the next turn.
            QPointer<QcwWindow> self { this };
            QMetaObject::invokeMethod(this, [self]()
            {
                if (self)
                {
                    self->close();
                }
            }, Qt::QueuedConnection);
        });
        *synchronous = false;

        if (!decided || !impl->closeApproved)
        {
            event->ignore();
            return;
        }
    }

    emit aboutToClose();

    if (impl->saveStateOnClose && !saveStateToFile(impl->stateFilePath()))
    {
        qWarning() << "QcwWindow: failed to save state to" << impl->stateFilePath();
    }

    impl->closeFloatingFolders();

    QMainWindow::closeEvent(event);

    impl->closeApproved = false;
    impl->confirmedViews.clear();
}

void QcwWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void QcwWindow::retranslateUi()
{
    impl->retranslateUi();
}

void QcwWindow::postSetupUi()
{
}

void QcwWindow::saveMoreState(QXmlStreamWriter& writer)
{
}

bool QcwWindow::restoreMoreState(QXmlStreamReader& reader)
{
    reader.skipCurrentElement();
    return true;
}

QMap<QString, QMenu*>& QcwWindow::registeredMenus()
{
    return impl->menus;
}

QMap<QString, QToolBar*>& QcwWindow::registeredToolBars()
{
    return impl->toolBars;
}

void QcwWindow::addTopToolBar(QToolBar* topToolBar)
{
    if (!topToolBar)
    {
        return;
    }

    addToolBar(Qt::ToolBarArea::TopToolBarArea, topToolBar);
    topToolBar->setVisible(impl->topToolBarsVisible);
}

QcwTabFolder* QcwWindow::createSharedFolder()
{
    return nullptr;
}

// -----------------------------------------------------------------------------
QcwWindowPrivate::QcwWindowPrivate(QcwWindow* interface) :
    owner { interface },
    topicService { new QcwTopicService { interface } },
    closeCoordinator { new QcwViewCloseCoordinator }
{
}

QcwWindowPrivate::~QcwWindowPrivate()
{
    disconnectViewHandlers();
    delete closeCoordinator;
}

void QcwWindowPrivate::createWidgets()
{
    // central widget
    auto widget = new QWidget(owner);
    widget->setObjectName("qcwCentralWidget");
    mainLayout = new QVBoxLayout;
    mainLayout->setContentsMargins(0, 0, 0, 0);
    widget->setLayout(mainLayout);
    owner->setCentralWidget(widget);

    // status bar
    auto statusBar = owner->statusBar();

    statusBarSpacer = new QWidget(statusBar);
    statusBar->addPermanentWidget(statusBarSpacer, 1);

    auto statusBarItemForViews = new QWidget(statusBar);
    statusBarPlaceholderLayout = new QHBoxLayout;
    statusBarPlaceholderLayout->setContentsMargins(0, 0, 0, 0);
    statusBarItemForViews->setLayout(statusBarPlaceholderLayout);

    statusBar->addPermanentWidget(statusBarItemForViews, 0);

    statusBar->setVisible(true);
}

void QcwWindowPrivate::prepareActions()
{
    // Make sure the built-in commands and contributions exist. The workbench registers them
    // during start(); a standalone window registers them here so its menus are complete too.
    QcwBuiltinCommands::registerCommands();

    auto* fileMenu = owner->menuBar()->addMenu(QcwWindow::tr("&File"));
    menus.insert(QcwWindowActionIds::MenuFile, fileMenu);

    auto* editMenu = owner->menuBar()->addMenu(QcwWindow::tr("&Edit"));
    menus.insert(QcwWindowActionIds::MenuEdit, editMenu);

    auto* windowMenu = owner->menuBar()->addMenu(QcwWindow::tr("&Window"));
    menus.insert(QcwWindowActionIds::MenuWindow, windowMenu);

    auto* appearanceMenu = new QMenu(QcwWindow::tr("Appearance"), windowMenu);
    menus.insert(QcwWindowActionIds::MenuWindowAppearance, appearanceMenu);

    auto* newViewMenu = new QMenu(QcwWindow::tr("New View"), windowMenu);
    menus.insert(QcwWindowActionIds::MenuNewView, newViewMenu);

    auto* perspectiveMenu = new QMenu(QcwWindow::tr("Perspective"), windowMenu);
    menus.insert(QcwWindowActionIds::MenuWindowPerspective, perspectiveMenu);

    auto* helpMenu = owner->menuBar()->addMenu(QcwWindow::tr("&Help"));
    menus.insert(QcwWindowActionIds::MenuHelp, helpMenu);

    registerBuiltinWindowHandlers();

    // Commands whose label is re-registered (language change, plugin reload) must refresh
    // their command QActions; the registry's signal is the only place that knows. A rebuild
    // follows because the same signal adds the first command that a contribution has been
    // waiting for, or takes away one a contribution still names.
    QObject::connect(commandRegistry, &QcwCommandRegistry::commandsChanged, owner, [this]()
    {
        createShortcutCommandActions();
        pruneCommandActions();
        syncCommandActionLabels();
        refreshAllCommandActions();
        scheduleRebuild();
    });

    // Contributions arrive outside prepareActions() too: MainWindow adds its View menu
    // contributions in its own constructor, an application may register more in preStart(),
    // and a language change re-registers the built-in ones. Debounce the rebuild so a batch
    // of registrations produces one pass.
    QObject::connect(contributionRegistry, &QcwContributionRegistry::contributionsChanged, owner, [this]()
    {
        scheduleRebuild();
    });

    createShortcutCommandActions();

    // The targets are deliberately not built here. A subclass registers its own tool bar for a
    // contribution target in its constructor body, which runs after this one, so building now
    // would create a tool bar for that target and leave the subclass's beside it as soon as the
    // deferred rebuild fills the map entry it replaced. setupUi() and restoreStateFromFile() are
    // the first points where the window is complete, and each builds the targets before handing
    // over to postSetupUi().
}

void QcwWindowPrivate::retranslateViewTitles()
{
    QSet<QcwTabFolder*> folders;
    auto collectFolders = [&folders](QcwPerspective* perspective)
    {
        if (!perspective)
        {
            return;
        }

        for (auto* folder : perspective->folderContainer()->folders())
        {
            folders.insert(folder);
        }

        const auto floating = perspective->floatingFolders();
        for (auto* folder : floating)
        {
            folders.insert(folder);
        }
    };

    collectFolders(dummyPerspective);
    for (auto perspective : std::as_const(perspectives))
    {
        collectFolders(perspective);
    }

    struct Retitle
    {
        QcwTabFolder* folder;
        int index;
        QcwView* view;
        QString wanted;
        int ordinal;
    };

    // One entry per view, read off the first tab that shows it: a view laid out in several folders
    // (the shared folder is one widget every perspective shows) reads the same title everywhere, so
    // scanning every tab would weigh it twice and leave the second copy compared against a baseline
    // this pass had already rewritten.
    QList<Retitle> pending;
    QSet<QcwView*> seen;
    for (auto* folder : std::as_const(folders))
    {
        if (!folder)
        {
            continue;
        }

        for (int index = 0, count = folder->count(); index < count; ++index)
        {
            auto* view = qobject_cast<QcwView*>(folder->tabPane(index));
            if (!view || seen.contains(view))
            {
                continue;
            }
            seen.insert(view);

            // No baseline, or a title that no longer matches it: this tab says what its user
            // typed, and a language change has no business rewriting that.
            const auto base = view->defaultTitle();
            const int ordinal = base.isEmpty() ? -1 : autoTitleOrdinal(folder->tabText(index), base);
            if (ordinal < 0)
            {
                continue;
            }

            auto* creator = pluginManager->getPluginT<QcwViewCreator>(view->type());
            const auto wanted = creator ? creator->defaultWindowTitle() : QString {};
            if (wanted.isEmpty())
            {
                continue;
            }

            pending.append({ folder, index, view, wanted, ordinal });
        }
    }

    if (pending.isEmpty())
    {
        return;
    }

    // Names this pass replaces are about to be dropped; every other pane in the process keeps
    // its own, because (type, title) is the key saved state finds a view by.
    QSet<QWidget*> retitled;
    for (const auto& entry : std::as_const(pending))
    {
        retitled.insert(entry.view);
    }

    QSet<QString> taken;
    for (auto* pane : tabPaneManager->allTabPanes().keys())
    {
        if (!retitled.contains(pane))
        {
            taken.insert(pane->windowTitle());
        }
    }

    // The lowest ordinal claims its name first, so tabs that already numbered themselves keep
    // that order rather than whichever tab this pass happened to reach first.
    std::sort(pending.begin(), pending.end(), [](const Retitle& a, const Retitle& b)
    {
        return a.wanted == b.wanted ? a.ordinal < b.ordinal : a.wanted < b.wanted;
    });

    for (auto& entry : pending)
    {
        // The tab's own name again, re-resolved: the plain default for the first view of a type
        // and the same ordinal makeNewViewName() gave the later ones.
        int ordinal = entry.ordinal;
        auto title = ordinal == 0 ? entry.wanted : QStringLiteral("%1 %2").arg(entry.wanted).arg(ordinal);
        while (taken.contains(title))
        {
            title = QStringLiteral("%1 %2").arg(entry.wanted).arg(++ordinal);
        }
        taken.insert(title);

        entry.view->setDefaultTitle(entry.wanted);

        // A view can be laid out in more than one folder, and every folder showing it carries the
        // name, so each of those tabs is rewritten, not only the one this entry was read off.
        const auto showing = tabPaneManager->getReferences(entry.view);
        for (auto* folder : showing)
        {
            for (int index = 0, count = folder->count(); index < count; ++index)
            {
                if (folder->tabPane(index) != entry.view)
                {
                    continue;
                }

                // setTabText() mirrors the string onto the pane's window title and re-grabs the
                // tab's mnemonic; the folder's own title is a copy of the current tab and needs
                // it too.
                folder->setTabText(index, title);

                if (folder->isFloating() && folder->currentIndex() == index)
                {
                    folder->setWindowTitle(title);
                }
            }
        }
    }
}

void QcwWindowPrivate::retranslateUi()
{
    retranslateViewTitles();

    for (auto action : perspectiveActionGroup->actions())
    {
        if (auto perspective = perspectives.value(action->data().toInt()))
        {
            action->setText(perspective->displayName());
        }
    }

    auto setMenu = [this](const QString& id, const QString& title)
    {
        if (auto menu = menus.value(id))
        {
            menu->setTitle(title);
        }
    };

    setMenu(QcwWindowActionIds::MenuFile, QcwWindow::tr("&File"));
    setMenu(QcwWindowActionIds::MenuEdit, QcwWindow::tr("&Edit"));
    setMenu(QcwWindowActionIds::MenuWindow, QcwWindow::tr("&Window"));
    setMenu(QcwWindowActionIds::MenuWindowAppearance, QcwWindow::tr("Appearance"));
    setMenu(QcwWindowActionIds::MenuNewView, QcwWindow::tr("New View"));
    setMenu(QcwWindowActionIds::MenuWindowPerspective, QcwWindow::tr("Perspective"));
    setMenu(QcwWindowActionIds::MenuHelp, QcwWindow::tr("&Help"));

    // Re-register the built-in commands so their metadata carries the new language; the
    // commandsChanged() connection in prepareActions() refreshes the command QActions.
    QcwBuiltinCommands::registerCommands();
}

void QcwWindowPrivate::createPerspectiveToolBar()
{
    perspectiveToolBar = new QToolBar(owner);
    perspectiveToolBar->setIconSize(QSize(16, 16));

    perspectiveActionGroup = new QActionGroup(perspectiveToolBar);
    perspectiveActionGroup->setExclusive(true);

    perspectiveToolBar->addAction(
        owner->commandAction(QString(QcwWindowActionIds::ActionOpenPerspective)));

    perspectiveToolBar->addSeparator();

    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setFixedHeight(26);
    owner->statusBar()->addPermanentWidget(line);

    owner->statusBar()->addPermanentWidget(perspectiveToolBar);
}

void QcwWindowPrivate::addPerspective(QcwPerspective* perspective)
{
    if (!perspective || perspectives.contains(perspective->id()))
    {
        return;
    }

    connectPerspective(perspective);
    perspectives.insert(perspective->id(), perspective);
    addPerspectiveButton(perspective);
}

void QcwWindowPrivate::connectPerspective(QcwPerspective* perspective)
{
    if (!perspective)
    {
        return;
    }

    QObject::connect(perspective, &QcwPerspective::currentViewChanged, owner, [this, perspective]()
    {
        handleCurrentViewChanged(perspective);
    });

    // A style pass covers the folders that exist when it runs; this announcement is how a
    // folder built afterwards - a reset layout or a perspective added later - still reaches
    // QcwWindowStyler.
    QObject::connect(perspective->folderContainer(), &QcwTabFolderContainer::folderCreated, owner,
                     &QcwWindow::folderAdded);

    // Every folder of this window answers close requests through the close protocol.
    QObject::connect(perspective->folderContainer(), &QcwTabFolderContainer::folderCreated, owner,
                     [this](QcwTabFolder* folder)
    {
        installFolderCloseHandler(folder);
    });

    QObject::connect(perspective, &QcwPerspective::handlerStateChanged, owner,
                     [this, perspective](const QString& commandId)
    {
        if (owner->currentPerspective() == perspective)
        {
            refreshCommandAction(commandId);
        }
    });

    // The list decides which contributions are placed, so the perspective that is current has to
    // re-place them; the others keep their answer for when they are switched to.
    QObject::connect(perspective, &QcwPerspective::visibleActionSetIdsChanged, owner, [this, perspective]()
    {
        if (owner->currentPerspective() == perspective)
        {
            rebuildTargets();
        }
    });

    for (auto folder : perspective->folderContainer()->folders())
    {
        installFolderCloseHandler(folder);
        emit owner->folderAdded(folder);
    }

    // Floating folders a restored layout put back are created by the perspective itself, so
    // this is the only place that can give their close buttons the protocol. They are not
    // announced through folderAdded(): they are no part of the container the styler covers.
    for (auto folder : perspective->floatingFolders())
    {
        installFolderCloseHandler(folder);
    }
}

void QcwWindowPrivate::addPerspectiveButton(QcwPerspective* perspective)
{
    const int perspectiveId = perspective->id();
    for (auto action : perspectiveActionGroup->actions())
    {
        if (action->data().toInt() == perspectiveId)
        {
            return;
        }
    }

    // displayName(), not name(): a session restored under another language must show that
    // language on its button at once, while name() stays the file the layout is saved under.
    auto action = new QAction(perspective->icon(), perspective->displayName(), perspectiveToolBar);
    action->setData(perspectiveId);
    action->setCheckable(true);

    perspectiveToolBar->addAction(action);
    perspectiveActionGroup->addAction(action);

    QObject::connect(action, &QAction::triggered, owner, [this, perspectiveId](bool checked)
    {
        if (checked)
        {
            owner->setCurrentPerspective(perspectiveId);
        }
    });
}

void QcwWindowPrivate::removePerspectiveButton(int perspectiveId)
{
    QAction* action { nullptr };
    for (auto act : perspectiveActionGroup->actions())
    {
        if (act->data().toInt() == perspectiveId)
        {
            action = act;
            break;
        }
    }

    if (action)
    {
        perspectiveActionGroup->removeAction(action);
        perspectiveToolBar->removeAction(action);
        action->deleteLater();
    }
}

void QcwWindowPrivate::setTopToolBarVisible(bool visible)
{
    topToolBarsVisible = visible;

    const auto allToolBars = owner->findChildren<QToolBar*>();
    for (auto toolBar : allToolBars)
    {
        if (owner->toolBarArea(toolBar) == Qt::TopToolBarArea)
        {
            toolBar->setVisible(visible);
        }
    }

    // A tool bar the contributions fill answers to one more condition: nothing placed in it
    // keeps it hidden whatever this says.
    for (auto* toolBar : std::as_const(toolBars))
    {
        toolBar->setVisible(visible && !toolBar->actions().isEmpty());
    }
}

QStringList QcwWindowPrivate::currentVisibleActionSets() const
{
    if (!currentPerspective)
    {
        return {};
    }

    return currentPerspective->visibleActionSetIds();
}

bool QcwWindowPrivate::firstWarningAbout(const QString& actionSetId)
{
    if (warnedActionSets.contains(actionSetId))
    {
        return false;
    }

    warnedActionSets.insert(actionSetId);
    return true;
}

// A command with a default shortcut has to answer even when nothing contributes it: the shortcut
// is the only way to reach it, and installing that shortcut is commandAction()'s job. Creating
// those actions eagerly also covers commands registered after the window exists.
void QcwWindowPrivate::createShortcutCommandActions()
{
    for (const auto& info : commandRegistry->commands())
    {
        if (info.defaultShortcut != QKeySequence())
        {
            owner->commandAction(info.id);
        }
    }
}

// A command unregistered from the catalog has lost its identity, so the window drops the QAction
// that mirrored it — otherwise commandAction() keeps handing back the cached one and a
// contribution still gets placed against a command that no longer exists. Deferred delete,
// because ~QAction takes the action out of any menu that is open on it right now.
void QcwWindowPrivate::pruneCommandActions()
{
    for (auto it = commandQactions.begin(); it != commandQactions.end();)
    {
        if (commandRegistry->contains(it.key()))
        {
            ++it;
            continue;
        }

        it.value()->deleteLater();
        it = commandQactions.erase(it);
    }
}

void QcwWindowPrivate::syncCommandActionLabels()
{
    for (auto it = commandQactions.cbegin(); it != commandQactions.cend(); ++it)
    {
        const auto info = commandRegistry->command(it.key());
        if (!info)
        {
            continue;
        }

        it.value()->setText(info->label);
        it.value()->setIcon(info->icon);
        it.value()->setCheckable(info->checkable);

        if (info->defaultShortcut != QKeySequence())
        {
            it.value()->setShortcut(info->defaultShortcut);
        }
        else
        {
            it.value()->setShortcut(QKeySequence());
        }
    }
}

void QcwWindowPrivate::scheduleRebuild()
{
    if (rebuildScheduled)
    {
        return;
    }

    rebuildScheduled = true;

    QPointer<QcwWindow> self { owner };
    QMetaObject::invokeMethod(owner, [this, self]()
    {
        if (!self)
        {
            return;
        }
        rebuildScheduled = false;
        rebuildTargets();
    }, Qt::QueuedConnection);
}

// One entry point for everything the contribution registry places, so a change of contributions,
// of commands or of the current perspective's action sets cannot refresh one kind of target and
// leave another holding yesterday's list.
void QcwWindowPrivate::rebuildTargets()
{
    buildMenus();
    buildToolBars();
}

void QcwWindowPrivate::buildMenus()
{
    for (auto it = menus.begin(); it != menus.end(); ++it)
    {
        buildMenuContent(it.key(), it.value());
    }
}

// Which entries a target holds, in the order they hold them: the same question for a menu and for
// a tool bar, so it is answered once — the target's contributions filtered against what the
// current perspective makes visible, put in order by QcwContributionSorter, with a separator
// marked in front of every entry whose groupId differs from the one before it.
QList<QcwWindowPrivate::PlacedEntry> QcwWindowPrivate::placedEntries(const QString& target)
{
    const auto visibleSets = currentVisibleActionSets();

    // The two registry rules of Section 7.3, which the sorter cannot check itself because it is
    // deliberately pure. A contribution whose action set was never registered is invisible: no
    // perspective can list an id that does not exist, so the entry would depend on a mistake. A
    // perspective that lists one is wrong the same way but only needs saying, since the id matches
    // nothing and the contributions it should have revealed are already dropped above.
    QList<QcwContribution> contributions;
    for (const auto& contribution : contributionRegistry->contributionsFor(target))
    {
        if (!contribution.actionSetId.isEmpty() && !actionSetRegistry->contains(contribution.actionSetId))
        {
            if (firstWarningAbout(contribution.actionSetId))
            {
                qWarning() << "QcwWindow: contribution" << contribution.id << "names unregistered action set"
                           << contribution.actionSetId;
            }

            continue;
        }

        contributions.append(contribution);
    }

    for (const auto& actionSetId : visibleSets)
    {
        if (!actionSetRegistry->contains(actionSetId) && firstWarningAbout(actionSetId))
        {
            qWarning() << "QcwWindow: the current perspective lists unregistered action set" << actionSetId;
        }
    }

    const auto sorted = QcwContributionSorter::sort(contributions, visibleSets, target);

    QList<PlacedEntry> placed;
    QString lastGroup;

    for (const auto& contribution : sorted)
    {
        // Only an unregistered command is left out here: it has no QAction at all. A registered
        // one whose handler is missing is placed and greyed out, and re-enables when a view that
        // handles it becomes current — no rebuild in between.
        QAction* action = owner->commandAction(contribution.commandId);
        if (!action)
        {
            // The catalog has no such command, so the contribution can never be placed: it names
            // a command that was mistyped, removed, or registered by something that never loaded.
            // This is the primary diagnostic for a missing entry, so it is said out loud.
            qWarning() << "QcwWindow: contribution" << contribution.id << "names unregistered command"
                       << contribution.commandId;
            continue;
        }

        placed.append({ action, !placed.isEmpty() && lastGroup != contribution.groupId });
        lastGroup = contribution.groupId;
    }

    return placed;
}

void QcwWindowPrivate::buildMenuContent(const QString& menuId, QMenu* menu)
{
    if (!menu)
    {
        return;
    }

    menu->clear();

    const auto placed = placedEntries(menuId);
    for (const auto& entry : placed)
    {
        if (entry.separatorBefore)
        {
            menu->addSeparator();
        }

        menu->addAction(entry.action);
    }

    // The Window menu appends its submenus after its own contributions.
    if (menuId == QcwWindowActionIds::MenuWindow)
    {
        if (!placed.isEmpty())
        {
            menu->addSeparator();
        }

        if (auto* submenu = menus.value(QcwWindowActionIds::MenuWindowAppearance))
        {
            menu->addMenu(submenu);
        }
        if (auto* submenu = menus.value(QcwWindowActionIds::MenuNewView))
        {
            menu->addMenu(submenu);
        }
        if (auto* submenu = menus.value(QcwWindowActionIds::MenuWindowPerspective))
        {
            menu->addMenu(submenu);
        }
    }

    // The New View menu is populated from the loaded view creators, not from contributions.
    if (menuId == QcwWindowActionIds::MenuNewView)
    {
        for (auto plugin : pluginManager->getPluginsByCategory(QcwViewCreator::Category))
        {
            auto viewCreator = qobject_cast<QcwViewCreator*>(plugin);
            if (!viewCreator || viewCreator->singleInstancePerWindow())
            {
                continue;
            }

            auto* action = menu->addAction(viewCreator->defaultWindowTitle());
            QObject::connect(action, &QAction::triggered, owner, [this, viewCreator]()
            {
                auto viewName = QcwWindow::makeNewViewName(viewCreator->defaultWindowTitle());
                if (auto folder = owner->createDetachedView(viewCreator->type(), viewName))
                {
                    folder->resize(600, 400);
                    folder->show();
                }
            });
        }

        if (menu->isEmpty())
        {
            auto* placeholder = menu->addAction(QcwWindow::tr("(No view creators available)"));
            placeholder->setEnabled(false);
        }
    }
}

// A tool bar exists because a contribution names its target: the first rebuild that sees a
// "ToolBar/<Name>" target creates one, and a target the registry stops naming is emptied and
// hidden rather than left behind as a stray strip. Tool bars are built in the order their first
// contribution was registered, so the leftmost one is the target registered first. An application
// that registered its own tool bar under that target in registeredToolBars() has that one filled
// instead of a freshly built one.
void QcwWindowPrivate::buildToolBars()
{
    QStringList wanted;
    for (const auto& contribution : contributionRegistry->allContributions())
    {
        if (!contribution.target.startsWith(qcw::kToolBarTargetPrefix))
        {
            continue;
        }

        if (!wanted.contains(contribution.target))
        {
            wanted.append(contribution.target);
        }
    }

    for (auto it = toolBars.begin(); it != toolBars.end(); ++it)
    {
        if (!wanted.contains(it.key()))
        {
            it.value()->clear();
            it.value()->setVisible(false);
        }
    }

    for (const auto& target : std::as_const(wanted))
    {
        auto* toolBar = toolBars.value(target);
        if (!toolBar)
        {
            toolBar = createPlacedToolBar(target);
            toolBars.insert(target, toolBar);
        }

        buildToolBarContent(target, toolBar);
    }
}

QToolBar* QcwWindowPrivate::createPlacedToolBar(const QString& target)
{
    const auto name = target.mid(int(qcw::kToolBarTargetPrefix.size()));

    auto* toolBar = new QToolBar(name, owner);
    auto slug = name;
    slug.replace(QLatin1Char('/'), QLatin1Char('_'));
    toolBar->setObjectName(QStringLiteral("qcwToolBar_") + slug);
    toolBar->setIconSize(QSize(16, 16));

    // The subclass entry point, so a tool bar the window builds for itself lands where a
    // subclass puts its own and picks up the answer "Show Toolbar" currently holds.
    owner->addTopToolBar(toolBar);

    return toolBar;
}

void QcwWindowPrivate::buildToolBarContent(const QString& target, QToolBar* toolBar)
{
    toolBar->clear();

    const auto placed = placedEntries(target);
    for (const auto& entry : placed)
    {
        if (entry.separatorBefore)
        {
            toolBar->addSeparator();
        }

        toolBar->addAction(entry.action);
    }

    // Nothing placed means nothing to offer, so the strip goes rather than sitting empty; the
    // "Show Toolbar" command still governs the whole row of them.
    toolBar->setVisible(!placed.isEmpty() && topToolBarsVisible);
}

void QcwWindowPrivate::registerBuiltinWindowHandlers()
{
    auto* openAppDir = new QcwLambdaCommandHandler(owner);
    openAppDir->onExecute([](const QVariantMap&)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QApplication::applicationDirPath()));
    });
    owner->registerHandler(QcwWindowActionIds::ActionOpenAppDir, openAppDir);

    auto* openLogDir = new QcwLambdaCommandHandler(owner);
    openLogDir->onExecute([](const QVariantMap&)
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QcwAppDirectories::logDirectory()));
    });
    owner->registerHandler(QcwWindowActionIds::ActionOpenLogDir, openLogDir);

    auto* showToolBar = new QcwLambdaCommandHandler(owner);
    showToolBar->onChecked([this]()
    {
        return topToolBarsVisible;
    });
    showToolBar->onSetChecked([this](bool checked)
    {
        setTopToolBarVisible(checked);
    });
    owner->registerHandler(QcwWindowActionIds::ActionShowToolBar, showToolBar);

    auto* showStatusBar = new QcwLambdaCommandHandler(owner);
    showStatusBar->onChecked([this]()
    {
        return owner->statusBar()->isVisible();
    });
    showStatusBar->onSetChecked([this](bool checked)
    {
        owner->statusBar()->setVisible(checked);
    });
    owner->registerHandler(QcwWindowActionIds::ActionShowStatusBar, showStatusBar);

    auto* commandPalette = new QcwLambdaCommandHandler(owner);
    commandPalette->onExecute([this](const QVariantMap&)
    {
        owner->openCommandPalette();
    });
    owner->registerHandler(QcwWindowActionIds::ActionCommandPalette, commandPalette);

    auto* openPerspective = new QcwLambdaCommandHandler(owner);
    openPerspective->onExecute([this](const QVariantMap&)
    {
        QcwOpenPerspectiveDialog dialog;
        dialog.resize(360, 400);

        if (dialog.exec() == QDialog::Accepted)
        {
            auto info = dialog.selectedPerspective();
            if (!info.name.isEmpty() && !info.type.isEmpty())
            {
                openPerspectiveFromInfo(info);
            }
        }
    });
    owner->registerHandler(QcwWindowActionIds::ActionOpenPerspective, openPerspective);

    auto* savePerspective = new QcwLambdaCommandHandler(owner);
    savePerspective->onExecute([this](const QVariantMap&)
    {
        if (!currentPerspective)
        {
            return;
        }

        QcwSavePerspectiveDialog dialog { currentPerspective->name(), owner };
        dialog.resize(360, 480);

        auto saveButton = dialog.saveButton();
        QObject::connect(saveButton, &QPushButton::clicked, &dialog, [this, &dialog](bool)
        {
            const auto name = dialog.newPerspectiveName().trimmed();

            if (const auto error = perspectiveNameError(name); !error.isEmpty())
            {
                QMessageBox::warning(&dialog, QcwWindow::tr("Invalid Perspective Name"), error);
                return;
            }

            auto info = availablePerspectiveInfos().value(name);
            if (info.name == name)
            {
                if (info.type != currentPerspective->type())
                {
                    QMessageBox::critical(
                        &dialog, QcwWindow::tr("Type Mismatch"),
                        QcwWindow::tr("A perspective with the name '%1' already exists, but the type is "
                                      "different from the current perspective. Cannot save.")
                            .arg(name));
                    return;
                }

                auto btn = QMessageBox::question(
                    &dialog, QcwWindow::tr("Overwrite Perspective"),
                    QcwWindow::tr("A perspective with the name '%1' already exists. Do you want to overwrite?")
                        .arg(name),
                    QMessageBox::Yes | QMessageBox::No);
                if (btn != QMessageBox::Yes)
                {
                    return;
                }
            }

            const auto oldName = currentPerspective->name();
            currentPerspective->setName(name);

            if (!savedPerspectives->saveState(currentPerspective))
            {
                currentPerspective->setName(oldName);
                QMessageBox::critical(&dialog, QcwWindow::tr("Save Perspective Failed"),
                                      QcwWindow::tr("Failed to save the perspective '%1'. Check that the configuration "
                                                    "directory is writable.")
                                          .arg(name));
                return;
            }

            savedPerspectives->addPerspective(name, currentPerspective->type());

            if (auto checkedAction = perspectiveActionGroup->checkedAction())
            {
                checkedAction->setText(name);
            }

            dialog.accept();
        });

        dialog.exec();
    });
    owner->registerHandler(QcwWindowActionIds::ActionSavePerspectiveAs, savePerspective);

    auto* resetPerspective = new QcwLambdaCommandHandler(owner);
    resetPerspective->onExecute([this](const QVariantMap&)
    {
        if (currentPerspective)
        {
            currentPerspective->resetLayout();
        }
    });
    owner->registerHandler(QcwWindowActionIds::ActionResetPerspective, resetPerspective);

    auto* closePerspective = new QcwLambdaCommandHandler(owner);
    closePerspective->onExecute([this](const QVariantMap&)
    {
        if (currentPerspective)
        {
            owner->closePerspective(currentPerspective->id());
        }
    });
    owner->registerHandler(QcwWindowActionIds::ActionClosePerspective, closePerspective);

    auto* closeAllPerspectives = new QcwLambdaCommandHandler(owner);
    closeAllPerspectives->onExecute([this](const QVariantMap&)
    {
        for (auto id : perspectives.keys())
        {
            owner->closePerspective(id);
        }
    });
    owner->registerHandler(QcwWindowActionIds::ActionCloseAllPerspectives, closeAllPerspectives);
}

QcwPerspective* QcwWindowPrivate::createPerspective(const QString& type)
{
    QcwPerspective* perspective { nullptr };
    if (type == "DummyPerspective")
    {
        perspective = QcwPerspective::createDummyPerspective(owner);
    }
    else
    {
        if (auto creator = pluginManager->getPluginT<QcwPerspectiveCreator>(type))
        {
            perspective = creator->create(owner);
        }
    }
    return perspective;
}

QcwPerspective* QcwWindowPrivate::restorePerspectiveState(QXmlStreamReader& reader,
                                                          QcwTabFolder* sharedFolder,
                                                          const QMap<int, QPair<QWidget*, int>>& tabPanes,
                                                          int& minFloatingFolderId)
{
    auto type = reader.attributes().value("type");
    QcwPerspective* perspective = createPerspective(type.toString());
    if (!perspective)
    {
        qWarning() << "QcwWindow: cannot create the perspective type" << type.toString() << "while restoring";
        return nullptr;
    }

    if (!perspective->restoreState(reader, sharedFolder, tabPanes, minFloatingFolderId))
    {
        qWarning() << "QcwWindow: cannot restore the perspective type" << type.toString();
        perspective->clear();
        perspective->deleteLater();
        return nullptr;
    }

    if (perspective->id() > lastPerspectiveId)
    {
        lastPerspectiveId = perspective->id();
    }

    return perspective;
}

void QcwWindowPrivate::openPerspectiveFromInfo(const QcwPerspectiveInfo& info)
{
    if (info.type.isEmpty())
    {
        return;
    }

    QcwPerspective* perspective { nullptr };
    for (const auto& [id, persp] : perspectives.asKeyValueRange())
    {
        if (persp->type() == info.type && persp->name() == info.name)
        {
            perspective = persp;
            owner->setCurrentPerspective(id);
            break;
        }
    }

    if (!perspective)
    {
        if (!info.hasSavedState) // a new perspective with a default layout
        {
            perspective = createPerspective(info.type);
            if (perspective)
            {
                perspective->createInitialLayout();
            }

            if (!perspective)
            {
                QMessageBox::warning(owner, QcwWindow::tr("Open Perspective Failed"),
                                     QcwWindow::tr("Failed to create a perspective of type '%1'").arg(info.type));
                return;
            }
        }
        else // try to restore state from the file
        {
            QList<QcwView*> createdViews;
            auto xmlData = savedPerspectives->loadState(info.name);
            if (!xmlData.isEmpty())
            {
                QXmlStreamReader reader(xmlData);
                if (reader.readNextStartElement() && reader.name() == "savedPerspective")
                {
                    QMap<int, QPair<QWidget*, int>> tabPanes;
                    QMap<QPair<QString, QString>, QcwView*> indexedViews;
                    QMap<QString, QcwView*> viewsByType;
                    for (auto view : collectViews())
                    {
                        tabPanes.insert(view->id(), { view, view->id() });
                        indexedViews.insert({ view->windowTitle(), view->type() }, view);
                        viewsByType.insert(view->type(), view);
                    }

                    while (reader.readNextStartElement())
                    {
                        if (reader.name() == "views")
                        {
                            while (reader.readNextStartElement())
                            {
                                if (reader.name() == "view")
                                {
                                    const int viewId = reader.attributes().value("id").toInt();
                                    const QPair<QString, QString> key { reader.attributes().value("name").toString(),
                                                                        reader.attributes().value("type").toString() };

                                    auto creator = pluginManager->getPluginT<QcwViewCreator>(key.second);
                                    auto existingView = indexedViews.value(key);
                                    if (!existingView && creator && creator->singleInstancePerWindow())
                                    {
                                        // A per-window singleton is identified by its type, so a saved
                                        // name that no longer matches still lands on the one view.
                                        existingView = viewsByType.value(key.second);
                                    }

                                    if (!existingView)
                                    {
                                        if (creator)
                                        {
                                            auto ba = reader.attributes().value("creatingArguments").toLocal8Bit();
                                            auto args = QcwSerializeUtils::deserialize<QList<QVariant>>(
                                                QByteArray::fromHex(ba));
                                            if (auto newView = creator->create(owner, true, args))
                                            {
                                                createdViews.append(newView);

                                                ba = reader.attributes().value("extraState").toLocal8Bit();
                                                auto state = QcwSerializeUtils::deserialize<QMap<QString, QVariant>>(
                                                    QByteArray::fromHex(ba));

                                                newView->setWindowTitle(key.first);
                                                newView->setWindowIcon(creator->defaultViewIcon());
                                                newView->restoreExtraState(state);

                                                // create() already gave the view its pane id; taking another
                                                // one here would skip an id and replace that first one.
                                                tabPanes[viewId] = { newView, newView->id() };
                                                indexedViews.insert(key, newView);
                                                viewsByType.insert(key.second, newView);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        tabPanes[viewId] = { existingView, existingView->id() };
                                    }
                                    reader.skipCurrentElement();
                                }
                                else
                                {
                                    reader.skipCurrentElement();
                                }
                            }
                        }
                        else if (reader.name() == "perspective" && !perspective)
                        {
                            perspective = restorePerspectiveState(reader, sharedFolder, tabPanes, lastFloatingFolderId);
                        }
                        else
                        {
                            reader.skipCurrentElement();
                        }
                    }

                    // A half-read document leaves a perspective full of half-read folders;
                    // treat it like a missing one rather than opening it.
                    if (reader.hasError() && perspective)
                    {
                        perspective->clear();
                        perspective->deleteLater();
                        perspective = nullptr;
                    }
                }
            }

            if (!perspective)
            {
                for (auto view : std::as_const(createdViews))
                {
                    view->deleteLater();
                }

                QMessageBox::warning(owner, QcwWindow::tr("Open Perspective Failed"),
                                     QcwWindow::tr("Failed to restore the perspective '%1' from file").arg(info.name));
                return;
            }
        }

        perspective->setId(++lastPerspectiveId);
        perspective->setName(info.name);

        owner->openPerspective(perspective);

        // The same reason as in restoreStateFromFile(): a saved perspective brings the titles as
        // its file wrote them, not as the current language names them.
        retranslateViewTitles();
    }
}

QSet<QcwView*> QcwWindowPrivate::collectViews() const
{
    QSet<QcwView*> views;

    if (dummyPerspective)
    {
        dummyPerspective->collectViews(views);
    }

    for (auto persp : perspectives)
    {
        persp->collectViews(views);
    }

    return views;
}

QList<QcwView*> QcwWindowPrivate::windowViews() const
{
    const auto viewSet = collectViews();
    QList<QcwView*> views;
    views.reserve(viewSet.size());
    for (auto* view : viewSet)
    {
        views.append(view);
    }
    return views;
}

void QcwWindowPrivate::removePerspective(int perspectiveId)
{
    auto it = perspectives.find(perspectiveId);
    if (it == perspectives.end())
    {
        return;
    }

    auto perspective = it.value();
    if (perspective == dummyPerspective)
    {
        return;
    }

    // remove action from the perspective tool bar
    removePerspectiveButton(perspectiveId);

    perspectives.erase(it);
    perspective->hide();

    if (perspective == currentPerspective)
    {
        // Switch perspective. An id no longer in the map is resolved to the dummy
        // perspective by setCurrentPerspective(), so no entry is left dangling.
        const int nextId =
            !perspectives.empty() ? perspectives.firstKey() : (dummyPerspective ? dummyPerspective->id() : -1);
        owner->setCurrentPerspective(nextId);
    }

    perspective->clear();
    perspective->deleteLater();
}

void QcwWindowPrivate::installFolderCloseHandler(QcwTabFolder* folder)
{
    if (!folder || folder->property("QcwCloseHandlerInstalled").toBool())
    {
        return;
    }

    folder->setProperty("QcwCloseHandlerInstalled", true);

    // The close button and the context menu of a tab only announce the request; whether the
    // pane is a view and may close is decided here.
    QObject::connect(folder, &QcwTabFolder::tabCloseRequested, owner, [this, folder](int index)
    {
        requestTabClose(folder, index);
    });

    QObject::connect(folder, &QcwTabFolder::aboutToClose, owner, [this, folder](QCloseEvent* event)
    {
        if (!event->isAccepted() || skipViewPrompts)
        {
            return;
        }

        auto views = viewsInFolder(folder);
        if (views.isEmpty())
        {
            return;
        }

        QList<QcwView*> unconfirmed;
        for (auto* view : views)
        {
            if (!confirmedViews.contains(view))
            {
                unconfirmed.append(view);
            }
        }

        // A close that already asked these views — the window or perspective they belong to
        // is going away — lets the folder go without a second question.
        if (unconfirmed.isEmpty())
        {
            for (auto* view : views)
            {
                confirmedViews.remove(view);
            }
            return;
        }

        const bool alreadyAsking = std::any_of(pendingFolderCloses.cbegin(), pendingFolderCloses.cend(),
                                               [folder](const QPointer<QcwTabFolder>& pending)
        {
            return pending == folder;
        });
        if (alreadyAsking)
        {
            event->ignore();
            return;
        }

        pendingFolderCloses.append(folder);

        QList<QPointer<QcwView>> asked;
        asked.reserve(unconfirmed.size());
        for (auto* view : unconfirmed)
        {
            asked.append(view);
        }

        // Answers that arrive on the spot leave the close event accepted, so the folder
        // still closes in this turn.
        auto synchronous = std::make_shared<bool>(true);
        auto approved = std::make_shared<bool>(false);
        const bool decided = closeCoordinator->canClose(
            unconfirmed, [this, folder = QPointer<QcwTabFolder> { folder }, asked, synchronous, approved](bool allowed)
        {
            pendingFolderCloses.removeAll(folder);

            if (*synchronous)
            {
                // The close event continues below with this answer; a second pass that
                // would consume a confirmation never happens, so none is recorded.
                *approved = allowed;
                return;
            }

            if (!allowed || !folder)
            {
                return;
            }

            // The close below runs the handler again, which finds these views confirmed
            // and lets the folder go without a second question.
            for (auto& view : asked)
            {
                if (view)
                {
                    confirmedViews.insert(view);
                }
            }

            // A late approval cannot resume this close event, and QWidget refuses a
            // close() re-issued from inside a close event, so the folder closes on the
            // next turn.
            QMetaObject::invokeMethod(folder, [folder]()
            {
                if (folder)
                {
                    folder->close();
                }
            }, Qt::QueuedConnection);
        });
        *synchronous = false;

        if (!decided || !*approved)
        {
            event->ignore();
        }
    });
}

void QcwWindowPrivate::requestTabClose(QcwTabFolder* folder, int index)
{
    auto* pane = folder ? folder->tabPane(index) : nullptr;
    if (!qobject_cast<QcwView*>(pane))
    {
        // A pane that is no view keeps the old rule: the application's own receiver decides.
        return;
    }

    pendingTabCloses.append({ folder, pane });

    // Close Others, Close All and the like emit one request per tab in a single call; the
    // batch is answered as one close, so a refusal stops the rest.
    if (!tabCloseFlushScheduled)
    {
        tabCloseFlushScheduled = true;
        QMetaObject::invokeMethod(owner, [this]()
        {
            flushTabCloses();
        }, Qt::QueuedConnection);
    }
}

void QcwWindowPrivate::flushTabCloses()
{
    tabCloseFlushScheduled = false;

    const auto requested = pendingTabCloses;
    pendingTabCloses.clear();

    QList<QPair<QPointer<QcwTabFolder>, QPointer<QWidget>>> tabs;
    QList<QcwView*> views;

    for (const auto& entry : requested)
    {
        auto folder = entry.first;
        auto pane = entry.second;
        auto* view = qobject_cast<QcwView*>(pane.data());

        // Between the request and this flush the tab may be gone or moved elsewhere.
        if (!folder || !view || folder->indexOf(pane) < 0)
        {
            continue;
        }

        if (!views.contains(view))
        {
            views.append(view);
        }

        tabs.append(entry);
    }

    if (tabs.isEmpty())
    {
        return;
    }

    closeCoordinator->canClose(views, [this, tabs](bool allowed)
    {
        if (!allowed)
        {
            return;
        }

        for (const auto& entry : tabs)
        {
            auto folder = entry.first;
            auto pane = entry.second;

            if (!folder || !pane)
            {
                continue;
            }

            const int index = folder->indexOf(pane);
            if (index >= 0)
            {
                folder->removeTab(index);
            }
        }
    });
}

void QcwWindowPrivate::installStatusWidget(QWidget* widget)
{
    // A caller-deleted widget may have left its owner connection behind; drop it before
    // it could mistake this widget for its own.
    if (statusWidgetOwnerConnection)
    {
        QObject::disconnect(statusWidgetOwnerConnection);
        statusWidgetOwnerConnection = {};
    }

    statusWidget = widget;
    statusWidgetOwner = widget->parentWidget();

    if (statusWidgetOwner)
    {
        // The owner cannot take the widget back once it is gone, and while the widget is
        // parked here the window is the only one that can clean it up.
        statusWidgetOwnerConnection = QObject::connect(statusWidgetOwner, &QObject::destroyed, owner, [this]()
        {
            if (statusWidget)
            {
                statusWidget->hide();
                statusWidget->deleteLater();
            }

            statusWidget = nullptr;
            statusWidgetOwner = nullptr;
            statusWidgetOwnerConnection = {};
        });
    }

    statusBarPlaceholderLayout->addWidget(widget);
    widget->show();
}

void QcwWindowPrivate::releaseStatusWidget()
{
    if (statusWidgetOwnerConnection)
    {
        QObject::disconnect(statusWidgetOwnerConnection);
        statusWidgetOwnerConnection = {};
    }

    if (!statusWidget)
    {
        statusWidgetOwner = nullptr;
        return;
    }

    auto widget = statusWidget;
    auto originalParent = statusWidgetOwner;

    statusWidget = nullptr;
    statusWidgetOwner = nullptr;

    if (auto item = statusBarPlaceholderLayout->takeAt(0))
    {
        delete item;
    }

    widget->hide();
    if (widget->parentWidget() != originalParent)
    {
        widget->setParent(originalParent);
    }
}

void QcwWindowPrivate::handleCurrentViewChanged(QcwPerspective* perspective)
{
    if (!perspective || perspective != currentPerspective)
    {
        return;
    }

    auto view = perspective->currentView();
    auto newStatusWidget = view ? view->statusBarWidget() : nullptr;

    if (newStatusWidget != statusWidget)
    {
        releaseStatusWidget();

        if (newStatusWidget)
        {
            installStatusWidget(newStatusWidget);
        }
    }

    connectViewHandlers(view);
    refreshAllCommandActions();

    emit owner->currentViewChanged(view);
}

void QcwWindowPrivate::closeFloatingFolders()
{
    auto closeAll = [](QcwPerspective* perspective)
    {
        if (!perspective)
        {
            return;
        }

        const auto folders = perspective->floatingFolders();
        for (auto folder : folders)
        {
            if (folder)
            {
                folder->close();
            }
        }
    };

    closeAll(dummyPerspective);

    for (auto perspective : std::as_const(perspectives))
    {
        closeAll(perspective);
    }
}

void QcwWindowPrivate::refreshCommandAction(const QString& commandId)
{
    auto* action = commandQactions.value(commandId, nullptr);
    if (!action)
    {
        return;
    }

    auto* handler = owner->resolveHandler(commandId);

    action->setEnabled(handler != nullptr && handler->isEnabled());

    // Where an entry appears is the contribution's decision; whether it can run is the handler's.
    // So a command nothing handles right now stays on screen and greyed out, and only a handler
    // that reports itself invisible withdraws it — which also stops its shortcut, since Qt
    // disables the shortcut of a hidden QAction.
    action->setVisible(handler == nullptr || handler->isVisible());

    if (action->isCheckable())
    {
        QSignalBlocker blocker(action);
        action->setChecked(handler != nullptr && handler->isChecked());
    }
}

void QcwWindowPrivate::refreshAllCommandActions()
{
    for (auto it = commandQactions.cbegin(); it != commandQactions.cend(); ++it)
    {
        refreshCommandAction(it.key());
    }
}

// Both halves go together: a handler that stops being listened to must also stop being watched
// for destruction, or its dying callback reaches into an impl nobody owns any more.
void QcwWindowPrivate::dropHandlerConnections(const QString& commandId)
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

void QcwWindowPrivate::connectViewHandlers(QcwView* view)
{
    disconnectViewHandlers();

    if (!view)
    {
        return;
    }

    const auto handlers = view->commandHandlers();
    for (auto it = handlers.cbegin(); it != handlers.cend(); ++it)
    {
        auto* handler = it.value();
        if (!handler)
        {
            continue;
        }

        const auto commandId = it.key();
        viewHandlerConnections.append(QObject::connect(handler, &QcwCommandHandler::stateChanged, owner,
                                                       [this, commandId]()
        {
            refreshCommandAction(commandId);
        }));
    }
}

void QcwWindowPrivate::disconnectViewHandlers()
{
    for (const auto& conn : viewHandlerConnections)
    {
        QObject::disconnect(conn);
    }
    viewHandlerConnections.clear();
}

QString QcwWindowPrivate::stateFilePath() const
{
    return QString("%1/%2.xml").arg(QcwAppDirectories::configDirectory(), windowId);
}

// -----------------------------------------------------------------------------

QcwPerspectiveListView::QcwPerspectiveListView(QWidget* parent) :
    QListView { parent }
{
}

void QcwPerspectiveListView::initialize()
{
    auto model = new QStandardItemModel(this);
    setModel(model);
    setItemDelegate(new QcwFixedRowHeightItemDelegate(this));

    const auto infos = availablePerspectiveInfos();
    for (const auto& [name, info] : infos.asKeyValueRange())
    {
        if (info.type == "DummyPerspective")
        {
            QStandardItem* item = new QStandardItem(QcwPerspective::dummyPerspectiveIcon(), name);
            item->setData(info.type, Qt::UserRole);
            item->setData(info.hasSavedState, Qt::UserRole + 1);
            model->appendRow(item);
        }
        else if (auto creator = pluginManager->getPluginT<QcwPerspectiveCreator>(info.type))
        {
            QStandardItem* item = new QStandardItem(creator->defaultPerspectiveIcon(), name);
            item->setData(info.type, Qt::UserRole);
            item->setData(info.hasSavedState, Qt::UserRole + 1);
            model->appendRow(item);
        }
    }

    setEditTriggers(QAbstractItemView::NoEditTriggers);
}

QcwPerspectiveInfo QcwPerspectiveListView::currentInfo() const
{
    QcwPerspectiveInfo info;

    auto index = currentIndex();
    if (index.isValid())
    {
        info.type = index.data(Qt::UserRole).toString();
        info.name = index.data(Qt::DisplayRole).toString();
        info.hasSavedState = index.data(Qt::UserRole + 1).toBool();
    }

    return info;
}

// -----------------------------------------------------------------------------

QcwOpenPerspectiveDialog::QcwOpenPerspectiveDialog(QWidget* parent) :
    QDialog { parent }
{
    auto vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(9, 9, 9, 9);
    vLayout->setSpacing(9);

    auto listView = new QcwPerspectiveListView(this);
    listView->initialize();

    connect(listView->selectionModel(), &QItemSelectionModel::currentChanged,
            [this, listView](const QModelIndex& curr, const QModelIndex& prev)
    {
        info = listView->currentInfo();
    });

    connect(listView, &QListView::doubleClicked, [this, listView](const QModelIndex& index)
    {
        if (index.isValid())
        {
            info = listView->currentInfo();
            accept();
        }
    });

    if (auto model = listView->model(); model && model->rowCount() > 0)
    {
        listView->setCurrentIndex(model->index(0, 0));
    }

    vLayout->addWidget(listView);

    auto buttonBox = new QDialogButtonBox(this);
    buttonBox->setOrientation(Qt::Orientation::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Open);

    connect(buttonBox, &QDialogButtonBox::accepted, this, qOverload<>(&QDialog::accept));
    connect(buttonBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));

    vLayout->addWidget(buttonBox);

    setLayout(vLayout);
    setWindowTitle(tr("Open Perspective"));
}

// -----------------------------------------------------------------------------

QcwSavePerspectiveDialog::QcwSavePerspectiveDialog(const QString& perspectiveName, QWidget* parent) :
    QDialog { parent }
{
    auto vLayout1 = new QVBoxLayout(this);
    vLayout1->setSpacing(12);
    vLayout1->setObjectName("verticalLayout_2");
    auto label = new QLabel(tr("Enter or select a name to save the current perspective as."), this);
    label->setWordWrap(true);

    vLayout1->addWidget(label);

    auto hLayout = new QHBoxLayout();
    hLayout->setObjectName("horizontalLayout");
    label = new QLabel(tr("Name:"), this);

    hLayout->addWidget(label);

    lineEdit = new QLineEdit(this);
    lineEdit->setObjectName("lineEdit");
    lineEdit->setText(perspectiveName);

    hLayout->addWidget(lineEdit);

    vLayout1->addLayout(hLayout);

    auto vLayout2 = new QVBoxLayout();
    vLayout2->setSpacing(3);
    vLayout2->setObjectName("verticalLayout");

    label = new QLabel(tr("Existing Perspectives:"), this);

    vLayout2->addWidget(label);

    listView = new QcwPerspectiveListView(this);
    listView->setObjectName("listView");
    listView->initialize();

    vLayout2->addWidget(listView);

    vLayout1->addLayout(vLayout2);

    buttonBox = new QDialogButtonBox(this);
    buttonBox->setObjectName("buttonBox");
    buttonBox->setOrientation(Qt::Orientation::Horizontal);
    buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Save);
    buttonBox->button(QDialogButtonBox::StandardButton::Save)->setEnabled(!perspectiveName.isEmpty());

    vLayout1->addWidget(buttonBox);

    connect(listView->selectionModel(), &QItemSelectionModel::currentChanged,
            [this](const QModelIndex& curr, const QModelIndex& prev)
    {
        lineEdit->setText(listView->currentInfo().name);
    });

    connect(lineEdit, &QLineEdit::textChanged, [this](const QString& text)
    {
        buttonBox->button(QDialogButtonBox::StandardButton::Save)->setEnabled(!text.isEmpty());
    });

    connect(buttonBox, &QDialogButtonBox::rejected, this, qOverload<>(&QDialog::reject));

    setWindowTitle(tr("Save Perspective As..."));
}

// -----------------------------------------------------------------------------

QString perspectiveNameError(const QString& name)
{
    if (name.isEmpty())
    {
        return QcwWindow::tr("The name must not be empty.");
    }

    // The name becomes a file name in the saved-perspectives directory, so it has to
    // stay a single portable path component.
    if (name == "." || name == ".." || name.contains('/') || name.contains('\\'))
    {
        return QcwWindow::tr("The name must not contain path separators.");
    }

    static const QString invalidCharacters = "<>:\"|?*";
    for (const auto c : name)
    {
        if (invalidCharacters.contains(c) || c.unicode() < 0x20)
        {
            return QcwWindow::tr("The name must not contain any of %1 or control characters.").arg(invalidCharacters);
        }
    }

    return {};
}

QMap<QString, QcwPerspectiveInfo> availablePerspectiveInfos()
{
    QMap<QString, QcwPerspectiveInfo> infos;
    for (auto plugin : pluginManager->getPluginsByCategory(QcwPerspectiveCreator::Category))
    {
        if (auto creator = qobject_cast<QcwPerspectiveCreator*>(plugin))
        {
            infos.insert(creator->defaultName(), { creator->type(), creator->defaultName(), false });
        }
    }

    for (const auto& [name, type] : savedPerspectives->allPerspectives().asKeyValueRange())
    {
        auto it = infos.find(name);
        if (it != infos.end())
        {
            it.value().hasSavedState = true;
        }
        else
        {
            infos.insert(name, { type, name, true });
        }
    }

    return infos;
}

QT_END_NAMESPACE
