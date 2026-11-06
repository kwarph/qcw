// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QLatin1String>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwWindowActionIds
 * @brief The well-known ids the window uses for its menus and its commands.
 *
 * @details Ids are commands the window, the workbench or an application registers in
 * QcwCommandRegistry and places with a QcwContribution; the menu ids double as the
 * contribution targets that fill those menus. Built-in ids have no namespace prefix;
 * application and plugin ids must carry one (see the naming convention in the design
 * document).
 */
struct QcwWindowActionIds
{
    // --- Menus ------------------------------------------------------------------------

    static constexpr QLatin1String MenuFile { "File" };
    static constexpr QLatin1String MenuEdit { "Edit" };
    static constexpr QLatin1String MenuWindow { "Window" };
    static constexpr QLatin1String MenuHelp { "Help" };
    static constexpr QLatin1String MenuWindowAppearance { "Window/Appearance" };
    static constexpr QLatin1String MenuWindowPerspective { "Window/Perspective" };

    // --- File -------------------------------------------------------------------------

    static constexpr QLatin1String ActionOpenAppDir { "File/OpenAppDir" };
    static constexpr QLatin1String ActionOpenLogDir { "File/OpenLogDir" };
    static constexpr QLatin1String ActionExit { "File/Exit" };

    static constexpr QLatin1String ActionSave { "File/Save" };
    static constexpr QLatin1String ActionSaveAs { "File/SaveAs" };
    static constexpr QLatin1String ActionSaveAll { "File/SaveAll" };

    // --- Edit -------------------------------------------------------------------------

    static constexpr QLatin1String ActionPreferences { "Edit/Preferences" };

    static constexpr QLatin1String ActionUndo { "Edit/Undo" };
    static constexpr QLatin1String ActionRedo { "Edit/Redo" };
    static constexpr QLatin1String ActionCut { "Edit/Cut" };
    static constexpr QLatin1String ActionCopy { "Edit/Copy" };
    static constexpr QLatin1String ActionPaste { "Edit/Paste" };
    static constexpr QLatin1String ActionDelete { "Edit/Delete" };
    static constexpr QLatin1String ActionRename { "Edit/Rename" };
    static constexpr QLatin1String ActionSelectAll { "Edit/SelectAll" };

    // --- View (handlers live in views; the application catalogues and places them) -----

    static constexpr QLatin1String ActionFilter { "View/Filter" };
    static constexpr QLatin1String ActionRefresh { "View/Refresh" };

    // --- Window -----------------------------------------------------------------------

    static constexpr QLatin1String ActionNewWindow { "Window/NewWindow" };
    static constexpr QLatin1String ActionRemoveWindow { "Window/RemoveWindow" };
    static constexpr QLatin1String ActionCommandPalette { "Window/CommandPalette" };

    static constexpr QLatin1String ActionShowToolBar { "Window/Appearance/ShowToolBar" };
    static constexpr QLatin1String ActionShowStatusBar { "Window/Appearance/ShowStatusBar" };
    static constexpr QLatin1String ActionThemes { "Window/Appearance/Themes" };

    static constexpr QLatin1String ActionOpenPerspective { "Window/Perspective/OpenPerspective" };
    static constexpr QLatin1String ActionSavePerspectiveAs { "Window/Perspective/SavePerspectiveAs" };
    static constexpr QLatin1String ActionResetPerspective { "Window/Perspective/ResetPerspective" };
    static constexpr QLatin1String ActionClosePerspective { "Window/Perspective/ClosePerspective" };
    static constexpr QLatin1String ActionCloseAllPerspectives { "Window/Perspective/CloseAllPerspectives" };

    static constexpr QLatin1String MenuNewView { "Window/NewView" };

    // --- Help -------------------------------------------------------------------------

    static constexpr QLatin1String ActionAbout { "Help/About" };
    static constexpr QLatin1String ActionAboutPlugins { "Help/AboutPlugins" };

    // --- Toolbars ---------------------------------------------------------------------

    /// The contribution target for the main tool bar: the window creates the QToolBar for it
    /// when it first builds its targets, unless an application has registered its own bar under
    /// this target.
    static constexpr QLatin1String ToolBarMain { "ToolBar/Main" };

    // --- Command parameters ------------------------------------------------------------

    /**
     * @brief The payload key a command's target window travels under.
     * @details QcwWindow puts its own id in every command its menus, tool bars and shortcuts
     * run, so a process-wide handler such as Remove This Window knows which window the user
     * meant. A command run through QcwWindow::executeCommand() without it addresses no window,
     * which is the difference between a plugin targeting another window and a menu entry doing
     * nothing.
     */
    static constexpr QLatin1String ParamWindowId { "windowId" };
};

QT_END_NAMESPACE
