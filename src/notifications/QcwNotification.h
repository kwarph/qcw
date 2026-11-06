// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QList>
#include <QString>

QT_BEGIN_NAMESPACE

class QAction;

/**
 * @struct QcwNotification
 * @brief Description of one notification shown by QcwNotificationManager::post().
 *
 * @details The struct only carries data; QcwNotificationManager decides when and where to
 * show it, and QcwNotificationBox renders it. The default values produce an informational
 * card that disappears after the manager's default timeout.
 *
 * @par Usage
 * @code
 * QcwNotification notification;
 * notification.level = QcwNotification::Level::Warning;
 * notification.title = tr("Connection lost");
 * notification.text = tr("Retrying in the background. <a href=\"details\">Details</a>");
 * notification.actions = { retryAction, detailsAction };
 * notificationManager->post(notification);
 * @endcode
 *
 * @note text is interpreted as rich text, so the caller is responsible for escaping any
 *       plain-text part of it. Link targets are not opened by the component; they arrive
 *       through QcwNotificationManager::linkActivated() and the caller decides what to do.
 *
 * @see QcwNotificationManager, QcwNotificationBox
 */
struct QcwNotification
{
    /// Severity of a notification; selects the accent color and the default icon.
    enum class Level
    {
        Information,
        Success,
        Warning,
        Critical
    };

    /**
     * @brief Identifier used by withdraw() and the link signal.
     * @details May be empty, in which case the notification cannot be addressed later.
     * Posting a non-empty id that is already on screen replaces the older notification
     * instead of stacking a second one.
     */
    QString id;

    /// Severity; determines the accent color and the icon used when icon is empty.
    Level level { Level::Information };

    /// Single-line heading; rendered as plain text.
    QString title;

    /// Body text; rendered as rich text with mouse-accessible links.
    QString text;

    /**
     * @brief Buttons shown at the bottom of the card, in order.
     * @details The actions are borrowed, not owned: the caller keeps them alive at least as
     * long as the card is shown. A disabled action produces a disabled button. Triggering
     * one emits QcwNotificationBox::actionTriggered() as well as the action's own signals.
     */
    QList<QAction*> actions;

    /**
     * @brief Time until the card closes by itself, in milliseconds.
     * @details 0 keeps the card open until the user closes it or triggers an action.
     * Negative values are only meaningful with QcwNotificationManager::post(), which
     * substitutes the manager's default timeout; a QcwNotificationBox built directly treats
     * them as 0.
     */
    int timeoutMs { -1 };

    /// Whether triggering any of actions closes the card; true by default.
    bool closeOnActionTriggered { true };
};

QT_END_NAMESPACE
