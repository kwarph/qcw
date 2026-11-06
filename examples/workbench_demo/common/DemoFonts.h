// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>

namespace demo
{
/**
 * @brief The font a demo text pane starts with and its page's Default button returns to.
 *
 * @details QFontDatabase::systemFont(FixedFont) names the generic "monospace" family on some
 * platforms, which a font dialog would show as if it were a family of its own. Resolving it
 * through QFontInfo keeps the family the font engine actually picked.
 */
inline QFont defaultMonospaceFont()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setFamily(QFontInfo { font }.family());
    return font;
}
} // namespace demo
