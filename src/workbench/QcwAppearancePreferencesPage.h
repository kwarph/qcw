// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPreferencesPage.h"

QT_BEGIN_NAMESPACE

class QcwAppearancePreferencesPagePrivate;

/**
 * @class QcwAppearancePreferencesPage
 * @brief Preferences page for the current theme, the UI language and the UI font.
 *
 * @details Registered under the "appearance" settings group. Theme edits go through
 * QcwThemeManager; language, font and the follow-system choice are stored below "appearance":
 * - @c appearance/themes/followSystem, @c appearance/themes/lightTheme,
 *   @c appearance/themes/darkTheme
 * - @c appearance/font
 * - @c appearance/language, which belongs to QcwTranslations: this page only asks it to store
 *   and apply a choice
 *
 * The font field shows the stored font, or the default one when the key is absent. The default
 * is the application font captured before any stored font was applied; the button beside the
 * field clears @c appearance/font and goes back to it.
 *
 * When "follow the system theme" is checked, the current theme is the preferred theme that
 * matches QStyleHints::colorScheme(), and it is re-applied whenever the system color scheme
 * changes. Qt::ColorScheme::Unknown is treated as light. A preferred theme that is not
 * registered is replaced by QcwThemeManager::DefaultThemeName, or the current theme is kept
 * when that one is unavailable too.
 *
 * The language selector offers QcwTranslations::availableLocales() — every locale with a
 * catalog under an application-owned base, plus the sources' own language — under their native
 * names, with "System Default" first for an empty choice. Picking one hands it to
 * QcwTranslations::setLanguage(), which installs the catalogs and publishes @c app/language, so
 * an application that keeps state derived from the language follows without reading the stored
 * key. Every string the framework itself draws — menu titles, tab titles, the preferences
 * dialog, a folder's menu and settings dialog, the command palette — follows at once, because
 * Qt delivers QEvent::LanguageChange to every widget and QcwWindow::retranslateUi() re-resolves
 * the titles the window derived. An application's own strings follow only where it re-applies
 * them in that event, as the demo's views and the built-in pages do; see
 * QcwWindow::retranslateUi() and QcwTranslations.
 *
 * @note This page is process-wide on purpose. The theme manager, QcwTranslations and
 * QcwWindowStyler all persist through QcwSettings, and the state they apply belongs to the
 * whole application, so the page reads and writes QcwSettings::get() directly instead of going
 * through QcwPreferencesPage::settings(). Do not put it into a QcwPreferencesWidget bound to a
 * project store: its edits would still change the global configuration.
 *
 * @note QcwWorkbench::initialize() calls applyStoredSettings() once, after QcwThemeManager has
 * been initialized and before plugins and windows are created. A page used without
 * QcwWorkbench must call applyStoredSettings() itself to restore the language, font and
 * followed theme at startup.
 * @note The page applies edits immediately; there is no Apply or Cancel step.
 *
 * @see QcwThemeDialog, QcwThemeManager, QcwTranslations, QcwSettings
 */
class QcwAppearancePreferencesPage : public QcwPreferencesPage
{
    Q_OBJECT
public:
    explicit QcwAppearancePreferencesPage(QWidget* parent = nullptr);
    ~QcwAppearancePreferencesPage() override;

    /**
     * @brief Applies the stored UI language and font and the followed system theme.
     * @details Hands the language to QcwTranslations::applyStoredLanguage(), then reads
     * "appearance/font" and "appearance/themes/followSystem". Requires a QApplication and, for
     * the theme, an initialized QcwThemeManager. QcwWorkbench::initialize() calls this during
     * startup.
     */
    static void applyStoredSettings();

    QString settingsGroup() const override;

    QString name() const override;

    bool matchesFilter(const QString& filterText) const override;

    void restoreDefaults() override;

    void loadSettings() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    QcwAppearancePreferencesPagePrivate* impl;
};

QT_END_NAMESPACE
