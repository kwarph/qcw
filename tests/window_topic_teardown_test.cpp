// A view that caches its owning window and unsubscribes from the window's topic service when
// a folder releases the pane cannot do that on the ParentChange itself: ~QcwWindow deletes the
// window's private before QWidget destroys the tree, and the releasePane setParent that emits
// that ParentChange happens during the teardown - the demo's OutlineView hit exactly that use
// after free. aboutToBeDestroyed() fires while every accessor still works, giving the view one
// clean moment to drop the window; QcwView::subscribeWindowTopic() relies on the same signal so
// that a view need never do this itself. This test keeps the signal contract: the view must
// unsubscribe once, in the signal, and never again afterwards.

#include "QcwTabFolder.h"
#include "QcwTopicService.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <cstdio>

namespace
{
constexpr QLatin1String SelectionTopic { "win/test/selection" };

int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

/// The OutlineView pattern in miniature: cache the window on ParentChange and Show, subscribe
/// to its topics, and drop everything through aboutToBeDestroyed().
class CachedWindowView : public QcwView
{
public:
    explicit CachedWindowView(QWidget* parent = nullptr) : QcwView { parent }
    {
    }

    int unsubscribes { 0 };
    int unsubscribeCallsWithValidService { 0 };
    int forgets { 0 };
    QcwWindow* window { nullptr };

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ParentChange || event->type() == QEvent::Show)
        {
            connectToWindow();
        }

        return QcwView::event(event);
    }

private:
    void connectToWindow()
    {
        auto* owner = owningWindow();
        if (owner == window)
        {
            return;
        }

        if (window)
        {
            disconnect(window, &QcwWindow::aboutToBeDestroyed, this, &CachedWindowView::forgetWindow);
            dropWindow();
        }

        window = owner;
        if (!window)
        {
            return;
        }

        connect(window, &QcwWindow::aboutToBeDestroyed, this, &CachedWindowView::forgetWindow,
                Qt::UniqueConnection);
        window->topicService()->subscribe(QString(SelectionTopic), this,
                                          [this](const QVariantMap&) {});
    }

    void forgetWindow()
    {
        ++forgets;
        dropWindow();
    }

    void dropWindow()
    {
        ++unsubscribes;
        if (window->topicService())
        {
            ++unsubscribeCallsWithValidService;
        }

        window->topicService()->unsubscribe(QString(SelectionTopic), this);
        window = nullptr;
    }
};
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QPointer<CachedWindowView> view;
    {
        TestWindow window;
        window.setupUi();
        window.resize(800, 500);
        window.show();

        view = new CachedWindowView;
        window.sharedFolder()->addTab(view, QStringLiteral("Outline"));
        QCoreApplication::processEvents();

        check(view->window == &window, "the view cached the window on attach");
        check(view->forgets == 0 && view->unsubscribes == 0, "nothing dropped while the window lives");
    }
    // The destructor above emitted aboutToBeDestroyed() before its body deleted the private,
    // and only then did QWidget tear down the folder that released this pane with a
    // setParent(nullptr) - the ParentChange that used to reach the deleted private.

    check(view->forgets == 1, "the view was told about the destruction exactly once");
    check(view->unsubscribes == 1, "the view unsubscribed exactly once");
    check(view->unsubscribeCallsWithValidService == 1, "that unsubscribe saw a live topic service");
    check(view->window == nullptr, "the window cache is gone");

    // The released pane waits for a deferred delete; deliver it like ~QcwWorkbench does.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
    check(view.isNull(), "the pane was deleted after the window");

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return gFailures == 0 ? 0 : 1;
}
