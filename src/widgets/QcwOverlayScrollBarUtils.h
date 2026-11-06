// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

QT_BEGIN_NAMESPACE

class QAbstractScrollArea;
class QObject;

/**
 * @brief Replaces the scroll bars of @p target with overlay bars, without naming the class that
 * does it.
 *
 * @details Attaching an overlay scroll bar is a set-up-and-forget operation: the object that
 * performs it (QcwOverlayScrollBarAttacher) needs no further call and exposes nothing to read, so
 * keeping it around would only hand a caller a handle on an implementation detail. This function
 * builds it, parents it to @p parent and drops its own reference, which is the whole public
 * interface.
 *
 * A scroll area whose class the caller chooses can be a QcwOverlayScrollArea instead, which has
 * the overlay built in and needs nothing attached; this is for the scroll areas whose class is
 * fixed — a QTreeView, a QPlainTextEdit, anything derived from QAbstractScrollArea.
 *
 * Call it once, while the scroll area is being configured, before it is shown.
 *
 * @param target The scroll area to decorate. A null target leaves nothing attached.
 * @param parent The owner of the attacher, normally the widget that owns @p target; when @p parent
 *               is destroyed the attachment ends with it. The overlay bars themselves are children
 *               of @p target and go with it.
 *
 * @code
 * // Example usage, as the demo views do it:
 * mTree = new QTreeView { this };
 * mTree->setHeaderHidden(true);
 * attachOverlayScrollBar(mTree, this);
 * @endcode
 *
 * @see QcwOverlayScrollBarAttacher, QcwOverlayScrollArea, QcwOverlayScrollBar
 */
QCW_EXPORT void attachOverlayScrollBar(QAbstractScrollArea* target, QObject* parent);

QT_END_NAMESPACE
