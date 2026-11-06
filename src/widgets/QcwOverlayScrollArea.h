// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QFlags>
#include <QScrollArea>

QT_BEGIN_NAMESPACE

class QcwOverlayScrollBar;

/**
 * @class QcwOverlayScrollArea
 * @brief A QScrollArea whose scroll bars float over the content and fade out of the way.
 *
 * @details Classic scroll bars take a strip off the viewport, so the content narrows and
 * reflows whenever one appears — and they stay on screen although they are only needed while
 * scrolling. This area turns both real scroll bars off and draws a QcwOverlayScrollBar over
 * the viewport instead: along the right edge for the vertical direction, along the bottom
 * edge for the horizontal one. The content keeps the whole viewport.
 *
 * The bars behave as the real ones do, and are simply see-through and fade out:
 * - Dragging a bar's handle scrolls the content, and clicking the trough above or below the
 *   handle pages it.
 * - A scroll fades the matching bar in, and the pointer coming within 20 px of a bar's edge
 *   fades it in as well. A bar the pointer is over stays up; one the pointer has left fades
 *   out 1500 ms later.
 * - A faded bar is transparent to the mouse, so clicks and the wheel go to the content
 *   underneath it.
 *
 * The same bars can be put on a scroll area of another class — a QTreeView, a
 * QPlainTextEdit — with QcwOverlayScrollBarAttacher. The bars share the real scroll bars'
 * range, page step, single step and value, and a direction that cannot be scrolled has no bar.
 *
 * @see QcwOverlayScrollBar QcwOverlayScrollBarAttacher
 */
class QCW_EXPORT QcwOverlayScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an empty area.
     * @param parent The parent widget.
     */
    explicit QcwOverlayScrollArea(QWidget* parent = nullptr);

protected:
    /// Lays the overlay bars out along the viewport's edges and picks up a new content.
    void resizeEvent(QResizeEvent* event) override;

    /**
     * @details Brings a bar up once the pointer comes near its edge, over the viewport or
     *          anywhere in the content.
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// Fades both bars out once the pointer has left the area as a whole.
    void leaveEvent(QEvent* event) override;

private:
    /// Copies the real bars' range, page step and single step, and hides a bar whose
    /// direction has nothing to scroll.
    void syncScrollBars();

private:
    QcwOverlayScrollBar* mVerticalBar { nullptr };
    QcwOverlayScrollBar* mHorizontalBar { nullptr };
};

QT_END_NAMESPACE
