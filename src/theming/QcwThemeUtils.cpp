// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwThemeUtils.h"
#include "QcwEnumUtils.h"
#include <QApplication>
#include <QDebug>
#include <QPalette>
#include <QRegularExpression>
#include <QStringList>
#include <array>

QT_BEGIN_NAMESPACE

namespace
{

QColor parseRgbFunction(const QString& colorString)
{
    static const QRegularExpression re(QStringLiteral("^\\s*(rgba?)\\s*\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,"
                                                      "\\s*(\\d{1,3})\\s*(?:,\\s*(\\d{1,3}))?\\s*\\)\\s*$"),
                                       QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = re.match(colorString);
    if (!match.hasMatch())
    {
        return {};
    }

    const bool isRgba = match.captured(1).compare(QStringLiteral("rgba"), Qt::CaseInsensitive) == 0;
    const bool hasAlpha = !match.captured(5).isEmpty();
    if (isRgba != hasAlpha)
    {
        return {};
    }

    const int red = match.captured(2).toInt();
    const int green = match.captured(3).toInt();
    const int blue = match.captured(4).toInt();
    const int alpha = hasAlpha ? match.captured(5).toInt() : 255;

    if (red > 255 || green > 255 || blue > 255 || alpha > 255)
    {
        return {};
    }

    return QColor(red, green, blue, alpha);
}

QColor parsePaletteColor(const QString& colorString)
{
    static const QRegularExpression re(QStringLiteral("^\\s*palette\\s*\\(\\s*([A-Za-z]+)\\s*\\)\\s*$"),
                                       QRegularExpression::CaseInsensitiveOption);

    const QRegularExpressionMatch match = re.match(colorString);
    if (!match.hasMatch() || !qApp)
    {
        return {};
    }

    bool ok = false;
    const int role = QcwEnumUtils<QPalette::ColorRole>::value(match.captured(1), &ok);
    if (!ok || role < 0 || role >= QPalette::NColorRoles || role == QPalette::NoRole)
    {
        return {};
    }

    return QApplication::palette().color(QPalette::Active, QPalette::ColorRole(role));
}

bool parseFourInts(const QString& text, const char* function, std::array<int, 4>& values)
{
    if (text.trimmed().isEmpty())
    {
        return false;
    }

    const QStringList fields = text.simplified().split(QLatin1Char(' '));
    if (fields.size() != 4)
    {
        qWarning() << "QcwThemeUtils::" << function << ": expected four space-separated integers:" << text;
        return false;
    }

    for (int i = 0; i < 4; ++i)
    {
        bool ok = false;
        const int value = fields.at(i).toInt(&ok);
        if (!ok)
        {
            qWarning() << "QcwThemeUtils::" << function << ": not an integer:" << fields.at(i) << "in" << text;
            return false;
        }

        values[i] = qMax(0, value);
    }

    return true;
}

} // namespace

QColor QcwThemeUtils::parseColor(const QString& colorString)
{
    const QString value = colorString.trimmed();

    if (value.startsWith(QStringLiteral("rgb"), Qt::CaseInsensitive))
    {
        return parseRgbFunction(value);
    }

    if (value.startsWith(QStringLiteral("palette"), Qt::CaseInsensitive))
    {
        return parsePaletteColor(value);
    }

    return QColor(value);
}

QMargins QcwThemeUtils::parseMargins(const QString& marginsString, QMargins fallback)
{
    std::array<int, 4> values {};
    if (!parseFourInts(marginsString, "parseMargins", values))
    {
        return fallback;
    }

    // values are in top, right, bottom, left order; QMargins takes left, top, right, bottom.
    return QMargins { values[3], values[0], values[1], values[2] };
}

QcwBorderRadii QcwThemeUtils::parseRadii(const QString& radiiString, QcwBorderRadii fallback)
{
    std::array<int, 4> values {};
    if (!parseFourInts(radiiString, "parseRadii", values))
    {
        return fallback;
    }

    return QcwBorderRadii { values[0], values[1], values[2], values[3] };
}

QT_END_NAMESPACE
