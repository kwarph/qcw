// Drives the repair paths of QcwTabFolder::restoreState(): a foreign root element
// is refused, unknown child elements are skipped while the known attributes still
// apply, a pane whose saved id changed is migrated, a tab whose pane is gone is
// dropped, and a damaged recentIndexes list is repaired so the MRU fallback index
// stays valid.

#include "QcwDockingTypes.h"
#include "QcwSerializeUtils.h"
#include "QcwTabFolder.h"

#include <QApplication>
#include <QMap>
#include <QString>
#include <QWidget>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <cstdio>
#include <utility>

namespace
{

int failures = 0;

void check(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

struct SavedTab
{
    QString text;
    int paneId { -1 };
    QString toolTip;
    bool closable { true };
    bool detachable { true };
};

QByteArray hexOf(const QByteArray& bytes)
{
    return bytes.toHex(' ');
}

QByteArray folderDocument(const QList<SavedTab>& tabs,
                          int currentIndex,
                          qcw::FolderFeatures features,
                          const QList<int>& recentIndexes,
                          const QString& unknownElement = {})
{
    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    writer.writeStartElement("folder");
    writer.writeAttribute("id", "17");
    writer.writeAttribute("currentIndex", QString::number(currentIndex));
    writer.writeAttribute("features", QString::number(features.toInt()));
    writer.writeAttribute("tabPosition", QString::number(std::to_underlying(qcw::TabPosition::Bottom)));
    writer.writeAttribute("floating", "0");
    writer.writeAttribute("iconSize", "24");
    writer.writeAttribute("minTextRectWidth", "40");
    writer.writeAttribute("state", QString::number(std::to_underlying(qcw::FolderState::Maximized)));
    writer.writeAttribute("recentIndexes",
                          QString::fromLatin1(hexOf(QcwSerializeUtils::serialize(recentIndexes))));

    if (!unknownElement.isEmpty())
    {
        writer.writeStartElement(unknownElement);
        writer.writeAttribute("note", "written by a newer version");
        writer.writeEndElement();
    }

    writer.writeStartElement("tabs");
    writer.writeAttribute("count", QString::number(tabs.size()));
    for (const auto& tab : std::as_const(tabs))
    {
        QMap<QString, QVariant> data;
        data.insert(qcw::TabPaneIdPropertyName, tab.paneId);

        writer.writeStartElement("tab");
        writer.writeAttribute("text", tab.text);
        writer.writeAttribute("toolTip", tab.toolTip);
        writer.writeAttribute("data", QString::fromLatin1(hexOf(QcwSerializeUtils::serialize(data))));
        writer.writeAttribute("closable", QString::number(tab.closable));
        writer.writeAttribute("detachable", QString::number(tab.detachable));
        writer.writeEndElement();
    }
    writer.writeEndElement();

    writer.writeEndElement();
    return xml;
}

bool restoreFolder(QcwTabFolder& folder, const QByteArray& xml, const QMap<int, QPair<QWidget*, int>>& panes)
{
    QXmlStreamReader reader(xml);
    if (!reader.readNextStartElement() || reader.name() != "folder")
    {
        return false;
    }
    return folder.restoreState(reader, panes);
}

void testWrongRootElement()
{
    std::fprintf(stderr, "--- foreign root element ---\n");

    QcwTabFolder folder;
    QXmlStreamReader reader(QByteArray("<other/>"));
    check(reader.readNextStartElement(), "the foreign document is readable");
    check(!folder.restoreState(reader, {}), "a root element other than folder is refused");
    check(folder.count() == 0, "the refused document adds no tabs");
}

void testUnknownElementsSkipped()
{
    std::fprintf(stderr, "--- unknown elements are skipped ---\n");

    QcwTabFolder folder;
    auto* pane = new QWidget;
    QMap<int, QPair<QWidget*, int>> panes { { 1, { pane, 1 } } };

    const auto xml = folderDocument({ SavedTab { "Alpha", 1 } }, 0, qcw::MultiTabs | qcw::ShowIcon, { 0 },
                                    QStringLiteral("futureExtension"));
    check(restoreFolder(folder, xml, panes), "a document with an unknown child still restores");

    check(folder.count() == 1, "the known tab survives next to the unknown element");
    check(folder.tabText(0) == "Alpha", "the tab text is restored");
    check(folder.features() == (qcw::MultiTabs | qcw::ShowIcon), "the features attribute is applied");
    check(folder.tabPosition() == qcw::TabPosition::Bottom, "the tab position attribute is applied");
    check(folder.iconSize() == 24, "the icon size attribute is applied");
    check(folder.minimumTextRectWidth() == 40, "the minimum text width attribute is applied");
    check(folder.state() == qcw::FolderState::Maximized, "the state attribute is applied");
    check(folder.currentIndex() == 0, "the saved current index is applied");
}

void testPaneIdMigration()
{
    std::fprintf(stderr, "--- pane id migration ---\n");

    QcwTabFolder folder;
    auto* pane = new QWidget;
    QMap<int, QPair<QWidget*, int>> panes { { 7, { pane, 42 } } };

    const auto xml = folderDocument({ SavedTab { "Moved", 7, QStringLiteral("tip"), false, true } }, 0,
                                    qcw::MultiTabs, { 0 });
    check(restoreFolder(folder, xml, panes), "the document restores");
    check(folder.count() == 1, "the mapped tab comes back");

    check(pane->property(qcw::TabPaneIdPropertyName).toInt() == 42, "the pane carries the current id");
    check(folder.tabData(0, qcw::TabPaneIdPropertyName).toInt() == 42,
          "the tab data records the current id instead of the saved one");
    check(folder.tabToolTip(0) == "tip", "the tool tip is restored");
    check(!folder.isTabClosable(0) && folder.isTabDetachable(0), "the closable and detachable flags are restored");
}

void testMissingPaneDropped()
{
    std::fprintf(stderr, "--- missing pane drops its tab ---\n");

    QcwTabFolder folder;
    auto* pane = new QWidget;
    QMap<int, QPair<QWidget*, int>> panes { { 1, { pane, 1 } } };

    const auto xml = folderDocument({ SavedTab { "Kept", 1 }, SavedTab { "Dropped", 2 } }, 1, qcw::MultiTabs,
                                    { 0, 1 });
    check(restoreFolder(folder, xml, panes), "the document restores with one pane missing");

    check(folder.count() == 1, "the tab of the dropped pane is gone");
    check(folder.tabText(0) == "Kept", "the other tab survives");
    check(folder.currentIndex() == 0, "the fallback index replaces the saved one");
}

void testRecentIndexesRepair()
{
    std::fprintf(stderr, "--- recent indexes repair ---\n");

    {
        QcwTabFolder folder;
        auto* first = new QWidget;
        auto* second = new QWidget;
        QMap<int, QPair<QWidget*, int>> panes { { 1, { first, 1 } }, { 2, { second, 2 } } };

        const auto xml = folderDocument({ SavedTab { "A", 1 }, SavedTab { "B", 2 } }, -1, qcw::MultiTabs,
                                        { 1, 7, 1 });
        check(restoreFolder(folder, xml, panes), "the document with a damaged MRU list restores");
        check(folder.count() == 2, "both tabs come back");

        // The repaired list is [1, 0]: the duplicate and the out-of-range entry
        // are dropped, the uncovered index is appended, and the missing current
        // index falls back to the last MRU entry.
        check(folder.currentIndex() == 0, "the MRU fallback picks the last surviving entry");
    }

    {
        QcwTabFolder folder;
        auto* first = new QWidget;
        auto* second = new QWidget;
        QMap<int, QPair<QWidget*, int>> panes { { 1, { first, 1 } }, { 2, { second, 2 } } };

        const auto xml = folderDocument({ SavedTab { "A", 1 }, SavedTab { "B", 2 } }, 1, qcw::MultiTabs,
                                        { 1, 7, 1 });
        check(restoreFolder(folder, xml, panes), "the same document with a valid current index restores");
        check(folder.currentIndex() == 1, "a valid saved current index wins over the MRU fallback");
    }
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    testWrongRootElement();
    testUnknownElementsSkipped();
    testPaneIdMigration();
    testMissingPaneDropped();
    testRecentIndexesRepair();

    if (failures == 0)
    {
        std::printf("folder_restore_state_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "folder_restore_state_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
