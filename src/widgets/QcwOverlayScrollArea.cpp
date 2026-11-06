// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwOverlayScrollArea.h"
#include "QcwOverlayScrollBar.h"

#include <QCursor>
#include <QMouseEvent>
#include <QScrollBar>

QT_BEGIN_NAMESPACE

QcwOverlayScrollArea::QcwOverlayScrollArea(QWidget* parent) :
    QScrollArea(parent)
{
    // Switch the real bars off and enable mouse tracking, so moves over the viewport arrive.
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);

    // One bar per direction, floating on the viewport.
    mVerticalBar = new QcwOverlayScrollBar(Qt::Vertical, viewport());
    mHorizontalBar = new QcwOverlayScrollBar(Qt::Horizontal, viewport());
    mVerticalBar->raise();
    mHorizontalBar->raise();

    // Both start faded out; a scroll or a pointer near an edge brings them up.
    mVerticalBar->hideWithAnimation();
    mHorizontalBar->hideWithAnimation();

    // The value is mirrored both ways; the "did it change" checks on each side stop the
    // echo from looping.
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value)
    {
        if (mVerticalBar->value() != value)
        {
            QSignalBlocker blocker(mVerticalBar);
            mVerticalBar->setValue(value);
        }
    });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int value)
    {
        if (mHorizontalBar->value() != value)
        {
            QSignalBlocker blocker(mHorizontalBar);
            mHorizontalBar->setValue(value);
        }
    });
    connect(mVerticalBar, &QScrollBar::valueChanged, this, [this](int value)
    {
        if (verticalScrollBar()->value() != value)
        {
            verticalScrollBar()->setValue(value);
        }
    });
    connect(mHorizontalBar, &QScrollBar::valueChanged, this, [this](int value)
    {
        if (horizontalScrollBar()->value() != value)
        {
            horizontalScrollBar()->setValue(value);
        }
    });

    // A changed range is copied over in full.
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, &QcwOverlayScrollArea::syncScrollBars);
    connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, &QcwOverlayScrollArea::syncScrollBars);

    // Bring the matching bar up on every scroll — wheel, keyboard, drag — for a direction
    // that can scroll.
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]()
    {
        if (verticalScrollBar()->maximum() > 0)
        {
            mVerticalBar->showWithAnimation();
        }
    });
    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]()
    {
        if (horizontalScrollBar()->maximum() > 0)
        {
            mHorizontalBar->showWithAnimation();
        }
    });

    // Watch pointer moves over the viewport (and, once set, over the content).
    viewport()->installEventFilter(this);
}

void QcwOverlayScrollArea::syncScrollBars()
{
    QSignalBlocker vBlocker(mVerticalBar);
    QSignalBlocker hBlocker(mHorizontalBar);

    mVerticalBar->setRange(verticalScrollBar()->minimum(), verticalScrollBar()->maximum());
    mVerticalBar->setPageStep(verticalScrollBar()->pageStep());
    mVerticalBar->setSingleStep(verticalScrollBar()->singleStep());

    mHorizontalBar->setRange(horizontalScrollBar()->minimum(), horizontalScrollBar()->maximum());
    mHorizontalBar->setPageStep(horizontalScrollBar()->pageStep());
    mHorizontalBar->setSingleStep(horizontalScrollBar()->singleStep());

    mVerticalBar->setVisible(verticalScrollBar()->maximum() > 0);
    mHorizontalBar->setVisible(horizontalScrollBar()->maximum() > 0);
}

void QcwOverlayScrollArea::resizeEvent(QResizeEvent* event)
{
    QScrollArea::resizeEvent(event);

    // Watch the content widget too, so a pointer anywhere in it can raise a bar. The
    // property marks it done; setWidget() hands in a new widget without the mark, which
    // the next resize then picks up.
    if (widget() && !widget()->property("_overlay_filter_installed").toBool())
    {
        widget()->installEventFilter(this);
        widget()->setMouseTracking(true);
        widget()->setProperty("_overlay_filter_installed", true);
    }

    // Bars 12 px thick along the viewport's right and bottom edges, 2 px in from them.
    int w = viewport()->width();
    int h = viewport()->height();
    int margin = 2;
    int thickness = 12;

    mVerticalBar->setGeometry(w - thickness - margin, margin, thickness, h - 2 * margin);
    mHorizontalBar->setGeometry(margin, h - thickness - margin, w - 2 * margin, thickness);
}

bool QcwOverlayScrollArea::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == viewport() || watched == widget())
    {
        if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            // Moves arrive from the viewport or the content, in different coordinates;
            // going through global ones treats both alike.
            QPoint viewportPos = viewport()->mapFromGlobal(mouseEvent->globalPosition().toPoint());

            // A pointer within 20 px of an edge raises the bar lying there, if that
            // direction can be scrolled at all.
            if (viewportPos.x() > viewport()->width() - 20 && !mVerticalBar->isHidden())
            {
                mVerticalBar->raise();
                mVerticalBar->showWithAnimation();
            }
            if (viewportPos.y() > viewport()->height() - 20 && !mHorizontalBar->isHidden())
            {
                mHorizontalBar->raise();
                mHorizontalBar->showWithAnimation();
            }
        }
    }
    return QScrollArea::eventFilter(watched, event);
}

// The bars fade out only once the pointer has left the whole area — a Leave also fires
// when it moves onto a child such as the viewport, so the position is re-checked first.
void QcwOverlayScrollArea::leaveEvent(QEvent* event)
{
    QScrollArea::leaveEvent(event);
    if (!rect().contains(mapFromGlobal(QCursor::pos())))
    {
        mVerticalBar->hideWithAnimation();
        mHorizontalBar->hideWithAnimation();
    }
}

QT_END_NAMESPACE
