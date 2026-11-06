// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwOverlayScrollBar.h"

#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>

QT_BEGIN_NAMESPACE

QcwOverlayScrollBar::QcwOverlayScrollBar(Qt::Orientation orientation, QWidget* parent) :
    QScrollBar(orientation, parent)
{
    // The fade runs on a QGraphicsOpacityEffect; while the bar is fully faded it is also
    // transparent to the mouse, so clicks reach the content underneath.
    mEffect = new QGraphicsOpacityEffect(this);
    mEffect->setOpacity(0.0);
    setGraphicsEffect(mEffect);

    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setMouseTracking(true);

    mAnimation = new QPropertyAnimation(mEffect, "opacity", this);
    mAnimation->setDuration(200);
    mAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    // A fade-out that lands hands the mouse back to the content.
    connect(mAnimation, &QPropertyAnimation::finished, this, [this]()
    {
        if (mEffect->opacity() <= 0.01)
        {
            setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
    });

    mHideTimer = new QTimer(this);
    mHideTimer->setSingleShot(true);
    mHideTimer->setInterval(1500);
    connect(mHideTimer, &QTimer::timeout, this, &QcwOverlayScrollBar::hideWithAnimation);

    // Holding the handle stops the countdown; letting go starts it over.
    connect(this, &QScrollBar::sliderPressed, this, [this]()
    {
        mHideTimer->stop();
    });
    connect(this, &QScrollBar::sliderReleased, this, [this]()
    {
        mHideTimer->start();
    });
}

void QcwOverlayScrollBar::showWithAnimation()
{
    mHideTimer->stop();
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

    // A bar that is shown or already fading in is never interrupted — owners call this on
    // every scroll and pointer move, and restarting the countdown is all such a call needs.
    if (mEffect->opacity() >= 1.0 ||
        (mAnimation->state() == QAbstractAnimation::Running && mAnimation->endValue().toReal() == 1.0))
    {
        mHideTimer->start();
        return;
    }

    mAnimation->stop();
    mAnimation->setStartValue(mEffect->opacity());
    mAnimation->setEndValue(1.0);
    mAnimation->start();
    mHideTimer->start();
}

void QcwOverlayScrollBar::hideWithAnimation()
{
    // underMouse() misfires once a QGraphicsOpacityEffect is installed, so compare the
    // global pointer position against the bar's rectangle in the parent's coordinates.
    QPoint globalCursor = QCursor::pos();
    QPoint localCursor = parentWidget()->mapFromGlobal(globalCursor);
    bool isMouseOver = geometry().contains(localCursor);

    if (isMouseOver || isSliderDown())
    {
        mHideTimer->start();
        return;
    }
    if (mEffect->opacity() <= 0.0)
    {
        return;
    }

    mAnimation->stop();
    mAnimation->setStartValue(mEffect->opacity());
    mAnimation->setEndValue(0.0);
    mAnimation->start();
}

void QcwOverlayScrollBar::keepVisible()
{
    showWithAnimation();
}

void QcwOverlayScrollBar::enterEvent(QEnterEvent* event)
{
    QScrollBar::enterEvent(event);
    showWithAnimation();
}

void QcwOverlayScrollBar::leaveEvent(QEvent* event)
{
    QScrollBar::leaveEvent(event);
    mHideTimer->start();
}

QT_END_NAMESPACE
