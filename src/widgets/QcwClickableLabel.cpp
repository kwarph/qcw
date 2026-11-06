// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwClickableLabel.h"
#include <QMouseEvent>

QT_BEGIN_NAMESPACE

QcwClickableLabel::QcwClickableLabel(QWidget* parent, Qt::WindowFlags f) :
    QLabel(parent, f)
{
}

QcwClickableLabel::QcwClickableLabel(const QString& text, QWidget* parent, Qt::WindowFlags f) :
    QLabel(text, parent, f)
{
}

void QcwClickableLabel::mouseReleaseEvent(QMouseEvent* event)
{
    // Qt's implicit mouse grab still delivers the release after a press here when the
    // pointer has left the label, so the release position decides whether this was a click.
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
    {
        emit clicked();
    }

    QLabel::mouseReleaseEvent(event);
}

void QcwClickableLabel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit doubleClicked();
    }

    QLabel::mouseDoubleClickEvent(event);
}

QT_END_NAMESPACE
