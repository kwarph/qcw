// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Exercises QcwWindow::commandAction(): creation on first use and eagerly for commands that
// carry a default shortcut, property sources, state refresh on handler stateChanged(),
// shortcut triggering, and the null return for unregistered commands.

#include "QcwCommandHandler.h"
#include "QcwCommandRegistry.h"
#include "QcwTabFolder.h"
#include "QcwView.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QPointer>

#include <cstdio>

namespace
{

constexpr QLatin1String CommandId { "Test/Toggle" };
constexpr QLatin1String UnregisteredId { "Test/Unknown" };
constexpr QLatin1String UiLessCommandId { "Test/UiLess" };
constexpr QLatin1String LateCommandId { "Test/LateShortcut" };
constexpr QLatin1String HiddenCommandId { "Test/Hidden" };
constexpr QLatin1String WindowId { "command-action-test" };

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

class ToggleHandler : public QcwCommandHandler
{
public:
    bool enabled { true };
    bool visible { true };
    bool checked { false };
    int executeCount { 0 };

    bool isEnabled() const override
    {
        return enabled;
    }
    bool isVisible() const override
    {
        return visible;
    }
    bool isChecked() const override
    {
        return checked;
    }
    void setChecked(bool value) override
    {
        checked = value;
    }

    void execute(const Params&) override
    {
        ++executeCount;
    }
};

/// Answers its command, and reports the visibility the test asks for.
class CountingHandler : public QcwCommandHandler
{
public:
    bool visible { true };
    int executeCount { 0 };

    bool isVisible() const override
    {
        return visible;
    }

    void execute(const Params&) override
    {
        ++executeCount;
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

void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
    }
}

void registerToggleCommand()
{
    commandRegistry->registerCommand({
        QString(CommandId),                         // id
        QStringLiteral("Test"),                     // category
        QStringLiteral("Toggle"),                   // label
        QStringLiteral("Toggles the test flag"),    // description
        QKeySequence(QStringLiteral("Ctrl+Alt+T")), // defaultShortcut
        {},                                         // icon
        true,                                       // checkable
    });
}

void testUnregisteredCommandActionIsNull()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    check(window.commandAction(QString(UnregisteredId)) == nullptr,
          "commandAction returns nullptr for an unregistered command");
}

void testCommandActionProperties()
{
    registerToggleCommand();

    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    auto* action = window.commandAction(QString(CommandId));
    check(action != nullptr, "commandAction creates a QAction");
    if (!action)
    {
        return;
    }

    check(action->text() == QStringLiteral("Toggle"), "the QAction label comes from QcwCommandInfo");
    check(action->isCheckable(), "the QAction is checkable because the command is");
    check(action->shortcut() == QKeySequence(QStringLiteral("Ctrl+Alt+T")), "the QAction carries the default shortcut");
    check(window.commandAction(QString(CommandId)) == action, "commandAction is idempotent");
}

void testHandlerStateRefreshesAction()
{
    registerToggleCommand();

    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    auto* action = window.commandAction(QString(CommandId));
    ToggleHandler handler;
    window.registerHandler(QString(CommandId), &handler);
    settle();

    check(action->isEnabled(), "the QAction is enabled while the handler is enabled");
    check(action->isVisible(), "the QAction is visible while the handler is visible");
    check(!action->isChecked(), "the QAction starts unchecked");

    handler.enabled = false;
    emit handler.stateChanged();
    settle();
    check(!action->isEnabled(), "stateChanged disables the QAction");

    handler.enabled = true;
    handler.visible = false;
    emit handler.stateChanged();
    settle();
    // Qt may report isEnabled() as false while the action is invisible; what matters for
    // the UI is that the action is hidden.
    check(!action->isVisible(), "stateChanged hides the QAction");

    handler.visible = true;
    emit handler.stateChanged();
    settle();
    check(action->isEnabled() && action->isVisible(), "stateChanged re-enables and shows the QAction");

    handler.checked = true;
    emit handler.stateChanged();
    settle();
    check(action->isChecked(), "stateChanged updates the checked state");

    window.unregisterHandler(QString(CommandId));
    settle();
    check(!action->isEnabled(), "unregistering the handler disables the QAction");
}

void testActionTriggerRunsHandler()
{
    registerToggleCommand();

    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    auto* action = window.commandAction(QString(CommandId));
    ToggleHandler handler;
    window.registerHandler(QString(CommandId), &handler);
    settle();

    action->trigger();
    settle();

    check(handler.executeCount == 1, "triggering the QAction runs the handler", QString::number(handler.executeCount));
    check(handler.checked, "the checkable handler received the toggled state");
}

// A command that leaves the catalog loses the QAction that mirrored it, so a contribution
// can no longer be placed against an id nothing declares any more.
void testUnregisteringCommandDeletesItsAction()
{
    registerToggleCommand();

    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    QPointer<QAction> action = window.commandAction(QString(CommandId));
    check(action != nullptr, "the QAction exists while the command is catalogued");

    commandRegistry->unregisterCommand(QString(CommandId));
    settle();

    check(window.commandAction(QString(CommandId)) == nullptr,
          "commandAction stops answering once the command is unregistered");

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(action.isNull(), "the window deletes the QAction of a command it has lost");
}

// A command without any contribution has no menu entry: the QAction the window adds for its
// shortcut is what makes it reachable at all, and an enabled action stays reachable even
// though nothing shows it in a menu.
void testUiLessCommandRunsFromItsShortcut()
{
    commandRegistry->registerCommand({
        QString(UiLessCommandId),                   // id
        QStringLiteral("Test"),                     // category
        QStringLiteral("UI-less"),                  // label
        {},                                         // description
        QKeySequence(QStringLiteral("Ctrl+Alt+Y")), // defaultShortcut
        {},                                         // icon
        false,                                      // checkable
    });

    TestWindow window;
    window.setId(WindowId);
    window.setupUi();

    CountingHandler handler;
    window.registerHandler(QString(UiLessCommandId), &handler);

    window.show();
    window.activateWindow();
    settle();

    // Nobody has asked for the action yet: the shortcut alone has to reach the command.
    QKeyEvent press { QEvent::KeyPress, Qt::Key_Y, Qt::ControlModifier | Qt::AltModifier };
    QApplication::sendEvent(&window, &press);
    settle();

    check(handler.executeCount == 1, "the shortcut of a menu-less command runs its handler",
          QString::number(handler.executeCount));

    auto* action = window.commandAction(QString(UiLessCommandId));
    check(action && action->isVisible() && action->isEnabled(),
          "a UI-less command action is created and stays enabled");

    auto* fileMenu = window.getMenu(QcwWindowActionIds::MenuFile);
    check(fileMenu && !fileMenu->actions().contains(action), "no menu holds the UI-less command");

    window.unregisterHandler(QString(UiLessCommandId));
    commandRegistry->unregisterCommand(QString(UiLessCommandId));
}

// Registering a shortcut-carrying command after the window exists installs the shortcut through
// commandsChanged(), again without anyone asking for the action first.
void testShortcutCommandRegisteredLater()
{
    TestWindow window;
    window.setId(WindowId);
    window.setupUi();
    settle();

    commandRegistry->registerCommand({
        QString(LateCommandId),                     // id
        QStringLiteral("Test"),                     // category
        QStringLiteral("Late shortcut"),            // label
        {},                                         // description
        QKeySequence(QStringLiteral("Ctrl+Alt+W")), // defaultShortcut
        {},                                         // icon
        false,                                      // checkable
    });

    CountingHandler handler;
    window.registerHandler(QString(LateCommandId), &handler);

    window.show();
    window.activateWindow();
    settle();

    QKeyEvent press { QEvent::KeyPress, Qt::Key_W, Qt::ControlModifier | Qt::AltModifier };
    QApplication::sendEvent(&window, &press);
    settle();

    check(handler.executeCount == 1, "a command registered after the window answers its shortcut",
          QString::number(handler.executeCount));

    window.unregisterHandler(QString(LateCommandId));
    commandRegistry->unregisterCommand(QString(LateCommandId));
}

// Hiding a command is stronger than greying it out: Qt disables an invisible QAction, so the
// entry leaves the menu and the shortcut stops answering. A handler that reports itself
// invisible is therefore unreachable until it shows itself again.
void testHiddenCommandIgnoresItsShortcut()
{
    commandRegistry->registerCommand({
        QString(HiddenCommandId),                   // id
        QStringLiteral("Test"),                     // category
        QStringLiteral("Hidden"),                   // label
        {},                                         // description
        QKeySequence(QStringLiteral("Ctrl+Alt+U")), // defaultShortcut
        {},                                         // icon
        false,                                      // checkable
    });

    TestWindow window;
    window.setId(WindowId);
    window.setupUi();

    CountingHandler handler;
    handler.visible = false;
    window.registerHandler(QString(HiddenCommandId), &handler);

    window.show();
    window.activateWindow();
    settle();

    auto* action = window.commandAction(QString(HiddenCommandId));
    check(action && !action->isVisible(), "a handler that hides its command hides its QAction");
    check(action && !action->isEnabled(), "Qt disables an invisible QAction as well");

    QKeyEvent press { QEvent::KeyPress, Qt::Key_U, Qt::ControlModifier | Qt::AltModifier };
    QApplication::sendEvent(&window, &press);
    settle();

    check(handler.executeCount == 0, "the shortcut of an invisible QAction does not fire",
          QString::number(handler.executeCount));

    window.unregisterHandler(QString(HiddenCommandId));
    commandRegistry->unregisterCommand(QString(HiddenCommandId));
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    testUnregisteredCommandActionIsNull();
    testCommandActionProperties();
    testHandlerStateRefreshesAction();
    testActionTriggerRunsHandler();
    testUnregisteringCommandDeletesItsAction();
    testUiLessCommandRunsFromItsShortcut();
    testShortcutCommandRegisteredLater();
    testHiddenCommandIgnoresItsShortcut();

    commandRegistry->unregisterCommand(QString(CommandId));

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
