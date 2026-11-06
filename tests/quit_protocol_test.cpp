// A quit goes through the views like every other close. QApplication abandons a quit whose
// window stays open, so QcwWorkbench answers the quit itself: the first quit is held by an
// asynchronous view and cancelled by its refusal, the second one waits for the approval and
// then ends the event loop with the windows closed.

#include "QcwTabFolder.h"
#include "QcwView.h"
#include "QcwWindow.h"
#include "QcwWorkbench.h"

#include <QApplication>
#include <QStandardPaths>
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

class QuitWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

class QuitWorkbench : public QcwWorkbench
{
public:
    QcwWindow* createWindow() override
    {
        return new QuitWindow;
    }
};

class QuitView : public QcwView
{
public:
    void canClose(const std::function<void(bool)>& callback) override
    {
        ++asks;
        pending = callback;
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

    int asks { 0 };
    std::function<void(bool)> pending;
};
} // namespace

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    QuitWorkbench workbench;
    if (!workbench.initialize(QStringLiteral("qcw-quit-protocol-test"),
                              QStringLiteral("Qcw Quit Protocol Test"), QStringLiteral("qcw.org"),
                              QStringLiteral("qcw"), QStringLiteral("1.0.0"),
                              /*quitOnLastWindowClosed=*/true, /*loadPlugins=*/false))
    {
        std::fprintf(stderr, "initialize failed\n");
        return 2;
    }

    if (!workbench.start())
    {
        std::fprintf(stderr, "start failed\n");
        return 3;
    }

    QcwWindow* window = nullptr;
    for (auto* widget : QApplication::topLevelWidgets())
    {
        if (auto* candidate = qobject_cast<QcwWindow*>(widget))
        {
            window = candidate;
            break;
        }
    }

    check(window != nullptr, "the workbench opened a window");
    if (!window)
    {
        return 1;
    }

    auto* view = new QuitView;
    window->sharedFolder()->addTab(view, QStringLiteral("Deferred"));
    QCoreApplication::processEvents();

    bool refusedStepRan = false;

    // First quit: the view has not answered yet, so nothing may close. Its refusal cancels
    // the quit and the application keeps running.
    QTimer::singleShot(0, &app, &QCoreApplication::quit);
    QTimer::singleShot(50, &app, [&]()
    {
        check(view->asks == 1, "a quit asks the window's views");
        check(window->isVisible(), "the quit waits for the view's answer");

        view->answer(false);

        QTimer::singleShot(100, &app, [&]()
        {
            refusedStepRan = true;
            check(window->isVisible(), "a refused view cancels the quit");
            check(view->asks == 1, "the refused quit asked once");

            // Second quit: the deferred approval lets it through.
            QCoreApplication::quit();

            QTimer::singleShot(100, &app, [&]()
            {
                check(view->asks == 2, "a new quit asks the view again");
                check(window->isVisible(), "the second quit waits for its answer too");

                view->answer(true);
            });
        });
    });

    // A quit that never resolves must fail the test instead of hanging it.
    QTimer::singleShot(5000, &app, [&]()
    {
        check(false, "the quit sequence completes");
        QCoreApplication::exit(1);
    });

    const int result = app.exec();

    check(result == 0, "the approved quit ends the event loop normally");
    check(refusedStepRan, "the refused quit did not end the event loop");
    check(!window->isVisible(), "the approved quit closed the window");
    check(view->asks == 2, "the window was asked once per quit");

    std::fprintf(stderr, "failures=%d\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
