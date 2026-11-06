// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Window > Remove This Window, end to end: the workbench owns the handler, so the command has to
// carry the id of the window its menu item belongs to. Without it the handler answers nothing,
// which is how the menu entry could look dead while the same command ran from the palette — the
// palette asks the window it is open in and needs no such key.

#include "QcwTabFolder.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"
#include "QcwWorkbench.h"

#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QStandardPaths>

#include <cstdio>
#include <functional>

namespace
{

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

/// Processes events until @p done, for at most two seconds: removing a window asks its views
/// first and deletes it on a later turn, so a single round of events can be too early.
void settleUntil(const std::function<bool()>& done)
{
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 2000)
    {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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

class TestWorkbench : public QcwWorkbench
{
public:
    QcwWindow* createWindow() override
    {
        return new TestWindow;
    }
};

QList<QcwWindow*> openWindows()
{
    QList<QcwWindow*> windows;
    for (auto* widget : QApplication::topLevelWidgets())
    {
        if (auto* window = qobject_cast<QcwWindow*>(widget))
        {
            windows.append(window);
        }
    }

    return windows;
}

} // namespace

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    TestWorkbench workbench;
    if (!workbench.initialize(QStringLiteral("qcw-window-remove-command-test"),
                              QStringLiteral("Qcw Window Remove Command Test"), QStringLiteral("qcw.org"),
                              QStringLiteral("qcw"), QStringLiteral("1.0.0"),
                              /*quitOnLastWindowClosed=*/false, /*loadPlugins=*/false))
    {
        std::fprintf(stderr, "initialize failed\n");
        return 2;
    }

    if (!workbench.start())
    {
        std::fprintf(stderr, "start failed\n");
        return 3;
    }

    QPointer<QcwWindow> first = openWindows().value(0);
    check(!first.isNull(), "the workbench opens a window");
    if (first.isNull())
    {
        return 1;
    }

    auto* newWindowAction = first->commandAction(QcwWindowActionIds::ActionNewWindow);
    check(newWindowAction && newWindowAction->isEnabled(), "the window offers the New Window command");
    if (!newWindowAction)
    {
        return 1;
    }

    newWindowAction->trigger();
    settleUntil([]() { return openWindows().size() == 2; });

    QPointer<QcwWindow> second;
    for (auto* window : openWindows())
    {
        if (window != first.data())
        {
            second = window;
        }
    }

    check(second && second->id() != first->id(), "New Window opens a second one with an id of its own",
          second ? second->id() : QString());
    if (!second)
    {
        return 1;
    }

    // The menu item of the first window: the same command QAction a tool bar or a shortcut runs.
    auto* removeAction = first->commandAction(QcwWindowActionIds::ActionRemoveWindow);
    check(removeAction && removeAction->isEnabled(), "the window offers the Remove This Window command");
    if (!removeAction)
    {
        return 1;
    }

    removeAction->trigger();
    settleUntil([&first]() { return first.isNull(); });

    check(first.isNull(), "the menu command removes the window it belongs to");
    check(second && openWindows().size() == 1, "the other window stays open",
          QString::number(openWindows().size()));

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
