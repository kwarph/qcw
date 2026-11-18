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
 * application and plugin ids must carry one (see docs/qcw-id-naming-conventions.md).
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

    static constexpr QLatin1String ActionOpenAppDir { "File/openAppDir" };
    static constexpr QLatin1String ActionOpenLogDir { "File/openLogDir" };
    static constexpr QLatin1String ActionExit { "File/exit" };

    static constexpr QLatin1String ActionSave { "File/save" };
    static constexpr QLatin1String ActionSaveAs { "File/saveAs" };
    static constexpr QLatin1String ActionSaveAll { "File/saveAll" };

    // --- Edit -------------------------------------------------------------------------

    static constexpr QLatin1String ActionPreferences { "Edit/preferences" };

    static constexpr QLatin1String ActionUndo { "Edit/undo" };
    static constexpr QLatin1String ActionRedo { "Edit/redo" };
    static constexpr QLatin1String ActionCut { "Edit/cut" };
    static constexpr QLatin1String ActionCopy { "Edit/copy" };
    static constexpr QLatin1String ActionPaste { "Edit/paste" };
    static constexpr QLatin1String ActionDelete { "Edit/delete" };
    static constexpr QLatin1String ActionRename { "Edit/rename" };
    static constexpr QLatin1String ActionSelectAll { "Edit/selectAll" };

    // --- View (handlers live in views; the application catalogues and places them) -----

    static constexpr QLatin1String ActionFilter { "View/filter" };
    static constexpr QLatin1String ActionRefresh { "View/refresh" };

    // --- Window -----------------------------------------------------------------------

    static constexpr QLatin1String ActionNewWindow { "Window/newWindow" };
    static constexpr QLatin1String ActionRemoveWindow { "Window/removeWindow" };
    static constexpr QLatin1String ActionCommandPalette { "Window/commandPalette" };

    static constexpr QLatin1String ActionShowToolBar { "Window/Appearance/showToolBar" };
    static constexpr QLatin1String ActionShowStatusBar { "Window/Appearance/showStatusBar" };
    static constexpr QLatin1String ActionThemes { "Window/Appearance/themes" };

    static constexpr QLatin1String ActionOpenPerspective { "Window/Perspective/openPerspective" };
    static constexpr QLatin1String ActionSavePerspectiveAs { "Window/Perspective/savePerspectiveAs" };
    static constexpr QLatin1String ActionResetPerspective { "Window/Perspective/resetPerspective" };
    static constexpr QLatin1String ActionClosePerspective { "Window/Perspective/closePerspective" };
    static constexpr QLatin1String ActionCloseAllPerspectives { "Window/Perspective/closeAllPerspectives" };

    static constexpr QLatin1String MenuNewView { "Window/NewView" };

    // --- Help -------------------------------------------------------------------------

    static constexpr QLatin1String ActionAbout { "Help/about" };
    static constexpr QLatin1String ActionAboutPlugins { "Help/aboutPlugins" };

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
