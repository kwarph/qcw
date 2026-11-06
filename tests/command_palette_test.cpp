// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Checks the command catalog and the palette that reads it: the registry emits only on a real
// change, setupUi() publishes the built-in commands, the palette lists the whole catalog
// whatever the menus show, greys out a command no handler answers, filters on the search text
// and runs the handler the window resolves for the selected command.

#include "QcwCommandPalette.h"
#include "QcwCommandRegistry.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QTreeView>

#include <cstdio>
#include <functional>

namespace
{

constexpr QLatin1String OpenProjectCommand { "Test/OpenProject" };
constexpr QLatin1String ProbeCommand { "Test/PaletteProbe" };
constexpr QLatin1String ScratchCommand { "Test/Scratch" };

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

void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
    }
}

/// Handles one command, counting the executions the palette causes.
class OpenProjectView : public QcwView
{
public:
    explicit OpenProjectView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
        handler = new QcwLambdaCommandHandler(this);
        handler->onExecute([this](const QVariantMap&)
        {
            ++executeCount;
        });
    }

    QHash<QString, QcwCommandHandler*> commandHandlers() const override
    {
        return { { QString(OpenProjectCommand), handler } };
    }

    int executeCount { 0 };
    QcwLambdaCommandHandler* handler { nullptr };
};

QLineEdit* filterOf(QcwCommandPalette* palette)
{
    return palette ? palette->findChild<QLineEdit*>(QStringLiteral("commandPaletteFilter")) : nullptr;
}

QTreeView* treeOf(QcwCommandPalette* palette)
{
    return palette ? palette->findChild<QTreeView*>(QStringLiteral("commandPaletteTree")) : nullptr;
}

/// The labels the palette offers under the filtered rows, so a listing assertion can name them.
QStringList visibleLabels(QTreeView* tree)
{
    QStringList labels;
    auto* model = tree ? tree->model() : nullptr;
    if (!model)
    {
        return labels;
    }

    std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& parent)
    {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const auto index = model->index(row, 0, parent);
            if (index.data(Qt::DisplayRole).isValid())
            {
                labels.append(index.data(Qt::DisplayRole).toString());
            }
            walk(index);
        }
    };
    walk({});

    return labels;
}

void sendKey(QWidget* widget, int key)
{
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
    settle();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // --- Registry semantics, before any window publishes anything ---------------------

    int commandsChanged = 0;
    QObject::connect(commandRegistry, &QcwCommandRegistry::commandsChanged, [&commandsChanged]()
    {
        ++commandsChanged;
    });

    QcwCommandInfo scratch;
    scratch.id = QString(ScratchCommand);
    scratch.category = QStringLiteral("Test");
    scratch.label = QStringLiteral("Scratch");
    scratch.description = QStringLiteral("A scratch command");
    scratch.defaultShortcut = QKeySequence(QKeySequence::Open);

    check(!commandRegistry->contains(ScratchCommand), "the catalog starts without the test command");
    check(commandRegistry->registerCommand(scratch) && commandsChanged == 1,
          "registerCommand publishes the info and emits commandsChanged");
    check(!commandRegistry->registerCommand(scratch) && commandsChanged == 1, "re-registering the same info is silent");

    auto changed = scratch;
    changed.label = QStringLiteral("Scratch Changed");
    check(commandRegistry->registerCommand(changed) && commandsChanged == 2 &&
              commandRegistry->command(ScratchCommand)->label == changed.label,
          "a changed label re-registers and emits again");

    QcwCommandInfo differentShortcut = scratch;
    differentShortcut.defaultShortcut = QKeySequence(QStringLiteral("Ctrl+F2"));
    check(commandRegistry->registerCommand(differentShortcut) && commandsChanged == 3,
          "a changed shortcut re-registers and emits again");
    check(!commandRegistry->registerCommand({}), "a command with an empty id is rejected");
    check(!commandRegistry->registerCommand(
              { QStringLiteral("not a valid id"), QStringLiteral("Test"), QStringLiteral("Bad") }),
          "a command with a malformed id is rejected");
    check(commandRegistry->unregisterCommand(ScratchCommand) && commandsChanged == 4 &&
              !commandRegistry->contains(ScratchCommand),
          "unregisterCommand removes the entry and emits");
    check(!commandRegistry->unregisterCommand(ScratchCommand), "unregistering an unknown id reports false");

    // --- The window publishes the built-ins --------------------------------------------

    // A command with no handler anywhere: the palette must still list it, greyed out.
    commandRegistry->registerCommand({ QString(ProbeCommand), QStringLiteral("Test"), QStringLiteral("Palette Probe"),
                                       QStringLiteral("Nothing handles this command") });
    // A command with no contribution at all: it is in the palette but in no menu.
    commandRegistry->registerCommand(
        { QString(OpenProjectCommand), QStringLiteral("File"), QStringLiteral("Open Project...") });

    QcwWindow window;
    window.setId("command-palette-test");
    window.setupUi();
    window.resize(800, 500);
    window.show();
    settle();

    auto saveInfo = commandRegistry->command(QcwWindowActionIds::ActionSave);
    check(saveInfo && saveInfo->category == QStringLiteral("File") &&
              saveInfo->defaultShortcut == QKeySequence(QKeySequence::Save),
          "setupUi() publishes the built-in commands with their shortcuts");

    auto* paletteEntry = window.commandAction(QcwWindowActionIds::ActionCommandPalette);
    check(paletteEntry && paletteEntry->text() == QStringLiteral("Command Palette...") &&
              paletteEntry->shortcut() == QKeySequence(QStringLiteral("Ctrl+Shift+P")),
          "the command QAction carries the catalogued label and shortcut");

    auto* windowMenu = window.getMenu(QcwWindowActionIds::MenuWindow);
    check(windowMenu && windowMenu->actions().contains(paletteEntry), "the Window menu offers the palette");

    paletteEntry->trigger();
    settle();
    auto* menuPalette = window.findChild<QcwCommandPalette*>();
    auto* menuTree = treeOf(menuPalette);
    check(menuPalette && menuPalette->isVisible() && menuTree && menuTree->model()->rowCount() > 0,
          "triggering the Window menu entry opens a palette with rows");

    // The catalog is what the palette lists, not the menus: a command with no contribution and
    // no handler is there, and the built-in File commands are grouped under their category.
    const QStringList allLabels = visibleLabels(menuTree);
    check(allLabels.contains(QStringLiteral("Palette Probe")) && allLabels.contains(QStringLiteral("Open Project...")),
          "the palette lists every catalogued command, contributed or not", allLabels.join(QLatin1Char(' ')));
    check(allLabels.contains(QStringLiteral("File")) && allLabels.contains(QStringLiteral("Test")),
          "commands are grouped under their category");
    menuPalette->reject();
    settle();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();

    // --- A handled command runs, an unhandled one does not -----------------------------

    auto folder = window.currentPerspective()->folderContainer()->centralFolder();
    auto* view = new OpenProjectView;
    folder->addTab(view, QStringLiteral("Open Project"));
    folder->setCurrentIndex(0);
    settle();

    window.openCommandPalette();
    settle();
    auto palette = QPointer<QcwCommandPalette> { window.findChild<QcwCommandPalette*>() };
    auto* tree = treeOf(palette.data());
    auto* edit = filterOf(palette.data());
    check(palette && palette->isVisible() && edit && tree, "openCommandPalette() shows the palette");

    edit->setText(QStringLiteral("open project"));
    settle();
    check(tree->currentIndex().isValid() && tree->currentIndex().data().toString() == QStringLiteral("Open Project..."),
          "the filter narrows the list and selects the one matching command",
          visibleLabels(tree).join(QLatin1Char(' ')));

    sendKey(edit, Qt::Key_Return);
    check(view->executeCount == 1, "Return runs the handler the window resolves", QString::number(view->executeCount));
    check(!palette || !palette->isVisible(), "the palette closes when it triggers a command");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();

    window.openCommandPalette();
    settle();
    palette = QPointer<QcwCommandPalette> { window.findChild<QcwCommandPalette*>() };
    tree = treeOf(palette.data());
    edit = filterOf(palette.data());

    // Palette Probe is catalogued but nobody handles it, so its row is listed and not selectable.
    edit->setText(QStringLiteral("palette probe"));
    settle();
    const QStringList probeLabels = visibleLabels(tree);
    check(probeLabels.contains(QStringLiteral("Palette Probe")), "an unhandled command stays listed",
          probeLabels.join(QLatin1Char(' ')));
    check(!tree->currentIndex().isValid(), "an unhandled command is greyed out, so nothing is selected");

    sendKey(edit, Qt::Key_Return);
    check(view->executeCount == 1 && palette && palette->isVisible(), "Return on a greyed row runs nothing");

    sendKey(edit, Qt::Key_Escape);
    check(!palette || !palette->isVisible(), "Escape closes the palette");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();

    // Down and Up walk the enabled rows only, which is what keeps a greyed command unreachable.
    window.openCommandPalette();
    settle();
    palette = QPointer<QcwCommandPalette> { window.findChild<QcwCommandPalette*>() };
    tree = treeOf(palette.data());
    edit = filterOf(palette.data());

    edit->setText(QStringLiteral("open"));
    settle();
    const auto firstLabel = tree->currentIndex().data().toString();
    sendKey(edit, Qt::Key_Down);
    const auto secondLabel = tree->currentIndex().data().toString();
    check(firstLabel != secondLabel && !secondLabel.isEmpty() && !probeLabels.contains(secondLabel),
          "the arrow keys move to the next enabled command", QStringLiteral("%1 -> %2").arg(firstLabel, secondLabel));
    sendKey(edit, Qt::Key_Up);
    check(tree->currentIndex().data().toString() == firstLabel, "Up returns to the previous enabled command");
    palette->reject();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();

    commandRegistry->unregisterCommand(QString(ProbeCommand));
    commandRegistry->unregisterCommand(QString(OpenProjectCommand));

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
