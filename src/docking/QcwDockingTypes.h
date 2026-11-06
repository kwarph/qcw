// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QFlags>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @namespace qcw
 * @brief Namespace for the QCW docking and tabbing types.
 *
 * @details Contains the enumerations, flags and string constants shared by the folder
 * widgets and the workbench code that arranges them.
 */
namespace qcw
{

/**
 * @brief MIME type of the drag-and-drop payload that moves a tab between folders.
 * @details QcwTabFolder writes the dragged tab under this type; folders and the folder
 *          container ignore drops that do not carry it.
 */
inline constexpr QLatin1String TabMimeType { "application/x-qcw-tab" };

/**
 * @brief Dynamic property that stores a tab pane's session id on its widget.
 * @details QcwView::id() reads it, and QcwTabFolder copies the same value into the tab
 *          data under this key so that restoring a session can find the pane again.
 */
inline constexpr const char* TabPaneIdPropertyName = "ViewId";

/**
 * @brief Dynamic property that stores the tab title a view's creator hands out on its widget.
 * @details It is stamped where a title is derived from QcwViewCreator::defaultWindowTitle() and is
 *          the baseline QcwWindow compares a tab's text against when the UI language changes: text
 *          still equal to it (with or without the ordinal QcwWindow::makeNewViewName() appends) is
 *          re-resolved from the creator, anything else is a rename and is left alone.
 * @see QcwView::defaultTitle()
 */
inline constexpr const char* ViewDefaultTitlePropertyName = "ViewDefaultTitle";

/**
 * @enum TabPosition
 * @brief Specifies the position of the tab bar within a folder.
 */
enum class TabPosition
{
    Top,   ///< Tab bar is positioned at the top.
    Bottom ///< Tab bar is positioned at the bottom.
};

/**
 * @enum FolderFeature
 * @brief Defines features and behaviors available for a tab folder.
 *
 * @details These flags can be combined using bitwise OR operations to configure
 * the behavior of a QcwTabFolder.
 *
 * @see FolderFeatures, QcwTabFolder::setFeature()
 */
// clang-format off
enum FolderFeature
{
    /**
     * @brief Allows the folder to display multiple tabs simultaneously.
     * @details If enabled, the tab bar will show all open tabs (subject to width constraints).
     * If disabled, the folder operates in "Single Tab Mode," where only one tab is visible
     * at a time, behavior similar to QComboBox.
     */
    MultiTabs           = 1,
    UseMRU              = 1 << 1,   ///< Enables Most Recently Used (MRU) ordering for tab switching.
    AcceptDroppedTabs   = 1 << 2,   ///< Allows tabs to be dropped into this folder from other folders.
    Minimizable         = 1 << 3,   ///< Enables the minimize button/functionality for the folder.
    Maximizable         = 1 << 4,   ///< Enables the maximize button/functionality for the folder.
    TabsMovable         = 1 << 5,   ///< Allows users to manually reorder tabs by dragging.
    ShowFullTitle       = 1 << 6,   ///< Displays the full title text of the tab.
    ShowIcon            = 1 << 7,   ///< Displays the icon associated with the tab.
    ShowClose           = 1 << 8,   ///< Displays the close button on the active/selected tab (does not affect whether the tab can be closed).
    ShowUnselectedClose = 1 << 9,   ///< Displays the close button when hovering over an unselected tab (does not affect whether the tab can be closed).

    /** @brief Default set of features enabled for a new tab folder. */
    DefaultTabFeatures = MultiTabs | AcceptDroppedTabs | UseMRU | Minimizable | Maximizable |
                         ShowFullTitle | ShowIcon | TabsMovable |
                         ShowClose | ShowUnselectedClose,

    NoFolderFeature     = 0         ///< No features enabled.
};
// clang-format on

Q_DECLARE_FLAGS(FolderFeatures, FolderFeature)
Q_DECLARE_OPERATORS_FOR_FLAGS(FolderFeatures)

/**
 * @enum FolderState
 * @brief Represents the current display state of a tab folder.
 */
enum class FolderState
{
    Normal,    ///< Standard docked state.
    Maximized, ///< Expanded to fill the available area.
    Minimized  ///< Collapsed to a small indicator or toolbar button.
};

/**
 * @enum FolderStartingIds
 * @brief First id of each of the folder id ranges used by QcwTabFolderContainer.
 *
 * @details Folder ids are unique within a container and split into three ranges: floating
 * folders count down from -1, shared folders use 1-99, and ordinary folders start at 100.
 * The enumerators name the start of a range, not a kind of folder state.
 */
enum class FolderStartingIds : int
{
    Floating = -1, ///< First id of the floating-folder range (-1, -2, ...).
    Shared = 1,    ///< First id of the shared-folder range (1-99).
    Normal = 100   ///< First id QcwTabFolderContainer hands out to ordinary folders.
};

/**
 * @enum DockArea
 * @brief Specifies the docking area within the folder container.
 *
 * @details These flags define where a folder or container can be docked.
 * They can be combined to allow multiple areas.
 *
 * @see DockAreas
 */
// clang-format off
enum DockArea
{
    LeftDockArea    = 1,        ///< Left side of the container.
    RightDockArea   = 1 << 1,   ///< Right side of the container.
    TopDockArea     = 1 << 2,   ///< Top side of the container.
    BottomDockArea  = 1 << 3,   ///< Bottom side of the container.
    CenterDockArea  = 1 << 4,   ///< Central area (usually for main content).

    /** @brief Combination of all border docking areas (Left, Right, Top, Bottom). */
    ContainerDockAreas = LeftDockArea | RightDockArea | TopDockArea | BottomDockArea,

    /** @brief Combination of all possible docking areas including Center. */
    AllDockAreas = ContainerDockAreas | CenterDockArea,

    NoDockArea      = 0         ///< Docking not allowed.
};
// clang-format on

Q_DECLARE_FLAGS(DockAreas, DockArea)
Q_DECLARE_OPERATORS_FOR_FLAGS(DockAreas)

/**
 * @enum DockTargetType
 * @brief Identifies the type of target during a drag-and-drop operation.
 */
enum class DockTargetType
{
    Folder,         ///< Dropping onto a specific QcwTabFolder.
    FolderContainer ///< Dropping onto the main QcwTabFolderContainer layout.
};

/**
 * @enum EmptyFolderAction
 * @brief Defines the action to take when a tab folder becomes empty.
 */
enum class EmptyFolderAction
{
    Keep,  ///< Keep the empty folder visible.
    Hide,  ///< Hide the folder but keep it in the layout structure.
    Delete ///< Remove the folder from the layout entirely.
};

} // namespace qcw

QT_END_NAMESPACE
