// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QLabel>

QT_BEGIN_NAMESPACE

class QMouseEvent;

/**
 * @class QcwClickableLabel
 * @brief A QLabel that reports clicks on itself.
 *
 * @details QLabel has no click signal, so a caption meant to act like a link or a button
 * needs a subclass to report one. This label emits clicked() when the left button is
 * released over it, and doubleClicked() when that release is the second one of a double
 * click. Text, alignment, word wrap, text interaction and painting stay QLabel's.
 *
 * Only a release that lands on the label counts: a press here followed by a drag out and a
 * release outside still reaches the label through Qt's implicit mouse grab, but emits
 * nothing.
 *
 * @par Double clicks
 * The mouse sequence of a double click is press, release, double-click, release, so the
 * label emits clicked(), doubleClicked() and clicked() again, in that order. A consumer
 * that connects both signals has to expect the single-click handler to run before the
 * double-click one.
 *
 * @note The label is mouse-only: it takes no focus and has no keyboard activation. It also
 * leaves QLabel's text interaction untouched, so selectable text still selects and an
 * anchor still emits linkActivated() in addition to clicked(). Turn text interaction off
 * on a label that should behave like a plain button.
 */
class QCW_EXPORT QcwClickableLabel : public QLabel
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an empty label.
     * @param parent The parent widget.
     * @param f The window flags, forwarded to QLabel.
     */
    explicit QcwClickableLabel(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    /**
     * @brief Constructs a label showing @p text.
     * @param text The label text, plain or rich.
     * @param parent The parent widget.
     * @param f The window flags, forwarded to QLabel.
     */
    explicit QcwClickableLabel(const QString& text, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

signals:
    /**
     * @brief Emitted when the left button is released over the label.
     * @details Named after QAbstractButton::clicked() rather than anything in QLabel: a
     *          press that starts elsewhere, or a release outside the label, emits nothing.
     */
    void clicked();

    /**
     * @brief Emitted when the label receives a left-button double click.
     * @details Qt delivers the double-click event before the second release, so clicked()
     *          has already fired for the first click when this signal arrives; see the
     *          class notes.
     */
    void doubleClicked();

protected:
    /**
     * @brief Emits clicked() for a left-button release inside the label.
     * @param event The release event, passed on to QLabel afterwards.
     */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /**
     * @brief Emits doubleClicked() for a left-button double click.
     * @param event The double-click event, passed on to QLabel afterwards.
     */
    void mouseDoubleClickEvent(QMouseEvent* event) override;
};

QT_END_NAMESPACE
