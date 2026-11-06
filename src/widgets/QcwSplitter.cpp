// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwSplitter.h"

#include <QPainter>

QT_BEGIN_NAMESPACE

QcwSplitterHandle::QcwSplitterHandle(Qt::Orientation orientation, QSplitter* parent) :
    QSplitterHandle { orientation, parent }
{
}

void QcwSplitterHandle::paintEvent(QPaintEvent*)
{
    // A narrow handle widens its grab area with contents margins and masks the rest of the
    // widget, so the groove has to stay inside contentsRect().
    QPainter painter(this);
    const QRect area = contentsRect();
    const QColor dark = palette().color(QPalette::Dark);
    const QColor light = palette().color(QPalette::Light);

    if (orientation() == Qt::Horizontal)
    {
        painter.fillRect(QRect { area.left(), area.top(), 1, area.height() }, dark);
        painter.fillRect(QRect { area.left() + 1, area.top(), 1, area.height() }, light);
    }
    else
    {
        painter.fillRect(QRect { area.left(), area.top(), area.width(), 1 }, dark);
        painter.fillRect(QRect { area.left(), area.top() + 1, area.width(), 1 }, light);
    }
}

// -----------------------------------------------------------------------------

QcwSplitter::QcwSplitter(QWidget* parent) :
    QcwSplitter { Qt::Horizontal, parent }
{
}

QcwSplitter::QcwSplitter(Qt::Orientation orientation, QWidget* parent) :
    QSplitter { orientation, parent }
{
    setHandleWidth(2);
}

QSplitterHandle* QcwSplitter::createHandle()
{
    return new QcwSplitterHandle { orientation(), this };
}

QT_END_NAMESPACE
