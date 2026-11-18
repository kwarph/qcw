// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwBuiltinCommands.h"

#include "QcwCommandRegistry.h"
#include "QcwContributionRegistry.h"
#include "QcwThemeIcons.h"
#include "QcwWindowActionIds.h"

#include <QApplication>
#include <QIcon>
#include <QKeySequence>
#include <QHash>
#include <QSet>
#include <QStyle>
#include <optional>
#include <utility>

QT_BEGIN_NAMESPACE

namespace
{

// The ids this class registered, so unregisterCommands() removes exactly those and never an
// application command that happens to share a prefix.
QSet<QString> registeredCommandIds;
QSet<QString> registeredContributionIds;

QString tr(const char* text)
{
    return QApplication::translate("QcwBuiltinCommands", text);
}

QIcon builtinCommandIcon(const QString& id)
{
    struct Entry
    {
        QLatin1String id;
        const char* themeName;
        std::optional<QStyle::StandardPixmap> fallback;
    };

    static const Entry icons[] = {
        { QcwWindowActionIds::ActionSave, "document-save", QStyle::SP_DialogSaveButton },
        { QcwWindowActionIds::ActionSaveAs, "document-save-as", QStyle::SP_DialogSaveButton },
        { QcwWindowActionIds::ActionSaveAll, "document-save-all", QStyle::SP_DialogSaveButton },
        { QcwWindowActionIds::ActionUndo, "edit-undo", std::nullopt },
        { QcwWindowActionIds::ActionRedo, "edit-redo", std::nullopt },
        { QcwWindowActionIds::ActionCut, "edit-cut", std::nullopt },
        { QcwWindowActionIds::ActionCopy, "edit-copy", std::nullopt },
        { QcwWindowActionIds::ActionPaste, "edit-paste", std::nullopt },
        { QcwWindowActionIds::ActionDelete, "edit-delete", QStyle::SP_TrashIcon },
        { QcwWindowActionIds::ActionRename, "edit-rename", std::nullopt },
        { QcwWindowActionIds::ActionSelectAll, "edit-select-all", std::nullopt },
    };

    for (const auto& entry : icons)
    {
        if (entry.id != id)
        {
            continue;
        }

        QIcon fallback;
        if (entry.fallback && QApplication::style())
        {
            fallback = QApplication::style()->standardIcon(*entry.fallback);
        }

        return QIcon::fromTheme(QString::fromLatin1(entry.themeName), fallback);
    }

    return {};
}

void registerCommand(const QString& id,
                     const QString& category,
                     const QString& label,
                     const QString& description,
                     const QKeySequence& shortcut,
                     bool checkable = false,
                     const QIcon& icon = {})
{
    QcwCommandInfo info;
    info.id = id;
    info.category = category;
    info.label = label;
    info.description = description;
    info.defaultShortcut = shortcut;
    info.checkable = checkable;
    info.icon = icon.isNull() ? builtinCommandIcon(id) : icon;
    registeredCommandIds.insert(id);
    commandRegistry->registerCommand(info);
}

void registerContribution(const QString& id,
                          const QString& target,
                          const QString& commandId,
                          const QString& groupId,
                          const QString& before = {},
                          const QString& after = {})
{
    QcwContribution contribution;
    contribution.id = id;
    contribution.target = target;
    contribution.commandId = commandId;
    contribution.groupId = groupId;
    contribution.before = before;
    contribution.after = after;
    registeredContributionIds.insert(id);
    contributionRegistry->registerContribution(contribution);
}

} // namespace

void QcwBuiltinCommands::registerCommands()
{
    if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
    {
        qWarning() << "QcwBuiltinCommands::registerCommands() requires a QGuiApplication;"
                   << "standard shortcuts cannot be resolved without one";
        return;
    }

    const QString file = QcwWindowActionIds::MenuFile;
    const QString edit = QcwWindowActionIds::MenuEdit;
    const QString window = QcwWindowActionIds::MenuWindow;
    const QString help = QcwWindowActionIds::MenuHelp;
    const QString appearance = QcwWindowActionIds::MenuWindowAppearance;
    const QString perspective = QcwWindowActionIds::MenuWindowPerspective;

    // --- File -------------------------------------------------------------------------

    registerCommand(QcwWindowActionIds::ActionOpenAppDir, file, tr("Open Application Directory..."),
                    tr("Open Application Directory"), QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionOpenLogDir, file, tr("Open Log Directory..."), tr("Open Log Directory"),
                    QKeySequence::UnknownKey);

    registerCommand(QcwWindowActionIds::ActionSave, file, tr("Save"), {}, QKeySequence::Save);
    registerCommand(QcwWindowActionIds::ActionSaveAs, file, tr("Save As..."), {}, QKeySequence::SaveAs);
    registerCommand(QcwWindowActionIds::ActionSaveAll, file, tr("Save All"), {}, QKeySequence::UnknownKey);

    registerCommand(QcwWindowActionIds::ActionExit, file, tr("Exit"),
                    tr("Close all windows of the application and exit the application"), QKeySequence::Quit);

    // --- Edit -------------------------------------------------------------------------

    registerCommand(QcwWindowActionIds::ActionPreferences, edit, tr("Preferences..."), tr("Preferences"),
                    QKeySequence::UnknownKey);

    registerCommand(QcwWindowActionIds::ActionUndo, edit, tr("Undo"), {}, QKeySequence::Undo);
    registerCommand(QcwWindowActionIds::ActionRedo, edit, tr("Redo"), {}, QKeySequence::Redo);
    registerCommand(QcwWindowActionIds::ActionCut, edit, tr("Cut"), {}, QKeySequence::Cut);
    registerCommand(QcwWindowActionIds::ActionCopy, edit, tr("Copy"), {}, QKeySequence::Copy);
    registerCommand(QcwWindowActionIds::ActionPaste, edit, tr("Paste"), {}, QKeySequence::Paste);
    registerCommand(QcwWindowActionIds::ActionDelete, edit, tr("Delete"), {}, QKeySequence::Delete);
    registerCommand(QcwWindowActionIds::ActionRename, edit, tr("Rename..."), {}, QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionSelectAll, edit, tr("Select All"), {}, QKeySequence::SelectAll);

    // --- Window -----------------------------------------------------------------------

    registerCommand(QcwWindowActionIds::ActionNewWindow, window, tr("New Window"), {}, QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionRemoveWindow, window, tr("Remove This Window"),
                    tr("Close and remove this window"), QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionCommandPalette, window, tr("Command Palette..."),
                    tr("Search and run a command"), QKeySequence(QStringLiteral("Ctrl+Shift+P")));

    registerCommand(QcwWindowActionIds::ActionShowToolBar, appearance, tr("Show Toolbar"), tr("Show toolbar"),
                    QKeySequence::UnknownKey, true);
    registerCommand(QcwWindowActionIds::ActionShowStatusBar, appearance, tr("Show Status Bar"), tr("Show status bar"),
                    QKeySequence::UnknownKey, true);

    registerCommand(QcwWindowActionIds::ActionThemes, appearance, tr("Themes..."), tr("Manage themes"),
                    QKeySequence::UnknownKey);

    registerCommand(QcwWindowActionIds::ActionOpenPerspective, perspective, tr("Open Perspective"), {},
                    QKeySequence::UnknownKey, /*checkable=*/false, qcw::themeIcon(qcw::Icon::NewPerspective));
    registerCommand(QcwWindowActionIds::ActionSavePerspectiveAs, perspective, tr("Save Perspective As..."),
                    tr("Save the current perspective as a new one"), QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionResetPerspective, perspective, tr("Reset Perspective"),
                    tr("Reset the current perspective to the initial layout"), QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionClosePerspective, perspective, tr("Close Perspective"),
                    tr("Close perspective"), QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionCloseAllPerspectives, perspective, tr("Close All Perspectives"),
                    tr("Close all perspectives"), QKeySequence::UnknownKey);

    // --- Help -------------------------------------------------------------------------

    registerCommand(QcwWindowActionIds::ActionAbout, help,
                    tr("About %1...").arg(QApplication::applicationDisplayName()),
                    tr("About %1").arg(QApplication::applicationDisplayName()), QKeySequence::UnknownKey);
    registerCommand(QcwWindowActionIds::ActionAboutPlugins, help, tr("About Plugins..."), tr("About Plugins"),
                    QKeySequence::UnknownKey);

    // --- Contributions ----------------------------------------------------------------

    // File
    registerContribution("File/openAppDir", file, QcwWindowActionIds::ActionOpenAppDir, "file-open");
    registerContribution("File/openLogDir", file, QcwWindowActionIds::ActionOpenLogDir, "file-open", {},
                         "File/openAppDir");
    registerContribution("File/save", file, QcwWindowActionIds::ActionSave, "file-save", {}, "File/openLogDir");
    registerContribution("File/saveAs", file, QcwWindowActionIds::ActionSaveAs, "file-save", {}, "File/save");
    registerContribution("File/saveAll", file, QcwWindowActionIds::ActionSaveAll, "file-save", {}, "File/saveAs");
    registerContribution("File/exit", file, QcwWindowActionIds::ActionExit, "file-exit", {}, "File/saveAll");

    // Edit
    registerContribution("Edit/undo", edit, QcwWindowActionIds::ActionUndo, "edit-undo");
    registerContribution("Edit/redo", edit, QcwWindowActionIds::ActionRedo, "edit-undo");
    registerContribution("Edit/cut", edit, QcwWindowActionIds::ActionCut, "edit-clipboard");
    registerContribution("Edit/copy", edit, QcwWindowActionIds::ActionCopy, "edit-clipboard");
    registerContribution("Edit/paste", edit, QcwWindowActionIds::ActionPaste, "edit-clipboard");
    registerContribution("Edit/delete", edit, QcwWindowActionIds::ActionDelete, "edit-clipboard");
    registerContribution("Edit/rename", edit, QcwWindowActionIds::ActionRename, "edit-clipboard");
    registerContribution("Edit/selectAll", edit, QcwWindowActionIds::ActionSelectAll, "edit-select");
    registerContribution("Edit/preferences", edit, QcwWindowActionIds::ActionPreferences, "edit-preferences");

    // Window
    registerContribution("Window/newWindow", window, QcwWindowActionIds::ActionNewWindow, "window-window");
    registerContribution("Window/removeWindow", window, QcwWindowActionIds::ActionRemoveWindow, "window-window");
    registerContribution("Window/commandPalette", window, QcwWindowActionIds::ActionCommandPalette, "window-window");
    registerContribution("Window/Appearance/showToolBar", appearance, QcwWindowActionIds::ActionShowToolBar,
                         "appearance");
    registerContribution("Window/Appearance/showStatusBar", appearance, QcwWindowActionIds::ActionShowStatusBar,
                         "appearance", {}, "Window/Appearance/showToolBar");
    registerContribution("Window/Appearance/themes", appearance, QcwWindowActionIds::ActionThemes, "appearance", {},
                         "Window/Appearance/showStatusBar");
    registerContribution("Window/Perspective/openPerspective", perspective, QcwWindowActionIds::ActionOpenPerspective,
                         "perspective-open");
    registerContribution("Window/Perspective/savePerspectiveAs", perspective,
                         QcwWindowActionIds::ActionSavePerspectiveAs, "perspective-edit");
    registerContribution("Window/Perspective/resetPerspective", perspective, QcwWindowActionIds::ActionResetPerspective,
                         "perspective-edit");
    registerContribution("Window/Perspective/closePerspective", perspective, QcwWindowActionIds::ActionClosePerspective,
                         "perspective-edit");
    registerContribution("Window/Perspective/closeAllPerspectives", perspective,
                         QcwWindowActionIds::ActionCloseAllPerspectives, "perspective-edit");

    // Help
    registerContribution("Help/about", help, QcwWindowActionIds::ActionAbout, "help-about");
    registerContribution("Help/aboutPlugins", help, QcwWindowActionIds::ActionAboutPlugins, "help-about");
}

void QcwBuiltinCommands::unregisterCommands()
{
    for (const auto& id : std::as_const(registeredContributionIds))
    {
        contributionRegistry->unregisterContribution(id);
    }
    registeredContributionIds.clear();

    for (const auto& id : std::as_const(registeredCommandIds))
    {
        commandRegistry->unregisterCommand(id);
    }
    registeredCommandIds.clear();
}

QT_END_NAMESPACE
