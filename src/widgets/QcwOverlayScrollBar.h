// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QScrollBar>

QT_BEGIN_NAMESPACE

class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;

/**
 * @class QcwOverlayScrollBar
 * @brief A see-through scroll bar that appears while the pointer is near it.
 *
 * @details A real QScrollBar, drawn by the themes with a rounded, semi-transparent handle and
 * no arrows, floating over the content of a scroll area. It starts faded out. A scroll, or the
 * pointer entering the bar, fades it in over 200 ms; from then on a 1500 ms countdown fades it
 * out again, unless the pointer is over the bar or its handle is held.
 *
 * While the bar is faded out it is transparent to the mouse, so a click over it reaches the
 * content underneath. The bar is still a full scroll bar: a press on its handle drags the
 * content, and a press on the trough above or below the handle pages the content, exactly as
 * a real bar does. Neither is possible while it is faded out, which is what keeps the bar out
 * of the way of the content.
 *
 * The look comes from the themes' QcwOverlayScrollBar rules, which follow those of QScrollBar.
 *
 * @see QcwOverlayScrollArea
 */
class QCW_EXPORT QcwOverlayScrollBar : public QScrollBar
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a bar, faded out.
     * @param orientation The direction the bar scrolls in.
     * @param parent The parent widget, normally the viewport of a scroll area.
     */
    QcwOverlayScrollBar(Qt::Orientation orientation, QWidget* parent = nullptr);

    /**
     * @brief Fades the bar in and counts down from 1500 ms.
     * @details Calling this while the bar is already fading in or fully shown leaves the fade
     *          alone and only starts the countdown over, so it is safe to call on every scroll
     *          and on every pointer move near the bar.
     */
    void showWithAnimation();

    /**
     * @brief Fades the bar out of the way, unless the pointer is on it or its handle is held.
     * @details In that case the countdown is started over instead, so the bar goes away once
     *          the pointer has left it and the handle has been let go. A bar that is already
     *          faded out is left alone.
     */
    void hideWithAnimation();

    /// Same as showWithAnimation(): fades the bar in and restarts the 1500 ms countdown.
    void keepVisible();

protected:
    /// Fades the bar in while the pointer is over it.
    void enterEvent(QEnterEvent* event) override;

    /// Starts the countdown once the pointer has left the bar.
    void leaveEvent(QEvent* event) override;

private:
    QGraphicsOpacityEffect* mEffect { nullptr };
    QPropertyAnimation* mAnimation { nullptr };
    QTimer* mHideTimer { nullptr };
};

QT_END_NAMESPACE
