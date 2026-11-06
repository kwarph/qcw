// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QString>

QT_BEGIN_NAMESPACE

namespace qcw
{

/**
 * @brief Whether @p id is a well-formed command, contribution, action set or anchor id.
 * @details Ids use A-Z a-z 0-9 . / _ - and must be non-empty. See the naming convention in
 * the design document.
 */
QCW_EXPORT bool isValidId(const QString& id);

/// Whether @p id is a reserved anchor; currently only "@start".
QCW_EXPORT bool isReservedAnchor(const QString& id);

} // namespace qcw

QT_END_NAMESPACE
