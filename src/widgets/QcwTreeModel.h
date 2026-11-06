// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QAbstractItemModel>

QT_BEGIN_NAMESPACE

class QcwTreeItem;

/**
 * @class QcwTreeModel
 * @brief Base of the tree models whose rows are QcwTreeItem objects.
 *
 * @details The item tree carries the structure only. No cell value, no column caption and no
 * item type lives in this class, so a subclass supplies all four through the pure virtuals:
 * createItem() builds one row, columnLabels() names the columns, the inherited data() reads a
 * cell, and clearExtraData() releases whatever the subclass attached to its items. Without a
 * data() override every cell is empty, which is why the class is abstract in spirit as well as
 * in name.
 *
 * The rows hang from an invisible root item, exactly as the views expect: rootItem() owns the
 * top-level rows, a top-level row's index is the invalid QModelIndex(), itemForIndex() maps an
 * index to its item and hands out the root for an invalid one, and indexFromItem() is the
 * inverse and returns the invalid index for the root. Because a row has one parent, only column
 * 0 of an index may have children — index() answers an index outside column 0 with an invalid
 * index, rowCount() and columnCount() with 0.
 *
 * The model owns every item it was given: rows come from createItem(), removeRows() deletes the
 * row and its whole subtree, and the destructor deletes the tree together with the row cut() is
 * holding. A subclass therefore keeps pointers to items, never ownership.
 *
 * Every structural change is announced — begin/endInsertRows() for insertRows() and paste(),
 * begin/endRemoveRows() for removeRows() and cut(), begin/endMoveRows() for the two sibling moves
 * and for promote() and demote() — so a view's selection, expansion state and persistent indexes
 * follow the rows instead of being left behind with stale row numbers.
 *
 * cut() and paste() move rows within this model alone. The clipboard is one slot on the model,
 * not MIME data, so drag and drop, the operating system's clipboard and a second view never see
 * it, and there is no undo — the next cut() deletes the row the previous one left in the slot.
 *
 * An index outlives its row only as a pointer: the operations that change the structure ask
 * rowItem(), which answers null for an index that no longer leads to the root, and they then
 * return their argument unchanged rather than acting on a row the tree does not contain.
 *
 * @see QcwTreeItem, QcwTreeProxyModel
 */
class QCW_EXPORT QcwTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /**
     * @brief Builds a model with an empty tree.
     * @details The invisible root item is created here and owns every row from then on.
     */
    explicit QcwTreeModel(QObject* parent = nullptr);

    /// Deletes the whole tree, including the row the cut() slot is holding.
    ~QcwTreeModel() override;

    /**
     * @brief Gives the index of one cell of a row.
     * @param row Row within @p parent.
     * @param column Column of the cell.
     * @param parent Parent row; invalid for a top-level row.
     * @return The index, or an invalid index for a row, a column or a parent outside the tree.
     * @details createIndex() stores the QcwTreeItem* as the index's internal pointer, which is
     *          what itemForIndex() reads back. An out-of-range row needs no extra check here
     *          because QcwTreeItem::childAt() answers null rather than asserting.
     */
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief Gives the index of the row above.
     * @param index Index of a row.
     * @return The parent's index in column 0, or an invalid index for a top-level row — the
     *         invisible root item is not a row and has no index.
     */
    QModelIndex parent(const QModelIndex& index) const override;

    /**
     * @brief Adds @p count empty rows below a parent row.
     * @param row Row number the first new row gets; rowCount(parent) is an allowed value, which
     *            appends.
     * @param count Number of rows to add.
     * @param parent Parent row; invalid adds top-level rows.
     * @return True when the rows were added, false when @p row, @p count or @p parent does not
     *         describe a place in the tree.
     * @details Each row comes from createItem(), so a subclass gets its own item type, and the
     *          rows are empty until data() has something to say about them. A rejected call changes
     *          nothing and announces nothing, which is what keeps beginInsertRows() and
     *          QList::insert() away from an out-of-range row.
     * @see removeRows(), createItem()
     */
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    /**
     * @brief Takes @p count rows out of the tree and deletes them.
     * @param row First row to remove.
     * @param count Number of rows to remove.
     * @param parent Parent row; invalid removes top-level rows.
     * @return True when the rows were removed, false when the range is not inside @p parent.
     * @details A removed row takes its whole subtree with it, so an index into one of these rows or
     *          into any descendant of them stops referring to a live item when this returns. The
     *          same range rules as for insertRows() apply.
     * @see insertRows(), clearData()
     */
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    /**
     * @brief Gives the caption of a column.
     * @param section Column number.
     * @param orientation Qt::Horizontal only; a vertical header gets the default empty value.
     * @param role Requested role.
     * @return The columnLabels() text for DisplayRole, EditRole and ToolTipRole alike, and
     *         Qt::AlignCenter for TextAlignmentRole; an empty value for anything else.
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /// Number of columns, from columnLabels(); 0 for a parent outside column 0.
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /// Number of children of @p parent, or of the invisible root when it is invalid; 0 for a
    /// parent outside column 0.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief Moves a row one position up among its siblings.
     * @param index Index of the row to move.
     * @return The index the row has after the move, or @p index unchanged when it is already the
     *         first row of its parent.
     * @see moveDown(), moveRow()
     */
    QModelIndex moveUp(const QModelIndex& index);

    /**
     * @brief Moves a row one position down among its siblings.
     * @param index Index of the row to move.
     * @return The index the row has after the move, or @p index unchanged when it is already the
     *         last row of its parent.
     * @see moveUp(), moveRow()
     */
    QModelIndex moveDown(const QModelIndex& index);

    /**
     * @brief Takes a row out of the tree and keeps it in the cut() slot.
     * @param index Index of the row to cut.
     * @return An index a view can select in the tree that is left — the row's previous sibling,
     *         or its parent when it was the first row — or an invalid index when nothing is left
     *         at that level.
     * @details The row keeps its whole subtree and belongs to the model until paste() re-attaches
     *          it. The slot holds one row, so a second cut() deletes the row the first one left
     *          there.
     * @see paste(), cutItem()
     */
    QModelIndex cut(const QModelIndex& index);

    /**
     * @brief Re-attaches the row cut() took out, as the next sibling of @p index.
     * @param index Index of a row in the tree; the pasted row lands under the same parent, right
     *              after it.
     * @return The index of the pasted row.
     * @note The slot is emptied by this call, so a second paste() does nothing — a row standing in
     *       two positions at once would be deleted twice as well.
     * @see cut()
     */
    QModelIndex paste(const QModelIndex& index);

    /**
     * @brief Moves a row one level up, beside its former parent.
     * @param index Index of the row to outdent.
     * @return The index the row has now, or @p index unchanged when it is already a top-level row.
     * @details The row becomes the sibling right after its former parent, which keeps it near the
     *          place the reader was looking at. Its own subtree moves with it.
     * @see demote(), cut()
     */
    QModelIndex promote(const QModelIndex& index);

    /**
     * @brief Moves a row one level down, as the last child of its previous sibling.
     * @param index Index of the row to indent.
     * @return The index the row has now, or @p index unchanged when it is the first row and has no
     *         previous sibling to indent into.
     * @see promote(), cut()
     */
    QModelIndex demote(const QModelIndex& index);

    /**
     * @brief Empties the tree and lets a subclass release the data it attached to the rows.
     * @details Every top-level row goes through removeRows(), subtrees with them, and the row in the
     *          cut() slot is deleted. clearExtraData() runs last, once no item is left that could
     *          still point at what it releases.
     * @see clearExtraData(), removeRows()
     */
    void clearData();

protected:
    /**
     * @brief Finds the item an index stands for.
     * @param index Index to look up.
     * @return The item, rootItem() for an invalid index, or null for a valid index that names no
     *         row.
     * @details The invalid index is the top-level parent, and the invisible root is what answers it
     *          — which is why a top-level row has no index of its own. A stale or foreign index is
     *          answered with null rather than with the root, so a caller's check fails instead of
     *          quietly treating the whole tree as the row it was given.
     * @see indexFromItem()
     */
    QcwTreeItem* itemForIndex(const QModelIndex& index) const;

    /**
     * @brief Finds the row an index stands for, refusing one that is no longer in the tree.
     * @param index Index to look up.
     * @return The item, or null for an invalid index, for an index with no item behind it, and for
     *         an item whose chain of parents does not end at rootItem().
     * @details The last case is what makes the mutating operations safe to call twice: cut() leaves
     *          the row it took out alive, an index into it or into one of its children stays
     *          pointable at, and acting on such a stale index would move a subtree the tree does not
     *          contain. Every operation that changes the structure asks this instead of
     *          itemForIndex(), and returns its argument unchanged when the answer is null.
     * @see itemForIndex(), indexFromItem()
     */
    QcwTreeItem* rowItem(const QModelIndex& index) const;

    /**
     * @brief Builds the index of an item.
     * @param item Item to look up; its row in its parent is the row of the result, in column 0.
     * @return The index, or an invalid index for the root item, for a null pointer and for a row
     *         that is attached to no parent — a detached row is not a row of any tree, so it has no
     *         index to hand out.
     * @see itemForIndex(), rowItem()
     */
    QModelIndex indexFromItem(QcwTreeItem* item) const;

    /// The invisible root item, which owns the top-level rows. @see itemForIndex()
    QcwTreeItem* rootItem() const;

    /// The row cut() detached and paste() has not re-attached yet; null while the slot is empty.
    QcwTreeItem* cutItem() const;

    /**
     * @brief Releases the data a subclass attached to its items.
     * @details Called by clearData(). The items themselves stay as they are, so this hook drops
     *          only what hangs outside them.
     */
    virtual void clearExtraData() = 0;

private:
    /**
     * @brief Moves a row to its neighbour @p delta rows away, within the same parent.
     * @param index Index of the row to move.
     * @param delta -1 moves up, +1 moves down.
     * @return The index the row has after the move, or @p index unchanged when @p delta points
     *         outside the siblings.
     * @details The shared body of moveUp() and moveDown(). begin/endMoveRows() around the change is
     *          what carries the views' persistent indexes along, which a plain swap of two list
     *          positions would leave behind.
     */
    QModelIndex moveRow(const QModelIndex& index, int delta);

    /**
     * @brief Builds one row of the subclass's item type.
     * @return An item that belongs to no parent; the caller adopts it.
     * @details insertRows() calls this once per row and hands the result to the tree, so the
     *          returned item must not be inserted anywhere by the factory itself.
     */
    virtual QcwTreeItem* createItem() const = 0;

    /**
     * @brief The captions of the columns, in column order.
     * @details headerData() and columnCount() ask for this on every call, so the returned list
     *          must be a member of the subclass — the reference would dangle if the override
     *          built a temporary — and must not be rebuilt per call either. Its size is the
     *          model's column count.
     */
    virtual const QStringList& columnLabels() const = 0;

private:
    QcwTreeItem* mRootItem { nullptr };
    QcwTreeItem* mCutItem { nullptr };
};

QT_END_NAMESPACE
