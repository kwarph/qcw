// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QFrame>

QT_BEGIN_NAMESPACE

class QcwTabFolder;

/**
 * @class QcwTabFolderSite
 * @brief A one-folder placeholder that holds a slot in the docking layout.
 *
 * @details QcwTabFolderSite keeps the place in a splitter or other container, while the
 * QcwTabFolder that fills it can be swapped, parked in a fast view or taken out without
 * touching the surrounding layout.
 *
 * @par Ownership
 * - The site installs its folder as the single item of an internal box layout and makes
 *   it a child widget, so destroying the site destroys the folder as well unless the
 *   folder was taken out first.
 * - The site never deletes a folder on its own; setFolder() hands the replaced folder
 *   back to the caller.
 *
 * @see QcwTabFolder
 * @see QcwTabFolderContainer
 */
class QCW_EXPORT QcwTabFolderSite : public QFrame
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a site and installs its first folder.
     * @param folder The folder to display, or nullptr for an empty site. The folder
     *               becomes a child of the site, see the ownership notes above.
     * @param parent The parent widget. Ownership of the site is transferred to it.
     * @note The folder's visibility is left untouched here: installed into a site that
     *       is already visible, it is shown through the layout.
     */
    explicit QcwTabFolderSite(QcwTabFolder* folder = nullptr, QWidget* parent = nullptr);

    /**
     * @brief Gets the folder this site holds.
     * @return Pointer to the folder, or nullptr while the site is empty.
     * @note The pointer may name a folder that is currently installed in another site:
     *       a fast view moves the widget without clearing the site that keeps it for
     *       restore. setFolder() is what brings it back.
     * @see setFolder()
     */
    QcwTabFolder* folder() const
    {
        return mFolder;
    }

    /**
     * @brief Installs a folder in this site and returns the one it held.
     * @param newFolder The folder to display, or nullptr to empty the site.
     * @return The previously held folder, or nullptr. The caller owns it: it is detached
     *         from this site when it is still installed here, so it does not linger as
     *         an unmanaged child once it no longer fills the slot.
     * @details Passing the folder the site already names re-installs it, which is how a
     *          site takes its folder back after a fast view moved it elsewhere.
     * @note The new folder becomes a child of the site, is given an expanding size
     *       policy and is shown.
     * @see folder()
     */
    QcwTabFolder* setFolder(QcwTabFolder* newFolder);

private:
    /// The folder this site keeps a slot for. Nullptr while the site is empty; may name
    /// a folder another site currently holds, see folder().
    QcwTabFolder* mFolder { nullptr };
};

QT_END_NAMESPACE
