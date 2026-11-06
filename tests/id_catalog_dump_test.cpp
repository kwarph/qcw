// QCW_DUMP_ID_CATALOG=1 ends QcwWorkbench::start() with the ids the three registries hold, so
// "the menu is missing an entry" can be compared against the catalog the entry should match.
// The dump lists ids only: the registries do not record who registered an entry.

#include "QcwActionSet.h"
#include "QcwActionSetRegistry.h"
#include "QcwBuiltinCommands.h"
#include "QcwCommand.h"
#include "QcwCommandRegistry.h"
#include "QcwContribution.h"
#include "QcwContributionRegistry.h"
#include "QcwTabFolder.h"
#include "QcwWindow.h"
#include "QcwWorkbench.h"

#include <QApplication>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>

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

class DumpWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

class DumpWorkbench : public QcwWorkbench
{
public:
    QcwWindow* createWindow() override
    {
        return new DumpWindow;
    }
};

QStringList gMessages;

void collectMessage(QtMsgType, const QMessageLogContext&, const QString& message)
{
    gMessages.append(message);
}

bool hasLine(const QString& line)
{
    return gMessages.contains(line);
}
} // namespace

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);

    QcwCommandInfo command;
    command.id = QStringLiteral("test/DumpCommand");
    command.category = QStringLiteral("Test");
    command.label = QStringLiteral("Dump Command");
    commandRegistry->registerCommand(command);

    QcwActionSet actionSet;
    actionSet.id = QStringLiteral("test/dump-set");
    actionSet.label = QStringLiteral("Dump Set");
    actionSetRegistry->registerActionSet(actionSet);

    QcwContribution contribution;
    contribution.id = QStringLiteral("test/dump/entry");
    contribution.target = QStringLiteral("File");
    contribution.commandId = command.id;
    contributionRegistry->registerContribution(contribution);

    DumpWorkbench workbench;
    if (!workbench.initialize(QStringLiteral("qcw-id-catalog-dump-test"),
                              QStringLiteral("Qcw Id Catalog Dump Test"), QStringLiteral("qcw.org"),
                              QStringLiteral("qcw"), QStringLiteral("1.0.0"),
                              /*quitOnLastWindowClosed=*/true, /*loadPlugins=*/false))
    {
        std::fprintf(stderr, "initialize failed\n");
        return 2;
    }

    qputenv("QCW_DUMP_ID_CATALOG", "1");
    const auto previousHandler = qInstallMessageHandler(collectMessage);
    const bool started = workbench.start();
    qInstallMessageHandler(previousHandler);
    qunsetenv("QCW_DUMP_ID_CATALOG");

    check(started, "the workbench starts with the dump enabled");
    check(hasLine(QStringLiteral("QCW command catalog (registered):")), "the dump announces the command catalog");
    check(hasLine(QStringLiteral("  File/Save")), "the dump lists a built-in command");
    check(hasLine(QStringLiteral("  test/DumpCommand")), "the dump lists an application command");
    check(hasLine(QStringLiteral("QCW action set catalog (registered):")), "the dump announces the action set catalog");
    check(hasLine(QStringLiteral("  test/dump-set")), "the dump lists an action set");
    check(hasLine(QStringLiteral("QCW contribution catalog (registered):")),
          "the dump announces the contribution catalog");
    check(hasLine(QStringLiteral("  file.save")), "the dump lists a built-in contribution");
    check(hasLine(QStringLiteral("  test/dump/entry")), "the dump lists an application contribution");

    contributionRegistry->unregisterContribution(contribution.id);
    actionSetRegistry->unregisterActionSet(actionSet.id);
    commandRegistry->unregisterCommand(command.id);
    QcwBuiltinCommands::unregisterCommands();

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
