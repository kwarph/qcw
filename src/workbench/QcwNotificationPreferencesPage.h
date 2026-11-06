// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPreferencesPage.h"

QT_BEGIN_NAMESPACE

class QcwNotificationPreferencesPagePrivate;

/**
 * @class QcwNotificationPreferencesPage
 * @brief Preferences page for the notification settings of QcwNotificationManager.
 *
 * @details Registered under the "notification" settings group, the prefix the manager uses
 * for its stored keys:
 * - @c notification/enabled
 * - @c notification/corner
 * - @c notification/maxVisibleBoxes
 * - @c notification/timeout
 *
 * Every control writes through the matching QcwNotificationManager setter, so an edit applies
 * immediately and the manager persists it. loadSettings() reads the manager's current values;
 * the page neither reads nor writes QSettings itself. restoreDefaults() puts the manager back
 * on its documented defaults: enabled, five visible cards, QcwNotificationManager::DefaultPosition
 * and QcwNotificationManager::DefaultTimeout.
 *
 * The second group posts sample notifications through QcwNotificationManager::post(), so they
 * show the effect of the settings above. The timeout control is in whole seconds and maps to
 * the manager's timeout in milliseconds; 0 keeps a card open until it is closed.
 *
 * @note On Wayland the compositor, not the application, places top-level windows, so the corner
 * setting cannot be honoured. The page shows a notice next to the settings when it runs under
 * Wayland; Windows and X11 sessions do not see it.
 *
 * @note The page applies edits immediately; there is no Apply or Cancel step.
 *
 * @see QcwNotificationManager, QcwNotification
 */
class QcwNotificationPreferencesPage : public QcwPreferencesPage
{
    Q_OBJECT
public:
    explicit QcwNotificationPreferencesPage(QWidget* parent = nullptr);
    ~QcwNotificationPreferencesPage() override;

    QString settingsGroup() const override;

    QString name() const override;

    bool matchesFilter(const QString& filterText) const override;

    void restoreDefaults() override;

    void loadSettings() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    QcwNotificationPreferencesPagePrivate* impl;
};

QT_END_NAMESPACE
