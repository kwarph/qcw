// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwOverlayScrollBarAttacher.h"
#include "QcwOverlayScrollArea.h"
#include "QcwOverlayScrollBar.h"
#include <QCursor>
#include <QMouseEvent>
#include <QTimer>

QT_BEGIN_NAMESPACE

QcwOverlayScrollBarAttacher::QcwOverlayScrollBarAttacher(QAbstractScrollArea* target, QObject* parent) :
    QObject(parent), mTarget(target)
{
    if (!mTarget)
    {
        return;
    }

    // Switch the real bars off and enable mouse tracking, so moves over the viewport arrive.
    mTarget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mTarget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mTarget->setMouseTracking(true);
    mTarget->viewport()->setMouseTracking(true);

    // The overlay bars. They are parented to mTarget itself, not to the viewport;
    // updateGeometry() expresses their place through the viewport's rect in the target's
    // coordinates.
    mVerticalBar = new QcwOverlayScrollBar(Qt::Vertical, mTarget);
    mHorizontalBar = new QcwOverlayScrollBar(Qt::Horizontal, mTarget);

    // A mouse event the bars did not take must not propagate to the target behind them.
    mVerticalBar->setAttribute(Qt::WA_NoMousePropagation, true);
    mHorizontalBar->setAttribute(Qt::WA_NoMousePropagation, true);

    mVerticalBar->raise();
    mHorizontalBar->raise();

    mVerticalBar->hideWithAnimation();
    mHorizontalBar->hideWithAnimation();

    // Keep the two sets of bars in step.
    QScrollBar* origVBar = mTarget->verticalScrollBar();
    QScrollBar* origHBar = mTarget->horizontalScrollBar();

    // Real bars -> overlay bars.
    connect(origVBar, &QScrollBar::valueChanged, this, [this](int value)
    {
        if (mVerticalBar->value() != value)
        {
            QSignalBlocker blocker(mVerticalBar);
            mVerticalBar->setValue(value);
        }
    });
    connect(origHBar, &QScrollBar::valueChanged, this, [this](int value)
    {
        if (mHorizontalBar->value() != value)
        {
            QSignalBlocker blocker(mHorizontalBar);
            mHorizontalBar->setValue(value);
        }
    });

    // Overlay bars -> real bars. No signal blocker here: the real bar receiving the
    // value is what scrolls the content.
    connect(mVerticalBar, &QScrollBar::valueChanged, origVBar, &QScrollBar::setValue);
    connect(mHorizontalBar, &QScrollBar::valueChanged, origHBar, &QScrollBar::setValue);

    // A changed range is copied over in full.
    connect(origVBar, &QScrollBar::rangeChanged, this, &QcwOverlayScrollBarAttacher::syncScrollBars);
    connect(origHBar, &QScrollBar::rangeChanged, this, &QcwOverlayScrollBarAttacher::syncScrollBars);

    // Bring the matching bar up on every scroll — wheel, keyboard, drag — for a
    // direction that can scroll.
    connect(origVBar, &QScrollBar::valueChanged, this, [this]()
    {
        if (mTarget->verticalScrollBar()->maximum() > 0)
        {
            mVerticalBar->showWithAnimation();
        }
    });
    connect(origHBar, &QScrollBar::valueChanged, this, [this]()
    {
        if (mTarget->horizontalScrollBar()->maximum() > 0)
        {
            mHorizontalBar->showWithAnimation();
        }
    });

    // The target is watched for resizes and leaves, its viewport for pointer moves and wheel turns.
    mTarget->installEventFilter(this);
    mTarget->viewport()->installEventFilter(this);

    // Place and sync the bars once the event loop turns, by which time the surrounding
    // layout has settled.
    QTimer::singleShot(0, this, &QcwOverlayScrollBarAttacher::updateGeometry);
    QTimer::singleShot(0, this, &QcwOverlayScrollBarAttacher::syncScrollBars);
}

QcwOverlayScrollBarAttacher::~QcwOverlayScrollBarAttacher()
{
    // Nothing to undo: the bars are children of the target and are cleaned up with it.
}

void QcwOverlayScrollBarAttacher::syncScrollBars()
{
    if (!mTarget)
    {
        return;
    }

    QSignalBlocker vBlocker(mVerticalBar);
    QSignalBlocker hBlocker(mHorizontalBar);

    QScrollBar* origVBar = mTarget->verticalScrollBar();
    QScrollBar* origHBar = mTarget->horizontalScrollBar();

    mVerticalBar->setRange(origVBar->minimum(), origVBar->maximum());
    mVerticalBar->setPageStep(origVBar->pageStep());
    mVerticalBar->setSingleStep(origVBar->singleStep());
    // Re-apply the value after the range: setRange() clamps it and could reset it.
    mVerticalBar->setValue(origVBar->value());

    mHorizontalBar->setRange(origHBar->minimum(), origHBar->maximum());
    mHorizontalBar->setPageStep(origHBar->pageStep());
    mHorizontalBar->setSingleStep(origHBar->singleStep());
    mHorizontalBar->setValue(origHBar->value());

    // A direction with nothing to scroll gets no bar at all.
    if (origVBar->maximum() == 0)
    {
        mVerticalBar->hide();
    }
    else
    {
        mVerticalBar->show();
    }

    if (origHBar->maximum() == 0)
    {
        mHorizontalBar->hide();
    }
    else
    {
        mHorizontalBar->show();
    }
}

void QcwOverlayScrollBarAttacher::updateGeometry()
{
    if (!mTarget)
    {
        return;
    }

    // The bars belong to the target, so their geometry is the viewport's rect expressed
    // in the target's coordinates.
    QRect vpRect = mTarget->viewport()->geometry();
    int w = vpRect.width();
    int h = vpRect.height();
    int x = vpRect.x();
    int y = vpRect.y();

    int margin = 2;
    int thickness = 12;

    mVerticalBar->setGeometry(x + w - thickness - margin, y + margin, thickness, h - 2 * margin);
    mHorizontalBar->setGeometry(x + margin, y + h - thickness - margin, w - 2 * margin, thickness);

    mVerticalBar->raise();
    mHorizontalBar->raise();
}

bool QcwOverlayScrollBarAttacher::eventFilter(QObject* watched, QEvent* event)
{
    if (!mTarget)
    {
        return QObject::eventFilter(watched, event);
    }

    // A resize of the target or of its viewport re-lays the bars.
    if ((watched == mTarget || watched == mTarget->viewport()) && event->type() == QEvent::Resize)
    {
        // Queued with a 0 ms timer so the target has finished resizing its own viewport
        // in the event (QPlainTextEdit does that inside its handler).
        QTimer::singleShot(0, this, &QcwOverlayScrollBarAttacher::updateGeometry);
    }

    // Leaving the target fades the bars out. A Leave also fires when the pointer merely
    // moved onto a child, so the global position is checked before hiding.
    if (watched == mTarget && event->type() == QEvent::Leave)
    {
        if (!mTarget->rect().contains(mTarget->mapFromGlobal(QCursor::pos())))
        {
            mVerticalBar->hideWithAnimation();
            mHorizontalBar->hideWithAnimation();
        }
    }

    // A pointer within 20 px of an edge raises the bar lying there; a wheel turn raises both.
    if (watched == mTarget->viewport())
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint pos = mouseEvent->position().toPoint();

            if (pos.x() > mTarget->viewport()->width() - 20 && !mVerticalBar->isHidden())
            {
                mVerticalBar->raise();
                mVerticalBar->showWithAnimation();
            }
            if (pos.y() > mTarget->viewport()->height() - 20 && !mHorizontalBar->isHidden())
            {
                mHorizontalBar->raise();
                mHorizontalBar->showWithAnimation();
            }
        }
        else if (event->type() == QEvent::Wheel)
        {
            if (!mVerticalBar->isHidden())
            {
                mVerticalBar->showWithAnimation();
            }
            if (!mHorizontalBar->isHidden())
            {
                mHorizontalBar->showWithAnimation();
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

QT_END_NAMESPACE
