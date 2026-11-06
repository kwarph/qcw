// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwFixedRowHeightItemDelegate.h"

QT_BEGIN_NAMESPACE

QcwFixedRowHeightItemDelegate::QcwFixedRowHeightItemDelegate(QObject* parent) :
    QStyledItemDelegate { parent }
{
}

QSize QcwFixedRowHeightItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(RowHeight);
    return size;
}

QT_END_NAMESPACE
