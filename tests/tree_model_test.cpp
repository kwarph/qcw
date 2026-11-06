// Drives QcwTreeModel: the invisible-root mapping every operation goes through; the range checks
// that keep a bad insertRows() or removeRows() away from begin/end*Rows() and away from building
// rows at all; the sibling moves and the outdent/indent pair, each announced with
// begin/endMoveRows() so a row's own persistent index and a view's selection follow it instead of
// staying at the old number; the one-slot cut()/paste() — a row pasted back with its subtree, the
// subtree a second cut() deletes, a paste() with an empty slot, the stale index of a row that
// already left the tree, and the destructor that owns a row no paste re-attached; clearData(),
// which empties tree, slot and the subclass's side data in that order; and QcwTreeItem's ownership
// handoff, where takeChild() hands the caller the row and deleting an attached row takes it out of
// its parent's list.

#include "QcwTreeItem.h"
#include "QcwTreeModel.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QHash>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QString>
#include <QStringList>
#include <QTreeView>
#include <cstdio>
#include <type_traits>

namespace
{

constexpr int NameColumn = 0;
constexpr int NoteColumn = 1;

int failures = 0;

// Every row is counted, so a row that leaks or is deleted twice shows up as a live count that does
// not return to the baseline.
int liveItems = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

/// A row of the test model: a name, and the order createItem() built it in.
class TestItem : public QcwTreeItem
{
public:
    TestItem()
    {
        ++liveItems;
    }

    ~TestItem() override
    {
        --liveItems;
    }

    QString text;
    int ordinal = 0;
};

class TestTreeModel : public QcwTreeModel
{
public:
    explicit TestTreeModel(QObject* parent = nullptr) : QcwTreeModel(parent)
    {
        mLabels << QStringLiteral("Name") << QStringLiteral("Note");
    }

    using QcwTreeModel::createIndex;
    using QcwTreeModel::cutItem;
    using QcwTreeModel::indexFromItem;
    using QcwTreeModel::itemForIndex;
    using QcwTreeModel::rootItem;

    /// Adds a row below @p parent and names it @p text.
    TestItem* addRow(const QString& text, const QModelIndex& parent = {})
    {
        const int row = rowCount(parent);
        if (!insertRows(row, 1, parent))
        {
            return nullptr;
        }

        auto* item = static_cast<TestItem*>(itemForIndex(parent)->childAt(row));
        item->text = text;
        return item;
    }

    /// The names of @p parent's children, in row order.
    QStringList texts(const QModelIndex& parent = {}) const
    {
        QStringList names;
        for (int row = 0; row < rowCount(parent); ++row)
        {
            names << index(row, NameColumn, parent).data(Qt::DisplayRole).toString();
        }
        return names;
    }

    /// The child of @p parent named @p text, or an invalid index while no such row is there.
    QModelIndex indexOf(const QString& text, const QModelIndex& parent = {}) const
    {
        for (int row = 0; row < rowCount(parent); ++row)
        {
            const auto child = index(row, NameColumn, parent);
            if (child.data(Qt::DisplayRole).toString() == text)
            {
                return child;
            }
        }

        return {};
    }

    /// Which createItem() call made the row, so two operations can be shown to name one item.
    int ordinalAt(const QModelIndex& index) const
    {
        const auto* item = itemAt(index);
        return item ? item->ordinal : -1;
    }

    const TestItem* itemAt(const QModelIndex& index) const
    {
        return static_cast<const TestItem*>(itemForIndex(index));
    }

    void setNote(const QcwTreeItem* item, const QString& note)
    {
        mNotes.insert(item, note);
    }

    int notesReleased() const
    {
        return mNotesReleased;
    }

protected:
    QcwTreeItem* createItem() const override
    {
        auto* item = new TestItem;
        item->ordinal = ++mCreated;
        return item;
    }

    const QStringList& columnLabels() const override
    {
        return mLabels;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole)
        {
            return {};
        }

        const auto* item = itemAt(index);
        if (!item)
        {
            return {};
        }

        if (index.column() == NameColumn)
        {
            return item->text;
        }

        return mNotes.value(item);
    }

    void clearExtraData() override
    {
        mNotes.clear();
        ++mNotesReleased;
    }

private:
    QStringList mLabels;
    QHash<const QcwTreeItem*, QString> mNotes;
    int mNotesReleased = 0;
    mutable int mCreated = 0;
};

/// Counts what the model owes its views, so a structural change that goes unannounced fails a check
/// instead of only corrupting a widget nobody looks at.
struct Notifications
{
    int aboutToInsert = 0;
    int inserted = 0;
    int aboutToRemove = 0;
    int removed = 0;
    int aboutToMove = 0;
    int moved = 0;
    int dataChanged = 0;

    void reset()
    {
        *this = {};
    }

    /// True when exactly one announced move happened and nothing else was announced.
    bool oneMove() const
    {
        return aboutToMove == 1 && moved == 1 && aboutToInsert == 0 && inserted == 0
            && aboutToRemove == 0 && removed == 0 && dataChanged == 0;
    }
};

void watch(const TestTreeModel& model, Notifications& sink)
{
    auto& out = sink;
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, [&out] { ++out.aboutToInsert; });
    QObject::connect(&model, &QAbstractItemModel::rowsInserted, [&out] { ++out.inserted; });
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeRemoved, [&out] { ++out.aboutToRemove; });
    QObject::connect(&model, &QAbstractItemModel::rowsRemoved, [&out] { ++out.removed; });
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeMoved, [&out] { ++out.aboutToMove; });
    QObject::connect(&model, &QAbstractItemModel::rowsMoved, [&out] { ++out.moved; });
    QObject::connect(&model, &QAbstractItemModel::dataChanged, [&out] { ++out.dataChanged; });
}

// alpha(a1(x), a2), beta(b1), gamma — three top-level rows, one branch three levels deep.
struct Fixture
{
    // Declared first so it outlives the model: the connections belong to the model and write into
    // this struct.
    Notifications notifications;
    TestTreeModel model;

    Fixture()
    {
        model.addRow(QStringLiteral("alpha"));
        model.addRow(QStringLiteral("beta"));
        model.addRow(QStringLiteral("gamma"));

        const auto alpha = model.indexOf(QStringLiteral("alpha"));
        const auto beta = model.indexOf(QStringLiteral("beta"));
        model.addRow(QStringLiteral("a1"), alpha);
        model.addRow(QStringLiteral("a2"), alpha);
        model.addRow(QStringLiteral("x"), model.indexOf(QStringLiteral("a1"), alpha));
        model.addRow(QStringLiteral("b1"), beta);

        // Watching starts after the fixture is built, so the numbers below describe only what a
        // test itself does.
        watch(model, notifications);
    }
};

/// The name the one-slot clipboard holds; empty while the slot holds nothing.
QString cutText(TestTreeModel& model)
{
    auto* item = model.cutItem();
    return item ? static_cast<TestItem*>(item)->text : QString {};
}

const QStringList kTopLevel { QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma") };
const QStringList kAlphaChildren { QStringLiteral("a1"), QStringLiteral("a2") };

void testRootMappingAndHeaders()
{
    std::fprintf(stderr, "--- root mapping, columns and headers ---\n");

    Fixture f;

    check(f.model.texts() == kTopLevel, "the top-level rows are the children of the invisible root");
    check(!f.model.indexOf(QStringLiteral("alpha")).parent().isValid(),
          "a top-level row's parent is the invalid index");

    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    check(f.model.texts(alpha) == kAlphaChildren, "the second level reads back in row order");

    const auto a1 = f.model.indexOf(QStringLiteral("a1"), alpha);
    const auto x = f.model.indexOf(QStringLiteral("x"), a1);
    check(x.data(Qt::DisplayRole).toString() == QStringLiteral("x"), "a third level is reachable");
    check(f.model.parent(x) == a1 && f.model.parent(a1) == alpha, "each row names its own parent");
    check(!f.model.parent(alpha).isValid(), "and the top level stops at the root");

    check(f.model.columnCount() == 2, "columnCount() comes from columnLabels()");
    check(f.model.headerData(NameColumn, Qt::Horizontal, Qt::DisplayRole).toString() == QStringLiteral("Name"),
          "the first header is the first label");
    check(f.model.headerData(NoteColumn, Qt::Horizontal, Qt::ToolTipRole).toString() == QStringLiteral("Note"),
          "a header's tooltip is its text");
    check(f.model.headerData(NoteColumn, Qt::Horizontal, Qt::EditRole).toString() == QStringLiteral("Note"),
          "and its edit text the same");
    check(!f.model.headerData(2, Qt::Horizontal, Qt::DisplayRole).isValid(),
          "a section outside the labels has no text");
    check(!f.model.headerData(NameColumn, Qt::Vertical, Qt::DisplayRole).isValid(),
          "a vertical header is left to the default");
    check(f.model.headerData(NameColumn, Qt::Horizontal, Qt::TextAlignmentRole).toInt() == Qt::AlignCenter,
          "headers are centered");

    const auto nameInRowZero = f.model.index(0, NameColumn);
    const auto noteInRowZero = f.model.index(0, NoteColumn);
    check(nameInRowZero.isValid() && noteInRowZero.isValid() && nameInRowZero != noteInRowZero,
          "both columns of a row have an index");
    check(f.model.rowCount(noteInRowZero) == 0 && f.model.columnCount(noteInRowZero) == 0,
          "only column 0 of an index may have children");
    check(!f.model.index(0, NameColumn, noteInRowZero).isValid(),
          "index() refuses a parent outside the first column");

    check(!f.model.index(3, NameColumn).isValid() && !f.model.index(-1, NameColumn).isValid(),
          "a row outside the children gives an invalid index");
    check(!f.model.index(0, 2).isValid() && !f.model.index(0, -1).isValid(),
          "and so does a column outside the labels");

    f.model.setNote(f.model.itemAt(alpha), QStringLiteral("kept aside"));
    check(alpha.siblingAtColumn(NoteColumn).data(Qt::DisplayRole).toString() == QStringLiteral("kept aside"),
          "the note column reads data the subclass keeps beside the items");

    // The hardened mapping: a valid index that stands for no row is not the root row.
    const auto nullPointerIndex = f.model.createIndex(0, NameColumn, nullptr);
    check(f.model.itemForIndex(nullPointerIndex) == nullptr,
          "an index with no item behind it is not answered with the root");
    check(f.model.rowCount(nullPointerIndex) == 0 && !f.model.index(0, NameColumn, nullPointerIndex).isValid(),
          "and such an index has no children");
    check(!f.model.parent(nullPointerIndex).isValid(), "nor a parent");
    check(f.model.itemForIndex(QModelIndex()) == f.model.rootItem(),
          "the invalid index does answer the root");
    check(!f.model.indexFromItem(f.model.rootItem()).isValid() && !f.model.indexFromItem(nullptr).isValid(),
          "and indexFromItem() gives the invalid index back for the root and for null");
    check(f.model.indexFromItem(f.model.itemForIndex(a1)) == a1, "indexFromItem() round-trips a row");
}

void testRangeChecksChangeNothing()
{
    std::fprintf(stderr, "--- insertRows() and removeRows() ranges ---\n");

    Fixture f;
    const int itemsBefore = liveItems;
    f.notifications.reset();

    check(!f.model.insertRows(0, 0), "an insert of no rows is refused");
    check(!f.model.insertRows(0, -1), "a negative count is refused");
    check(!f.model.insertRows(-1, 1), "a negative row is refused");
    check(!f.model.insertRows(4, 1), "a row past the end of the top level is refused");
    check(!f.model.insertRows(0, 1, f.model.index(0, NoteColumn)),
          "a parent outside the first column is refused");
    check(!f.model.removeRows(0, 0), "a remove of no rows is refused");
    check(!f.model.removeRows(-1, 1), "a remove starting below the first row is refused");
    check(!f.model.removeRows(2, 2), "a range running past the last row is refused");
    check(!f.model.removeRows(5, 1), "a remove outside the top level is refused");

    check(f.model.texts() == kTopLevel, "a refused call leaves the tree as it was");
    check(liveItems == itemsBefore, "a refused insert does not even build the rows it was asked for");
    check(f.notifications.aboutToInsert == 0 && f.notifications.inserted == 0
            && f.notifications.aboutToRemove == 0 && f.notifications.removed == 0,
          "and a refused call announces nothing");

    check(f.model.insertRows(3, 2), "rowCount() is an allowed insert row, so appending works");
    check(f.model.rowCount() == 5 && f.notifications.aboutToInsert == 1 && f.notifications.inserted == 1,
          "the two rows arrive with one announcement");
    check(f.model.removeRows(3, 2) && f.model.texts() == kTopLevel, "and the same rows leave again");
    check(liveItems == itemsBefore, "a removed row is deleted, subtree included");
    check(f.notifications.aboutToRemove == 1 && f.notifications.removed == 1,
          "the remove is announced once, in the pair endRemoveRows() closes");

    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    const auto a1 = f.model.indexOf(QStringLiteral("a1"), alpha);
    check(f.model.insertRows(1, 1, a1), "a row can be appended under a nested parent");
    check(f.model.rowCount(a1) == 2 && f.model.texts(a1).size() == 2, "and shows up under it");
    check(f.model.removeRows(1, 1, a1) && f.model.texts(a1) == (QStringList { QStringLiteral("x") }),
          "removing it again leaves the original children");
    check(liveItems == itemsBefore, "the nested round trip gave nothing back");
}

void testInsertedRowsKeepCreationOrder()
{
    std::fprintf(stderr, "--- inserted rows keep the order createItem() made them in ---\n");

    Fixture f;
    check(f.model.insertRows(0, 3), "three rows are inserted above the tree");
    check(f.model.ordinalAt(f.model.index(0, NameColumn)) == 8
            && f.model.ordinalAt(f.model.index(1, NameColumn)) == 9
            && f.model.ordinalAt(f.model.index(2, NameColumn)) == 10,
          "the first row built is the first row of the block, not the last");
    check(f.model.ordinalAt(f.model.index(3, NameColumn)) == 1,
          "and the row that was there already sits below the block");
}

void testMoveUpAndMoveDown()
{
    std::fprintf(stderr, "--- moveUp() and moveDown() ---\n");

    Fixture f;
    const auto beta = f.model.indexOf(QStringLiteral("beta"));
    f.notifications.reset();

    const auto movedUp = f.model.moveUp(beta);
    check(f.model.texts() == (QStringList { QStringLiteral("beta"), QStringLiteral("alpha"),
                                           QStringLiteral("gamma") }),
          "the row exchanges places with its previous sibling");
    check(movedUp.row() == 0 && movedUp.data(Qt::DisplayRole).toString() == QStringLiteral("beta"),
          "moveUp() returns the index the row has now");
    check(f.notifications.oneMove(), "the move is announced with begin/endMoveRows()");

    const auto movedDown = f.model.moveDown(movedUp);
    check(f.model.texts() == kTopLevel, "moveDown() puts it back");
    check(movedDown == beta, "and returns the original position");

    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    check(f.model.moveUp(alpha) == alpha, "a first row cannot move up");
    const auto gamma = f.model.indexOf(QStringLiteral("gamma"));
    check(f.model.moveDown(gamma) == gamma, "a last row cannot move down");
    check(f.model.texts() == kTopLevel, "an impossible move leaves the order alone");

    const auto a2 = f.model.indexOf(QStringLiteral("a2"), alpha);
    const auto nestedUp = f.model.moveUp(a2);
    check(f.model.texts(alpha) == (QStringList { QStringLiteral("a2"), QStringLiteral("a1") }),
          "a nested row moves among its own siblings");
    check(f.model.parent(nestedUp) == alpha, "and stays under the same parent");
    check(f.model.texts(f.model.indexOf(QStringLiteral("a1"), alpha)) == (QStringList { QStringLiteral("x") }),
          "a moved row keeps the subtree that hangs below it");

    check(!f.model.moveUp(QModelIndex()).isValid() && !f.model.moveDown(QModelIndex()).isValid(),
          "an invalid index moves nothing");
    check(f.notifications.dataChanged == 0, "a move is not announced as a data change");
}

void testSelectionAndPersistentIndexesFollowTheRow()
{
    std::fprintf(stderr, "--- a view's selection and a persistent index follow the moved row ---\n");

    Fixture f;
    QTreeView view;
    view.setModel(&f.model);

    const auto beta = f.model.indexOf(QStringLiteral("beta"));
    const QPersistentModelIndex held(beta);
    auto* selection = view.selectionModel();
    selection->select(beta, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    selection->setCurrentIndex(beta, QItemSelectionModel::NoUpdate);
    check(selection->currentIndex() == beta && selection->selectedRows() == (QModelIndexList { beta }),
          "the view selected the second top-level row");

    const auto movedUp = f.model.moveUp(beta);
    check(held.row() == 0 && held.data(Qt::DisplayRole).toString() == QStringLiteral("beta"),
          "the persistent index followed the row instead of keeping the old number");
    check(selection->currentIndex() == movedUp
            && selection->selectedRows() == (QModelIndexList { movedUp }),
          "and the view's selection still names the same row, where it moved to");

    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    const auto a1 = f.model.indexOf(QStringLiteral("a1"), alpha);
    const QPersistentModelIndex heldChild(a1);
    f.model.promote(a1);
    check(!heldChild.parent().isValid() && heldChild == f.model.indexOf(QStringLiteral("a1")),
          "an outdent carries the persistent index into the new parent as well");

    const auto gamma = f.model.indexOf(QStringLiteral("gamma"));
    const QPersistentModelIndex heldGamma(gamma);
    f.model.cut(gamma);
    check(!heldGamma.isValid(), "cut() announces a remove, so an index into the cut row stops being valid");
    check(!heldChild.data(Qt::DisplayRole).toString().compare(QStringLiteral("a1")),
          "while the rows that stayed are untouched");
}

void testCutAndPaste()
{
    std::fprintf(stderr, "--- cut() and paste() ---\n");

    Fixture f;
    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    const auto beta = f.model.indexOf(QStringLiteral("beta"));
    const int betaOrdinal = f.model.ordinalAt(beta);
    f.notifications.reset();

    const auto afterCut = f.model.cut(beta);
    check(f.model.texts() == (QStringList { QStringLiteral("alpha"), QStringLiteral("gamma") }),
          "cut() takes the row out of the tree");
    check(f.notifications.aboutToRemove == 1 && f.notifications.removed == 1,
          "the removal is announced in the pair endRemoveRows() closes");
    check(afterCut == alpha, "cut() returns the previous sibling, which a view can select");
    check(cutText(f.model) == QStringLiteral("beta"), "the row waits in the one-slot clipboard");
    check(!f.model.indexOf(QStringLiteral("beta")).isValid(), "the cut row is no longer reachable as a row");

    const auto pasted = f.model.paste(alpha);
    check(f.model.texts() == kTopLevel, "paste() puts it back after the row it was given");
    check(pasted == beta && f.model.ordinalAt(pasted) == betaOrdinal,
          "the pasted row is the very item that was cut");
    check(f.notifications.inserted == 1 && f.notifications.removed == 1,
          "the insertion is announced once, on top of the one remove");
    check(f.model.cutItem() == nullptr, "paste() empties the slot");

    f.notifications.reset();
    check(f.model.paste(alpha) == alpha, "a second paste() has nothing to attach and says so");
    check(f.model.texts() == kTopLevel && f.notifications.inserted == 0
            && f.notifications.aboutToInsert == 0 && liveItems == 7,
          "it changes nothing, announces nothing, and does not dereference the empty slot");

    const auto gamma = f.model.indexOf(QStringLiteral("gamma"));
    check(f.model.paste(gamma.siblingAtColumn(NoteColumn)) == gamma.siblingAtColumn(NoteColumn),
          "an empty slot answers any index, in any column, with the index it was given");
    check(f.model.texts() == kTopLevel, "and the tree keeps the three rows it had");

    f.model.cut(gamma);
    const auto pastedAgain = f.model.paste(beta.siblingAtColumn(NoteColumn));
    check(pastedAgain.column() == NameColumn
            && pastedAgain.data(Qt::DisplayRole).toString() == QStringLiteral("gamma"),
          "paste() takes the row from the item, not from the column the index happened to be in");
    check(f.model.texts() == kTopLevel, "and lands it after the row that index stood for");
    check(liveItems == 7, "the second round trip built nothing and lost nothing");
}

void testStaleIndexesNameNoRow()
{
    std::fprintf(stderr, "--- an index whose row left the tree names no row ---\n");

    Fixture f;
    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    const auto a1 = f.model.indexOf(QStringLiteral("a1"), alpha);
    const int itemsBefore = liveItems;
    f.notifications.reset();

    f.model.cut(alpha);

    // Both indexes still point at live items, because the slot keeps them, but the tree no longer
    // contains them, so every structural operation has to refuse them.
    check(f.model.cut(a1) == a1, "an index into the cut row's subtree is refused by cut()");
    check(f.model.cut(alpha) == alpha, "and the cut row is not cut a second time");
    check(cutText(f.model) == QStringLiteral("alpha"), "so the slot still holds the row the first cut() left");
    check(f.model.promote(a1) == a1 && f.model.demote(a1) == a1, "nor is that row outdented or indented");
    check(f.model.moveUp(a1) == a1 && f.model.moveDown(a1) == a1, "nor moved among its siblings");
    check(f.model.paste(a1) == a1, "nor pasted below itself");
    check(f.model.texts() == (QStringList { QStringLiteral("beta"), QStringLiteral("gamma") }),
          "the tree is exactly what the one cut() left");
    check(f.notifications.aboutToRemove == 1 && f.notifications.aboutToMove == 0
            && f.notifications.inserted == 0 && f.notifications.dataChanged == 0,
          "and a refused operation announces nothing on top of the one remove");
    check(liveItems == itemsBefore, "nothing was deleted, so no row of the subtree went missing");

    const auto beta = f.model.indexOf(QStringLiteral("beta"));
    const auto pasted = f.model.paste(beta);
    check(pasted == f.model.indexOf(QStringLiteral("alpha")),
          "and the row the stale indexes named is still the very row paste() attaches");
    check(f.model.texts(a1) == (QStringList { QStringLiteral("x") }), "with its subtree where it was");
    check(liveItems == itemsBefore, "the whole detour neither built nor freed an item");
}

void testCutCarriesAndReplacesItsSubtree()
{
    std::fprintf(stderr, "--- the cut row's subtree travels with it, and one slot replaces the last ---\n");

    Fixture f;
    const int itemsBefore = liveItems;
    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));

    const auto afterCut = f.model.cut(alpha);
    check(f.model.texts() == (QStringList { QStringLiteral("beta"), QStringLiteral("gamma") }),
          "a row with children can be cut out of the top level");
    check(!afterCut.isValid(), "the first row leaving has no sibling to point at and no parent index");
    check(f.model.cutItem()->childCount() == 2, "its children came along into the slot");
    check(liveItems == itemsBefore, "the slot keeps the whole subtree alive");

    const auto pasted = f.model.paste(f.model.indexOf(QStringLiteral("gamma")));
    check(f.model.texts() == (QStringList { QStringLiteral("beta"), QStringLiteral("gamma"),
                                           QStringLiteral("alpha") }),
          "and it lands after the row paste() was given");
    const auto alphaAgain = f.model.indexOf(QStringLiteral("alpha"));
    check(f.model.texts(alphaAgain) == kAlphaChildren, "the second level came back intact");
    check(f.model.texts(f.model.indexOf(QStringLiteral("a1"), alphaAgain)) == (QStringList { QStringLiteral("x") }),
          "three levels deep survived the round trip");
    check(liveItems == itemsBefore && pasted == alphaAgain, "nothing was built or lost on the way");

    f.model.cut(f.model.indexOf(QStringLiteral("beta")));
    check(liveItems == itemsBefore, "cutting into an empty slot only moves the row out of the tree");
    check(cutText(f.model) == QStringLiteral("beta") && f.model.cutItem()->childCount() == 1,
          "the slot holds that row with its own child");
    check(f.model.texts() == (QStringList { QStringLiteral("gamma"), QStringLiteral("alpha") }),
          "and the tree lost only that row");

    f.model.cut(f.model.indexOf(QStringLiteral("gamma")));
    check(liveItems == itemsBefore - 2, "the second cut() deletes the row the first one left in the slot");
    check(cutText(f.model) == QStringLiteral("gamma") && f.model.cutItem()->childCount() == 0,
          "the slot holds the newer row, and nothing but the newer row");
    check(f.model.texts() == (QStringList { QStringLiteral("alpha") }), "one row is left in the tree");

    f.model.clearData();
    check(liveItems == 0, "clearData() empties the tree and the slot with it");
}

void testCutRowIsOwnedOnceOnDestruction()
{
    std::fprintf(stderr, "--- the destructor owns a row the slot still holds ---\n");

    const int itemsBefore = liveItems;
    {
        TestTreeModel model;
        model.addRow(QStringLiteral("one"));
        model.addRow(QStringLiteral("two"));
        const auto one = model.indexOf(QStringLiteral("one"));
        check(!model.cut(one).isValid(), "the first row leaves the top level with nothing beside it");
        check(model.rootItem()->childCount() == 1, "the tree holds one row");
        check(model.cutItem() != nullptr, "the slot holds the other");
    }
    check(liveItems == itemsBefore, "tree and slot were each deleted once, so nothing leaked or double-freed");
}

void testPromote()
{
    std::fprintf(stderr, "--- promote() ---\n");

    Fixture f;
    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    const auto a1 = f.model.indexOf(QStringLiteral("a1"), alpha);
    const int a1Ordinal = f.model.ordinalAt(a1);
    const QPersistentModelIndex held(a1);
    f.notifications.reset();

    const auto promoted = f.model.promote(a1);
    check(f.model.texts() == (QStringList { QStringLiteral("alpha"), QStringLiteral("a1"),
                                           QStringLiteral("beta"), QStringLiteral("gamma") }),
          "the row becomes the sibling right after its former parent");
    check(f.model.texts(alpha) == (QStringList { QStringLiteral("a2") }), "and left its former parent");
    check(promoted.row() == 1 && !promoted.parent().isValid() && f.model.ordinalAt(promoted) == a1Ordinal,
          "it is a top-level row now, and the same row");
    check(f.notifications.oneMove(), "the outdent is announced with begin/endMoveRows()");
    check(held.row() == 1 && !held.parent().isValid(), "the row's own persistent index moved with it");
    check(f.model.texts(promoted) == (QStringList { QStringLiteral("x") }), "its subtree came along");

    f.notifications.reset();
    check(f.model.promote(promoted) == promoted, "a top-level row has nothing to outdent into");
    check(f.notifications.aboutToMove == 0 && f.notifications.moved == 0,
          "and a refused outdent announces nothing");

    f.notifications.reset();
    const auto demotedBack = f.model.demote(promoted);
    check(f.model.texts(alpha) == (QStringList { QStringLiteral("a2"), QStringLiteral("a1") }),
          "the same row indents back as the last child of its previous sibling");
    check(demotedBack.parent() == alpha && f.model.ordinalAt(demotedBack) == a1Ordinal,
          "as its previous sibling's last child, still the same row");
    check(f.notifications.oneMove(), "the indent is announced the same way");
    check(held.parent() == alpha && held.row() == 1, "and the persistent index followed it down");

    f.notifications.reset();
    check(!f.model.demote(QModelIndex()).isValid() && !f.model.promote(QModelIndex()).isValid(),
          "an invalid index neither indents nor outdents");
    check(f.notifications.aboutToMove == 0, "and announces nothing either");
}

void testPromoteAndDemoteInTheTopLevel()
{
    std::fprintf(stderr, "--- promote() and demote() between top-level rows ---\n");

    Fixture f;
    const auto gamma = f.model.indexOf(QStringLiteral("gamma"));
    f.notifications.reset();

    const auto demoted = f.model.demote(gamma);
    const auto beta = f.model.indexOf(QStringLiteral("beta"));
    check(f.model.texts() == (QStringList { QStringLiteral("alpha"), QStringLiteral("beta") }),
          "the last top-level row goes down a level");
    check(f.model.texts(beta) == (QStringList { QStringLiteral("b1"), QStringLiteral("gamma") }),
          "as the last child of its previous top-level sibling");
    check(demoted.parent() == beta && f.model.ordinalAt(demoted) == 3,
          "with the returned index pointing at that sibling's new child");
    check(f.notifications.oneMove(), "one announced move, no data change");

    f.notifications.reset();
    const auto promoted = f.model.promote(f.model.indexOf(QStringLiteral("gamma"), beta));
    check(f.model.texts() == kTopLevel, "promoting it again restores the three top-level rows");
    check(f.model.texts(beta) == (QStringList { QStringLiteral("b1") }), "and empties its former parent");
    check(promoted.row() == 2 && !promoted.parent().isValid(), "back at the end of the top level");
    check(f.notifications.oneMove(), "again announced as a move");

    f.notifications.reset();
    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    check(f.model.demote(alpha) == alpha, "a first row has no previous sibling to indent into");
    check(f.model.texts() == kTopLevel && f.notifications.aboutToMove == 0, "and the tree is left alone");
}

void testClearData()
{
    std::fprintf(stderr, "--- clearData() ---\n");

    Fixture f;
    const int releasesBefore = f.model.notesReleased();
    const auto alpha = f.model.indexOf(QStringLiteral("alpha"));
    f.model.setNote(f.model.itemAt(alpha), QStringLiteral("kept aside"));
    f.model.cut(f.model.indexOf(QStringLiteral("gamma")));
    f.notifications.reset();

    f.model.clearData();

    check(f.model.rowCount() == 0, "the tree is empty");
    check(f.model.cutItem() == nullptr, "and the row the slot held with it");
    check(liveItems == 0, "every row, including the cut one and both subtrees, was deleted exactly once");
    check(f.notifications.aboutToRemove == 1 && f.notifications.removed == 1,
          "the rows left as one announced remove, not silently");
    check(f.model.notesReleased() == releasesBefore + 1, "the subclass hook ran once, after the items were gone");
    check(!f.model.indexOf(QStringLiteral("alpha")).isValid(), "nothing is left to find");

    check(f.model.insertRows(0, 1) && f.model.rowCount() == 1, "an emptied model takes rows again");
}

void testItemOwnership()
{
    std::fprintf(stderr, "--- QcwTreeItem ownership ---\n");

    static_assert(!std::is_copy_constructible_v<QcwTreeItem> && !std::is_move_constructible_v<QcwTreeItem>,
                  "an item owns its children, so copying one would delete them twice");

    Fixture f;
    auto* root = f.model.rootItem();
    const int itemsBefore = liveItems;

    check(root->childAt(3) == nullptr && root->childAt(-1) == nullptr,
          "childAt() answers null for a row outside the children");
    check(root->rowOfChild(root) == -1, "rowOfChild() answers -1 for an item that is not a child");
    check(root->hasChildren() && root->children().size() == root->childCount(),
          "children() copies the row order without taking ownership");

    auto* taken = root->takeChild(0);
    check(taken != nullptr && static_cast<TestItem*>(taken)->text == QStringLiteral("alpha"),
          "takeChild() gives the caller the row");
    check(taken->parent() == nullptr, "and leaves it unparented");
    check(root->childCount() == 2, "the parent no longer holds it");
    check(liveItems == itemsBefore, "being taken out of the tree deletes nothing");

    delete taken;
    check(liveItems == itemsBefore - 4, "deleting it took its whole subtree with it");

    auto* orphan = new TestItem;
    orphan->text = QStringLiteral("orphan");
    root->addChild(orphan);
    check(root->childCount() == 3 && orphan->parent() == root, "addChild() adopts the row and parents it");
    delete orphan;
    check(root->childCount() == 2 && root->childAt(2) == nullptr,
          "deleting an attached row takes it out of its parent's list, leaving no dangling pointer");

    auto* sibling = new TestItem;
    root->insertChild(0, sibling);
    check(f.model.texts() == (QStringList { QStringLiteral(""), QStringLiteral("beta"),
                                           QStringLiteral("gamma") }),
          "insertChild() puts the row at the asked position");
    check(f.model.rowCount() == 3 && !f.model.index(0, NameColumn).parent().isValid(),
          "a row added behind the model's back is still mapped, though no view was told");
    delete sibling;
    check(root->childCount() == 2, "and it detaches itself again when it is deleted");
    check(liveItems == itemsBefore - 4, "each of them deleted once");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    const int itemsAtStart = liveItems;

    testRootMappingAndHeaders();
    testRangeChecksChangeNothing();
    testInsertedRowsKeepCreationOrder();
    testMoveUpAndMoveDown();
    testSelectionAndPersistentIndexesFollowTheRow();
    testCutAndPaste();
    testStaleIndexesNameNoRow();
    testCutCarriesAndReplacesItsSubtree();
    testCutRowIsOwnedOnceOnDestruction();
    testPromote();
    testPromoteAndDemoteInTheTopLevel();
    testClearData();
    testItemOwnership();

    if (liveItems != itemsAtStart)
    {
        std::fprintf(stderr, "FAIL: %d row(s) outlived the models that owned them\n", liveItems - itemsAtStart);
        ++failures;
    }

    if (failures == 0)
    {
        std::printf("tree_model_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "tree_model_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
