// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QColor>
#include <QMargins>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwBorderRadii
 * @brief Corner radii of a rounded rectangle, in the order used by theme files.
 * @details The order matches the CSS border-radius shorthand and parseRadii(): top-left,
 * top-right, bottom-right, bottom-left. Zero means a square corner.
 */
struct QcwBorderRadii
{
    int topLeft { 0 };     ///< Radius of the top-left corner.
    int topRight { 0 };    ///< Radius of the top-right corner.
    int bottomRight { 0 }; ///< Radius of the bottom-right corner.
    int bottomLeft { 0 };  ///< Radius of the bottom-left corner.
};

/**
 * @struct QcwThemeUtils
 * @brief Helpers for interpreting the color, margin and radius values stored in theme data.
 *
 * @details Theme files are plain JSON, so colors arrive as strings. Besides the forms QColor
 * understands directly (color names and hex strings), QCW themes use two additional forms:
 * rgb()/rgba() with integer channels and palette(ColorRole), which resolves against the
 * application palette. parseColor() accepts all of them so that theme-aware widgets do not have
 * to branch on the notation themselves.
 *
 * Margins and radii are strings of four space-separated integers, parsed by parseMargins() and
 * parseRadii(). Both accept a fallback that they return when the text is empty or malformed, so
 * a caller can keep its own defaults without validating the string twice.
 *
 * @note The struct is never instantiated; every function is static.
 * @see QcwThemeManager, QcwWidgetTheme
 */
struct QCW_EXPORT QcwThemeUtils
{
    /**
     * @brief Converts a theme color string to a QColor.
     * @param colorString One of:
     * @li a color name or hex string accepted by QColor, for example @c "#ef3d96";
     * @li @c rgb(r, g, b) or @c rgba(r, g, b, a) with integer channels, for example
     *     @c "rgb(120, 35, 87)" or @c "rgba(120, 35, 87, 76)". The alpha channel is an integer
     *     in the range 0-255, as in Qt style sheets, not the 0.0-1.0 fraction used by CSS;
     * @li @c palette(ColorRole), for example @c "palette(WindowText)". The role name must be
     *     the exact, case-sensitive key of a QPalette::ColorRole enumerator and is resolved
     *     against the active color group of QApplication::palette().
     *
     * Function names are compared case-insensitively and surrounding whitespace is ignored.
     * @return The parsed color, or an invalid QColor (QColor::isValid() returns @c false) when
     * the string is malformed, a channel is outside 0-255, the palette role is unknown, NoRole
     * or NColorRoles, or a palette reference is used without a QApplication.
     *
     * @note A palette reference is resolved at call time. Callers that cache the returned color
     * must parse again after the application palette or the current theme changes.
     */
    static QColor parseColor(const QString& colorString);

    /**
     * @brief Parses four space-separated integers into margins.
     * @param marginsString Text in top, right, bottom, left order, for example @c "6 6 6 6".
     * @param fallback Value returned when @p marginsString is empty or malformed.
     * @return The parsed margins, with negative components clamped to zero, or @p fallback.
     *
     * @details Runs of spaces and tabs between the numbers are accepted. Non-empty input with
     * a wrong number of fields or a non-numeric field is rejected and reported with
     * qWarning(); an empty string is treated as "not configured" and falls back silently.
     */
    static QMargins parseMargins(const QString& marginsString, QMargins fallback = {});

    /**
     * @brief Parses four space-separated integers into corner radii.
     * @param radiiString Text in top-left, top-right, bottom-right, bottom-left order, for
     *        example @c "4 4 4 4".
     * @param fallback Value returned when @p radiiString is empty or malformed.
     * @return The parsed radii, with negative components clamped to zero, or @p fallback.
     *
     * @details Applies the same whitespace handling and validation as parseMargins().
     */
    static QcwBorderRadii parseRadii(const QString& radiiString, QcwBorderRadii fallback = {});
};

QT_END_NAMESPACE
