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
 */
enum class Icon
{
    Perspective,    ///< Multi-pane perspective layout.
    View,           ///< Framed view with a title bar.
    NewPerspective, ///< Perspective layout with a plus.
    NewView,        ///< View with a plus.
    FolderMaximize, ///< Square used by the maximize button.
    FolderRestore,  ///< Overlapping squares used by the restore button.
    FolderMinimize, ///< Horizontal line used by the minimize button.
    TabList         ///< Bulleted list used by the tab-list button.
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
