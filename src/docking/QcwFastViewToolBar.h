// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QToolBar>
#include <QSize>

QT_BEGIN_NAMESPACE

class QEvent;
class QcwTabFolder;
class QcwFastViewToolBarPrivate;

/**
 * @class QcwFastViewToolBar
 * @brief The row of buttons a minimized QcwTabFolder leaves behind, modelled on Eclipse's fast view.
 *
 * @details One toolbar stands for one minimized folder and is docked to the left or the right edge of
 * the window. It always starts with a Restore button, followed either by one button per tab or by a
 * single button for the whole folder, depending on @p listAllTabs. QcwTabFolderContainer does not make
 * these itself; it reports what it minimized through folderMinimized() and the workbench builds the
 * fast view in answer to that.
 *
 * @par Hosting the folder
 * The constructor takes the folder out of the QcwTabFolderSite it was minimized from and installs it
 * in an internal Qt::Popup site, so the toolbar owns the folder for as long as it exists: destroying
 * a toolbar destroys the folder still parked in it. Pressing a tab button pops the folder up over the
 * area @p anchorWidget covers and selects that tab; pressing Restore puts the folder back into its
 * original site and asks the layout to show it again.
 *
 * @par The popup around the folder
 * A folder and the page it shows have the same colors as a folder of the layout, so a folder filling
 * its popup edge to edge has no boundary the eye can find. The window style decides how that boundary
 * is drawn, through two values it leaves in QcwSessionData before any fast view exists:
 * @c "fastViewMargin", the width of the popup color kept around the folder, and @c "fastViewHasBorder",
 * whether the popup site draws a frame around the folder instead. A style whose folders are separated
 * by margins sets the first and none of the second; a style whose folders are fenced by a one pixel
 * window border sets the second and leaves the first at zero.
 *
 * With a margin, one goes at the top, one at the bottom, and one on the side that faces the window -
 * the right for a toolbar docked on the right, the left for one docked on the left. The folder's
 * top-left corner sits on the popup's and the popup on the anchor's, so the folder lines up with the
 * folders around it, pushed down by the top margin to the same corner of the window they start from.
 * The popup is @p fastViewSize plus those margins, so the folder inside keeps the size it was given.
 * With a border, the folder is inset by the frame width instead and the popup grows by that much, so
 * the frame lines up with the window's own border.
 *
 * @par Closing itself
 * Nothing is expected to remove the toolbar. QcwTabFolder::aboutToRestore() and
 * QcwTabFolder::aboutToDelete() both hand the folder back and schedule the toolbar for deletion, so
 * the fast view disappears as soon as the folder no longer needs it.
 *
 * @note The folder has to be installed in a QcwTabFolderSite when the toolbar is built, because that
 * is the site Restore returns it to.
 *
 * @see QcwTabFolder, QcwTabFolderContainer::folderMinimized(), QcwTabFolderSite
 */
class QCW_EXPORT QcwFastViewToolBar : public QToolBar
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the fast view for a minimized tab folder.
     *
     * @param folder The folder to host. It is reparented into the popup site owned by the toolbar.
     * @param anchorWidget The area the popup is opened over. The toolbar is docked beside it and the
     *                     popup is aligned to the edge they share, so this is a positioning reference
     *                     only: the widget neither contains nor owns the folder.
     * @param fastViewSize The size the folder is shown at inside the popup; the popup itself is
     *                     this size plus the margin it keeps around the folder.
     * @param listAllTabs One button per tab when true, one button for the whole folder when false.
     *                    A folder without tabs always gets the single button.
     * @param parent The parent widget. Ownership of the toolbar is transferred to it.
     */
    explicit QcwFastViewToolBar(QcwTabFolder* folder,
                                QWidget* anchorWidget,
                                const QSize& fastViewSize = QSize(600, 400),
                                bool listAllTabs = true,
                                QWidget* parent = nullptr);

    /** @brief Destroys the toolbar and, with it, a folder that is still parked in its popup. */
    ~QcwFastViewToolBar();

    /**
     * @brief Gets the folder this toolbar is the fast view for.
     *
     * @return The hosted folder. The pointer stays valid for as long as the toolbar lives.
     */
    QcwTabFolder* folder() const;

    /**
     * @brief Tells whether @p folder still sits in this fast view's popup.
     *
     * @details The toolbar keeps naming its folder after another site has taken it (see
     *          QcwTabFolderSite), so the parent is what tells a folder still parked here from
     *          one that moved on.
     */
    bool parksFolder(const QcwTabFolder* folder) const;

    /**
     * @brief Takes the folder out of this fast view and hands it back to the site it came from.
     *
     * @return True when the folder was still parked here and has been released; false when
     *         another site has taken it in the meantime.
     *
     * @details The folder is put back in its original site with the features the fast view
     *          disabled restored, but without asking the layout to show it and without
     *          touching its state. QcwPerspective::clear() uses this for the window's shared
     *          folder: destroying a toolbar destroys a folder still parked in it, and clearing
     *          a perspective must not take the window-owned shared folder down.
     */
    bool releaseFolder();

protected:
    /**
     * @brief Rebuilds the buttons when the UI language changes.
     * @details The buttons carry the folder's tab texts and two labels of the toolbar's own,
     *          all of them captured when they were created, so the only way to refresh them is
     *          to build the set again.
     */
    void changeEvent(QEvent* event) override;

private:
    QcwFastViewToolBarPrivate* impl;
};

QT_END_NAMESPACE
