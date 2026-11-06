// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwNotification.h"
#include <QFrame>

QT_BEGIN_NAMESPACE

class QcwNotificationBoxPrivate;

/**
 * @class QcwNotificationBox
 * @brief The card that displays one QcwNotification.
 *
 * @details The box is a parentless, always-on-top window: a tool window on Wayland and the
 * desktop platforms, and a tooltip window under X11 so window managers such as KDE's do not
 * add a taskbar entry for every card. It stays above the application and remains visible
 * while the application is minimized. It has no title bar and is positioned by
 * QcwNotificationManager. Use createDefaultNotificationBox() or a creator installed with
 * QcwNotificationManager::setNotificationBoxCreator() to obtain a fully built card; a
 * subclass constructed directly has to call setupUi() itself.
 *
 * The default card shows the level icon, the title, the rich text body, a close button and
 * one button per QcwNotification::actions entry. The body's links are forwarded through
 * linkActivated() and are not opened here. When QcwNotification::timeoutMs is positive, the
 * card closes itself after that time, except while the mouse hovers over it.
 *
 * The box deletes itself on close, so it must be heap-allocated. A card that is shown is
 * deleted when it closes; one that is never shown must be deleted by its creator. It belongs
 * to the GUI thread.
 *
 * @note The default layout uses the application palette, but the accent color per level is
 * fixed and does not come from the active theme.
 *
 * @see QcwNotification, QcwNotificationManager
 */
class QCW_EXPORT QcwNotificationBox : public QFrame
{
    Q_OBJECT

public:
    /**
     * @brief Builds a card for @p notification and shows nothing yet.
     * @details The card is a top-level window; call show() or let the manager do it. The
     * actions in @p notification are borrowed and must outlive the card.
     */
    explicit QcwNotificationBox(const QcwNotification& notification);
    ~QcwNotificationBox() override;

    /**
     * @brief Creates a card with the default layout and calls setupUi().
     * @param notification The notification to display; it is copied into the card.
     * @return A new, parentless card. A shown card deletes itself on close; a card that is
     *         never shown must be deleted by the caller.
     */
    static QcwNotificationBox* createDefaultNotificationBox(const QcwNotification& notification);

    /**
     * @brief Builds the card's child widgets and layout.
     * @details Every concrete box class implements its own layout here. The creator calls it
     * once after construction, because a constructor cannot call the most-derived override;
     * an implementation must not be called twice.
     */
    virtual void setupUi() = 0;

    /// The id of the notification, or an empty string when it has none.
    QString id() const;

    /**
     * @brief Height for the fixed card width, honouring the word-wrapped body.
     * @details QLabel reports a golden-ratio width for wrapped text, which would make the
     * card too short at its fixed width; the override asks the layout for the height at that
     * width instead.
     */
    QSize sizeHint() const override;

signals:
    /// Emitted when a link in the body is activated; @p href is the link target.
    void linkActivated(const QString& href);

    /// Emitted when one of the notification's actions is triggered.
    void actionTriggered(QAction* action);

    /// Emitted once when the card is closing, before it is deleted.
    void dismissed();

protected:
    void showEvent(QShowEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    /// The notification this card was created for.
    const QcwNotification& notification() const;

private:
    QcwNotificationBoxPrivate* impl;
};

QT_END_NAMESPACE
