// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE

class QEvent;

/**
 * @class QcwThemeManager
 * @brief Process-wide registry of themes and the one currently applied.
 *
 * @details A theme is a directory containing QcwPalette.theme and, optionally, one
 * <tt><i>widgetType</i>.theme</tt> JSON file per theme-aware widget. initialize() restores the
 * themes recorded in QcwSettings and scans the installed <tt>qcw/themes</tt> data directory and
 * <tt>applicationDirPath()/themes</tt>; registerTheme() adds themes discovered later, for
 * example by a plugin.
 *
 * QcwPalette.theme is a JSON object that names the widget style the theme expects and lists
 * QPalette color roles:
 * @code
 * {
 *     "style": "Fusion",
 *     "colors": {
 *         "Window": "#232629",
 *         "WindowText": "#eff0f1",
 *         "Base": "#1b1e20"
 *     },
 *     "disabledColors": {
 *         "WindowText": "#7b8389"
 *     }
 * }
 * @endcode
 * The @c colors object is applied to the Active, Inactive and Disabled color groups;
 * @c disabledColors then overrides individual roles in the Disabled group. Role names are the
 * exact QPalette::ColorRole keys. changeTheme() switches the widget style when the theme asks
 * for a different one, applies the palette and emits themeChanged().
 *
 * A theme may also ship a @c main.css next to QcwPalette.theme. It becomes the application
 * stylesheet, so widget rules that are not tied to a window style - toolbars, tool buttons,
 * scrollbars - also reach dialogs and other top-level windows. A window style's windowCss and a
 * folder renderer's stylesheet still take precedence for the properties they set on their own
 * widgets. The file is loaded again on every theme change; a theme without it clears the rules
 * an earlier theme applied, while an application stylesheet the host installed before any theme
 * provided one is left alone.
 *
 * Theme-aware widgets obtain their @c .theme JSON through loadWidgetThemeData() and reload it
 * when themeChanged() arrives.
 *
 * @note initialize() must be called once on the GUI thread, after QApplication exists and the
 * application and organization names are set. QcwWorkbench::initialize() does this. Later calls
 * are ignored. The manager is not thread-safe and belongs to the GUI thread.
 * @note This is the single global manager: instance() creates it on first use and always returns
 * the same object. Constructing a second one is pointless because no widget or plugin talks to
 * it.
 * @note Use the @c themeManager macro instead of calling instance() directly.
 *
 * @see QcwWidgetTheme, QcwThemeUtils
 */
class QCW_EXPORT QcwThemeManager : public QObject
{
    Q_OBJECT
public:
    /// File name of the palette every theme directory must contain.
    static constexpr QLatin1StringView PaletteFileName { "QcwPalette.theme" };
    /// Name of the fallback theme preferred when the current theme becomes unavailable.
    static constexpr QLatin1StringView DefaultThemeName { "QcwLight" };

public:
    /// Returns the process-wide manager, creating it on first call.
    static QcwThemeManager* instance();

    /**
     * @brief Restores the theme registry, resolves the current theme and applies it.
     *
     * @details Themes recorded in QcwSettings are restored first, then the installed
     * <tt>qcw/themes</tt> data directory and the directories under
     * <tt>applicationDirPath()/themes</tt> are scanned; a theme next to the executable replaces
     * one from the data directory, and any scanned theme replaces a restored entry with the same
     * name. Only directories that contain QcwPalette.theme are considered. The stored
     * current theme is kept when it is still available; otherwise DefaultThemeName ("QcwLight")
     * becomes current, or, if that theme does not exist either, the first theme in name order.
     *
     * The resolved registry and current theme are written back to QcwSettings. When a theme was
     * selected, its palette and main.css are applied and themeChanged() is emitted, so widgets
     * created before initialization reload their data.
     *
     * @note Idempotent: after the first call this function returns immediately. It requires a
     * QApplication and the application/organization identity, which QcwWorkbench sets before
     * calling it.
     */
    void initialize();

    /**
     * @brief Registers the theme in @p themeDir under @p themeName.
     * @param themeName Name used by changeTheme() and reported by allThemes().
     * @param themeDir Directory containing QcwPalette.theme; may be relative.
     * @return @c true if the theme was added; @c false if a parameter is empty,
     *         QcwPalette.theme is missing, @p themeName is already registered, or another theme
     *         already uses @p themeDir.
     *
     * @details The directory is stored as an absolute path and the registry is persisted
     * immediately. A directory belongs to at most one theme. Registering does not change the
     * current theme; call changeTheme() for that. themeAdded() is emitted on success.
     *
     * @see removeTheme(), changeTheme()
     */
    bool registerTheme(const QString& themeName, const QString& themeDir);

    /**
     * @brief Removes the theme registered under @p themeName.
     * @return @c true if the theme was registered and has been removed; @c false otherwise.
     *
     * @details Removing the current theme promotes the same fallback initialize() would choose
     * (DefaultThemeName, or the first remaining theme), applies its palette and application
     * stylesheet and emits themeChanged(). If no theme remains, the current theme becomes empty,
     * the application palette returns to the widget style's standard palette and the application
     * stylesheet is cleared. The registry and, when it changed, the current theme are persisted
     * immediately.
     *
     * @see registerTheme()
     */
    bool removeTheme(const QString& themeName);

    /**
     * @brief Makes @p themeName the current theme and applies its palette and main.css.
     * @param themeName Name of a registered theme.
     * @return @c true if @p themeName is registered and is now the current theme; @c false for
     *         an unknown name, in which case nothing changes.
     *
     * @details The current theme is persisted, its palette and application stylesheet are
     * applied and themeChanged() is emitted. Selecting the theme that is already current is a
     * no-op and returns @c true.
     *
     * @see themeChanged(), currentTheme()
     */
    bool changeTheme(const QString& themeName);

    /// Returns the name of the current theme, or an empty string when no theme is available.
    QString currentTheme() const;

    /// Returns the directory of the current theme, or an empty string when none is current.
    QString currentThemeDirectory() const;

    /**
     * @brief Returns the directory registered for @p themeName.
     * @return The absolute theme directory, or an empty string when @p themeName is unknown.
     * @see currentThemeDirectory(), allThemes()
     */
    QString themeDirectory(const QString& themeName) const;

    /// Returns whether a theme is registered under @p themeName.
    bool hasTheme(const QString& themeName) const;

    /**
     * @brief Returns whether @p themeDirectory is used by a registered theme.
     * @param themeDirectory Directory to look up; may be relative.
     * @return @c true when a registered theme uses @p themeDirectory.
     *
     * @details Both sides are compared as cleaned absolute paths, so relative input, "."
     * components and a trailing separator do not matter. Symbolic-link and case aliases are
     * not resolved.
     *
     * @see hasTheme(), allThemes()
     */
    bool isThemeDirectoryUsed(const QString& themeDirectory) const;

    /**
     * @brief Loads the JSON data of a widget theme from the current theme directory.
     * @param widgetType Base name of the widget theme file: "Common" reads
     *        @c currentThemeDirectory()/Common.theme. A trailing ".theme" is accepted as well.
     * @return The parsed JSON object, or an empty object when there is no current theme, the
     *         file does not exist, cannot be read, is not valid JSON, or does not contain a JSON
     *         object. A malformed file is reported with qWarning().
     *
     * @details The data is returned as stored. Placeholders in widget themes are not expanded
     * here; theme-aware widgets that need theme-relative resource paths can resolve them against
     * currentThemeDirectory().
     *
     * @see QcwWidgetTheme, currentThemeDirectory()
     */
    QJsonObject loadWidgetThemeData(const QString& widgetType) const;

    /**
     * @brief Returns all registered themes, keyed by theme name.
     * @return The live map from theme names to absolute directories; do not modify it.
     */
    const QMap<QString, QString>& allThemes() const;

protected:
    /**
     * @brief Re-asserts the current theme palette against foreign palette changes.
     *
     * @details A desktop platform theme (for example KDE's) pushes the system palette through
     * QGuiApplication::setPalette() when the desktop theme changes, which would otherwise
     * overwrite the palette QcwThemeManager applied. The first ApplicationPaletteChange not
     * triggered by our own applyTheme() is swallowed and replaced by the current theme's palette,
     * so the application stays on its selected theme. Theme following remains a QcwThemeManager
     * decision (which registered theme is current), never a desktop override.
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    /// Emitted after the current theme's palette and application stylesheet have been applied
    /// or reset.
    void themeChanged();

    /**
     * @brief Emitted after registerTheme() has added a theme.
     * @param themeName Name of the newly registered theme.
     */
    void themeAdded(const QString& themeName);

private:
    void saveThemes();
    void saveCurrentTheme();
    void applyTheme();
    void applyAppCss();
    QString fallbackTheme() const;

private:
    QString mCurrTheme;
    QMap<QString, QString> mThemes; ///< Theme name to absolute theme directory.
    bool mInitialized { false };
    bool mApplyingPalette { false }; ///< Set while applyTheme() sets the palette, to ignore our own event.
    bool mAppCssApplied { false };   ///< Set once a theme's main.css became the application stylesheet.
};

/// Shorthand for QcwThemeManager::instance().
#ifndef themeManager
#define themeManager QcwThemeManager::instance()
#endif

QT_END_NAMESPACE
