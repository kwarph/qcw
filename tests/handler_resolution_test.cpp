// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Exercises QcwWindow::resolveHandler() and executeCommand() at each scope: view,
// perspective, window and workbench. Also checks that a handler's accepts() is consulted,
// that the current view's handler wins over the others, and that the command QAction a window
// hands its menus and tool bars carries that window's id.

#include "QcwCommandHandler.h"
#include "QcwCommandRegistry.h"
#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"
#include "QcwWorkbench.h"

#include <QAction>
#include <QApplication>
#include <QStringList>

#include <cstdio>

namespace
{

constexpr QLatin1String CommandId { "Test/DoThing" };
constexpr QLatin1String WindowId { "handler-resolution-test" };

int gFailures = 0;

void check(bool ok, const char* what, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!ok)
    {
        ++gFailures;
    }
}

/// A handler that records the parameters it was asked to run with.
class RecordingHandler : public QcwCommandHandler
{
public:
    bool enabled { true };
    bool acceptsAll { true };
    int executeCount { 0 };
    QVariantMap lastParams;

    bool isEnabled() const override
    {
        return enabled;
    }

    bool accepts(const Params& params) const override
    {
        return acceptsAll || params.contains(QStringLiteral("expected"));
    }

    void execute(const Params& params) override
    {
        ++executeCount;
        lastParams = params;
    }
};

class TestView : public QcwView
{
public:
    explicit TestView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
    }

    QHash<QString, QcwCommandHandler*> commandHandlers() const override
    {
        return handlers;
    }

    void setHandler(const QString& commandId, QcwCommandHandler* handler)
    {
        if (handler)
        {
            handlers.insert(commandId, handler);
        }
        else
        {
            handlers.remove(commandId);
        }
    }

private:
    QHash<QString, QcwCommandHandler*> handlers;
};

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

class TestWorkbench : public QcwWorkbench
{
public:
    QcwWindow* createWindow() override
    {
        return new TestWindow;
    }
};

class TestPerspective : public QcwPerspective
{
public:
    explicit TestPerspective(QcwWindow* window) :
        QcwPerspective(window)
    {
    }

    void createInitialLayout() override
    {
        auto container = folderContainer();
        auto central = window()->sharedFolder();
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
    }

    QcwTabFolder* folder() const
    {
        return mFolder;
    }

private:
    QcwTabFolder* mFolder { nullptr };
};

void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
    }
}

void testNoHandler()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    check(window.resolveHandler(QString(CommandId)) == nullptr, "no handler registered: resolveHandler is null");
    check(!window.executeCommand(QString(CommandId)), "executeCommand returns false with no handler");
    check(!window.isCommandEnabled(QString(CommandId)), "isCommandEnabled returns false with no handler");
}

void testWindowHandler()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    RecordingHandler handler;
    window.registerHandler(QString(CommandId), &handler);

    check(window.resolveHandler(QString(CommandId)) == &handler, "window handler resolves");
    check(window.isCommandEnabled(QString(CommandId)), "window handler is enabled");

    QVariantMap params;
    params.insert(QStringLiteral("value"), 42);
    check(window.executeCommand(QString(CommandId), params), "executeCommand runs the window handler");
    check(handler.executeCount == 1, "the handler ran once", QString::number(handler.executeCount));
    check(handler.lastParams.value(QStringLiteral("value")).toInt() == 42, "the handler received the parameters");

    window.unregisterHandler(QString(CommandId));
    check(window.resolveHandler(QString(CommandId)) == nullptr, "unregisterHandler drops the window handler");
}

void testWorkbenchHandler()
{
    TestWorkbench workbench;
    TestWindow window;
    window.setId(WindowId);
    window.setWorkbench(&workbench);
    window.setupUi();
    settle();

    auto* action = window.commandAction(QString(CommandId));
    check(action != nullptr, "the command has an action");
    check(action && !action->isEnabled(), "an unhandled command action starts greyed out");

    RecordingHandler global;
    workbench.registerGlobalHandler(QString(CommandId), &global);

    check(window.resolveHandler(QString(CommandId)) == &global, "global handler resolves when no other scope has one");
    check(action && action->isEnabled(), "registering the global handler enables its command action");
    check(window.executeCommand(QString(CommandId)), "executeCommand runs the global handler");
    check(global.executeCount == 1, "the global handler ran once", QString::number(global.executeCount));

    workbench.unregisterGlobalHandler(QString(CommandId));
    check(window.resolveHandler(QString(CommandId)) == nullptr, "unregisterGlobalHandler drops the global handler");
    check(action && !action->isEnabled(), "unregistering the global handler greys the command action out again");

    auto* transient = new RecordingHandler;
    workbench.registerGlobalHandler(QString(CommandId), transient);
    check(action && action->isEnabled(), "a new global handler enables the command action again");

    delete transient;
    check(window.resolveHandler(QString(CommandId)) == nullptr, "a destroyed global handler drops itself");
    check(action && !action->isEnabled(), "destroying the global handler greys the command action out again");
}

void testCommandActionAddressesItsWindow()
{
    TestWorkbench workbench;

    TestWindow first;
    first.setId(QStringLiteral("handler-resolution-first"));
    first.setWorkbench(&workbench);
    first.setupUi();

    TestWindow second;
    second.setId(QStringLiteral("handler-resolution-second"));
    second.setWorkbench(&workbench);
    second.setupUi();
    settle();

    RecordingHandler global;
    workbench.registerGlobalHandler(QString(CommandId), &global);

    auto* firstAction = first.commandAction(QString(CommandId));
    auto* secondAction = second.commandAction(QString(CommandId));
    check(firstAction && secondAction, "each window has its own action for the command");
    if (!firstAction || !secondAction)
    {
        return;
    }

    // Running a command from a window's own menu item or tool bar button has to say which window
    // ran it: a process-wide handler chooses between the open windows by that id. A call without it
    // answers nothing, which is how a menu entry could look dead while the palette ran the same
    // command — the palette asks the window it is open in, which needs no such key.
    firstAction->trigger();
    check(global.executeCount == 1, "triggering the action runs the global handler",
          QString::number(global.executeCount));
    check(global.lastParams.value(QString(QcwWindowActionIds::ParamWindowId)) == first.id(),
          "the action of the first window names that window",
          global.lastParams.value(QString(QcwWindowActionIds::ParamWindowId)).toString());

    secondAction->trigger();
    check(global.lastParams.value(QString(QcwWindowActionIds::ParamWindowId)) == second.id(),
          "the action of the second window names that window instead",
          global.lastParams.value(QString(QcwWindowActionIds::ParamWindowId)).toString());
}

void testPerspectiveOverridesWindow()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    RecordingHandler windowHandler;
    RecordingHandler perspectiveHandler;
    window.registerHandler(QString(CommandId), &windowHandler);

    auto* perspective = new TestPerspective { &window };
    perspective->createInitialLayout();
    perspective->registerHandler(QString(CommandId), &perspectiveHandler);
    window.openPerspective(perspective);
    settle();

    check(window.resolveHandler(QString(CommandId)) == &perspectiveHandler,
          "perspective handler wins over the window handler");

    window.executeCommand(QString(CommandId));
    check(perspectiveHandler.executeCount == 1 && windowHandler.executeCount == 0,
          "the perspective handler ran, the window handler did not");
}

void testViewOverridesAll()
{
    TestWindow window;
    window.setId(WindowId);
    window.setWorkbench(nullptr);
    window.setupUi();
    settle();

    RecordingHandler windowHandler;
    RecordingHandler perspectiveHandler;
    RecordingHandler viewHandler;

    window.registerHandler(QString(CommandId), &windowHandler);

    auto* perspective = new TestPerspective { &window };
    perspective->createInitialLayout();
    perspective->registerHandler(QString(CommandId), &perspectiveHandler);
    window.openPerspective(perspective);
    settle();

    auto* view = new TestView { perspective->folder() };
    view->setHandler(QString(CommandId), &viewHandler);
    perspective->folder()->addTab(view, QStringLiteral("View"), {});

    auto* currentFolder = perspective->currentFolder();
    check(currentFolder != nullptr, "a current folder exists",
          currentFolder ? currentFolder->objectName() : QStringLiteral("none"));

    // The view needs to be current for its handler to win.
    perspective->activateFolder(perspective->folder());
    settle();

    check(window.resolveHandler(QString(CommandId)) == &viewHandler,
          "view handler wins over perspective and window handlers");

    window.executeCommand(QString(CommandId));
    check(viewHandler.executeCount == 1 && perspectiveHandler.executeCount == 0 && windowHandler.executeCount == 0,
          "only the view handler ran");
}

void testAcceptsRejectsParameters()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    RecordingHandler handler;
    handler.acceptsAll = false;
    window.registerHandler(QString(CommandId), &handler);

    check(!window.executeCommand(QString(CommandId)),
          "executeCommand returns false when accepts() rejects the parameters");
    check(handler.executeCount == 0, "the handler did not run");

    QVariantMap params;
    params.insert(QStringLiteral("expected"), QStringLiteral("yes"));
    check(window.executeCommand(QString(CommandId), params), "executeCommand runs when accepts() is satisfied");
    check(handler.executeCount == 1, "the handler ran once");
}

void testDisabledHandler()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    RecordingHandler handler;
    handler.enabled = false;
    window.registerHandler(QString(CommandId), &handler);

    check(!window.isCommandEnabled(QString(CommandId)), "a disabled handler makes the command disabled");
    check(!window.executeCommand(QString(CommandId)), "executeCommand refuses a disabled handler");
    check(handler.executeCount == 0, "the handler did not run");
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    commandRegistry->registerCommand({
        QString(CommandId),                      // id
        QStringLiteral("Test"),                  // category
        QStringLiteral("Do Thing"),              // label
        QStringLiteral("Runs the test command"), // description
        QKeySequence(),                          // defaultShortcut
        {},                                      // icon
        false,                                   // checkable
    });

    testNoHandler();
    testWindowHandler();
    testWorkbenchHandler();
    testCommandActionAddressesItsWindow();
    testPerspectiveOverridesWindow();
    testViewOverridesAll();
    testAcceptsRejectsParameters();
    testDisabledHandler();

    commandRegistry->unregisterCommand(QString(CommandId));

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
