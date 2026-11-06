// A quit with no window left — Remove Window closed the last one — has nothing to ask and
// nothing to close, and must still end the event loop instead of being swallowed by the
// workbench's quit filter.

#include "QcwWindow.h"
#include "QcwWorkbench.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>

namespace
{
class EmptyWindow : public QcwWindow
{
};

class EmptyWorkbench : public QcwWorkbench
{
public:
    QcwWindow* createWindow() override
    {
        return new EmptyWindow;
    }
};
} // namespace

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    EmptyWorkbench workbench;
    if (!workbench.initialize(QStringLiteral("qcw-quit-without-windows-test"),
                              QStringLiteral("Qcw Quit Without Windows Test"), QStringLiteral("qcw.org"),
                              QStringLiteral("qcw"), QStringLiteral("1.0.0"),
                              /*quitOnLastWindowClosed=*/true, /*loadPlugins=*/false))
    {
        std::fprintf(stderr, "initialize failed\n");
        return 2;
    }

    // start() is what creates and tracks windows; skipping it leaves the workbench in the
    // state Remove Window leaves behind when it removes the last one.
    bool quitAsked = false;
    QTimer::singleShot(0, &app, [&quitAsked]()
    {
        quitAsked = true;
        QCoreApplication::quit();
    });

    // A swallowed quit would leave the loop running; fail the test instead of hanging it.
    QTimer::singleShot(5000, &app, [&app]()
    {
        std::fprintf(stderr, "FAIL the quit never ended the event loop\n");
        QCoreApplication::exit(1);
    });

    const bool ok = app.exec() == 0 && quitAsked;
    std::fprintf(stderr, "%s a quit with no window ends the event loop\n", ok ? "ok  " : "FAIL");

    return ok ? 0 : 1;
}
