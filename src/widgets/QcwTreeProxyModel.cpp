// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTreeProxyModel.h"

QT_BEGIN_NAMESPACE

QcwTreeProxyModel::QcwTreeProxyModel(QObject* parent) :
    QSortFilterProxyModel { parent }
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

QString QcwTreeProxyModel::filterText() const
{
    return mFilterText;
}

void QcwTreeProxyModel::clearFilterText()
{
    setFilterText(QString {});
}

void QcwTreeProxyModel::setFilterText(const QString& filterText)
{
    if (mFilterText == filterText)
    {
        return;
    }

    beginFilterChange();
    mFilterText = filterText;
    endFilterChange(Direction::Rows);
    emit filterTextChanged(filterText);
}

bool QcwTreeProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid())
    {
        return false;
    }

    // The gate is consulted before the search text, so excluded rows stay hidden even
    // while the whole tree is shown.
    if (isRowExcluded(index))
    {
        return false;
    }

    if (mFilterText.isEmpty())
    {
        return true;
    }

    if (rowMatchesFilter(index))
    {
        return true;
    }

    // A matching ancestor accepts this row as part of its subtree. An ancestor that is
    // excluded is never shown, so it cannot make this row reachable either.
    for (auto parent = index.parent(); parent.isValid(); parent = parent.parent())
    {
        if (!isRowExcluded(parent) && rowMatchesFilter(parent))
        {
            return true;
        }
    }

    // A match further down accepts this row so the match stays reachable.
    return hasMatchingDescendant(index);
}

bool QcwTreeProxyModel::isRowExcluded(const QModelIndex& sourceIndex) const
{
    return false;
}

bool QcwTreeProxyModel::rowMatchesFilter(const QModelIndex& sourceIndex) const
{
    const auto textMatches = [this, &sourceIndex](int column)
    {
        return sourceIndex.siblingAtColumn(column)
            .data(filterRole())
            .toString()
            .contains(mFilterText, filterCaseSensitivity());
    };

    const int keyColumn = filterKeyColumn();
    if (keyColumn >= 0)
    {
        if (textMatches(keyColumn))
        {
            return true;
        }
    }
    else
    {
        // columnCount() describes the children of the index, so the row's own count comes
        // from its parent.
        const int columnCount = sourceModel()->columnCount(sourceIndex.parent());
        for (int column = 0; column < columnCount; ++column)
        {
            if (textMatches(column))
            {
                return true;
            }
        }
    }

    return matchesAdditionalConditions(sourceIndex, mFilterText);
}

bool QcwTreeProxyModel::hasMatchingDescendant(const QModelIndex& sourceIndex) const
{
    const auto model = sourceModel();
    const int rowCount = model->rowCount(sourceIndex);

    for (int row = 0; row < rowCount; ++row)
    {
        const auto child = model->index(row, 0, sourceIndex);
        if (child.isValid() && !isRowExcluded(child)
            && (rowMatchesFilter(child) || hasMatchingDescendant(child)))
        {
            return true;
        }
    }

    return false;
}

bool QcwTreeProxyModel::matchesAdditionalConditions(const QModelIndex& sourceIndex, const QString& filterText) const
{
    return false;
}

QT_END_NAMESPACE
