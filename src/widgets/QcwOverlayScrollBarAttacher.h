// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QObject>

QT_BEGIN_NAMESPACE

class QAbstractScrollArea;
class QcwOverlayScrollBar;

/**
 * @class QcwOverlayScrollBarAttacher
 * @brief Floats QcwOverlayScrollBar bars over a scroll area of any class.
 *
 * @details QcwOverlayScrollArea has the overlay built in; this class gives the same behaviour
 * to a scroll area that cannot be turned into one — a QTreeView, a QPlainTextEdit, anything
 * derived from QAbstractScrollArea. Constructing the attacher with a target is the whole
 * setup: the target's real scroll bars are switched off, an overlay bar takes the place of
 * every direction that can scroll, lying along the right and bottom edge of the viewport,
 * and the two sets of bars stay mirrored in value, range, page step and single step. A
 * scroll, the pointer coming within 20 px of a bar's edge or a wheel turn fades the
 * matching bar in; it fades out 1500 ms after the pointer has left the area.
 *
 * The attacher needs no other call. The bars are children of the target, so they go with it.
 *
 * @see QcwOverlayScrollBar QcwOverlayScrollArea
 */
class QCW_EXPORT QcwOverlayScrollBarAttacher : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Replaces the scroll bars of @p target with overlay bars.
     * @param target The scroll area to decorate. A null target leaves the attacher doing nothing.
     * @param parent The parent object, normally the widget that owns the target.
     */
    explicit QcwOverlayScrollBarAttacher(QAbstractScrollArea* target, QObject* parent = nullptr);

    /// Drops the event filter. The bars belong to the target and are cleaned up with it.
    ~QcwOverlayScrollBarAttacher();

protected:
    /// Re-lays the bars when the target or its viewport resizes, fades them out when the
    /// pointer leaves, and brings them up near an edge or on a wheel turn.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Places the bars along the viewport's edges, in the target's own coordinates.
    void updateGeometry();

    /// Copies the real bars' range, page step, single step and value, and hides a bar whose
    /// direction has nothing to scroll.
    void syncScrollBars();

private:
    QAbstractScrollArea* mTarget;
    QcwOverlayScrollBar* mVerticalBar;
    QcwOverlayScrollBar* mHorizontalBar;
};

QT_END_NAMESPACE
