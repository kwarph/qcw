// Drives QcwTreeProxyModel: the isRowExcluded() gate hides rows from the first mapping,
// before any filter text exists; the search text matches a row, its ancestors (a matching
// node brings its subtree) or its descendants (a match deep down keeps its ancestors
// reachable); an excluded row can neither show itself nor justify showing another row;
// matchesAdditionalConditions() widens the match set beyond the filter role; and
// setFilterText() re-filters, ignores a repeated text and announces every change.

#include "QcwTreeProxyModel.h"

#include <QApplication>
#include <QModelIndex>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <cstdio>

namespace
{

constexpr int DotFileRole = Qt::UserRole;
constexpr int KeywordRole = Qt::UserRole + 1;

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

// The preferences-style pair of hooks: dot rows are excluded whatever the filter says,
// and rows carry searchable keywords outside the display role.
class TestProxyModel : public QcwTreeProxyModel
{
protected:
    bool isRowExcluded(const QModelIndex& sourceIndex) const override
    {
        return sourceIndex.data(DotFileRole).toBool();
    }

    bool matchesAdditionalConditions(const QModelIndex& sourceIndex, const QString& filterText) const override
    {
        return sourceIndex.data(KeywordRole).toString().contains(filterText, filterCaseSensitivity());
    }
};

// The tree the dialog demo used: two top-level rows, two plain children under the first,
// two dot-flagged children under the second, and one keyword hidden on QDir.
class Fixture
{
public:
    Fixture()
    {
        proxy.setSourceModel(&model);
        proxy.setDynamicSortFilter(true);
        proxy.setFilterRole(Qt::DisplayRole);

        auto* widgets = addTopLevel(QStringLiteral("MyDialog"));
        addChild(widgets, QStringLiteral("QLineEdit"));
        addChild(widgets, QStringLiteral("QDir"), QStringLiteral("folder"));

        auto* proxyNode = addTopLevel(QStringLiteral("MyTreeProxyModel"));
        addChild(proxyNode, QStringLiteral("QFile"), {}, true);
        addChild(proxyNode, QStringLiteral("QStandardItemModel"), {}, true);
    }

    /// The display texts of the proxy's children of @p parent, in view order.
    QStringList childTexts(const QModelIndex& parent = {}) const
    {
        QStringList texts;
        for (int row = 0; row < proxy.rowCount(parent); ++row)
        {
            texts << proxy.index(row, 0, parent).data(Qt::DisplayRole).toString();
        }
        return texts;
    }

    /// The proxy index whose text is @p text, or an invalid index while no visible row
    /// carries it.
    QModelIndex indexOf(const QString& text, const QModelIndex& parent = {}) const
    {
        for (int row = 0; row < proxy.rowCount(parent); ++row)
        {
            auto index = proxy.index(row, 0, parent);
            if (index.data(Qt::DisplayRole).toString() == text)
            {
                return index;
            }
        }

        return {};
    }

    QStandardItemModel model;
    TestProxyModel proxy;

private:
    QStandardItem* addTopLevel(const QString& text)
    {
        auto* item = new QStandardItem(text);
        model.appendRow(item);
        return item;
    }

    void addChild(QStandardItem* parentItem, const QString& text,
                  const QString& keyword = {}, bool dotFile = false)
    {
        auto* item = new QStandardItem(text);
        if (!keyword.isEmpty())
        {
            item->setData(keyword, KeywordRole);
        }
        if (dotFile)
        {
            item->setData(true, DotFileRole);
        }
        parentItem->appendRow(item);
    }
};

void testInitialTreeHidesExcludedRows()
{
    std::fprintf(stderr, "--- initial tree hides excluded rows ---\n");

    Fixture f;

    check(f.childTexts() == (QStringList { QStringLiteral("MyDialog"),
                                           QStringLiteral("MyTreeProxyModel") }),
          "both top level rows show without a filter");
    check(!f.indexOf(QStringLiteral("QFile")).isValid()
              && !f.indexOf(QStringLiteral("QStandardItemModel")).isValid(),
          "excluded rows stay hidden before any filter text was ever set");
    check(f.indexOf(QStringLiteral("MyTreeProxyModel")).isValid()
              && f.childTexts(f.indexOf(QStringLiteral("MyTreeProxyModel"))).isEmpty(),
          "the excluded rows leave their parent without children");
}

void testMatchingAncestorKeepsSubtree()
{
    std::fprintf(stderr, "--- matching ancestor keeps its subtree ---\n");

    Fixture f;
    f.proxy.setFilterText(QStringLiteral("My"));

    check(f.childTexts() == (QStringList { QStringLiteral("MyDialog"),
                                           QStringLiteral("MyTreeProxyModel") }),
          "both matching top level rows show");

    const auto widgets = f.indexOf(QStringLiteral("MyDialog"));
    check(f.childTexts(widgets) == (QStringList { QStringLiteral("QLineEdit"),
                                                   QStringLiteral("QDir") }),
          "the children of a matching ancestor are accepted by the ancestor rule");

    check(!f.indexOf(QStringLiteral("QFile")).isValid(),
          "an excluded child of a matching ancestor stays hidden");
}

void testDescendantMatchKeepsAncestorPath()
{
    std::fprintf(stderr, "--- descendant match keeps the ancestor path ---\n");

    Fixture f;
    f.proxy.setFilterText(QStringLiteral("QDir"));

    check(f.childTexts() == (QStringList { QStringLiteral("MyDialog") }),
          "the ancestor of the match is shown and the other top level row is gone");

    const auto widgets = f.indexOf(QStringLiteral("MyDialog"));
    check(f.childTexts(widgets) == (QStringList { QStringLiteral("QDir") }),
          "only the matching child survives next to the ancestor");

    // The same text in another case finds the same rows: matching is case-insensitive.
    f.proxy.setFilterText(QStringLiteral("qdir"));
    check(f.childTexts() == (QStringList { QStringLiteral("MyDialog") }),
          "the filter is case-insensitive by default");
}

void testExcludedRowCannotJustifyAncestor()
{
    std::fprintf(stderr, "--- excluded row cannot justify its ancestor ---\n");

    Fixture f;
    // QFile is the only text match below MyTreeProxyModel, and QFile is excluded.
    f.proxy.setFilterText(QStringLiteral("QFile"));

    check(f.childTexts().isEmpty(),
          "an excluded match does not bring its ancestors back into the tree");
}

void testAdditionalConditionsWidenTheMatch()
{
    std::fprintf(stderr, "--- additional conditions widen the match ---\n");

    Fixture f;
    // Nothing displays the word; only QDir's hidden keyword carries it.
    f.proxy.setFilterText(QStringLiteral("folder"));

    check(f.childTexts() == (QStringList { QStringLiteral("MyDialog") }),
          "a row matched only through matchesAdditionalConditions() keeps its path visible");

    const auto widgets = f.indexOf(QStringLiteral("MyDialog"));
    check(f.childTexts(widgets) == (QStringList { QStringLiteral("QDir") }),
          "the keyword-matched row itself is shown");
}

void testClearFilterRestoresTree()
{
    std::fprintf(stderr, "--- clearing the filter restores the tree ---\n");

    Fixture f;
    f.proxy.setFilterText(QStringLiteral("QDir"));
    f.proxy.setFilterText(QString {});

    check(f.childTexts() == (QStringList { QStringLiteral("MyDialog"),
                                           QStringLiteral("MyTreeProxyModel") }),
          "an empty text shows the whole tree again");
    check(!f.indexOf(QStringLiteral("QFile")).isValid(),
          "but never the excluded rows");
}

void testFilterTextChangedSignal()
{
    std::fprintf(stderr, "--- filterTextChanged contract ---\n");

    Fixture f;
    int emissions = 0;
    QString lastText { QStringLiteral("untouched") };
    QObject::connect(&f.proxy, &QcwTreeProxyModel::filterTextChanged,
                     [&](const QString& text)
                     {
                         ++emissions;
                         lastText = text;
                     });

    f.proxy.setFilterText(QStringLiteral("QDir"));
    check(emissions == 1 && lastText == QStringLiteral("QDir"),
          "a new text is announced with itself");

    f.proxy.setFilterText(QStringLiteral("QDir"));
    check(emissions == 1, "the text that is already set changes nothing");
    check(f.proxy.filterText() == QStringLiteral("QDir"), "the filter text is readable");

    f.proxy.clearFilterText();
    check(emissions == 2 && lastText.isEmpty() && f.proxy.filterText().isEmpty(),
          "clearFilterText() empties the text and announces it");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    testInitialTreeHidesExcludedRows();
    testMatchingAncestorKeepsSubtree();
    testDescendantMatchKeepsAncestorPath();
    testExcludedRowCannotJustifyAncestor();
    testAdditionalConditionsWidenTheMatch();
    testClearFilterRestoresTree();
    testFilterTextChangedSignal();

    if (failures == 0)
    {
        std::printf("tree_proxy_model_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "tree_proxy_model_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
