// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwDockingTypes.h"
#include "QcwGlobal.h"
#include <QWidget>

#include <QMap>
#include <QPair>
#include <QSet>

QT_BEGIN_NAMESPACE

class QXmlStreamWriter;
class QXmlStreamReader;
class QSplitter;
class QcwTabFolder;
class QcwTabFolderContainerPrivate;

/**
 * @class QcwTabFolderContainer
 * @brief A dockable container widget that manages QcwTabFolder instances using a splitter-based layout.
 *
 * @details QcwTabFolderContainer provides a flexible docking system similar to IDE layouts.
 * It allows creating, arranging, minimizing, maximizing, and closing tab folders.
 * It uses a hierarchy of QSplitter widgets to manage the spatial arrangement of folders.
 *
 * @par Key Features
 * - Dynamic folder creation and docking (Left, Right, Top, Bottom, Center).
 * - Support for central folder designation.
 * - Drag-and-drop support for reorganizing tabs and folders.
 * - Focus management and state tracking (Minimized/Maximized/Normal).
 *
 * @see QcwTabFolder
 * @see QcwTabFolderContainerPrivate
 */
class QCW_EXPORT QcwTabFolderContainer : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a QcwTabFolderContainer widget.
     * @param parent The parent widget. Ownership is transferred to this widget.
     */
    explicit QcwTabFolderContainer(QWidget* parent = nullptr);

    /** @brief Destructs the QcwTabFolderContainer widget. */
    ~QcwTabFolderContainer() override;

    /**
     * @brief Creates a new tab folder and docks it relative to a reference folder.
     * @param reference The reference folder to dock against. If nullptr, docks to the container root.
     * @param dockArea The area where the new folder should be docked (e.g., LeftDockArea, CenterDockArea).
     * @return Pointer to the newly created QcwTabFolder; @p reference itself when @p dockArea
     *         is CenterDockArea (the tab lands in the reference instead of a new folder);
     *         or nullptr if the area is not allowed for @p reference, or @p reference is
     *         nullptr with CenterDockArea.
     * @note The caller does not own the returned pointer; the container manages its lifecycle.
     */
    QcwTabFolder* createFolder(QcwTabFolder* reference, qcw::DockArea dockArea);

    /**
     * @brief Gets the folder shared into this container, if any.
     * @return Pointer to the shared folder, or nullptr.
     * @details A shared folder is a single QcwTabFolder instance reused across the
     *          perspectives of a window (see QcwWindow::sharedFolder()). Each container
     *          may hold at most one; it is hosted in its own QcwTabFolderSite but owned
     *          by the window, so the container detaches it on clear() and destruction
     *          rather than deleting it.
     * @see addSharedFolder()
     */
    QcwTabFolder* sharedFolder() const;

    /**
     * @brief Docks an externally owned folder as this container's shared folder.
     * @param folder The folder to share. Ownership stays with the caller (typically QcwWindow);
     *               the container only hosts it. Ignored when nullptr.
     * @param reference The reference folder to dock against, or nullptr for the container root.
     * @param dockArea The docking area. CenterDockArea is refused: a shared folder needs its
     *                 own site so it can be swapped in and out per perspective.
     * @return True on success; false if the container already has a shared folder, @p folder
     *         is null, or the dock area is not allowed.
     * @details The container only hosts the folder. Closing its last tab never deletes it:
     *          EmptyFolderAction::Delete does not apply, because the caller keeps the folder
     *          and may hand it to another container afterwards. Hide still works while this
     *          container is the one hosting it.
     * @see sharedFolder()
     */
    bool addSharedFolder(QcwTabFolder* folder, QcwTabFolder* reference, qcw::DockArea dockArea);

    /**
     * @brief Gets the designated central folder.
     * @return Pointer to the central folder, or nullptr if none is set.
     * @see setCentralFolder()
     */
    QcwTabFolder* centralFolder() const;

    /**
     * @brief Sets the designated central folder.
     * @details The central folder typically has special behavior, such as not being deletable
     * when empty and absorbing the layout's resize slack (see QSplitter stretch factors).
     * @param folder The folder to set as central, or nullptr to clear the designation.
     * @see centralFolder()
     */
    void setCentralFolder(QcwTabFolder* folder);

    /**
     * @brief Checks if a given folder is the designated central folder.
     * @param folder The folder to check.
     * @return True if the folder is the central folder; false otherwise.
     */
    bool isCentralFolder(QcwTabFolder* folder) const;

    /**
     * @brief Checks if a folder is currently taken out of the visible layout.
     * @param folder The folder to check.
     * @return True if the folder was minimized by itself, or hidden by the container while
     *         another folder is maximized.
     */
    bool isMinimizedFolder(QcwTabFolder* folder) const;

    /**
     * @brief Finds the folder of this container that shows a specific pane widget.
     * @param child The pane widget to search for; nullptr matches no folder.
     * @return Pointer to the parent QcwTabFolder, or nullptr if not found.
     */
    QcwTabFolder* parentFolder(QWidget* child) const;

    /**
     * @brief Gets the folder the container currently treats as active.
     * @return Pointer to the current folder, or nullptr when the container has none.
     * @note The container tracks this for minimize/current-switch bookkeeping; the
     *       visual active state on the folders is applied by the consumer through
     *       QcwTabFolder::setActive().
     * @see currentFolderChanged()
     */
    QcwTabFolder* currentFolder() const;

    /**
     * @brief Gets all currently managed tab folders.
     * @return Set of QcwTabFolder pointers, including the shared folder.
     */
    QSet<QcwTabFolder*> folders() const;

    /**
     * @brief Gets the action performed when a folder becomes empty.
     * @param folder The folder to query.
     * @return The current EmptyFolderAction (e.g., Delete, Hide).
     * @see setEmptyFolderAction()
     */
    qcw::EmptyFolderAction emptyFolderAction(QcwTabFolder* folder) const;

    /**
     * @brief Sets the action to perform when a folder becomes empty.
     * @param folder The folder to configure.
     * @param action The desired action (e.g., qcw::EmptyFolderAction::Delete).
     * @note Ignored for the central folder, which survives being emptied. A shared folder is
     *       never deleted this way either; only Hide applies to it.
     */
    void setEmptyFolderAction(QcwTabFolder* folder, qcw::EmptyFolderAction action);

    /**
     * @brief Gets the allowed docking areas for a specific folder.
     * @param folder The folder to query.
     * @return Bitwise OR of allowed qcw::DockArea flags; NoDockArea if the folder is
     *         not managed by this container.
     * @see setAllowedDockAreas()
     */
    qcw::DockAreas allowedDockAreas(QcwTabFolder* folder) const;

    /**
     * @brief Sets the allowed docking areas for a specific folder.
     * @param folder The folder to configure; ignored when not managed by this container.
     * @param areas Bitwise OR of allowed qcw::DockArea flags.
     * @note CenterDockArea in @p areas also turns the folder's AcceptDroppedTabs
     *       feature on or off, since it is what makes the folder a drop target.
     */
    void setAllowedDockAreas(QcwTabFolder* folder, qcw::DockAreas areas);

    /**
     * @brief Gets the default allowed docking areas for the container.
     * @return Default bitwise OR of allowed qcw::DockArea flags.
     * @note This applies when docking relative to no folder, i.e. against the
     *       container itself (reference == nullptr).
     */
    qcw::DockAreas allowedDockAreas() const;

    /**
     * @brief Sets the default allowed docking areas for the container.
     * @param areas Bitwise OR of allowed qcw::DockArea flags.
     */
    void setAllowedDockAreas(qcw::DockAreas areas);

    /**
     * @brief Gets the root splitter of the container hierarchy.
     * @return Pointer to the top-level QSplitter.
     */
    QSplitter* rootSplitter() const;

    /**
     * @brief Finds the immediate parent splitter of a given widget.
     * @param child The widget whose parent splitter is sought.
     * @return Pointer to the parent QSplitter, or nullptr if @p child is null or has none.
     */
    QSplitter* parentSplitter(QWidget* child) const;

    /**
     * @brief Writes the whole docking layout as a @c folderContainer element.
     * @param writer The stream to write into. The element is written inside whatever
     *               element the caller has open; the container writes no document header.
     * @details Records the per-folder settings and states, the splitter tree with the
     *          folder sites, the splitter sizes, and the maximized/minimized bookkeeping,
     *          addressing folders by their @c FolderId property. The shared folder is
     *          referenced by id only; its content is saved by its owner.
     * @see restoreState()
     */
    void saveState(QXmlStreamWriter& writer) const;

    /**
     * @brief Restores a layout written by saveState(), recreating the folders it held.
     * @param reader Positioned on the @c folderContainer start element; consumed through
     *               its end element.
     * @param folder The live shared folder to re-host, when the saved state had one;
     *               a saved shared folder without this argument is dropped.
     * @param tabPanes Maps a saved pane id to the widget to re-attach and its current id,
     *                 passed through to each QcwTabFolder::restoreState().
     * @return @c false when @p reader is already in an error state or a restored folder
     *         refused its element.
     *
     * @details Unknown or semantically unusable pieces are skipped and repaired in place;
     *          they do not fail the restore. Parsing continues after a failing folder so the
     *          element is always consumed through its end. On failure the container holds a
     *          partial layout; the caller is expected to discard it.
     * @note Existing folders must be removed first (see clear()); this call only adds.
     * @see saveState()
     */
    bool restoreState(QXmlStreamReader& reader,
                      QcwTabFolder* folder = nullptr,
                      const QMap<int, QPair<QWidget*, int>>& tabPanes = {});

    /**
     * @brief Removes every folder from the layout and resets the container to empty.
     * @details Folders hosted here are scheduled for deletion; the shared folder is only
     *          detached, since its owner (typically QcwWindow) keeps it alive. The
     *          per-folder settings, the central/current designation and the default
     *          allowed dock areas return to their initial values.
     */
    void clear();

signals:
    /**
     * @brief Emitted when a folder is added to the container.
     * @param folder Pointer to the folder just added.
     * @note Emitted for folders created by createFolder(), for the shared folder hosted
     *       by addSharedFolder(), and for folders recreated by restoreState(), including
     *       the shared folder re-hosted there.
     */
    void folderCreated(QcwTabFolder* folder);

    /**
     * @brief Emitted when a folder has been collapsed out of the layout.
     * @param folder Pointer to the minimized folder.
     * @param fastViewToolBarArea The window tool bar area (left or right edge) the
     *        container picked for the folder's fast-view button, whichever is closer
     *        to the folder's former position.
     * @see QcwTabFolder::minimizeRequested()
     */
    void folderMinimized(QcwTabFolder* folder, Qt::ToolBarArea fastViewToolBarArea);

    /**
     * @brief Emitted when a folder is restored from minimized or maximized state.
     * @param folder Pointer to the restored folder.
     * @note Also emitted for siblings the container brings back while undoing another
     *       folder's maximization.
     */
    void folderRestored(QcwTabFolder* folder);

    /**
     * @brief Emitted when a folder managed by this container is destroyed.
     * @param folder The folder that has just been removed from the container's bookkeeping.
     * @note The folder is already being destroyed; receivers must only compare the pointer,
     * never call into it.
     */
    void folderDestroyed(QcwTabFolder* folder);

    /**
     * @brief Emitted when the container's current folder changed.
     * @param folder The new current folder, or nullptr when none remains.
     * @note The consumer decides how to reflect this visually, since it also knows
     *       about floating folders outside this container.
     * @see currentFolder()
     */
    void currentFolderChanged(QcwTabFolder* folder);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;

    void dragMoveEvent(QDragMoveEvent* event) override;

    void dragLeaveEvent(QDragLeaveEvent* event) override;

    void dropEvent(QDropEvent* event) override;

    void showEvent(QShowEvent* event) override;

    /**
     * @details Keeps the splitter proportions of the layout that is being hidden, because the
     *          shared folder leaves for another perspective's container while this one is
     *          hidden and collapses the site that used to hold it.
     */
    void hideEvent(QHideEvent* event) override;

private:
    QcwTabFolderContainerPrivate* impl;
};

QT_END_NAMESPACE
