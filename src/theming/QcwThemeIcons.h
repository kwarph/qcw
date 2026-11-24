// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QIcon>

QT_BEGIN_NAMESPACE

namespace qcw
{

/**
 * @enum Icon
 * @brief Built-in icons that follow the application palette.
 *
 * @details The artwork is taken from Microsoft's Codicons (CC BY 4.0), so the workbench chrome
 * shares one 16x16 grid with the icons an application ships. The files under
 * `src/resources/icons` keep their `qcw::Icon` names; the codicon each one came from, the
 * license and the refresh instructions are recorded in that directory's README.
 */
enum class Icon
{
    Perspective,    ///< Multi-pane perspective layout (codicon `layout`).
    View,           ///< Framed view with a title bar (codicon `window`).
    NewPerspective, ///< Perspective layout with a plus (codicon `new-collection`).
    NewView,        ///< View with a plus (codicon `new-session`).
    FolderMaximize, ///< Square used by the maximize button (codicon `chrome-maximize`).
    FolderRestore,  ///< Overlapping squares used by the restore button (codicon `chrome-restore`).
    FolderMinimize, ///< Horizontal line used by the minimize button (codicon `chrome-minimize`).
    TabList         ///< Bulleted list used by the tab-list button (codicon `list-unordered`).
};

/**
 * @brief Returns the built-in @p icon, colored from the current application palette.
 *
 * @details The SVG source is loaded once and tinted at paint time, so an icon obtained
 * before a theme switch still follows the new palette without being re-created. Normal
 * and active icons use QPalette::WindowText mixed slightly towards QPalette::Window so
 * the strokes do not read as pure black or pure white; disabled icons use the disabled
 * WindowText, and selected icons use QPalette::HighlightedText.
 *
 * The returned QIcon shares a cached engine with later calls for the same icon. Call
 * this function on the GUI thread.
 */
QCW_EXPORT QIcon themeIcon(Icon icon);

} // namespace qcw

QT_END_NAMESPACE
