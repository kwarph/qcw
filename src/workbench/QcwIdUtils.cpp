// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwIdUtils.h"

QT_BEGIN_NAMESPACE

namespace qcw
{

bool isValidId(const QString& id)
{
    if (id.isEmpty())
    {
        return false;
    }

    for (const QChar c : id)
    {
        if (c.isLetterOrNumber() || c == QLatin1Char('.') || c == QLatin1Char('/') || c == QLatin1Char('_') ||
            c == QLatin1Char('-'))
        {
            continue;
        }

        return false;
    }

    return true;
}

bool isReservedAnchor(const QString& id)
{
    return id == QLatin1String("@start") || id == QLatin1String("@end");
}

} // namespace qcw

QT_END_NAMESPACE
