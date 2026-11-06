// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTreeItem.h"

QT_BEGIN_NAMESPACE

QcwTreeItem::QcwTreeItem(QcwTreeItem* parent) :
    mParent(parent)
{
}

QcwTreeItem::~QcwTreeItem()
{
    // Out of the parent's list first: deleting a row that is still attached must not leave the
    // pointer behind. No view is told, which is why a row of a live model goes through the model's
    // removeRows() instead.
    if (mParent)
    {
        mParent->mChildren.removeOne(this);
    }

    clearChildren();
}

QcwTreeItem* QcwTreeItem::takeChild(int row)
{
    QcwTreeItem* item = mChildren.takeAt(row);
    if (item)
    {
        item->mParent = nullptr;
    }

    return item;
}

void QcwTreeItem::clearChildren()
{
    // Emptied before the deletes, because every child takes itself out of this list.
    auto children = std::exchange(mChildren, {});
    qDeleteAll(children);
}

QT_END_NAMESPACE
