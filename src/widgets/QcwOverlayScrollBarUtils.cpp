// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwOverlayScrollBarUtils.h"
#include "QcwOverlayScrollBarAttacher.h"

QT_BEGIN_NAMESPACE

void attachOverlayScrollBar(QAbstractScrollArea* target, QObject* parent)
{
    auto* attacher = new QcwOverlayScrollBarAttacher(target, parent);
}

QT_END_NAMESPACE
