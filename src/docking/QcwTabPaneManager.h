// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QMap>
#include <QSet>
#include <atomic>

QT_BEGIN_NAMESPACE

class QWidget;
class QcwTabFolder;

/**
 * @class QcwTabPaneManager
 * @brief Tracks which folders display each tab pane and owns the pane-id counter.
 *
 * A pane widget is referenced once per displaying folder and is owned by none
 * of them while more than one folder shows it. When the last reference
 * disappears through removeReference() the manager deletes the pane; a pane
 * destroyed by any other path is dropped from the bookkeeping automatically.
 *
 * @par Thread safety
 * nextPaneId() and resetStartingPaneId() are atomic and safe from any thread;
 * the reference bookkeeping must only be touched from the GUI thread.
 *
 * @note This is the single global instance: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because lookups go through
 * the @c tabPaneManager macro, so a second manager would never be seen.
 * @see QcwTabFolder, QcwTabFolderContainer
 */
class QCW_EXPORT QcwTabPaneManager
{
public:
    /**
     * @brief Returns the process-wide singleton instance.
     * @return Pointer to the one manager, created on first use.
     */
    static QcwTabPaneManager* instance();

    /**
     * @brief Registers @p folder as one holder of @p tabPane.
     *
     * Idempotent: a folder holds a given pane at most once. A null pane
     * or folder is ignored.
     *
     * @param tabPane The pane widget being displayed.
     * @param folder The folder that now displays it.
     */
    void addReference(QWidget* tabPane, QcwTabFolder* folder);

    /**
     * @brief Unregisters @p folder as a holder of @p tabPane.
     *
     * When this was the last holder, the pane is deleteLater()ed. Unknown
     * panes and folders are ignored.
     *
     * @param tabPane The pane widget that is no longer displayed here.
     * @param folder The folder releasing it.
     */
    void removeReference(QWidget* tabPane, QcwTabFolder* folder);

    /**
     * @brief Returns the folders currently showing @p tabPane.
     * @param tabPane The pane widget to query.
     * @return Copy of the holder set, empty for an unknown pane.
     */
    QSet<QcwTabFolder*> getReferences(QWidget* tabPane) const;

    /**
     * @brief Direct read-only view of the pane-to-folders map.
     *
     * Snapshot the keys (e.g. with keys()) before anything that could add or
     * remove a reference; those invalidate iterators and references into the
     * map.
     *
     * @return The internal map, not a copy.
     */
    const auto& allTabPanes() const
    {
        return mTabPanes;
    }

    /**
     * @brief Hands out the next pane id.
     * @return An id ascending from 0, never reused within one counter lifetime.
     */
    int nextPaneId();

    /**
     * @brief Raises the starting point of the pane-id counter.
     *
     * Subsequent nextPaneId() calls continue at @p id, for resuming past ids
     * loaded from a saved layout. Only ever raises the counter, matching the
     * never-decreasing contract of nextPaneId().
     *
     * @param id The lowest value the next hand-out may take.
     */
    void resetStartingPaneId(int id);

    /**
     * @brief Forgets every reference.
     *
     * With @p deletePanes the panes are deleteLater()ed first; either way
     * callers must not hand those widgets to a later add without
     * re-referencing them. Also restarts the pane-id counter at 0, so this is
     * for whole-session teardown (new layout, restore failure), not for
     * closing individual panes.
     *
     * @param deletePanes Whether the tracked pane widgets should be deleted.
     */
    void clear(bool deletePanes);

private:
    QMap<QWidget*, QSet<QcwTabFolder*>> mTabPanes;
    std::atomic_int mPaneId { 0 };
};

#ifndef tabPaneManager
#define tabPaneManager QcwTabPaneManager::instance()
#endif

QT_END_NAMESPACE
