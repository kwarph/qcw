// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QList>

QT_BEGIN_NAMESPACE

/**
 * @class QcwTreeItem
 * @brief One row of a QcwTreeModel tree: a parent pointer, the children it owns, and whatever a
 *        subclass adds.
 *
 * @details An item owns its children, so deleting an item deletes its whole subtree, and the
 * destructor is virtual because subclasses add data that has to be released with the row. The
 * destructor also takes the item out of its parent's list, which is why deleting a row that is
 * still attached cannot leave a dangling pointer behind — but it tells no view, so a row of a live
 * model goes through QcwTreeModel::removeRows() instead.
 *
 * Ownership moves in exactly three ways: addChild() and insertChild() take an unattached item into
 * the tree, takeChild() detaches a child and hands it to the caller — which owns it and must either
 * delete it or attach it again — and clearChildren() deletes the children. The invisible root item
 * is an ordinary item and is where the top-level rows live, so a top-level row is not a detached
 * one: its parent is the root, and only takeChild() leaves mParent null.
 *
 * A row is its position in the parent's list, which gives three different answers to a bad row
 * number: childAt() returns null so callers can probe, rowOfChild() returns -1 for an item that is
 * not a child, and insertChild() and takeChild() assert instead.
 *
 * Copying is deleted: the children are owned, so a copied list of them would delete every row
 * twice.
 *
 * @see QcwTreeModel
 */
class QCW_EXPORT QcwTreeItem
{
public:
    /// Builds an item; with a @p parent it is not added to it, so callers pair the two calls.
    explicit QcwTreeItem(QcwTreeItem* parent = nullptr);

    /// Deletes the item, its whole subtree, and its own entry in its parent's list.
    virtual ~QcwTreeItem();

    /// The parent item, or null for a row that takeChild() detached.
    QcwTreeItem* parent() const
    {
        return mParent;
    }

    /// The child at @p row, or null when @p row is outside the children.
    QcwTreeItem* childAt(int row) const
    {
        return mChildren.value(row);
    }

    /// The row of @p child, or -1 when the item is not one of the children.
    int rowOfChild(QcwTreeItem* child) const
    {
        return mChildren.indexOf(child);
    }

    /// Number of children.
    int childCount() const
    {
        return mChildren.count();
    }

    /// True when the row has children; a view shows the expansion control on such a row.
    bool hasChildren() const
    {
        return !mChildren.isEmpty();
    }

    /// The children, in row order. The copy holds pointers, not ownership.
    QList<QcwTreeItem*> children() const
    {
        return mChildren;
    }

    /**
     * @brief Adopts @p item as the child at @p row.
     * @details @p item must belong to no parent, or two lists hold it, and deleting it only takes it
     *          out of the one that adopted it last. @p row is passed straight to QList::insert(), so
     *          it must be an existing row or childCount() to append.
     * @see addChild(), takeChild()
     */
    void insertChild(int row, QcwTreeItem* item)
    {
        item->mParent = this;
        mChildren.insert(row, item);
    }

    /**
     * @brief Adopts @p item as the last child.
     * @details Same rule as insertChild(): the item must belong to no parent yet.
     * @see insertChild(), takeChild()
     */
    void addChild(QcwTreeItem* item)
    {
        item->mParent = this;
        mChildren.push_back(item);
    }

    /**
     * @brief Detaches the child at @p row and gives it to the caller.
     * @return The detached item, whose mParent is null from now on.
     * @details Ownership crosses here: the caller must delete the item or attach it somewhere, and
     *          so must every caller of this method. @p row must name an existing child.
     * @see insertChild(), addChild()
     */
    QcwTreeItem* takeChild(int row);

    /// Deletes every child, and their subtrees with them.
    void clearChildren();

private:
    Q_DISABLE_COPY_MOVE(QcwTreeItem)

    QcwTreeItem* mParent { nullptr };
    QList<QcwTreeItem*> mChildren;
};

QT_END_NAMESPACE
