// Drives the container side of FolderState: maximizing hides every sibling and
// restoring brings them (and the splitter proportions) back, restoring one
// hidden sibling at a time while another folder is maximized, maximizing the
// only folder, and deleting a folder while it is maximized.

#include "QcwDockingTypes.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QPointer>
#include <QSplitter>
#include <QWidget>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <cstdio>

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

void settle()
{
    for (int i = 0; i < 3; ++i)
    {
        QApplication::processEvents();
    }
}

QcwTabFolderContainer* makeContainer(QWidget* host)
{
    auto* layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* container = new QcwTabFolderContainer(host);
    layout->addWidget(container);
    return container;
}

QcwTabFolder* folderById(QcwTabFolderContainer* container, int id)
{
    for (auto* folder : container->folders())
    {
        if (folder->property("FolderId").toInt() == id)
        {
            return folder;
        }
    }
    return nullptr;
}

struct Scene
{
    QWidget host;
    QcwTabFolderContainer* container { nullptr };
    QcwTabFolder* left { nullptr };
    QcwTabFolder* middle { nullptr };
    QcwTabFolder* right { nullptr };
    QSplitter* root { nullptr };

    Scene()
    {
        host.resize(900, 600);
        container = makeContainer(&host);
        host.show();
        settle();

        left = container->createFolder(nullptr, qcw::LeftDockArea);
        middle = container->createFolder(left, qcw::RightDockArea);
        right = container->createFolder(middle, qcw::RightDockArea);
        left->addTab(new QWidget, "LeftTab");
        middle->addTab(new QWidget, "MiddleTab");
        right->addTab(new QWidget, "RightTab");
        settle();

        root = container->rootSplitter();
    }

    bool siteVisible(QcwTabFolder* folder) const
    {
        return folder->parentWidget() && folder->parentWidget()->isVisible();
    }
};

void testMaximizeRestoreSizes()
{
    std::fprintf(stderr, "--- maximize and restore ---\n");

    Scene scene;
    check(scene.left && scene.middle && scene.right, "three folders are created");
    check(scene.root && scene.root->count() == 3, "the root splitter holds the three folders");

    scene.root->setSizes({ 200, 300, 400 });
    settle();
    const auto before = scene.root->sizes();

    scene.middle->maximize();
    settle();

    check(scene.middle->state() == qcw::FolderState::Maximized, "the maximized folder records its state");
    check(scene.left->state() == qcw::FolderState::Minimized && scene.right->state() == qcw::FolderState::Minimized,
          "the siblings record the minimized state");
    check(!scene.siteVisible(scene.left) && !scene.siteVisible(scene.right), "the sibling sites are hidden");
    check(scene.siteVisible(scene.middle), "the maximized site stays visible");

    scene.middle->maximize();
    settle();

    check(scene.middle->state() == qcw::FolderState::Normal && scene.left->state() == qcw::FolderState::Normal &&
              scene.right->state() == qcw::FolderState::Normal,
          "restoring returns every folder to the normal state");
    check(scene.siteVisible(scene.left) && scene.siteVisible(scene.middle) && scene.siteVisible(scene.right),
          "restoring shows every site again");
    check(scene.root->sizes() == before, "restoring brings back the splitter proportions");
}

void testRestoreSiblingsOneByOne()
{
    std::fprintf(stderr, "--- restore siblings while maximized ---\n");

    Scene scene;
    scene.root->setSizes({ 200, 300, 400 });
    settle();
    const auto before = scene.root->sizes();

    scene.middle->maximize();
    settle();

    // A minimized folder's maximize() is its restore request, what the fast view
    // toolbar does for a hidden folder.
    scene.left->maximize();
    settle();

    check(scene.left->state() == qcw::FolderState::Normal && scene.siteVisible(scene.left),
          "restoring a sibling shows it again");
    check(scene.middle->state() == qcw::FolderState::Maximized && scene.siteVisible(scene.middle),
          "the maximized folder stays maximized");
    check(scene.right->state() == qcw::FolderState::Minimized && !scene.siteVisible(scene.right),
          "the other sibling stays hidden");

    scene.right->maximize();
    settle();

    check(scene.left->state() == qcw::FolderState::Normal && scene.middle->state() == qcw::FolderState::Normal &&
              scene.right->state() == qcw::FolderState::Normal,
          "restoring the last hidden sibling ends the maximization");
    check(scene.siteVisible(scene.left) && scene.siteVisible(scene.middle) && scene.siteVisible(scene.right),
          "every site is visible again");
    std::fprintf(stderr, "sizes before=%d,%d,%d after=%d,%d,%d\n", before.at(0), before.at(1), before.at(2),
                 scene.root->sizes().at(0), scene.root->sizes().at(1), scene.root->sizes().at(2));
    check(scene.root->sizes() == before, "the one-by-one restore brings back the proportions");
}

void testMinimizeMaximizedFolder()
{
    std::fprintf(stderr, "--- minimize the maximized folder ---\n");

    Scene scene;
    scene.root->setSizes({ 200, 300, 400 });
    settle();
    const auto before = scene.root->sizes();

    scene.middle->maximize();
    settle();

    scene.middle->minimize();
    settle();

    check(scene.middle->state() == qcw::FolderState::Minimized && !scene.siteVisible(scene.middle),
          "the maximized folder itself is the one that minimizes");
    check(scene.left->state() == qcw::FolderState::Normal && scene.right->state() == qcw::FolderState::Normal,
          "ending the maximization brings its hidden siblings back");
    check(scene.siteVisible(scene.left) && scene.siteVisible(scene.right), "the sibling sites are shown again");

    scene.middle->maximize();
    settle();

    check(scene.middle->state() == qcw::FolderState::Normal && scene.siteVisible(scene.middle),
          "restoring the minimized folder shows it again");
    check(scene.left->state() == qcw::FolderState::Normal && scene.right->state() == qcw::FolderState::Normal &&
              scene.siteVisible(scene.left) && scene.siteVisible(scene.right),
          "the restored layout keeps every folder");
    check(scene.root->sizes() == before, "restoring the minimized folder brings back the proportions");
}

void testSaveRestoreMaximizedState()
{
    std::fprintf(stderr, "--- save and restore a maximized layout ---\n");

    Scene scene;
    scene.root->setSizes({ 200, 300, 400 });
    settle();
    const auto before = scene.root->sizes();

    const int leftId = scene.left->property("FolderId").toInt();
    const int middleId = scene.middle->property("FolderId").toInt();
    const int rightId = scene.right->property("FolderId").toInt();

    scene.middle->maximize();
    settle();

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    scene.container->saveState(writer);

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    settle();

    QXmlStreamReader reader(xml);
    check(reader.readNextStartElement() && reader.name() == "folderContainer", "the document starts with the container");
    check(container->restoreState(reader), "the maximized layout restores");
    settle();

    auto* left = folderById(container, leftId);
    auto* middle = folderById(container, middleId);
    auto* right = folderById(container, rightId);
    check(left && middle && right, "all three folders come back");
    if (!left || !middle || !right)
    {
        return;
    }

    check(middle->state() == qcw::FolderState::Maximized, "the maximized folder comes back maximized");
    check(left->state() == qcw::FolderState::Minimized && right->state() == qcw::FolderState::Minimized,
          "the siblings come back minimized");
    check(!left->parentWidget()->isVisible() && !right->parentWidget()->isVisible(), "the sibling sites stay hidden");
    check(middle->parentWidget()->isVisible(), "the maximized site stays visible");

    const auto restoredRoot = container->rootSplitter();
    middle->maximize();
    settle();

    check(middle->state() == qcw::FolderState::Normal && left->state() == qcw::FolderState::Normal &&
              right->state() == qcw::FolderState::Normal,
          "restoring the maximized folder returns the whole layout to normal");
    check(left->parentWidget()->isVisible() && right->parentWidget()->isVisible(), "the sibling sites are shown");
    std::fprintf(stderr, "sizes before=%d,%d,%d after=%d,%d,%d\n", before.at(0), before.at(1), before.at(2),
                 restoredRoot->sizes().at(0), restoredRoot->sizes().at(1), restoredRoot->sizes().at(2));
    check(restoredRoot->sizes() == before, "the stored proportions come back with the restore");
}

void testSaveRestoreMinimizedState()
{
    std::fprintf(stderr, "--- save and restore a minimized layout ---\n");

    Scene scene;
    scene.root->setSizes({ 200, 300, 400 });
    settle();
    const auto before = scene.root->sizes();

    const int leftId = scene.left->property("FolderId").toInt();
    const int middleId = scene.middle->property("FolderId").toInt();
    const int rightId = scene.right->property("FolderId").toInt();

    scene.right->minimize();
    settle();

    check(scene.right->state() == qcw::FolderState::Minimized && !scene.siteVisible(scene.right),
          "minimizing hides the folder's site");

    QByteArray xml;
    QXmlStreamWriter writer(&xml);
    scene.container->saveState(writer);

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    settle();

    int minimizedReports = 0;
    QObject::connect(container, &QcwTabFolderContainer::folderMinimized,
                     [&minimizedReports](QcwTabFolder*, Qt::ToolBarArea)
    {
        ++minimizedReports;
    });

    QXmlStreamReader reader(xml);
    check(reader.readNextStartElement() && reader.name() == "folderContainer", "the document starts with the container");
    check(container->restoreState(reader), "the minimized layout restores");
    settle();

    auto* left = folderById(container, leftId);
    auto* middle = folderById(container, middleId);
    auto* right = folderById(container, rightId);
    check(left && middle && right, "all three folders come back");
    if (!left || !middle || !right)
    {
        return;
    }

    check(right->state() == qcw::FolderState::Minimized, "the minimized folder comes back minimized");
    check(!right->parentWidget()->isVisible(), "its site stays hidden");
    check(left->state() == qcw::FolderState::Normal && middle->state() == qcw::FolderState::Normal,
          "the other folders come back normal");
    check(left->parentWidget()->isVisible() && middle->parentWidget()->isVisible(), "their sites are visible");
    check(minimizedReports == 1, "restoring reports the minimized folder again");

    right->maximize();
    settle();

    check(right->state() == qcw::FolderState::Normal && right->parentWidget()->isVisible(),
          "restoring the minimized folder shows it again");
    check(left->state() == qcw::FolderState::Normal && middle->state() == qcw::FolderState::Normal &&
              left->parentWidget()->isVisible() && middle->parentWidget()->isVisible(),
          "the other folders are untouched by the restore");
    std::fprintf(stderr, "sizes before=%d,%d,%d after=%d,%d,%d\n", before.at(0), before.at(1), before.at(2),
                 container->rootSplitter()->sizes().at(0), container->rootSplitter()->sizes().at(1),
                 container->rootSplitter()->sizes().at(2));
    check(container->rootSplitter()->sizes() == before, "the proportions stored at minimize time come back");
}

void testMaximizeOnlyFolder()
{
    std::fprintf(stderr, "--- maximize the only folder ---\n");

    QWidget host;
    host.resize(600, 400);
    auto* container = makeContainer(&host);
    host.show();
    settle();

    auto* only = container->createFolder(nullptr, qcw::LeftDockArea);
    only->addTab(new QWidget, "OnlyTab");
    settle();

    only->maximize();
    settle();

    check(only->state() == qcw::FolderState::Normal, "a folder with no siblings has nothing to maximize over");
    check(only->parentWidget() && only->parentWidget()->isVisible(), "the folder stays visible");

    // Restoring it again is harmless.
    only->maximize();
    settle();
    check(only->state() == qcw::FolderState::Normal, "restoring a folder that never maximized changes nothing");
}

void testDeleteMaximizedFolder()
{
    std::fprintf(stderr, "--- delete the maximized folder ---\n");

    Scene scene;
    scene.root->setSizes({ 200, 300, 400 });
    settle();
    const auto before = scene.root->sizes();

    scene.middle->maximize();
    settle();
    check(scene.middle->state() == qcw::FolderState::Maximized, "the folder is maximized");

    QPointer<QcwTabFolder> guard { scene.middle };
    scene.middle->removeTab(0); // the container's default empty-folder action is Delete
    settle();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    settle();

    check(guard.isNull(), "the emptied folder is deleted");
    check(scene.left->state() == qcw::FolderState::Normal && scene.right->state() == qcw::FolderState::Normal,
          "the siblings are restored to the normal state");
    check(scene.siteVisible(scene.left) && scene.siteVisible(scene.right), "the sibling sites are shown again");
    check(!scene.container->folders().contains(scene.middle), "the deleted folder left the container");
    std::fprintf(stderr, "sizes before=%d,%d,%d after=%d,%d\n", before.at(0), before.at(1), before.at(2),
                 scene.root->sizes().value(0), scene.root->sizes().value(1));
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    testMaximizeRestoreSizes();
    testRestoreSiblingsOneByOne();
    testMinimizeMaximizedFolder();
    testSaveRestoreMaximizedState();
    testSaveRestoreMinimizedState();
    testMaximizeOnlyFolder();
    testDeleteMaximizedFolder();

    if (failures == 0)
    {
        std::printf("folder_maximize_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "folder_maximize_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
