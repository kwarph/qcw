// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwCommand.h"

QT_BEGIN_NAMESPACE

bool QcwCommandInfo::operator==(const QcwCommandInfo& other) const
{
    // QIcon has no operator==; cacheKey() is the cheapest stable proxy for "the same icon".
    // It is not exact: two icons with the same pixmaps but built independently can produce
    // different keys, and one icon that has been painted can change its key. The only caller
    // is QcwCommandRegistry::registerCommand(), which uses this to decide whether to emit
    // commandsChanged(); a false negative costs one extra signal, a false positive skips a
    // refresh, so the cheaper proxy is the right trade.
    return id == other.id && category == other.category && label == other.label && description == other.description &&
           defaultShortcut == other.defaultShortcut && icon.cacheKey() == other.icon.cacheKey() &&
           checkable == other.checkable;
}

QT_END_NAMESPACE
