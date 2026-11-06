// The close protocol makes every user close ask the views first. A view that refuses keeps
// its tab, its perspective, its floating folder or the window alive; a view that answers
// later holds the close open until it does; one refusal stops the asks and cancels the whole
// batch a Close Others/All produced. The shared folder belongs to the window, not to a perspective, so its views are
// not asked when a perspective closes. forceClose() is the one path that does not ask.

#include "QcwFloatingFolderMonitor.h"
#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <cstdio>
#include <functional>

namespace
{
int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

void settle()
{
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

/// A perspective with the shared folder at its centre and one kept folder of its own.
class TestPerspective : public QcwPerspective
{
public:
    explicit TestPerspective(QcwWindow* window) :
        QcwPerspective { window }
    {
    }

    void createInitialLayout() override
    {
        auto* container = folderContainer();
        auto* central = window()->sharedFolder();
        if (central)
        {
            container->addSharedFolder(central, nullptr, qcw::RightDockArea);
        }
        else
        {
            central = container->createFolder(nullptr, qcw::LeftDockArea);
        }
        container->setCentralFolder(central);

        mFolder = container->createFolder(central, qcw::LeftDockArea);
        container->setEmptyFolderAction(mFolder, qcw::EmptyFolderAction::Keep);
    }

    QcwTabFolder* folder() const
    {
        return mFolder;
    }

private:
    QcwTabFolder* mFolder { nullptr };
};

class TestView : public QcwView
{
public:
    enum class Mode
    {
        Allow,
        Refuse,
        Defer
    };

    explicit TestView(Mode mode = Mode::Allow) :
        mode(mode)
    {
    }

    void canClose(const std::function<void(bool)>& callback) override
    {
        ++asks;
        ++totalAsks;

        switch (mode)
        {
            case Mode::Allow:
                callback(true);
                break;
            case Mode::Refuse:
                callback(false);
                break;
            case Mode::Defer:
                pending = callback;
                break;
        }
    }

    void answer(bool allowed)
    {
        auto callback = std::move(pending);
        pending = {};
        if (callback)
        {
            callback(allowed);
        }
    }

    Mode mode { Mode::Allow };
    int asks { 0 };
    std::function<void(bool)> pending;

    /// Survives the view, so a test can count questions after a close deleted it.
    static int totalAsks;
};

int TestView::totalAsks = 0;

/// A shown window whose shared folder already carries @p views.
TestWindow* makeWindow(const char* id)
{
    auto* window = new TestWindow;
    window->setId(QString::fromLatin1(id));
    window->setSaveStateOnClose(false);
    window->setupUi();
    window->resize(800, 500);
    window->show();
    settle();
    return window;
}

void testTabClose()
{
    TestView::totalAsks = 0;

    TestWindow* window = makeWindow("close-protocol-tabs");
    auto* folder = window->sharedFolder();

    auto* allow = new TestView { TestView::Mode::Allow };
    auto* refuse = new TestView { TestView::Mode::Refuse };
    auto* deferred = new TestView { TestView::Mode::Defer };
    folder->addTab(allow, QStringLiteral("Allow"));
    folder->addTab(refuse, QStringLiteral("Refuse"));
    folder->addTab(deferred, QStringLiteral("Deferred"));
    settle();

    // Close a background tab: every view is asked, not only the current one.
    QPointer<TestView> allowGuard { allow };
    folder->tabCloseRequested(folder->indexOf(allow));
    settle();
    check(TestView::totalAsks == 1, "closing a background tab asks its view");
    check(allowGuard.isNull(), "an approved tab closes");
    check(folder->count() == 2, "the approved tab is gone");

    folder->tabCloseRequested(folder->indexOf(refuse));
    settle();
    check(refuse->asks == 1, "a refusing view is asked");
    check(folder->indexOf(refuse) >= 0, "a refused tab stays open");

    folder->tabCloseRequested(folder->indexOf(deferred));
    settle();
    check(deferred->asks == 1, "a view may answer later");
    check(folder->indexOf(deferred) >= 0, "the tab stays while the answer is outstanding");

    QPointer<TestView> deferredGuard { deferred };
    deferred->answer(true);
    settle();
    check(deferredGuard.isNull(), "the tab closes after the deferred approval");

    // Close All emits one request per tab, descending. The batch is one close: the views
    // are asked in order, the first refusal stops the asks, and a refusal cancels the whole
    // batch — a tab approved before the refusal stays open too.
    auto* third = new TestView { TestView::Mode::Allow };
    auto* second = new TestView { TestView::Mode::Refuse };
    auto* first = new TestView { TestView::Mode::Allow };
    folder->addTab(first, QStringLiteral("First"));
    folder->addTab(second, QStringLiteral("Second"));
    folder->addTab(third, QStringLiteral("Third"));
    settle();

    const int askedBeforeBatch = TestView::totalAsks;
    QPointer<TestView> thirdGuard { third };
    folder->tabCloseRequested(folder->indexOf(third));
    folder->tabCloseRequested(folder->indexOf(second));
    folder->tabCloseRequested(folder->indexOf(first));
    settle();

    check(TestView::totalAsks == askedBeforeBatch + 2, "the batch asks until the first refusal");
    check(!thirdGuard.isNull() && folder->indexOf(third) >= 0,
          "a refusal cancels the whole batch, including tabs approved before it");
    check(folder->indexOf(second) >= 0 && folder->indexOf(first) >= 0,
          "the refused tab and the ones after it stay");
    check(folder->count() == 4, "a refused batch leaves every tab open");

    // Window close: the refused view keeps the window; once it agrees the window closes.
    bool answered = false;
    bool allowed = true;
    window->canClose([&answered, &allowed](bool ok)
    {
        answered = true;
        allowed = ok;
    });
    settle();
    check(answered && !allowed, "QcwWindow::canClose reports a refusal");
    check(window->isVisible(), "asking does not close the window");

    window->close();
    settle();
    check(window->isVisible(), "a refused view keeps the window open");

    refuse->mode = TestView::Mode::Allow;
    second->mode = TestView::Mode::Allow;
    window->close();
    settle();
    check(!window->isVisible(), "the window closes once its views agree");

    delete window;
    settle();
}

void testDeferredWindowClose()
{
    TestWindow* window = makeWindow("close-protocol-deferred-window");
    auto* view = new TestView { TestView::Mode::Defer };
    window->sharedFolder()->addTab(view, QStringLiteral("Deferred"));
    settle();

    window->close();
    settle();
    check(view->asks == 1, "closing the window asks its view");
    check(window->isVisible(), "a deferred answer keeps the window open");

    view->answer(false);
    settle();
    check(window->isVisible(), "a deferred refusal keeps the window open");

    window->close();
    settle();
    check(view->asks == 2, "a second close asks again");
    check(window->isVisible(), "the second close waits for its answer");

    view->answer(true);
    settle();
    check(!window->isVisible(), "an approved deferred answer closes the window");

    delete window;
    settle();
}

void testPerspectiveClose()
{
    TestWindow* window = makeWindow("close-protocol-perspective");
    auto* sharedView = new TestView { TestView::Mode::Refuse };
    window->sharedFolder()->addTab(sharedView, QStringLiteral("Shared"));
    settle();

    auto* perspective = new TestPerspective { window };
    perspective->createInitialLayout();
    auto* ownView = new TestView { TestView::Mode::Refuse };
    perspective->folder()->addTab(ownView, QStringLiteral("Own"));
    window->openPerspective(perspective);
    settle();

    window->closePerspective(perspective->id());
    settle();
    check(ownView->asks == 1, "closing a perspective asks the views it owns");
    check(sharedView->asks == 0, "the shared folder's views are not asked");
    check(window->allPerspectives().contains(perspective), "a refused view keeps the perspective");

    QPointer<QcwPerspective> perspectiveGuard { perspective };
    QPointer<TestView> ownViewGuard { ownView };
    ownView->mode = TestView::Mode::Allow;
    window->closePerspective(perspective->id());
    settle();
    check(perspectiveGuard.isNull(), "an approved perspective close destroys the perspective");
    check(ownViewGuard.isNull(), "its views go with it");
    check(sharedView->asks == 0, "the shared folder's views were never asked");

    delete window;
    settle();
}

void testFloatingFolderClose()
{
    TestWindow* window = makeWindow("close-protocol-floating");

    auto* floating = new QcwTabFolder;
    floating->setAttribute(Qt::WA_DeleteOnClose);
    floating->setProperty("WindowId", window->id());
    floating->setFloating(true);

    auto* view = new TestView { TestView::Mode::Refuse };
    floating->addTab(view, QStringLiteral("Floating"));
    floating->resize(400, 300);
    floating->show();
    settle();

    // The monitor announcement is what a real detach emits; the window registers the folder
    // and installs its close handler from it.
    floatingFolderMonitor->floatingFolderCreated(floating);
    settle();

    QPointer<QcwTabFolder> floatingGuard { floating };
    floating->close();
    settle();
    check(!floatingGuard.isNull() && floatingGuard->isVisible(), "a refused view keeps the floating folder");
    check(view->asks == 1, "the floating folder's view was asked");

    view->mode = TestView::Mode::Allow;
    floating->close();
    settle();
    check(floatingGuard.isNull() || !floatingGuard->isVisible(), "an approved close closes the floating folder");

    // A folder without DeleteOnClose survives its close and can be shown again; the
    // synchronous approval must not leave a confirmation behind that would let the next
    // close of the same folder skip the question.
    auto* reusable = new QcwTabFolder;
    reusable->setProperty("WindowId", window->id());
    reusable->setFloating(true);
    auto* reusableView = new TestView { TestView::Mode::Allow };
    reusable->addTab(reusableView, QStringLiteral("Reusable"));
    reusable->resize(400, 300);
    reusable->show();
    settle();
    floatingFolderMonitor->floatingFolderCreated(reusable);
    settle();

    reusable->close();
    settle();
    check(!reusable->isVisible(), "an approved close hides a floating folder without DeleteOnClose");
    check(reusableView->asks == 1, "the reusable folder's view was asked");

    reusable->show();
    settle();
    reusable->close();
    settle();
    check(reusableView->asks == 2, "a later close of the same folder asks again");
    check(!reusable->isVisible(), "the later close hides it again");

    delete reusable;
    settle();

    delete window;
    settle();
}

void testForceClose()
{
    TestWindow* window = makeWindow("close-protocol-force");
    auto* view = new TestView { TestView::Mode::Refuse };
    window->sharedFolder()->addTab(view, QStringLiteral("Refuse"));
    settle();

    window->forceClose();
    settle();
    check(view->asks == 0, "forceClose does not ask");
    check(!window->isVisible(), "forceClose closes the window");

    delete window;
    settle();
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    testTabClose();
    testDeferredWindowClose();
    testPerspectiveClose();
    testFloatingFolderClose();
    testForceClose();

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return gFailures == 0 ? 0 : 1;
}
