// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include "QcwNotification.h"
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <functional>

QT_BEGIN_NAMESPACE

class QcwNotificationBox;
class QcwNotificationManagerPrivate;

/**
 * @brief Factory for notification cards, installed with setNotificationBoxCreator().
 * @details The function receives the notification to display and returns a new, parentless
 * card whose layout is already built (setupUi() has been called). Returning @c nullptr makes
 * post() drop the notification with a warning. The manager takes over the card and deletes it
 * when it closes.
 */
using QcwNotificationBoxCreator = std::function<QcwNotificationBox*(const QcwNotification&)>;

/**
 * @class QcwNotificationManager
 * @brief The single global entry point for showing QcwNotification cards.
 *
 * @details post() creates a QcwNotificationBox per notification, places the cards in a
 * stack in one of the screen's corners and keeps at most maxVisibleBoxes() of them. Each
 * card is a parentless, always-on-top window, so the notifications stay visible while the
 * application is minimized. Under X11 a card is a tooltip window, which keeps it out of
 * taskbars that list utility windows. A second instance of the manager would be pointless,
 * because nothing connects to it.
 *
 * The stack belongs to the screen of the current focus window. When no window has focus —
 * for example while the application is minimized — the screen of the last focused window is
 * used, and the primary screen when the manager has not seen one yet. The newest
 * notification sits closest to the configured corner.
 *
 * Cards close themselves after the timeout, when the user clicks their close button, or
 * when an action is triggered and the notification keeps closeOnActionTriggered set;
 * hovering a card pauses its timer. Posting a non-empty id that is already visible replaces
 * the older card, and posting beyond maxVisibleBoxes() closes the oldest card instead of
 * queueing.
 *
 * Whether notifications are posted at all, the corner, the visible-card limit and the
 * default timeout are restored from QcwSettings when the manager is created and written back
 * by the corresponding setters. The setters apply immediately and may be called at runtime,
 * for example from a preferences page. While notifications are disabled, post() ignores new
 * ones; cards already on screen stay until they close.
 *
 * @note Use the @c notificationManager macro instead of calling instance() directly.
 * @note The manager belongs to the GUI thread; all methods must be called from it, after
 *       the QApplication object exists.
 * @note The constructor reads QcwSettings, so the application and organization names and any
 *       installed settings provider must be in place before the manager is first used.
 * @note Links and actions are not handled here: linkActivated() identifies the notification
 *       for the caller, and actions keep their own connections.
 *
 * @see QcwNotification, QcwNotificationBox
 */
class QCW_EXPORT QcwNotificationManager : public QObject
{
    Q_OBJECT

public:
    /// Fallback corner when no valid corner is stored in QcwSettings.
    static constexpr Qt::Corner DefaultPosition { Qt::BottomRightCorner };
    /// Fallback number of visible cards when no valid limit is stored in QcwSettings.
    static constexpr int DefaultVisibleBoxes { 5 };
    /// Fallback timeout in milliseconds for notifications that leave timeoutMs negative.
    static constexpr int DefaultTimeout { 8000 };

public:
    QcwNotificationManager();

    ~QcwNotificationManager();

    /// The manager singleton; created on first call.
    static QcwNotificationManager* instance();

    /**
     * @brief Installs the factory used by post() to create cards.
     * @param creator Factory returning a fully built card; @c nullptr restores the default
     *        QcwNotificationBox::createDefaultNotificationBox().
     */
    void setNotificationBoxCreator(QcwNotificationBoxCreator creator);

    /**
     * @brief Shows @p notification as a new card.
     * @details Substitutes the default timeout for a negative timeoutMs, replaces a visible
     * notification with the same non-empty id, closes the oldest card when the stack is
     * full, then positions the stack on the current screen. An empty title or text simply
     * hides that part of the card. When notifications are disabled, the call returns
     * without creating a card.
     *
     * The actions are borrowed and must stay alive while the card is visible.
     */
    void post(const QcwNotification& notification);

    /// Closes the visible notification with @p id; does nothing when none matches.
    void withdraw(const QString& id);

    /// Closes all visible notifications.
    void clear();

    /**
     * @brief Enables or disables showing notifications at runtime; enabled by default.
     * @details This may be called at any time, typically from a preferences page bound to a
     * checkbox. Disabling takes effect immediately: every later post() is ignored, while
     * cards that are already on screen stay and close as usual. The state is persisted and
     * restored on the next start.
     */
    void setNotificationEnabled(bool enabled);

    /// Returns whether post() currently shows notifications.
    bool isNotificationEnabled() const;

    /**
     * @brief Sets the corner the stack is anchored to; repositions visible cards immediately.
     * @details @p corner must be one of the four Qt::Corner values. The choice is persisted
     * and restored on the next start.
     */
    void setPosition(Qt::Corner corner);

    /// Returns the corner the stack is anchored to.
    Qt::Corner position() const;

    /**
     * @brief Sets how many cards may be visible at once; the default is 5.
     * @details Values below 1 are raised to 1. Lowering the limit closes the oldest cards.
     * The limit is persisted and restored on the next start.
     */
    void setMaxVisibleBoxes(int count);

    /// Returns the maximum number of cards visible at once.
    int maxVisibleBoxes() const;

    /**
     * @brief Sets the timeout used by notifications with a negative timeoutMs; 0 means "never".
     * @details Negative values are raised to 0. The timeout is persisted and restored on the
     * next start.
     */
    void setDefaultTimeout(int ms);

    /// Returns the timeout used when a notification leaves timeoutMs negative.
    int defaultTimeout() const;

signals:
    /**
     * @brief A link in a notification body was activated.
     * @param notificationId The id of the card, empty when it has none.
     * @param href The link target exactly as written in QcwNotification::text.
     */
    void linkActivated(const QString& notificationId, const QString& href);

private:
    QcwNotificationManagerPrivate* impl;
};

/// Shorthand for QcwNotificationManager::instance().
#ifndef notificationManager
#define notificationManager QcwNotificationManager::instance()
#endif

QT_END_NAMESPACE
