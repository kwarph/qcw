// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE

class QcwTabFolder;

/**
 * @class QcwFloatingFolderMonitor
 * @brief Process-wide channel that records which QcwWindow a floating folder belongs to.
 *
 * @details Dragging a tab out of a folder tears that tab off into a brand new floating
 * (top-level) QcwTabFolder. The window the tab came from is the only one that can adopt
 * the folder — it has to close the folder when the window closes, and its current
 * perspective has to write the folder into the saved state — yet the folder is created
 * deep inside QcwTabFolder's detach handling, which holds no pointer to any window.
 * Ownership therefore travels by id: the detached folder is stamped with the "WindowId"
 * dynamic property, and this monitor broadcasts the new folder to the whole process.
 *
 * @par Picking up your own folders
 * Every QcwWindow listens to floatingFolderCreated() and keeps the folders whose
 * "WindowId" equals its own QcwWindow::id(); the others are for a different window and are
 * ignored. Re-docking a floating folder into another window re-stamps the property, so the
 * broadcast is the only moment the association has to be announced.
 *
 * @par One monitor per process
 * The monitor keeps no state, and instance() builds the single global one on first use and
 * hands out that same object ever after. Constructing another monitor is possible but
 * pointless: nothing connects to it, so a folder detached through it is never registered
 * and is lost when the window state is saved.
 *
 * @see QcwTabFolder, QcwPerspective::registerFloatingFolder()
 */
class QCW_EXPORT QcwFloatingFolderMonitor : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a monitor.
     * @param parent Ignored in practice: the process-wide monitor created by instance() has
     *               no parent, because it must outlive every window that listens to it.
     * @see instance()
     */
    explicit QcwFloatingFolderMonitor(QObject* parent = nullptr);

    /**
     * @brief Returns the process-wide monitor.
     * @details Built on the first call and the same object ever after, so the pointer needs
     *          no check before it is emitted through or connected to.
     */
    static QcwFloatingFolderMonitor* instance();

signals:
    /**
     * @brief Emitted right after a floating folder has been detached and shown.
     * @param floatingFolder The new folder, a top-level window with Qt::WA_DeleteOnClose:
     *                       the window that adopts it owns it, receivers must not delete it.
     * @note Raised from the detach handler of the folder that lost the tab, so the source
     *       folder is still mid-operation — receivers may only work with @p floatingFolder.
     * @note Nothing is emitted when the tab could not be taken, and the id a receiver is
     *       looking for is read from the folder's "WindowId" property, not passed here.
     */
    void floatingFolderCreated(QcwTabFolder* floatingFolder);
};

/// Shorthand for the process-wide monitor, see QcwFloatingFolderMonitor::instance().
/// Guarded so a project that already defines it keeps its own definition.
#ifndef floatingFolderMonitor
#define floatingFolderMonitor QcwFloatingFolderMonitor::instance()
#endif

QT_END_NAMESPACE
