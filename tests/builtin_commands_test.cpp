// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Verifies that QcwBuiltinCommands registers the metadata and contributions the window
// expects: every built-in menu has entries, command ids are catalogued, and the registrations
// are idempotent.

#include "QcwBuiltinCommands.h"
#include "QcwCommandRegistry.h"
#include "QcwContributionRegistry.h"
#include "QcwContributionSorter.h"
#include "QcwWindowActionIds.h"

#include <QApplication>
#include <QStringList>

#include <cstdio>

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

QStringList idsOf(const QList<QcwContribution>& contributions)
{
    QStringList ids;
    for (const auto& contribution : contributions)
    {
        ids.append(contribution.id);
    }
    return ids;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QcwBuiltinCommands::unregisterCommands();
    QcwBuiltinCommands::registerCommands();

    // --- Command metadata ------------------------------------------------------------

    check(commandRegistry->contains(QcwWindowActionIds::ActionSave), "File/Save is catalogued");
    check(commandRegistry->contains(QcwWindowActionIds::ActionCopy), "Edit/Copy is catalogued");
    check(commandRegistry->contains(QcwWindowActionIds::ActionExit), "File/Exit is catalogued");
    check(commandRegistry->contains(QcwWindowActionIds::ActionNewWindow), "Window/NewWindow is catalogued");
    check(commandRegistry->contains(QcwWindowActionIds::ActionThemes), "Window/Appearance/Themes is catalogued");

    const auto saveInfo = commandRegistry->command(QcwWindowActionIds::ActionSave);
    check(saveInfo && saveInfo->category == QcwWindowActionIds::MenuFile, "Save is catalogued under File");
    check(saveInfo && saveInfo->defaultShortcut == QKeySequence(QKeySequence::Save), "Save carries the Save shortcut");

    const auto showStatusInfo = commandRegistry->command(QcwWindowActionIds::ActionShowStatusBar);
    check(showStatusInfo && showStatusInfo->checkable, "Show Status Bar is checkable");

    // --- Contributions ---------------------------------------------------------------

    check(!contributionRegistry->contributionsFor(QcwWindowActionIds::MenuFile).isEmpty(), "File has contributions");
    check(!contributionRegistry->contributionsFor(QcwWindowActionIds::MenuEdit).isEmpty(), "Edit has contributions");
    check(!contributionRegistry->contributionsFor(QcwWindowActionIds::MenuWindow).isEmpty(),
          "Window has contributions");
    check(!contributionRegistry->contributionsFor(QcwWindowActionIds::MenuHelp).isEmpty(), "Help has contributions");

    // --- Sorting ---------------------------------------------------------------------

    const auto fileSorted = QcwContributionSorter::sort(
        contributionRegistry->contributionsFor(QcwWindowActionIds::MenuFile), {}, QcwWindowActionIds::MenuFile);
    const QStringList fileIds = idsOf(fileSorted);
    check(fileIds.contains(QStringLiteral("file.save")), "the sorted File menu contains Save");
    check(fileIds.indexOf(QStringLiteral("file.save")) < fileIds.indexOf(QStringLiteral("file.exit")),
          "Save comes before Exit");

    const auto appearanceSorted =
        QcwContributionSorter::sort(contributionRegistry->contributionsFor(QcwWindowActionIds::MenuWindowAppearance),
                                    {}, QcwWindowActionIds::MenuWindowAppearance);
    const QStringList appearanceIds = idsOf(appearanceSorted);
    check(appearanceIds.size() == 3, "Window/Appearance has three entries", QString::number(appearanceIds.size()));
    check(appearanceIds.indexOf(QStringLiteral("window.appearance.showToolBar")) <
              appearanceIds.indexOf(QStringLiteral("window.appearance.showStatusBar")),
          "Show Toolbar comes before Show Status Bar");
    check(appearanceIds.indexOf(QStringLiteral("window.appearance.showStatusBar")) <
              appearanceIds.indexOf(QStringLiteral("window.appearance.themes")),
          "Show Status Bar comes before Themes");

    // --- Idempotence -----------------------------------------------------------------

    const int fileContributionsBefore = contributionRegistry->contributionsFor(QcwWindowActionIds::MenuFile).size();
    const int cataloguedBefore = commandRegistry->commands().size();

    QcwBuiltinCommands::registerCommands();
    check(contributionRegistry->contributionsFor(QcwWindowActionIds::MenuFile).size() == fileContributionsBefore,
          "re-registering does not duplicate the File contributions",
          QStringLiteral("%1 -> %2")
              .arg(fileContributionsBefore)
              .arg(contributionRegistry->contributionsFor(QcwWindowActionIds::MenuFile).size()));
    check(commandRegistry->commands().size() == cataloguedBefore, "re-registering does not grow the catalog",
          QStringLiteral("%1 -> %2").arg(cataloguedBefore).arg(commandRegistry->commands().size()));

    QcwBuiltinCommands::unregisterCommands();
    check(!commandRegistry->contains(QcwWindowActionIds::ActionSave), "unregister removes the built-in command");
    check(contributionRegistry->contributionsFor(QcwWindowActionIds::MenuFile).isEmpty(),
          "unregister removes the built-in contributions");

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
