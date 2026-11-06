// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QList>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE

class QcwWindow;
class QcwWindowStylerPrivate;

/**
 * @class QcwWindowStyler
 * @brief Applies the configured window style to every QcwWindow.
 *
 * @details A window style is a <tt>QcwWindow.<i>style</i>.theme</tt> JSON file in the current
 * theme directory:
 * @code
 * {
 *     "folderRenderer": "QcwStyledTabFolderRenderer",
 *     "centralMargins": "8 8 8 8",
 *     "windowCss": "QcwWindow { background-color: #eff0f1; }"
 * }
 * @endcode
 * @c folderRenderer names a creator registered with QcwTabFolderRendererCreators; it is used
 * to build a renderer for every tab folder of the window. @c centralMargins becomes the
 * margin of the window's central layout, in top, right, bottom, left order; without it the
 * folders fill the central area. @c windowCss becomes the window's stylesheet and holds only
 * what this style adds to the theme's application stylesheet: it can target the widgets around
 * the folders - @c QMenuBar, @c QToolBar, @c QStatusBar and @c QSplitter::handle - and override
 * rules such as the tool button radius. The rules shared by all window styles live in the
 * theme's @c main.css, which QcwThemeManager applies application-wide (see
 * QcwThemeManager::themeChanged()). The style name comes from QcwSettings (see
 * currentStyle()).
 *
 * A style is named by its file name and nothing else, so the name is what the appearance
 * preferences show: the shipped themes offer @c Framed, whose folders are flush and separated
 * by drawn lines, and @c Cards, whose folders are rounded panels separated by gaps.
 *
 * initialize() collects the styles of the current theme, applies the configured one and then
 * re-applies it whenever QcwThemeManager::themeChanged() arrives. The application is deferred
 * to the next event-loop pass on that signal so the theme-aware renderers have reloaded their
 * own data before new ones are constructed. A style without a theme file, or one whose
 * renderer is not registered, falls back to QcwTabFolderRenderer::createDefaultRenderer() and
 * an empty window stylesheet. When the stored name is not among the collected styles - which is
 * what a theme that renames a style leaves behind - the fallback is reported once with a log
 * warning and a notification card whose button opens the appearance preferences, where the user
 * picks a style again; nothing migrates the stored name.
 *
 * New windows do not reach initialize()'s sweep; call applyStyle(QcwWindow*) after building
 * one, which QcwWorkbench does for the windows it creates. A styled window is then followed,
 * so a folder built later - by a reset perspective, for example - is styled when the window
 * announces it through QcwWindow::folderAdded().
 *
 * @note This is the single global styler: instance() creates it on first use and always
 * returns the same object. Constructing a second one is pointless because nothing connects to
 * it.
 * @note Use the @c windowStyler macro instead of calling instance() directly.
 *
 * @see QcwTabFolderRendererCreators, QcwThemeManager
 */
class QCW_EXPORT QcwWindowStyler : public QObject
{
    Q_OBJECT
public:
    /// The style applied when QcwSettings names none; it works without a theme file.
    static constexpr QLatin1StringView FramedStyle { "Framed" };

    QcwWindowStyler();
    ~QcwWindowStyler() override;

    /// Returns the process-wide styler, creating it on first call.
    static QcwWindowStyler* instance();

    /**
     * @brief Collects the current theme's styles, applies the configured one to all windows
     *        and starts following theme changes.
     *
     * @details Call it once during startup, after QcwThemeManager::initialize() and before the
     * workbench restores or creates its windows. Windows that already exist are styled
     * immediately; later ones are styled by applyStyle(QcwWindow*) before they are shown.
     * Later calls are ignored.
     *
     * A window stylesheet would otherwise make its widgets ignore QApplication::setFont(), so
     * initialize() also enables Qt::AA_UseStyleSheetPropagationInWidgetStyles; the appearance
     * preferences rely on that when they change the application font.
     */
    void initialize();

    /**
     * @brief Makes @p style the current window style and applies it immediately.
     * @param style The style name, or an empty string for FramedStyle.
     *
     * @details Persists the style in QcwSettings under
     * <tt>appearance/windowStyle/currentStyle</tt>. The name is stored as given, so one the
     * current theme has no file for still becomes currentStyle(); it applies as the built-in
     * fallback and is reported once.
     */
    void setCurrentStyle(const QString& style);

    /**
     * @brief The style to apply.
     * @details The name stored in QcwSettings, or FramedStyle when none is stored.
     */
    QString currentStyle() const;

    /**
     * @brief The styles available in the current theme, FramedStyle included.
     * @details The first entry is always FramedStyle; theme styles follow in file-name
     * order. Before initialize() the list holds FramedStyle only.
     */
    QList<QString> allStyles() const;

    /**
     * @brief Rebuilds allStyles() from the current theme directory.
     * @details Emits stylesChanged(). initialize() calls this and repeats it on every theme
     * change; expose it for a caller that installs styles while the theme stays current.
     */
    void collectStyles();

    /// Applies the current style to every top-level QcwWindow.
    void applyStyle();

    /**
     * @brief Applies the current style to @p window.
     * @param window The window to style, or nullptr to do nothing.
     * @details Use it for a window created after initialize()'s sweep. The window is then
     * followed, so a folder it creates later - a reset perspective, for example - receives
     * the current style as well.
     */
    void applyStyle(QcwWindow* window) const;

signals:
    /// Emitted after collectStyles() has rebuilt the style list.
    void stylesChanged();

private:
    /**
     * @brief Reports a stored @p style the current theme has no file for.
     * @details Once per name: a qWarning() line and a warning notification naming the style and
     * the styles on offer. The card stays open until the user acts, and its button runs the
     * application's Preferences command and closes the card with it. Both applyStyle() overloads
     * call it, so a stale value is announced on the first window styled.
     */
    void reportUnresolvedStyle(const QString& style) const;

    /**
     * @brief Connects @p window's folderAdded() to the current style.
     * @details Called by both applyStyle() overloads; connects once per window and forgets it
     * when the window is destroyed.
     */
    void followWindow(QcwWindow* window) const;

    QcwWindowStylerPrivate* impl;
};

/// Shorthand for QcwWindowStyler::instance().
#ifndef windowStyler
#define windowStyler QcwWindowStyler::instance()
#endif

QT_END_NAMESPACE
