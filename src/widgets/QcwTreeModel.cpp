// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTreeModel.h"
#include "QcwTreeItem.h"

QT_BEGIN_NAMESPACE

QcwTreeModel::QcwTreeModel(QObject* parent) :
    QAbstractItemModel(parent), mRootItem { new QcwTreeItem(nullptr) }
{
}

QcwTreeModel::~QcwTreeModel()
{
    delete mRootItem;

    // The row in the cut() slot was taken out of the tree, so it is not part of that subtree.
    delete mCutItem;
}

QModelIndex QcwTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0 || column < 0 || column >= columnCount(parent) || (parent.isValid() && parent.column() != 0))
    {
        return QModelIndex();
    }

    QcwTreeItem* parentItem = itemForIndex(parent);
    if (parentItem)
    {
        if (QcwTreeItem* item = parentItem->childAt(row))
        {
            return createIndex(row, column, item);
        }
    }

    return QModelIndex();
}

QModelIndex QcwTreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QModelIndex();
    }

    QcwTreeItem* childItem = itemForIndex(index);
    if (!childItem)
    {
        return QModelIndex();
    }

    // The invisible root is not a row, so indexFromItem() answers a top-level row's parent with the
    // invalid index the views expect.
    return indexFromItem(childItem->parent());
}

bool QcwTreeModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (count <= 0 || row < 0)
    {
        return false;
    }

    if (parent.isValid() && parent.column() != 0)
    {
        return false;
    }

    QcwTreeItem* parentItem = parent.isValid() ? rowItem(parent) : mRootItem;
    if (!parentItem || row > parentItem->childCount())
    {
        return false;
    }

    beginInsertRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i)
    {
        // row + i, not row: the rows keep the order createItem() produced them in.
        parentItem->insertChild(row + i, createItem());
    }
    endInsertRows();

    return true;
}

bool QcwTreeModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (count <= 0 || row < 0)
    {
        return false;
    }

    if (parent.isValid() && parent.column() != 0)
    {
        return false;
    }

    QcwTreeItem* parentItem = parent.isValid() ? rowItem(parent) : mRootItem;
    if (!parentItem || count > parentItem->childCount() - row)
    {
        return false;
    }

    beginRemoveRows(parent, row, row + count - 1);
    for (int i = 0; i < count; ++i)
    {
        // Row after row: what followed the removed one shifts down into its place.
        delete parentItem->takeChild(row);
    }
    endRemoveRows();

    return true;
}

QVariant QcwTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && section >= 0 && section < columnLabels().size())
    {
        switch (role)
        {
        case Qt::DisplayRole:
        case Qt::EditRole:
        case Qt::ToolTipRole:
            return columnLabels()[section];
        case Qt::TextAlignmentRole:
            return Qt::AlignCenter;
        default:
            break;
        }
    }

    return QVariant();
}

int QcwTreeModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() && parent.column() != 0 ? 0 : columnLabels().size();
}

int QcwTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() && parent.column() != 0)
    {
        return 0;
    }

    QcwTreeItem* parentItem = itemForIndex(parent);
    return parentItem ? parentItem->childCount() : 0;
}

QModelIndex QcwTreeModel::moveUp(const QModelIndex& index)
{
    return moveRow(index, -1);
}

QModelIndex QcwTreeModel::moveDown(const QModelIndex& index)
{
    return moveRow(index, 1);
}

QModelIndex QcwTreeModel::moveRow(const QModelIndex& index, int delta)
{
    QcwTreeItem* item = rowItem(index);
    if (!item)
    {
        return index;
    }

    QcwTreeItem* parentItem = item->parent();
    const int oldRow = parentItem->rowOfChild(item);
    const int newRow = oldRow + delta;
    if (newRow < 0 || newRow >= parentItem->childCount())
    {
        return index;
    }

    // A destination in the same parent is counted in the numbering from before the row is taken
    // out, so a downward move names the row it steps over rather than the row it lands on.
    const int destinationChild = newRow > oldRow ? newRow + 1 : newRow;
    const QModelIndex parentIndex = indexFromItem(parentItem);

    if (!beginMoveRows(parentIndex, oldRow, oldRow, parentIndex, destinationChild))
    {
        return index;
    }

    QcwTreeItem* moved = parentItem->takeChild(oldRow);
    parentItem->insertChild(newRow, moved);
    endMoveRows();

    return indexFromItem(item);
}

QModelIndex QcwTreeModel::cut(const QModelIndex& index)
{
    QcwTreeItem* item = rowItem(index);
    if (!item)
    {
        return index;
    }

    QcwTreeItem* parentItem = item->parent();
    const int row = parentItem->rowOfChild(item);
    const QModelIndex parentIndex = indexFromItem(parentItem);

    // One slot, so the row the previous cut() left there goes with it.
    delete mCutItem;
    mCutItem = nullptr;

    beginRemoveRows(parentIndex, row, row);
    parentItem->takeChild(row);
    endRemoveRows();

    // From here the item belongs to the model and to no parent, which is what keeps it out of the
    // destructor's sweep of the tree.
    mCutItem = item;

    // The row a view can select now: the neighbour that took the cut row's place, or the parent row
    // when it was the first one. The root has no index, so an emptied top level answers invalid.
    return row > 0 ? indexFromItem(parentItem->childAt(row - 1)) : parentIndex;
}

QModelIndex QcwTreeModel::paste(const QModelIndex& index)
{
    if (!mCutItem)
    {
        return index;
    }

    QcwTreeItem* sibling = rowItem(index);
    if (!sibling)
    {
        return index;
    }

    QcwTreeItem* parentItem = sibling->parent();
    const int row = parentItem->rowOfChild(sibling) + 1;
    const QModelIndex parentIndex = indexFromItem(parentItem);

    QcwTreeItem* item = mCutItem;
    mCutItem = nullptr;

    beginInsertRows(parentIndex, row, row);
    parentItem->insertChild(row, item);
    endInsertRows();

    return indexFromItem(item);
}

QModelIndex QcwTreeModel::promote(const QModelIndex& index)
{
    QcwTreeItem* item = rowItem(index);
    if (!item)
    {
        return index;
    }

    QcwTreeItem* parentItem = item->parent();
    if (parentItem == mRootItem)
    {
        return index; // Already a top-level item
    }

    QcwTreeItem* grandParentItem = parentItem->parent();
    const int sourceRow = parentItem->rowOfChild(item);
    const int destinationRow = grandParentItem->rowOfChild(parentItem) + 1;

    if (!beginMoveRows(indexFromItem(parentItem), sourceRow, sourceRow, indexFromItem(grandParentItem), destinationRow))
    {
        return index;
    }

    parentItem->takeChild(sourceRow);
    grandParentItem->insertChild(destinationRow, item);
    endMoveRows();

    return indexFromItem(item);
}

QModelIndex QcwTreeModel::demote(const QModelIndex& index)
{
    QcwTreeItem* item = rowItem(index);
    if (!item)
    {
        return index;
    }

    QcwTreeItem* parentItem = item->parent();
    const int sourceRow = parentItem->rowOfChild(item);
    if (sourceRow == 0)
    {
        return index; // No preceding sibling to move this under
    }

    QcwTreeItem* sibling = parentItem->childAt(sourceRow - 1);
    const int destinationRow = sibling->childCount();

    if (!beginMoveRows(indexFromItem(parentItem), sourceRow, sourceRow, indexFromItem(sibling), destinationRow))
    {
        return index;
    }

    parentItem->takeChild(sourceRow);
    sibling->addChild(item);
    endMoveRows();

    return indexFromItem(item);
}

QcwTreeItem* QcwTreeModel::rowItem(const QModelIndex& index) const
{
    QcwTreeItem* item = itemForIndex(index);
    if (!item || item == mRootItem)
    {
        return nullptr;
    }

    // Reaching the root is what makes a row a row: an item whose chain stops at a detached item —
    // the row cut() holds, or one of its children — is still pointed at by an index, but the tree
    // no longer contains it.
    for (QcwTreeItem* ancestor = item->parent(); ancestor; ancestor = ancestor->parent())
    {
        if (ancestor == mRootItem)
        {
            return item;
        }
    }

    return nullptr;
}

QcwTreeItem* QcwTreeModel::itemForIndex(const QModelIndex& index) const
{
    if (index.isValid())
    {
        return static_cast<QcwTreeItem*>(index.internalPointer());
    }

    return mRootItem;
}

QModelIndex QcwTreeModel::indexFromItem(QcwTreeItem* item) const
{
    // A detached item is not a row of anything, so it has no index: the root has no index of its
    // own, and the row cut() holds has none until paste() puts it back.
    if (!item || item == mRootItem || !item->parent())
    {
        return QModelIndex();
    }

    return createIndex(item->parent()->rowOfChild(item), 0, item);
}

void QcwTreeModel::clearData()
{
    const int count = rowCount();
    if (count > 0)
    {
        removeRows(0, count);
    }

    delete mCutItem;
    mCutItem = nullptr;

    // Last, because what the hook releases is what the items were pointing at.
    clearExtraData();
}

QcwTreeItem* QcwTreeModel::rootItem() const
{
    return mRootItem;
}

QcwTreeItem* QcwTreeModel::cutItem() const
{
    return mCutItem;
}

QT_END_NAMESPACE
