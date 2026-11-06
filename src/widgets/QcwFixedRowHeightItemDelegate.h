// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QStyledItemDelegate>

QT_BEGIN_NAMESPACE

/**
 * @class QcwFixedRowHeightItemDelegate
 * @brief An item delegate whose only job is to give every row of a view the same height.
 *
 * @details QStyledItemDelegate sizes a row from its own contents, so rows carrying a
 * bigger font or a taller icon become taller than their neighbours. This delegate keeps
 * everything else from QStyledItemDelegate — painting, editing, checking — and replaces
 * only the height of QStyledItemDelegate::sizeHint() with the constant RowHeight, which
 * makes the rows of the host QAbstractItemView uniform.
 *
 * Items still supply the usual roles (Qt::DisplayRole, Qt::DecorationRole, Qt::FontRole,
 * ...) and the width of a row is whatever QStyledItemDelegate computes for them.
 *
 * @par Pairing the delegate with the view
 * RowHeight is fixed on purpose: a row whose font or icon needs more space than RowHeight
 * clips its contents rather than growing the row. Views using this delegate should
 * therefore call QAbstractItemView::setIconSize() with IconSize, the largest icon that
 * still fits a row.
 *
 * @see QAbstractItemView::setItemDelegate()
 */
class QCW_EXPORT QcwFixedRowHeightItemDelegate : public QStyledItemDelegate
{
public:
    /// Height of a single row, in pixels.
    static constexpr int RowHeight = 26;
    /// Icon size that fits a row, in pixels; pass it to QAbstractItemView::setIconSize().
    static constexpr int IconSize = 16;

public:
    /**
     * @brief Constructs the delegate.
     * @param parent The parent object. Note that QAbstractItemView::setItemDelegate()
     *               reparents the delegate to the view, which then owns it.
     */
    explicit QcwFixedRowHeightItemDelegate(QObject* parent = nullptr);

    /**
     * @brief Returns the size of the row at @p index.
     * @details The width is QStyledItemDelegate's; the height is always RowHeight.
     */
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

QT_END_NAMESPACE
