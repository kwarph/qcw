// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "MainWindow.h"

#include "DemoServices.h"
#include "QcwCommand.h"
#include "QcwCommandRegistry.h"
#include "QcwContribution.h"
#include "QcwContributionRegistry.h"
#include "QcwPerspective.h"
#include "QcwPerspectiveCreator.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwView.h"
#include "QcwWindowActionIds.h"

#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace
{

constexpr QLatin1String PlaceholderTitleName { "editorPlaceholderTitle" };
constexpr QLatin1String PlaceholderHintName { "editorPlaceholderHint" };

// Mirrors the Project view's welcome page: an empty shared folder otherwise draws nothing at
// all once its tab bar is gone, which makes the editor area look like it vanished.
QWidget* createEditorPlaceholder(QWidget* parent)
{
    auto* placeholder = new QWidget { parent };
    auto* layout = new QVBoxLayout { placeholder };
    layout->addStretch();

    auto* title = new QLabel { MainWindow::tr("No editor is open"), placeholder };
    title->setObjectName(QString(PlaceholderTitleName));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-weight: bold;"));

    auto* hint = new QLabel { MainWindow::tr("Open a file from the Project view to edit it here."), placeholder };
    hint->setObjectName(QString(PlaceholderHintName));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addStretch();
    return placeholder;
}

// One row per tool bar button: the target it belongs to, the contribution's own id, the command
// it places, and the group that separates it from the buttons either side. Everything about these
// two tool bars is a declaration, so their order, their separators and what a perspective's action
// sets hide all work exactly as they do in a menu.
struct ToolBarEntry
{
    QLatin1String target;
    QLatin1String contributionId;
    QLatin1String commandId;
    QLatin1String groupId;
};

// QLatin1String's const char* constructor is explicit, so a row of the table below cannot name a
// literal directly; this wraps one without the noise of repeating the type every line.
constexpr QLatin1String latin1(const char* text)
{
    return QLatin1String { text };
}

const ToolBarEntry kToolBarEntries[] = {
    { QcwWindowActionIds::ToolBarMain, latin1("demo/toolbar/main/newProject"), demo::ActionNewProject,
      latin1("project") },
    { QcwWindowActionIds::ToolBarMain, latin1("demo/toolbar/main/openProject"), demo::ActionOpenProject,
      latin1("project") },
    { QcwWindowActionIds::ToolBarMain, latin1("demo/toolbar/main/newFile"), demo::ActionNewFile,
      latin1("project") },
    { QcwWindowActionIds::ToolBarMain, latin1("demo/toolbar/main/save"), QcwWindowActionIds::ActionSave,
      latin1("save") },
    { QcwWindowActionIds::ToolBarMain, latin1("demo/toolbar/main/saveAs"), QcwWindowActionIds::ActionSaveAs,
      latin1("save") },
    { QcwWindowActionIds::ToolBarMain, latin1("demo/toolbar/main/saveAll"), QcwWindowActionIds::ActionSaveAll,
      latin1("save") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/undo"), QcwWindowActionIds::ActionUndo, latin1("undo") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/redo"), QcwWindowActionIds::ActionRedo, latin1("undo") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/cut"), QcwWindowActionIds::ActionCut, latin1("clipboard") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/copy"), QcwWindowActionIds::ActionCopy, latin1("clipboard") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/paste"), QcwWindowActionIds::ActionPaste,
      latin1("clipboard") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/delete"), QcwWindowActionIds::ActionDelete,
      latin1("clipboard") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/rename"), QcwWindowActionIds::ActionRename,
      latin1("clipboard") },
    { demo::ToolBarEdit, latin1("demo/toolbar/edit/selectAll"), QcwWindowActionIds::ActionSelectAll,
      latin1("select") },
};

} // namespace

MainWindow::MainWindow(QWidget* parent) :
    QcwWindow { parent }
{
    setWindowTitle(tr("Workbench Demo"));

    // The QcwWindow constructor already built the File/Edit/Window/Help skeleton and
    // registered the built-in commands. The View menu and the two tool bars are this
    // application's own; the commands that land in them are registered below, and the window
    // rebuilds the menus and tool bars when the contribution registry announces them.
    addViewMenu();
    addToolBars();
    registerFileCommands();
    registerViewCommands();
}

void MainWindow::addToolBars()
{
    // The bars themselves are the demo's, so the demo says what they are called and how their
    // buttons look, and hands them in under their targets: the window fills these two instead of
    // building plain tool bars of its own from the target names.
    mMainToolBar = new QToolBar(tr("Main"), this);
    mEditToolBar = new QToolBar(tr("Edit"), this);

    for (auto* toolBar : { mMainToolBar, mEditToolBar })
    {
        toolBar->setIconSize(QSize(16, 16));
        toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        addTopToolBar(toolBar);
    }

    registeredToolBars().insert(QString(QcwWindowActionIds::ToolBarMain), mMainToolBar);
    registeredToolBars().insert(QString(demo::ToolBarEdit), mEditToolBar);

    for (const auto& entry : kToolBarEntries)
    {
        QcwContribution contribution;
        contribution.id = QString(entry.contributionId);
        contribution.target = QString(entry.target);
        contribution.commandId = QString(entry.commandId);
        contribution.groupId = QString(entry.groupId);
        contributionRegistry->registerContribution(contribution);
    }
}

void MainWindow::addViewMenu()
{
    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    registeredMenus().insert(QString(demo::MenuView), viewMenu);

    // The window creates File/Edit/Window/Help in its constructor; place View right after Edit.
    if (auto* windowMenu = getMenu(QcwWindowActionIds::MenuWindow))
    {
        menuBar()->insertMenu(windowMenu->menuAction(), viewMenu);
    }
}

void MainWindow::registerFileCommands()
{
    // Project is a demo concept, so the ids live in DemoServices.h rather than in
    // QcwWindowActionIds. ProjectView handles them, so they answer while it is the current view
    // and sit greyed out while it is not: the contributions place them either way, and only a
    // handler decides whether they can run.
    registerAppFileCommand(QString(demo::ActionNewProject), QStringLiteral("demo/file/newProject"),
                           tr("New Project..."), QKeySequence::UnknownKey);

    registerAppFileCommand(QString(demo::ActionOpenProject), QStringLiteral("demo/file/openProject"),
                           tr("Open Project..."), QKeySequence::Open);

    registerAppFileCommand(QString(demo::ActionCloseProject), QStringLiteral("demo/file/closeProject"),
                           tr("Close Project"), QKeySequence::UnknownKey);

    registerAppFileCommand(QString(demo::ActionNewFile), QStringLiteral("demo/file/newFile"), tr("New File..."),
                           QKeySequence::New);
}

void MainWindow::registerAppFileCommand(const QString& id,
                                        const QString& contributionId,
                                        const QString& label,
                                        QKeySequence shortcut)
{
    QcwCommandInfo info;
    info.id = id;
    info.category = QString(QcwWindowActionIds::MenuFile);
    info.label = label;
    info.defaultShortcut = shortcut;
    commandRegistry->registerCommand(info);

    QcwContribution contribution;
    contribution.id = contributionId;
    contribution.target = QString(QcwWindowActionIds::MenuFile);
    contribution.commandId = id;
    contribution.groupId = QStringLiteral("file-project");
    contribution.before = QString(QcwWindowActionIds::ActionSave);
    contributionRegistry->registerContribution(contribution);
}

void MainWindow::registerViewCommands()
{
    // The View menu belongs to the demo. Its commands are the application's own, not built-ins the
    // window already catalogues, so registering them here is what makes them searchable in the
    // command palette, and the contributions are what make them appear in the menu. ProjectView
    // supplies the handlers.
    registerAppViewCommand(QString(QcwWindowActionIds::ActionFilter), QStringLiteral("demo/view/filter"),
                           tr("Filter..."), tr("Filter the project files"));

    registerAppViewCommand(QString(QcwWindowActionIds::ActionRefresh), QStringLiteral("demo/view/refresh"),
                           tr("Refresh"), tr("Refresh the project tree"));
}

void MainWindow::registerAppViewCommand(const QString& id,
                                        const QString& contributionId,
                                        const QString& label,
                                        const QString& description)
{
    QcwCommandInfo info;
    info.id = id;
    info.category = tr("View");
    info.label = label;
    info.description = description;
    commandRegistry->registerCommand(info);

    QcwContribution contribution;
    contribution.id = contributionId;
    contribution.target = QString(demo::MenuView);
    contribution.commandId = id;
    contribution.groupId = QStringLiteral("view-project");
    contributionRegistry->registerContribution(contribution);
}

QcwTabFolder* MainWindow::createSharedFolder()
{
    return new QcwTabFolder { this };
}

void MainWindow::postSetupUi()
{
    connect(this, &MainWindow::currentViewChanged, [this](QcwView* view)
    {
        if (view && view->property("isEditor").toBool())
        {
            setWindowTitle(view->windowTitle());
        }
    });

    // Both a fresh window and a restored one run this, so the placeholder also reaches a shared
    // folder that restoreStateFromFile() rebuilt instead of createSharedFolder().
    if (auto* shared = sharedFolder())
    {
        shared->setPlaceholder(createEditorPlaceholder(shared));
    }

    // A restored window shows what was saved, even when only the dummy perspective survived
    // the last session; a fresh window shows the demo layout.
    if (isRestoredFromState())
    {
        return;
    }

    auto* creator = pluginManager->getPluginT<QcwPerspectiveCreator>(QStringLiteral("ProjectPerspective"));
    if (!creator)
    {
        return;
    }

    auto* perspective = creator->create(this);
    if (!perspective)
    {
        return;
    }

    perspective->createInitialLayout();
    openPerspective(perspective);
}

void MainWindow::retranslateUi()
{
    // The base re-resolves the view titles, the perspective buttons, the built-in menu titles
    // and the built-in command labels; registering the commands again is what reaches the
    // palette and the command QActions.
    QcwWindow::retranslateUi();

    setWindowTitle(tr("Workbench Demo"));

    if (auto* viewMenu = getMenu(QString(demo::MenuView)))
    {
        viewMenu->setTitle(tr("&View"));
    }

    if (mMainToolBar)
    {
        mMainToolBar->setWindowTitle(tr("Main"));
    }
    if (mEditToolBar)
    {
        mEditToolBar->setWindowTitle(tr("Edit"));
    }

    if (auto* shared = sharedFolder())
    {
        if (auto* title = shared->findChild<QLabel*>(QString(PlaceholderTitleName)))
        {
            title->setText(tr("No editor is open"));
        }
        if (auto* hint = shared->findChild<QLabel*>(QString(PlaceholderHintName)))
        {
            hint->setText(tr("Open a file from the Project view to edit it here."));
        }
    }

    // Re-register the demo's own commands so the palette and the command QActions follow the
    // new language.
    registerFileCommands();
    registerViewCommands();
}
