// Exercises the objects that travel between perspectives instead of living in one: the
// window's shared folder and the views two perspectives can show at once. Each case below
// is a contract from the QcwPerspective/QcwWindow documentation:
// - resetting a perspective keeps the window's shared folder, even while it is minimized;
// - resetting a hidden perspective leaves the shared folder with the visible one;
// - a reset drops a per-window singleton from the shared folder only when the rebuilt
//   layout holds it elsewhere;
// - closing a perspective does not ask a view that another perspective keeps open;
// - a detached folder answers its own and its tabs' close buttons through the window;
// - a minimized shared folder returns to normal when its perspective is left;
// - closing a perspective releases a shared view its floating folder held, without asking;
// - destroying the window disposes of its floating folders and their views.

#include "QcwDockingTypes.h"
#include "QcwFastViewToolBar.h"
#include "QcwPerspective.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabPaneManager.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <cstdio>
#include <utility>

namespace
{
constexpr QLatin1String SharedViewType { "SharedObjectsSharedView" };
constexpr QLatin1String SingletonViewType { "SharedObjectsSingletonView" };
constexpr QLatin1String FloatingViewType { "SharedObjectsFloatingView" };

int gFailures = 0;
int gCloseAsks = 0;

void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

void check(bool condition, const char* message, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", condition ? "ok  " : "FAIL", message, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!condition)
    {
        ++gFailures;
    }
}

class TestView : public QcwView
{
public:
    using QcwView::QcwView;
};

/// Refuses every close request, so a close that asks it cannot proceed.
class RefusingView : public TestView
{
public:
    using TestView::TestView;

    void canClose(const std::function<void(bool)>& callback) override
    {
        ++gCloseAsks;
        callback(false);
    }
};

class TestViewCreator : public QcwViewCreator
{
public:
    QString version() const override
    {
        return QStringLiteral("1.0.0");
    }
    QString provider() const override
    {
        return QStringLiteral("test");
    }
    QString updateTime() const override
    {
        return {};
    }
    QString description() const override
    {
        return type();
    }
    QString details() const override
    {
        return {};
    }
    QIcon defaultViewIcon() const override
    {
        return {};
    }
    QString defaultWindowTitle() const override
    {
        return type();
    }
};

class SharedViewCreator : public TestViewCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwViewCreator)
public:
    QString type() const override
    {
        return QString(SharedViewType);
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new RefusingView { parent };
    }
};

class SingletonViewCreator : public TestViewCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwViewCreator)
public:
    QString type() const override
    {
        return QString(SingletonViewType);
    }
    bool singleInstancePerWindow() const override
    {
        return true;
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new TestView { parent };
    }
};

class FloatingViewCreator : public TestViewCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwViewCreator)
public:
    QString type() const override
    {
        return QString(FloatingViewType);
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new TestView { parent };
    }
};

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

/// Lays the window's shared folder out as its central folder, with one tab of its own.
class SharedPerspective : public QcwPerspective
{
public:
    using QcwPerspective::QcwPerspective;

    void createInitialLayout() override
    {
        auto container = folderContainer();
        auto shared = window()->sharedFolder();
        if (!shared)
        {
            return;
        }

        container->addSharedFolder(shared, nullptr, qcw::LeftDockArea);
        container->setCentralFolder(shared);

        if (shared->count() == 0)
        {
            shared->addTab(new QWidget, QStringLiteral("SharedTab"));
        }
    }
};

/// A perspective whose rebuilt layout puts the singleton into a docked folder.
class SingletonPerspective : public SharedPerspective
{
public:
    using SharedPerspective::SharedPerspective;

    void createInitialLayout() override
    {
        SharedPerspective::createInitialLayout();

        singletonFolder = folderContainer()->createFolder(window()->sharedFolder(), qcw::RightDockArea);
        auto viewInfo = getOrCreateView(QString(SingletonViewType));
        if (singletonFolder && viewInfo.first)
        {
            singletonFolder->addTab(viewInfo.first, viewInfo.first->windowTitle(), viewInfo.second);
        }
    }

    QcwTabFolder* singletonFolder { nullptr };
};

/// A perspective that shows the shared view type, reusing another perspective's instance.
class SharingPerspective : public QcwPerspective
{
public:
    using QcwPerspective::QcwPerspective;

    void createInitialLayout() override
    {
        auto container = folderContainer();
        ownFolder = container->createFolder(nullptr, qcw::LeftDockArea);
        container->setCentralFolder(ownFolder);

        auto viewInfo = getOrCreateView(QString(SharedViewType));
        view = viewInfo.first;
        if (ownFolder && view)
        {
            ownFolder->addTab(view, view->windowTitle(), viewInfo.second);
        }
    }

    QcwTabFolder* ownFolder { nullptr };
    QcwView* view { nullptr };
};

void testResetWithMinimizedSharedFolder()
{
    std::fprintf(stderr, "--- reset with a minimized shared folder ---\n");

    TestWindow window;
    window.setId(QStringLiteral("shared-objects-minimized-reset"));
    window.setupUi();
    window.resize(900, 600);

    auto perspective = new SharedPerspective { &window };
    perspective->createInitialLayout();
    window.openPerspective(perspective);
    window.show();
    settle();

    QPointer<QcwTabFolder> shared { window.sharedFolder() };
    check(shared != nullptr, "the window has a shared folder");
    if (!shared)
    {
        return;
    }

    const int tabsBefore = shared->count();
    shared->minimize();
    settle();
    check(shared->state() == qcw::FolderState::Minimized, "the shared folder minimizes");

    perspective->resetLayout();
    settle();

    check(!shared.isNull(), "the reset keeps the shared folder alive");
    check(window.sharedFolder() == shared.data(), "the window still names the same shared folder");
    if (!shared)
    {
        return;
    }

    check(shared->state() == qcw::FolderState::Normal, "the rebuilt layout sees a normal folder");
    check(shared->count() == tabsBefore, "the reset keeps the shared folder's tabs",
          QStringLiteral("tabs=%1").arg(shared->count()));
    check(window.currentPerspective() && window.currentPerspective()->folderContainer()->isAncestorOf(shared),
          "the rebuilt layout hosts the shared folder again");
    check(shared->isVisible(), "the shared folder is visible after the reset");
}

void testResetOfHiddenPerspective()
{
    std::fprintf(stderr, "--- reset of a hidden perspective ---\n");

    TestWindow window;
    window.setId(QStringLiteral("shared-objects-hidden-reset"));
    window.setupUi();
    window.resize(900, 600);

    auto first = new SharedPerspective { &window };
    first->createInitialLayout();
    window.openPerspective(first);

    auto second = new SharedPerspective { &window };
    second->createInitialLayout();
    window.openPerspective(second);

    window.show();
    settle();
    window.setCurrentPerspective(first->id());
    settle();

    QPointer<QcwTabFolder> shared { window.sharedFolder() };
    check(shared && first->folderContainer()->isAncestorOf(shared), "the visible perspective holds the shared folder");
    check(shared && shared->isVisible(), "the shared folder is visible");

    second->resetLayout();
    settle();

    check(shared && first->folderContainer()->isAncestorOf(shared),
          "the reset of a hidden perspective leaves the folder with the visible one");
    check(shared && shared->isVisible(), "the shared folder is still visible");
}

void testResetKeepsSingletonTheLayoutDoesNotShow()
{
    std::fprintf(stderr, "--- reset keeps a singleton the layout does not show ---\n");

    TestWindow window;
    window.setId(QStringLiteral("shared-objects-singleton-reset"));
    window.setupUi();
    window.resize(900, 600);

    auto perspective = new SharedPerspective { &window };
    perspective->createInitialLayout();
    window.openPerspective(perspective);
    window.show();
    settle();

    auto shared = window.sharedFolder();
    auto creator = pluginManager->getPluginT<QcwViewCreator>(QString(SingletonViewType));
    auto view = creator ? creator->create(nullptr, true) : nullptr;
    check(shared != nullptr && view != nullptr, "the singleton view is created");
    if (!shared || !view)
    {
        return;
    }

    shared->addTab(view, QString(SingletonViewType), creator->defaultViewIcon());
    QPointer<QcwView> viewGuard { view };
    settle();
    check(shared->indexOf(view) != -1, "the shared folder shows the singleton");

    // The rebuilt layout does not mention the singleton. It lives in the window's shared
    // folder, which this perspective only borrows, so the reset has to leave it alone.
    perspective->resetLayout();
    settle();

    check(!viewGuard.isNull(), "the reset does not destroy the singleton");
    check(!viewGuard.isNull() && shared->indexOf(viewGuard.data()) != -1,
          "the shared folder keeps the singleton's tab");
    check(shared->count() == 2, "the shared folder keeps all its tabs",
          QStringLiteral("tabs=%1").arg(shared->count()));

    // The rebuilt layout that does show the singleton keeps one tab for it: the copy in the
    // shared folder goes, because the docked folder now holds the same view.
    auto singleton = new SingletonPerspective { &window };
    singleton->createInitialLayout();
    window.openPerspective(singleton);
    settle();

    singleton->resetLayout();
    settle();

    check(!viewGuard.isNull(), "the reset keeps the singleton alive in its rebuilt folder");
    check(singleton->singletonFolder && singleton->singletonFolder->indexOf(viewGuard.data()) != -1,
          "the rebuilt folder holds the singleton");
    check(shared->count() == 1, "the duplicate tab in the shared folder is dropped",
          QStringLiteral("tabs=%1").arg(shared->count()));
}

void testClosePerspectiveLeavesReusedViewAlone()
{
    std::fprintf(stderr, "--- closing a perspective that reuses a view ---\n");

    QcwWindow window;
    window.setId(QStringLiteral("shared-objects-reused-view"));
    window.setupUi();
    window.resize(900, 600);

    auto first = new SharingPerspective { &window };
    first->createInitialLayout();
    window.openPerspective(first);

    auto second = new SharingPerspective { &window };
    second->createInitialLayout();
    window.openPerspective(second);

    window.show();
    settle();

    QPointer<QcwPerspective> firstGuard { first };
    QPointer<QcwView> viewGuard { first->view };
    check(first->view && first->view == second->view, "both perspectives show the same view");
    check(!viewGuard.isNull() && tabPaneManager->getReferences(viewGuard.data()).size() == 2,
          "the view's pane is referenced by both folders");

    gCloseAsks = 0;
    window.closePerspective(first->id());
    settle();

    check(gCloseAsks == 0, "the close does not ask a view it leaves open",
          QStringLiteral("asked=%1").arg(gCloseAsks));
    check(firstGuard.isNull(), "the perspective is removed");
    check(!viewGuard.isNull(), "the reused view stays open");
    check(!viewGuard.isNull() && !tabPaneManager->getReferences(viewGuard.data()).isEmpty(),
          "the other perspective still holds the view's tab");
}

void testDetachedFolderCloseProtocol()
{
    std::fprintf(stderr, "--- detached folder close protocol ---\n");

    QcwWindow window;
    window.setId(QStringLiteral("shared-objects-detached-close"));
    window.setupUi();
    window.resize(900, 600);
    window.show();
    settle();

    // The folder's own close button.
    auto folder = window.createDetachedView(QString(SharedViewType), QStringLiteral("Detached"));
    QPointer<QcwTabFolder> folderGuard { folder };
    QPointer<QcwView> viewGuard { folder ? qobject_cast<QcwView*>(folder->tabPane(0)) : nullptr };
    check(folder && viewGuard, "the detached folder shows a view");
    if (!folder || !viewGuard)
    {
        return;
    }

    gCloseAsks = 0;
    folder->close();
    settle();

    check(gCloseAsks == 1, "the detached folder's close button asks its view",
          QStringLiteral("asked=%1").arg(gCloseAsks));
    check(!folderGuard.isNull(), "a refusal keeps the detached folder open");
    check(!viewGuard.isNull(), "the refusing view survives");

    // The close button of a tab inside a detached folder.
    auto plainFolder = window.createDetachedView(QString(FloatingViewType), QStringLiteral("DetachedPlain"));
    QPointer<QcwTabFolder> plainFolderGuard { plainFolder };
    QPointer<QcwView> plainViewGuard { plainFolder ? qobject_cast<QcwView*>(plainFolder->tabPane(0)) : nullptr };
    check(plainFolder && plainViewGuard, "a second detached folder shows its view");
    if (!plainFolder || !plainViewGuard)
    {
        return;
    }

    plainFolder->tabCloseRequested(0);
    settle();

    check(plainFolderGuard.isNull(), "the window removes the tab and the emptied detached folder");
    check(plainViewGuard.isNull(), "the closed tab's view goes with it");
}

void testSwitchWhileSharedFolderMinimized()
{
    std::fprintf(stderr, "--- switching perspectives with a minimized shared folder ---\n");

    TestWindow window;
    window.setId(QStringLiteral("shared-objects-minimized-switch"));
    window.setupUi();
    window.resize(900, 600);

    auto first = new SharedPerspective { &window };
    first->createInitialLayout();
    window.openPerspective(first);

    auto second = new SharedPerspective { &window };
    second->createInitialLayout();
    window.openPerspective(second);

    window.show();
    settle();
    window.setCurrentPerspective(first->id());
    settle();

    QPointer<QcwTabFolder> sharedGuard { window.sharedFolder() };
    check(sharedGuard != nullptr, "the window has a shared folder");
    if (!sharedGuard)
    {
        return;
    }

    sharedGuard->minimize();
    settle();
    check(sharedGuard->state() == qcw::FolderState::Minimized, "the shared folder minimizes in the first perspective");
    check(!sharedGuard->features().testFlag(qcw::Minimizable), "a parked folder cannot be minimized a second time");

    window.setCurrentPerspective(second->id());
    settle();

    check(!sharedGuard.isNull(), "switching keeps the shared folder alive");
    if (!sharedGuard)
    {
        return;
    }

    check(sharedGuard->state() == qcw::FolderState::Normal, "the second perspective gets a normal shared folder",
          QStringLiteral("state=%1").arg(std::to_underlying(sharedGuard->state())));
    check(sharedGuard->features().testFlag(qcw::Minimizable),
          "the second perspective can minimize the shared folder again");
    check(second->folderContainer()->isAncestorOf(sharedGuard),
          "the shared folder is hosted by the second perspective");
    check(window.findChildren<QcwFastViewToolBar*>().isEmpty(),
          "the first perspective's fast view is discarded with the minimized state");

    // The same trip back: the folder minimized in the second perspective returns normal.
    sharedGuard->minimize();
    settle();
    check(sharedGuard->state() == qcw::FolderState::Minimized, "the shared folder minimizes in the second perspective");

    window.setCurrentPerspective(first->id());
    settle();
    check(!sharedGuard.isNull() && sharedGuard->state() == qcw::FolderState::Normal,
          "the first perspective gets a normal shared folder back");
    check(!sharedGuard.isNull() && first->folderContainer()->isAncestorOf(sharedGuard),
          "the shared folder is hosted by the first perspective again");
}

void testClosePerspectiveReleasesSharedViewInFloatingFolder()
{
    std::fprintf(stderr, "--- closing a perspective with a shared view floating ---\n");

    QcwWindow window;
    window.setId(QStringLiteral("shared-objects-floating-shared"));
    window.setupUi();
    window.resize(900, 600);

    auto first = new SharingPerspective { &window };
    first->createInitialLayout();
    window.openPerspective(first);

    auto second = new SharingPerspective { &window };
    second->createInitialLayout();
    window.openPerspective(second);

    window.show();
    settle();

    auto shared = second->view;
    auto folder = window.createDetachedView(QString(FloatingViewType), QStringLiteral("Floating"));
    QPointer<QcwTabFolder> folderGuard { folder };
    QPointer<QcwView> viewGuard { shared };
    check(folder && shared && second->ownFolder, "the detached folder and the shared view exist");
    if (!folder || !shared || !second->ownFolder)
    {
        return;
    }

    // The folder belongs to the current perspective, which is the second one; moving the
    // shared tab into it leaves the first perspective's tab as the only other reference.
    int indexInAdopter = -1;
    second->ownFolder->takeTab(folder, second->ownFolder->indexOf(shared), indexInAdopter);
    settle();
    check(folder->indexOf(shared) != -1 && tabPaneManager->getReferences(shared).size() == 2,
          "the shared view floats with the second perspective");

    gCloseAsks = 0;
    window.closePerspective(second->id());
    settle();

    check(gCloseAsks == 0, "closing the perspective does not ask the view it leaves open",
          QStringLiteral("asked=%1").arg(gCloseAsks));
    check(folderGuard.isNull(), "the floating folder closes with its perspective");
    check(!viewGuard.isNull(), "the shared view survives");
    check(!viewGuard.isNull() && first->ownFolder && first->ownFolder->indexOf(shared) != -1,
          "the other perspective still shows it");
}

void testWindowDestructionClosesFloatingFolders()
{
    std::fprintf(stderr, "--- destroying the window closes its floating folders ---\n");

    QPointer<QcwTabFolder> folderGuard;
    QPointer<QcwView> viewGuard;

    auto window = new QcwWindow;
    window->setId(QStringLiteral("shared-objects-floating-teardown"));
    window->setupUi();
    window->resize(700, 500);
    window->show();
    settle();

    auto folder = window->createDetachedView(QString(FloatingViewType), QStringLiteral("Floating"));
    check(folder != nullptr, "the detached view is created");
    if (folder)
    {
        folder->resize(400, 300);
        folder->show();
        settle();

        folderGuard = folder;
        viewGuard = qobject_cast<QcwView*>(folder->tabPane(0));
    }

    delete window;
    settle();

    check(folderGuard.isNull(), "destroying the window destroys its floating folder");
    check(viewGuard.isNull(), "the floating folder's view goes with it");
}

void testWindowDestructionWithMinimizedSharedFolder()
{
    std::fprintf(stderr, "--- destroying the window with a minimized shared folder ---\n");

    QPointer<QcwTabFolder> sharedGuard;

    auto window = new TestWindow;
    window->setId(QStringLiteral("shared-objects-minimized-teardown"));
    window->setupUi();
    window->resize(800, 600);

    auto perspective = new SharedPerspective { window };
    perspective->createInitialLayout();
    window->openPerspective(perspective);
    window->show();
    settle();

    sharedGuard = window->sharedFolder();
    check(sharedGuard != nullptr, "the window has a shared folder");
    if (sharedGuard)
    {
        sharedGuard->minimize();
        settle();
        check(sharedGuard->state() == qcw::FolderState::Minimized, "the shared folder minimizes");
    }

    delete window;
    settle();

    check(sharedGuard.isNull(), "the window takes the minimized shared folder down with it");
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    pluginManager->registerPlugin(new SharedViewCreator);
    pluginManager->registerPlugin(new SingletonViewCreator);
    pluginManager->registerPlugin(new FloatingViewCreator);

    testResetWithMinimizedSharedFolder();
    testResetOfHiddenPerspective();
    testResetKeepsSingletonTheLayoutDoesNotShow();
    testClosePerspectiveLeavesReusedViewAlone();
    testDetachedFolderCloseProtocol();
    testSwitchWhileSharedFolderMinimized();
    testClosePerspectiveReleasesSharedViewInFloatingFolder();
    testWindowDestructionClosesFloatingFolders();
    testWindowDestructionWithMinimizedSharedFolder();

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    delete pluginManager->removePlugin(QString(SharedViewType));
    delete pluginManager->removePlugin(QString(SingletonViewType));
    delete pluginManager->removePlugin(QString(FloatingViewType));

    return gFailures == 0 ? 0 : 1;
}

#include "perspective_shared_objects_test.moc"
