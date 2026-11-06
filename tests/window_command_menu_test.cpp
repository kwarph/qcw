// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Checks the File and Edit menus and the tool bars QcwWindow builds from contributions: an
// application command placed before a built-in one by naming it in `before`, the separators the
// group changes make, the one command QAction a menu item, a tool bar button and the palette share,
// the current view's handler driving enabled as tabs change, the entries a contribution or an
// action set takes away, the warnings the placement rules raise: a contribution that cannot be
// placed, and an action set id — on a contribution or in a perspective — that nobody registered,
// and that a tool bar an application registers for a target stays the only one for it.

#include "QcwActionSetRegistry.h"
#include "QcwBuiltinCommands.h"
#include "QcwCommandRegistry.h"
#include "QcwContributionRegistry.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QMenu>
#include <QStringList>
#include <QTimer>
#include <QToolBar>

#include <cstdio>
#include <functional>

namespace
{

constexpr QLatin1String OpenProjectCommand { "Test/OpenProject" };
constexpr QLatin1String ScratchCommand { "Test/Scratch" };
constexpr QLatin1String DebugActionSet { "test/debug" };
constexpr QLatin1String TestToolBarTarget { "ToolBar/Test" };
constexpr QLatin1String DebugToolBarTarget { "ToolBar/Debug" };

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

/// The entries with their separators, so an ordering assertion sees what a target holds.
QStringList entriesOf(const QList<QAction*>& actions)
{
    QStringList entries;
    for (const auto* action : actions)
    {
        entries.append(action->isSeparator() ? QStringLiteral("---") : action->text());
    }

    return entries;
}

QStringList entriesOf(QMenu* menu)
{
    return entriesOf(menu->actions());
}

QStringList entriesOf(QToolBar* toolBar)
{
    return entriesOf(toolBar->actions());
}

void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
    }
}

/// Warnings the placement diagnostics raised while collectWarning() was the message handler.
QStringList gWarnings;

void collectWarning(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    // Only the window's placement diagnostics, so unrelated noise cannot upset the assertions.
    if (type == QtWarningMsg && message.startsWith(QStringLiteral("QcwWindow: ")))
    {
        gWarnings.append(message);
    }
}

/// Runs @p work with collectWarning() installed, and returns what it warned about.
QStringList warningsDuring(const std::function<void()>& work)
{
    gWarnings.clear();
    auto* previous = qInstallMessageHandler(collectWarning);
    work();
    qInstallMessageHandler(previous);
    return gWarnings;
}

/// Handles File/Save and Edit/Copy, like a text editor would.
class EditorCommandView : public QcwView
{
public:
    explicit EditorCommandView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
        saveHandler = new QcwLambdaCommandHandler(this);
        saveHandler->onExecute([this](const QVariantMap&)
        {
            ++saveCount;
        });

        copyHandler = new QcwLambdaCommandHandler(this);
        copyHandler->onExecute([this](const QVariantMap&)
        {
            ++copyCount;
        });
    }

    QHash<QString, QcwCommandHandler*> commandHandlers() const override
    {
        return { { QString(QcwWindowActionIds::ActionSave), saveHandler },
                 { QString(QcwWindowActionIds::ActionCopy), copyHandler } };
    }

    int saveCount { 0 };
    int copyCount { 0 };
    QcwLambdaCommandHandler* saveHandler { nullptr };
    QcwLambdaCommandHandler* copyHandler { nullptr };
};

/// Handles Edit/Delete and a command of its own, leaving File/Save to nobody.
class ProjectCommandView : public QcwView
{
public:
    explicit ProjectCommandView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
        deleteHandler = new QcwLambdaCommandHandler(this);
        deleteHandler->onExecute([this](const QVariantMap&)
        {
            ++deleteCount;
        });

        refreshHandler = new QcwLambdaCommandHandler(this);
        refreshHandler->onExecute([this](const QVariantMap&)
        {
            ++refreshCount;
        });
    }

    QHash<QString, QcwCommandHandler*> commandHandlers() const override
    {
        return { { QString(QcwWindowActionIds::ActionDelete), deleteHandler },
                 { QString(ScratchCommand), refreshHandler } };
    }

    int deleteCount { 0 };
    int refreshCount { 0 };
    QcwLambdaCommandHandler* deleteHandler { nullptr };
    QcwLambdaCommandHandler* refreshHandler { nullptr };
};

/// A perspective with one empty folder, so a switch to it changes nothing but the action sets.
class RestrictedPerspective : public QcwPerspective
{
public:
    explicit RestrictedPerspective(QcwWindow* window) :
        QcwPerspective { window }
    {
    }

    void createInitialLayout() override
    {
        auto container = folderContainer();
        auto folder = container->createFolder(nullptr, qcw::LeftDockArea);
        container->setCentralFolder(folder);
        activateFolder(folder);
    }
};

/// A window that owns the tool bar of a contribution target, the way an application's main window
/// does. The registration happens in the constructor body, which runs after QcwWindow's own.
class AppToolBarWindow : public QcwWindow
{
public:
    explicit AppToolBarWindow(QWidget* parent = nullptr) :
        QcwWindow { parent }
    {
        auto* appToolBar = new QToolBar(QStringLiteral("Debug"), this);
        appToolBar->setObjectName(QStringLiteral("app-debug-bar"));
        registeredToolBars().insert(QString(DebugToolBarTarget), appToolBar);
        addTopToolBar(appToolBar);
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QcwBuiltinCommands::registerCommands();

    // An application command of its own, placed before the built-in Save by naming the command
    // id in `before` and living in a group of its own, which is what makes the separators.
    commandRegistry->registerCommand(
        { QString(OpenProjectCommand), QString(QcwWindowActionIds::MenuFile), QStringLiteral("Open Project...") });
    contributionRegistry->registerContribution({ QStringLiteral("test/openProject"),
                                                 {},
                                                 QString(QcwWindowActionIds::MenuFile),
                                                 QString(OpenProjectCommand),
                                                 QString(QcwWindowActionIds::ActionSave),
                                                 {},
                                                 QStringLiteral("file-project"),
                                                 true });

    // A command that belongs to an action set, to be hidden again by the perspective below.
    actionSetRegistry->registerActionSet({ QString(DebugActionSet), QStringLiteral("Debug") });
    commandRegistry->registerCommand(
        { QString(ScratchCommand), QStringLiteral("Test"), QStringLiteral("Scratch Command") });
    contributionRegistry->registerContribution({ QStringLiteral("test/scratch"),
                                                 QString(DebugActionSet),
                                                 QString(QcwWindowActionIds::MenuEdit),
                                                 QString(ScratchCommand),
                                                 {},
                                                 QStringLiteral("edit.undo"),
                                                 QStringLiteral("test-group"),
                                                 true });

    // Two tool bar targets nobody created a QToolBar for. The window builds both from the target
    // names, groups them the way it groups menus, and hides a bar left with nothing to place.
    contributionRegistry->registerContribution({ QStringLiteral("test/toolbar/save"),
                                                 {},
                                                 QString(TestToolBarTarget),
                                                 QString(QcwWindowActionIds::ActionSave),
                                                 {},
                                                 {},
                                                 QStringLiteral("file"),
                                                 true });
    contributionRegistry->registerContribution({ QStringLiteral("test/toolbar/scratch"),
                                                 QString(DebugActionSet),
                                                 QString(TestToolBarTarget),
                                                 QString(ScratchCommand),
                                                 {},
                                                 QStringLiteral("test/toolbar/save"),
                                                 QStringLiteral("scratch"),
                                                 true });
    contributionRegistry->registerContribution({ QStringLiteral("test/toolbar/copy"),
                                                 QString(DebugActionSet),
                                                 QString(DebugToolBarTarget),
                                                 QString(QcwWindowActionIds::ActionCopy),
                                                 {},
                                                 {},
                                                 QStringLiteral("clipboard"),
                                                 true });

    QcwWindow window;
    window.setId("window-command-menu-test");
    window.setupUi();
    window.resize(800, 500);
    window.show();
    settle();

    auto fileMenu = window.getMenu(QcwWindowActionIds::MenuFile);
    auto editMenu = window.getMenu(QcwWindowActionIds::MenuEdit);
    check(fileMenu && editMenu, "the File and Edit menus exist");
    if (!fileMenu || !editMenu)
    {
        return 1;
    }

    // The built-in groups plus the application's own entry, ordered by the contribution sorter and
    // divided by a separator wherever a groupId changes.
    const QStringList expected { QStringLiteral("Open Application Directory..."),
                                 QStringLiteral("Open Log Directory..."),
                                 QStringLiteral("---"),
                                 QStringLiteral("Open Project..."),
                                 QStringLiteral("---"),
                                 QStringLiteral("Save"),
                                 QStringLiteral("Save As..."),
                                 QStringLiteral("Save All"),
                                 QStringLiteral("---"),
                                 QStringLiteral("Exit") };
    check(entriesOf(fileMenu) == expected, "File holds the sorted contributions with a separator between each group",
          entriesOf(fileMenu).join(QLatin1Char(' ')));

    // A menu item is the command QAction, not a copy of it: the same object answers for the
    // shortcut, the palette and any tool bar that reuses the command.
    auto* saveAction = window.commandAction(QcwWindowActionIds::ActionSave);
    check(saveAction && fileMenu->actions().contains(saveAction), "the menu item is the command QAction");
    check(window.commandAction(QcwWindowActionIds::ActionSave) == saveAction, "commandAction is the same object twice");

    auto folder = window.currentPerspective()->folderContainer()->centralFolder();
    auto* project = new ProjectCommandView;
    auto* editor = new EditorCommandView;
    folder->addTab(project, QStringLiteral("Project"));
    folder->addTab(editor, QStringLiteral("Editor"));
    folder->setCurrentIndex(1); // Editor current
    settle();

    // The editor handles Save and Copy, so both are enabled; Delete has no handler here, so it
    // stays in the menu greyed out — placement is the contribution's call, ability the handler's.
    check(saveAction->isEnabled(), "the current view's handler enables Save");
    check(window.commandAction(QcwWindowActionIds::ActionCopy)->isVisible(), "the current view's handler shows Copy");
    auto* deleteAction = window.commandAction(QcwWindowActionIds::ActionDelete);
    check(deleteAction && deleteAction->isVisible() && !deleteAction->isEnabled(),
          "a command no view handles is shown disabled, not hidden");

    // Triggering the menu item runs the handler the window resolved for the current view.
    saveAction->trigger();
    settle();
    check(editor->saveCount == 1, "triggering the menu item runs the view handler", QString::number(editor->saveCount));
    check(window.executeCommand(QcwWindowActionIds::ActionCopy) && editor->copyCount == 1,
          "executeCommand runs the same handler the menu item does");

    // The same command QAction is what a tab switch re-targets: the view changes, the menu keeps
    // its entries and only the states move.
    folder->setCurrentIndex(0);
    QCoreApplication::processEvents();
    settle();

    check(!saveAction->isEnabled() && saveAction->isVisible(), "Save greys out when its handler is gone");
    check(entriesOf(fileMenu) == expected, "switching the view does not rebuild the menu");
    check(deleteAction->isVisible() && deleteAction->isEnabled(), "the project's Delete is now the handled one");
    check(window.executeCommand(QcwWindowActionIds::ActionDelete) && project->deleteCount == 1,
          "the new view's handler answers the command");
    check(window.executeCommand(QString(ScratchCommand)) && project->refreshCount == 1,
          "a command only this view handles resolves through the window");

    // The tool bar side of the rule above. This view handles neither Save nor Copy, and the bar did
    // not collapse when it became current: the buttons stay where the contributions put them and
    // only grey out, which is what a hand-picked list used to lose on every tab switch.
    auto* testToolBar = window.getToolBar(QString(TestToolBarTarget));
    auto* debugToolBar = window.getToolBar(QString(DebugToolBarTarget));
    check(testToolBar && debugToolBar, "contributions to a ToolBar/ target build that tool bar");
    check(testToolBar && entriesOf(testToolBar) == QStringList({ QStringLiteral("Save"),
                                                                QStringLiteral("---"),
                                                                QStringLiteral("Scratch Command") }),
          "the tool bar holds the sorted contributions with a separator between its groups",
          testToolBar ? entriesOf(testToolBar).join(QLatin1Char(' ')) : QString());
    check(testToolBar && testToolBar->actions().contains(saveAction) && testToolBar->isVisible(),
          "the button is the command QAction, on screen though no view handles it");
    check(debugToolBar && debugToolBar->actions().contains(window.commandAction(QcwWindowActionIds::ActionCopy)),
          "the second tool bar holds its own command QAction");

    // What an application that owns a target's tool bar gets: that bar, and only it. Building the
    // contributions in the base class constructor, before such a registration could happen, left
    // the generated bar in the top area beside the application's — which is what a second window
    // opened by Window > New Window showed twice.
    //
    // Scoped so this second window is gone before the warning assertions below: the diagnostics
    // count per window, and a live window would report each one a second time.
    {
        AppToolBarWindow appWindow;
        appWindow.setId(QStringLiteral("window-command-menu-test-app"));
        appWindow.setupUi();
        settle();

        auto* claimed = appWindow.getToolBar(QString(DebugToolBarTarget));
        check(claimed && claimed->objectName() == QStringLiteral("app-debug-bar"),
              "the registered tool bar is the one that target hands back");
        check(claimed && entriesOf(claimed).contains(QStringLiteral("Copy")),
              "the contributions are placed in the application's own bar",
              claimed ? entriesOf(claimed).join(QLatin1Char(' ')) : QString());

        QStringList generatedBars;
        for (auto* toolBar : appWindow.findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly))
        {
            if (toolBar->objectName().startsWith(QStringLiteral("qcwToolBar_")))
            {
                generatedBars.append(toolBar->objectName());
            }
        }
        check(!generatedBars.contains(QStringLiteral("qcwToolBar_Debug")),
              "no generated bar was left beside the application's", generatedBars.join(QLatin1Char(' ')));
    }

    // A contribution whose command was never catalogued cannot be placed, so the menu leaves it
    // out — and says so, since a missing entry is otherwise silent about why it is missing.
    const int fileEntriesBefore = fileMenu->actions().size();
    const auto orphanWarnings = warningsDuring([&]
    {
        contributionRegistry->registerContribution({ QStringLiteral("test/orphan"),
                                                     {},
                                                     QString(QcwWindowActionIds::MenuFile),
                                                     QStringLiteral("Test/Ghost"),
                                                     {},
                                                     QStringLiteral("file.save"),
                                                     QStringLiteral("file-project"),
                                                     true });
        settle();
    });
    check(fileMenu->actions().size() == fileEntriesBefore, "a contribution without a command is left out");
    check(orphanWarnings.size() == 1 && orphanWarnings.first().contains(QStringLiteral("test/orphan"))
              && orphanWarnings.first().contains(QStringLiteral("Test/Ghost")),
          "the unplaceable contribution is reported once, naming itself and its command",
          orphanWarnings.join(QLatin1Char(' ')));

    // Cataloguing the command makes the same contribution placeable and the warning stops: the
    // contribution was never wrong, only early, which is how a plugin that registers its command
    // after its contribution is allowed to work.
    commandRegistry->registerCommand(
        { QStringLiteral("Test/Ghost"), QStringLiteral("Test"), QStringLiteral("Ghost Command") });
    const auto laterWarnings = warningsDuring(settle);
    check(fileMenu->actions().size() > fileEntriesBefore, "the contribution is placed once its command exists");
    check(laterWarnings.isEmpty(), "a placed contribution has nothing to report",
          laterWarnings.join(QLatin1Char(' ')));

    // Unregistering the command takes its QAction away, so the entry it made disappears too — the
    // menu no longer shows something the catalog has forgotten. Its contribution goes with it, so
    // the builds that follow are not exercises in reporting a dangling one.
    commandRegistry->unregisterCommand(QString(OpenProjectCommand));
    contributionRegistry->unregisterContribution(QStringLiteral("test/openProject"));
    settle();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    check(window.commandAction(QString(OpenProjectCommand)) == nullptr,
          "the command action of the app command is gone");
    const QStringList entriesAfterUnregister = entriesOf(fileMenu);
    check(!entriesAfterUnregister.contains(QStringLiteral("Open Project...")) &&
              entriesAfterUnregister.contains(QStringLiteral("Save")),
          "File loses the entry of the unregistered command and keeps the rest",
          entriesAfterUnregister.join(QLatin1Char(' ')));

    // Switching perspective re-sorts the contributions, so an action set the new perspective does
    // not list loses its menu entry and its tool bar button alike, and a perspective with an empty
    // list shows every set again.
    auto* unrestricted = window.currentPerspective();
    auto* scratchAction = window.commandAction(QString(ScratchCommand));
    check(scratchAction && editMenu->actions().contains(scratchAction), "the action set starts visible");

    auto* restricted = new RestrictedPerspective(&window);
    restricted->setVisibleActionSetIds({ QStringLiteral("test/other") });
    const auto perspectiveWarnings = warningsDuring([&]
    {
        window.openPerspective(restricted);
        settle();
    });
    check(perspectiveWarnings.size() == 1 && perspectiveWarnings.first().contains(QStringLiteral("test/other")),
          "a perspective listing an unregistered action set is reported once",
          perspectiveWarnings.join(QLatin1Char(' ')));
    check(!editMenu->actions().contains(scratchAction) && !testToolBar->actions().contains(scratchAction),
          "an action set the perspective does not list loses its entry in the menu and the tool bar");
    check(!debugToolBar->isVisible(), "a tool bar with nothing left to place hides itself");

    window.setCurrentPerspective(unrestricted->id());
    settle();
    check(editMenu->actions().contains(scratchAction) && testToolBar->actions().contains(scratchAction),
          "the empty list means every action set is visible again");
    check(debugToolBar->isVisible(), "the tool bar returns once something is placed in it");

    // The list of the perspective that is already open has the same effect: setVisibleActionSetIds()
    // announces the change, so the window re-places without a perspective switch to wait for.
    unrestricted->setVisibleActionSetIds({ QStringLiteral("test/other") });
    settle();
    check(!editMenu->actions().contains(scratchAction) && !debugToolBar->isVisible(),
          "changing the current perspective's action sets re-places menus and tool bars at once");
    unrestricted->setVisibleActionSetIds({});
    settle();
    check(testToolBar->actions().contains(scratchAction), "and the same happens back the other way");

    // Section 7.3's other registry rule: a contribution naming an action set nobody registered is
    // invisible even while the perspective shows everything, since no perspective could ever reveal
    // it on purpose. The window says so once — and the entry comes back once the set exists, at the
    // next build, because the action set registry emits no signal of its own.
    commandRegistry->registerCommand(
        { QStringLiteral("Test/Typo"), QStringLiteral("Test"), QStringLiteral("Typo Command") });
    settle();

    const auto typoWarnings = warningsDuring([&]
    {
        contributionRegistry->registerContribution({ QStringLiteral("test/typo"),
                                                     QStringLiteral("test/typo-set"),
                                                     QString(DebugToolBarTarget),
                                                     QStringLiteral("Test/Typo"),
                                                     {},
                                                     {},
                                                     QStringLiteral("typo"),
                                                     true });
        settle();
    });
    auto* typoAction = window.commandAction(QStringLiteral("Test/Typo"));
    check(typoAction && !debugToolBar->actions().contains(typoAction),
          "a contribution naming an unregistered action set is not placed");
    check(typoWarnings.size() == 1 && typoWarnings.first().contains(QStringLiteral("test/typo"))
              && typoWarnings.first().contains(QStringLiteral("test/typo-set")),
          "the unregistered action set is reported once, naming the contribution",
          typoWarnings.join(QLatin1Char(' ')));

    // Registering the set and adding an unrelated contribution cause the next build, which places
    // the entry that had to wait and does not repeat the warning.
    actionSetRegistry->registerActionSet({ QStringLiteral("test/typo-set"), QStringLiteral("Typo") });
    const auto recoveryWarnings = warningsDuring([&]
    {
        contributionRegistry->registerContribution({ QStringLiteral("test/typoSibling"),
                                                     {},
                                                     QString(QcwWindowActionIds::MenuEdit),
                                                     QStringLiteral("Test/Typo"),
                                                     {},
                                                     {},
                                                     QStringLiteral("typo"),
                                                     true });
        settle();
    });
    check(editMenu->actions().contains(typoAction), "a sibling contribution without an action set places the command");
    check(debugToolBar->actions().contains(typoAction), "the next build places the entry whose action set now exists");
    check(recoveryWarnings.isEmpty(), "the same bad action set is not reported again",
          recoveryWarnings.join(QLatin1Char(' ')));

    contributionRegistry->unregisterContribution(QStringLiteral("test/scratch"));
    contributionRegistry->unregisterContribution(QStringLiteral("test/orphan"));
    contributionRegistry->unregisterContribution(QStringLiteral("test/toolbar/save"));
    contributionRegistry->unregisterContribution(QStringLiteral("test/toolbar/scratch"));
    contributionRegistry->unregisterContribution(QStringLiteral("test/toolbar/copy"));
    contributionRegistry->unregisterContribution(QStringLiteral("test/typo"));
    contributionRegistry->unregisterContribution(QStringLiteral("test/typoSibling"));
    actionSetRegistry->unregisterActionSet(QStringLiteral("test/typo-set"));
    commandRegistry->unregisterCommand(QString(ScratchCommand));
    commandRegistry->unregisterCommand(QStringLiteral("Test/Ghost"));
    commandRegistry->unregisterCommand(QStringLiteral("Test/Typo"));
    QcwBuiltinCommands::unregisterCommands();

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
