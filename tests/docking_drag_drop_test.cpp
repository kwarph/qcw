// Drives the drag-and-drop paths of QcwTabFolder and QcwTabFolderContainer.
//
// The handlers consult QDropEvent::source(), which answers from the live drag
// manager rather than from the event itself. Hand-built events are therefore
// delivered while a real QDrag is running, so they see the same source a real
// drop would; a timer ends that drag with Escape once the events are through.
//
// Covered here: tab reordering inside one folder (including drops on the empty
// strip and on the far half of a distant tab), adoption by another folder at
// its end and mid-list, tabs marked non-detachable and the feature that refuses
// foreign tabs, the container's same-splitter shortcut for a single-tab folder
// (including the splitter orientation guard that keeps a below-drop from
// replacing its reference's column), container-edge drops against the root
// splitter (a no-op at the edge a single-tab folder already occupies, a
// reorder for one in the middle, a new root splitter for a perpendicular
// edge), and the drag overlay.

#include "QcwDockingTypes.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"

#include <QApplication>
#include <QDataStream>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QSplitter>
#include <QTimer>
#include <QWidget>
#include <cstdio>
#include <functional>

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

/// Tab geometry is private; indexAt() is not, so the bar is scanned pixel by pixel.
QRect tabRect(QcwTabFolder* folder, int index)
{
    QRect result;
    for (int y = 0; y < folder->height(); ++y)
    {
        for (int x = 0; x < folder->width(); ++x)
        {
            if (folder->indexAt(QPoint(x, y)) == index)
            {
                result = result.united(QRect(x, y, 1, 1));
            }
        }
    }
    return result;
}

void pressOnTab(QcwTabFolder* folder, int index)
{
    const QRect rect = tabRect(folder, index);
    check(!rect.isNull(), "tab rect is discoverable");
    const QPoint pos = rect.center();
    QMouseEvent press(QEvent::MouseButtonPress, pos, folder->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(folder, &press);
}

struct DragMime
{
    QMimeData data;

    explicit DragMime(int index)
    {
        QByteArray bytes;
        QDataStream stream(&bytes, QIODevice::WriteOnly);
        stream << index;
        data.setData(qcw::TabMimeType, bytes);
    }

    explicit DragMime(const QString& format, const QByteArray& bytes)
    {
        data.setData(format, bytes);
    }
};

bool sendDragEnter(QWidget* target, const QPoint& pos, const DragMime& mime)
{
    QDragEnterEvent event(QPointF(pos), Qt::MoveAction | Qt::CopyAction, &mime.data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
    return event.isAccepted();
}

bool sendDragMove(QWidget* target, const QPoint& pos, const DragMime& mime)
{
    QDragMoveEvent event(QPointF(pos), Qt::MoveAction | Qt::CopyAction, &mime.data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
    return event.isAccepted();
}

struct DropResult
{
    bool accepted { false };
    Qt::DropAction action { Qt::IgnoreAction };
};

DropResult sendDrop(QWidget* target, const QPoint& pos, const DragMime& mime)
{
    QDropEvent event(QPointF(pos), Qt::MoveAction | Qt::CopyAction, &mime.data, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
    return { event.isAccepted(), event.dropAction() };
}

void sendDragLeave(QWidget* target)
{
    QDragLeaveEvent event;
    QApplication::sendEvent(target, &event);
}

bool orderIs(QcwTabFolder* folder, std::initializer_list<const char*> texts)
{
    if (folder->count() != static_cast<int>(texts.size()))
    {
        return false;
    }

    int index = 0;
    for (auto* text : texts)
    {
        if (folder->tabText(index++) != QString::fromLatin1(text))
        {
            return false;
        }
    }
    return true;
}

/// Runs a live drag from @p source while @p during delivers hand-built events.
void withDragFrom(QObject* source, int index, const std::function<void()>& during)
{
    auto* mime = new QMimeData;
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << index;
    mime->setData(qcw::TabMimeType, bytes);

    // Heap-allocated without a parent: the drag manager schedules a QDrag for
    // deletion once exec() returns, and a stack object would be freed twice.
    auto* drag = new QDrag(source);
    drag->setMimeData(mime);

    QTimer::singleShot(
        0, qApp,
        [during]
        {
            during();
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(qApp, &escape);
        });

    drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
    delete drag; // removes the manager's posted DeferredDelete with it
}

struct DragProbe
{
    bool enter { false };
    bool move { false };
    DropResult drop;
};

/// Presses a tab and drives one complete drag onto @p target at @p pos.
DragProbe dragTab(QcwTabFolder* source, int index, QWidget* target, const QPoint& pos)
{
    pressOnTab(source, index);

    DragProbe probe;
    withDragFrom(source, index,
                 [&]
                 {
                     DragMime mime(index);
                     probe.enter = sendDragEnter(target, pos, mime);
                     probe.move = sendDragMove(target, pos, mime);
                     probe.drop = sendDrop(target, pos, mime);
                 });
    return probe;
}

QWidget* overlayOf(QcwTabFolderContainer* container)
{
    for (auto* child : container->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))
    {
        if (child != container->rootSplitter())
        {
            return child;
        }
    }
    return nullptr;
}

QPoint folderPointIn(QWidget* ancestor, QcwTabFolder* folder, const QPoint& local)
{
    return folder->mapTo(ancestor, local);
}

void testSelfReorder()
{
    std::fprintf(stderr, "--- self reorder ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.addTab(new QWidget, "Charlie");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    pressOnTab(&folder, 0);
    const QRect targetRect = tabRect(&folder, 2);
    const QPoint pos = targetRect.center();

    bool enterOk = false;
    bool moveOk = false;
    DropResult drop;

    withDragFrom(&folder, 0,
                 [&]
                 {
                     DragMime mime(0);
                     enterOk = sendDragEnter(&folder, pos, mime);
                     moveOk = sendDragMove(&folder, pos, mime);
                     drop = sendDrop(&folder, pos, mime);
                 });

    check(enterOk, "self drag enter is accepted");
    check(moveOk, "self drag move is accepted");
    check(drop.accepted, "self drop is accepted");
    check(drop.action == Qt::MoveAction, "self drop reports MoveAction");
    check(folder.count() == 3, "self reorder keeps all tabs");
    check(folder.tabText(0) == "Bravo" && folder.tabText(1) == "Charlie" && folder.tabText(2) == "Alpha",
          "the dragged tab moves to the drop position and the others shift left");
    check(folder.currentIndex() == 2, "the dropped tab becomes current");
}

void testReorderAdjustments()
{
    std::fprintf(stderr, "--- reorder adjustments ---\n");

    auto makeFolder = []
    {
        auto* folder = new QcwTabFolder;
        folder->resize(700, 300);
        folder->addTab(new QWidget, "Alpha");
        folder->addTab(new QWidget, "Bravo");
        folder->addTab(new QWidget, "Charlie");
        folder->setCurrentIndex(0);
        folder->show();
        QApplication::processEvents();
        return folder;
    };

    // The strip beside the last tab belongs to the bar but to no tab: dropping
    // there moves the tab to the end.
    {
        auto* folder = makeFolder();
        const QRect last = tabRect(folder, 2);
        const QPoint pos(last.right() + 20, last.center().y());
        DragProbe probe = dragTab(folder, 0, folder, pos);

        check(probe.enter && probe.move && probe.drop.accepted, "a strip drop is accepted");
        check(probe.drop.action == Qt::MoveAction, "a strip drop is a move");
        check(orderIs(folder, { "Bravo", "Charlie", "Alpha" }), "a strip drop moves the tab to the end");
        check(folder->currentIndex() == 2, "the stripped tab becomes current");
        delete folder;
    }

    // Dragging down one slot and up one slot: the neighbour itself is the target.
    {
        auto* folder = makeFolder();
        DragProbe probe = dragTab(folder, 0, folder, tabRect(folder, 1).center());

        check(probe.drop.accepted, "the one-slot-down drop is accepted");
        check(orderIs(folder, { "Bravo", "Alpha", "Charlie" }), "dragging down one slot swaps the pair");
        check(folder->currentIndex() == 1, "the moved tab becomes current");
        delete folder;
    }
    {
        auto* folder = makeFolder();
        DragProbe probe = dragTab(folder, 2, folder, tabRect(folder, 1).center());

        check(probe.drop.accepted, "the one-slot-up drop is accepted");
        check(orderIs(folder, { "Alpha", "Charlie", "Bravo" }), "dragging up one slot swaps the pair");
        check(folder->currentIndex() == 1, "the moved tab becomes current");
        delete folder;
    }

    // A distant tab answers by which half received the drop: the leading half
    // lands the dragged tab in front of it, the trailing half behind it.
    {
        auto* folder = makeFolder();
        const QRect third = tabRect(folder, 2);
        const QPoint pos(third.center().x() - 2, third.center().y());
        DragProbe probe = dragTab(folder, 0, folder, pos);

        check(probe.drop.accepted, "the leading-half drop is accepted");
        check(orderIs(folder, { "Bravo", "Alpha", "Charlie" }), "the leading half lands the tab before the target");
        delete folder;
    }
    {
        auto* folder = makeFolder();
        const QRect first = tabRect(folder, 0);
        const QPoint pos(first.center().x() + 2, first.center().y());
        DragProbe probe = dragTab(folder, 2, folder, pos);

        check(probe.drop.accepted, "the trailing-half drop is accepted");
        check(orderIs(folder, { "Alpha", "Charlie", "Bravo" }), "the trailing half lands the tab after the target");
        delete folder;
    }
}

void testCrossFolderAdopt()
{
    std::fprintf(stderr, "--- cross-folder adopt ---\n");

    QcwTabFolder source;
    source.resize(500, 250);
    auto* movedPane = new QWidget;
    source.addTab(movedPane, "Source");
    source.addTab(new QWidget, "Kept");
    source.show();

    QcwTabFolder target;
    target.resize(500, 250);
    target.addTab(new QWidget, "Target");
    target.show();
    QApplication::processEvents();

    pressOnTab(&source, 0);
    const QPoint pos = tabRect(&target, 0).center();

    bool moveOk = false;
    DropResult drop;
    withDragFrom(&source, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(&target, pos, mime);
                     moveOk = sendDragMove(&target, pos, mime);
                     drop = sendDrop(&target, pos, mime);
                 });

    check(moveOk, "a folder with AcceptDroppedTabs accepts the move");
    check(drop.accepted, "the foreign drop is accepted");
    check(target.count() == 2, "the target gained the tab");
    check(source.count() == 1, "the source lost the tab");
    check(target.tabText(0) == "Target" && target.tabText(1) == "Source", "the adopted tab lands after the last tab");
    check(target.tabPane(1) == movedPane, "the pane travels with the tab");
    check(movedPane->parentWidget() == &target, "the pane is reparented to the target");

    // Dropping on the trailing half of a middle tab lands between it and its successor.
    QcwTabFolder midTarget;
    midTarget.resize(600, 250);
    midTarget.addTab(new QWidget, "T0");
    midTarget.addTab(new QWidget, "T1");
    midTarget.addTab(new QWidget, "T2");
    midTarget.show();

    QcwTabFolder midSource;
    midSource.resize(500, 250);
    midSource.addTab(new QWidget, "Mid");
    midSource.addTab(new QWidget, "Holder");
    midSource.show();
    QApplication::processEvents();

    pressOnTab(&midSource, 0);
    const QRect middleTab = tabRect(&midTarget, 1);
    const QPoint midPos(middleTab.center().x() + 2, middleTab.center().y());

    DropResult midDrop;
    withDragFrom(&midSource, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(&midTarget, midPos, mime);
                     check(sendDragMove(&midTarget, midPos, mime), "the mid-list move is accepted");
                     midDrop = sendDrop(&midTarget, midPos, mime);
                 });

    check(midDrop.accepted, "the mid-list drop is accepted");
    check(orderIs(&midTarget, { "T0", "T1", "Mid", "T2" }),
          "the adopted tab lands after the tab it was dropped on");

    // A tab marked non-detachable may only reorder inside its folder; the folder
    // itself already refuses to adopt it, so this guards the same rule at the target.
    QcwTabFolder locked;
    locked.resize(500, 250);
    auto* lockedPane = new QWidget;
    locked.addTab(lockedPane, "Locked");
    locked.addTab(new QWidget, "Movable");
    locked.setTabDetachable(0, false);
    locked.show();
    QApplication::processEvents();

    pressOnTab(&locked, 0);
    const QPoint lockedPos = tabRect(&midTarget, 0).center();

    bool lockedEnterOk = false;
    bool lockedMoveOk = true;
    DropResult lockedDrop;
    withDragFrom(&locked, 0,
                 [&]
                 {
                     DragMime mime(0);
                     lockedEnterOk = sendDragEnter(&midTarget, lockedPos, mime);
                     lockedMoveOk = sendDragMove(&midTarget, lockedPos, mime);
                     lockedDrop = sendDrop(&midTarget, lockedPos, mime);
                 });

    check(lockedEnterOk, "the target folder sees the enter for a locked tab");
    check(!lockedMoveOk, "the target folder refuses to move a tab that is not detachable");
    check(!lockedDrop.accepted, "the target folder refuses to drop a tab that is not detachable");
    check(locked.count() == 2 && locked.tabText(0) == "Locked", "the locked tab stays in its folder");
    check(midTarget.count() == 4, "the refused drop leaves the target untouched");

    // With the feature off the folder refuses the move; the source keeps its tab.
    QcwTabFolder closed;
    closed.resize(500, 250);
    closed.addTab(new QWidget, "Closed");
    closed.setFeature(qcw::AcceptDroppedTabs, false);
    closed.show();
    QApplication::processEvents();

    pressOnTab(&source, 0);
    const QPoint closedPos = tabRect(&closed, 0).center();
    bool closedEnterOk = true;
    bool closedMoveOk = true;
    withDragFrom(&source, 0,
                 [&]
                 {
                     DragMime mime(0);
                     closedEnterOk = sendDragEnter(&closed, closedPos, mime);
                     closedMoveOk = sendDragMove(&closed, closedPos, mime);
                 });

    check(!closedEnterOk, "a folder without AcceptDroppedTabs ignores the enter");
    check(!closedMoveOk, "a folder without AcceptDroppedTabs ignores the move");
    check(closed.count() == 1 && source.count() == 1, "the refused move leaves both folders untouched");
}

QcwTabFolderContainer* makeContainer(QWidget* host)
{
    auto* layout = new QHBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* container = new QcwTabFolderContainer(host);
    layout->addWidget(container);
    return container;
}

void testSameSplitterShortcut()
{
    std::fprintf(stderr, "--- same-splitter shortcut ---\n");

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    QApplication::processEvents();

    auto* left = container->createFolder(nullptr, qcw::LeftDockArea);
    auto* middle = container->createFolder(left, qcw::RightDockArea);
    auto* right = container->createFolder(middle, qcw::RightDockArea);
    auto* farRight = container->createFolder(right, qcw::RightDockArea);
    check(left && middle && right && farRight, "four folders are created");
    left->addTab(new QWidget, "LeftTab");
    middle->addTab(new QWidget, "MiddleTab");
    right->addTab(new QWidget, "RightTab");
    farRight->addTab(new QWidget, "FarRightTab");
    QApplication::processEvents();

    QSplitter* root = container->rootSplitter();
    check(root->count() == 4, "the root splitter holds four sites");
    check(root->orientation() == Qt::Horizontal, "the root splitter is horizontal");

    QWidget* overlay = overlayOf(container);
    check(overlay != nullptr, "the container has a dock overlay child");

    // The single-tab folder on the right is dropped on the middle folder's left
    // edge: the folder itself moves in front of the reference, no tab leaves it.
    pressOnTab(right, 0);
    const QPoint frontPos = folderPointIn(container, middle, QPoint(5, middle->height() / 2));

    bool moveOk = false;
    DropResult drop;
    withDragFrom(right, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(container, frontPos, mime);
                     moveOk = sendDragMove(container, frontPos, mime);
                     if (overlay)
                     {
                         check(overlay->isVisible(), "the overlay is shown while docking");
                     }
                     drop = sendDrop(container, frontPos, mime);
                 });

    check(moveOk, "the container accepts the folder move");
    check(drop.accepted, "the shortcut drop is accepted");
    check(overlay && !overlay->isVisible(), "the overlay hides after the drop");
    check(container->folders().size() == 4, "the shortcut creates no folder");
    check(right->count() == 1, "the moved folder keeps its tab");
    check(root->widget(0) == left->parentWidget() && root->widget(1) == right->parentWidget() &&
              root->widget(2) == middle->parentWidget() && root->widget(3) == farRight->parentWidget(),
          "the folder lands in front of the reference");
    check(root->sizes().size() == 4, "the splitter keeps one size per site");
    QApplication::processEvents();

    // The other direction: the left folder dropped on the middle folder's right
    // edge lands right after it.
    pressOnTab(left, 0);
    const QPoint backPos = folderPointIn(container, middle, QPoint(middle->width() - 5, middle->height() / 2));

    DropResult backDrop;
    withDragFrom(left, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(container, backPos, mime);
                     check(sendDragMove(container, backPos, mime), "the container accepts the back drop");
                     backDrop = sendDrop(container, backPos, mime);
                 });

    check(backDrop.accepted, "the back drop is accepted");
    check(root->widget(0) == right->parentWidget() && root->widget(1) == middle->parentWidget() &&
              root->widget(2) == left->parentWidget() && root->widget(3) == farRight->parentWidget(),
          "the folder lands behind the reference");
    QApplication::processEvents();

    // Cancelling a move hides the overlay again.
    pressOnTab(left, 0);
    withDragFrom(left, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(container, backPos, mime);
                     sendDragMove(container, backPos, mime);
                     if (overlay)
                     {
                         check(overlay->isVisible(), "the overlay is visible before the leave");
                     }
                     sendDragLeave(container);
                     if (overlay)
                     {
                         check(!overlay->isVisible(), "the overlay hides on drag leave");
                     }
                 });
}

void testOrientationGuard()
{
    std::fprintf(stderr, "--- splitter orientation guard ---\n");

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    container->rootSplitter()->setOrientation(Qt::Vertical);
    host.show();
    QApplication::processEvents();

    auto* top = container->createFolder(nullptr, qcw::TopDockArea);
    auto* bottom = container->createFolder(top, qcw::BottomDockArea);
    auto* reference = top;
    auto* source = container->createFolder(reference, qcw::RightDockArea);
    check(top && bottom && reference && source, "the nested layout is built");
    reference->addTab(new QWidget, "ReferenceTab");
    source->addTab(new QWidget, "MovingTab");
    QApplication::processEvents();

    QSplitter* refSplitter = container->parentSplitter(reference->parentWidget());
    check(refSplitter && refSplitter->orientation() == Qt::Horizontal, "the reference sits in a horizontal splitter");
    check(container->parentSplitter(source->parentWidget()) == refSplitter, "the source shares that splitter");

    QPointer<QcwTabFolder> sourceGuard = source;
    pressOnTab(source, 0);
    const QPoint belowPos = folderPointIn(container, reference, QPoint(reference->width() / 2, reference->height() - 5));

    bool moveOk = false;
    DropResult drop;
    withDragFrom(source, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(container, belowPos, mime);
                     moveOk = sendDragMove(container, belowPos, mime);
                     drop = sendDrop(container, belowPos, mime);
                 });

    check(moveOk, "the container accepts the below drop");
    check(drop.accepted, "the below drop is accepted");

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    QcwTabFolder* created = nullptr;
    for (auto* folder : container->folders())
    {
        if (folder != reference && folder != bottom)
        {
            created = folder;
        }
    }

    check(created != nullptr, "a new folder is created for the tab");
    check(created && created->count() == 1 && created->tabText(0) == "MovingTab", "the tab moved into the new folder");
    check(sourceGuard.isNull() || source->count() == 0, "the emptied source folder is gone");

    if (created)
    {
        QSplitter* pairSplitter = container->parentSplitter(created->parentWidget());
        check(pairSplitter && pairSplitter->orientation() == Qt::Vertical,
              "the new folder stacks below the reference instead of replacing its column");
        check(pairSplitter && pairSplitter->widget(0) == reference->parentWidget(),
              "the reference stays above the new folder");
    }
}

void testContainerEdgeDrop()
{
    std::fprintf(stderr, "--- container edge drop ---\n");

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    QApplication::processEvents();

    auto* existing = container->createFolder(nullptr, qcw::LeftDockArea);
    existing->addTab(new QWidget, "ExistingTab");
    QApplication::processEvents();

    QcwTabFolder standalone;
    standalone.resize(400, 200);
    auto* pane = new QWidget;
    standalone.addTab(pane, "IncomingTab");
    standalone.show();
    QApplication::processEvents();

    pressOnTab(&standalone, 0);
    const QPoint edgePos(5, container->height() / 2);

    bool moveOk = false;
    DropResult drop;
    QPointer<QcwTabFolder> createdGuard;
    QObject::connect(container, &QcwTabFolderContainer::folderCreated,
                     [&createdGuard](QcwTabFolder* folder) { createdGuard = folder; });

    withDragFrom(&standalone, 0,
                 [&]
                 {
                     DragMime mime(0);
                     sendDragEnter(container, edgePos, mime);
                     moveOk = sendDragMove(container, edgePos, mime);
                     drop = sendDrop(container, edgePos, mime);
                 });

    check(moveOk, "the container accepts a tab dragged over its edge");
    check(drop.accepted, "the edge drop is accepted");
    check(createdGuard != nullptr, "the edge drop creates a folder");
    check(createdGuard && createdGuard->count() == 1 && createdGuard->tabText(0) == "IncomingTab",
          "the new folder holds the dragged tab");
    check(pane->parentWidget() == createdGuard, "the pane moved into the new folder");
    check(standalone.count() == 0, "the source folder loses its only tab");
    check(container->rootSplitter()->count() == 2, "the new site joins the root splitter");
    check(createdGuard && container->rootSplitter()->widget(0) == createdGuard->parentWidget(),
          "a left-edge drop puts the new folder first");
}

void testContainerRootEdges()
{
    std::fprintf(stderr, "--- container root edges ---\n");

    // A tab from outside dropped at the container's trailing edge joins a
    // single-site root after the existing site.
    {
        QWidget host;
        host.resize(900, 600);
        auto* container = makeContainer(&host);
        host.show();
        QApplication::processEvents();

        auto* existing = container->createFolder(nullptr, qcw::LeftDockArea);
        existing->addTab(new QWidget, "Existing");
        QApplication::processEvents();

        QcwTabFolder incoming;
        incoming.resize(400, 200);
        incoming.addTab(new QWidget, "Incoming");
        incoming.show();
        QApplication::processEvents();

        QPointer<QcwTabFolder> createdFolder;
        QObject::connect(container, &QcwTabFolderContainer::folderCreated,
                         [&createdFolder](QcwTabFolder* folder) { createdFolder = folder; });

        const QPoint rightEdge(container->width() - 5, container->height() / 2);
        DragProbe probe = dragTab(&incoming, 0, container, rightEdge);

        check(probe.move && probe.drop.accepted, "the trailing edge drop is accepted");
        check(createdFolder && createdFolder->tabText(0) == "Incoming", "the trailing edge drop creates a folder");
        check(container->rootSplitter()->count() == 2 &&
                  container->rootSplitter()->widget(1) == createdFolder->parentWidget(),
              "a trailing edge drop appends the folder to a single-site root");

        // A second tab from outside joins the now multi-site root at its end.
        QcwTabFolder incoming2;
        incoming2.resize(400, 200);
        incoming2.addTab(new QWidget, "Second");
        incoming2.show();
        QApplication::processEvents();

        createdFolder = nullptr;
        DragProbe appended = dragTab(&incoming2, 0, container, rightEdge);

        check(appended.move && appended.drop.accepted, "the second trailing edge drop is accepted");
        check(container->rootSplitter()->count() == 3 &&
                  container->rootSplitter()->widget(2) == createdFolder->parentWidget(),
              "a trailing edge drop appends to a multi-site root");
    }

    // A tab from outside dropped on the leading edge perpendicular to a
    // horizontal root puts itself before the whole layout.
    {
        QWidget topHost;
        topHost.resize(900, 600);
        auto* container = makeContainer(&topHost);
        topHost.show();
        QApplication::processEvents();

        auto* first = container->createFolder(nullptr, qcw::LeftDockArea);
        auto* second = container->createFolder(first, qcw::RightDockArea);
        first->addTab(new QWidget, "First");
        second->addTab(new QWidget, "Second");
        QApplication::processEvents();

        QSplitter* root = container->rootSplitter();
        check(root->count() == 2 && root->orientation() == Qt::Horizontal, "the root is a horizontal pair");

        QcwTabFolder incoming;
        incoming.resize(400, 200);
        incoming.addTab(new QWidget, "TopIncoming");
        incoming.show();
        QApplication::processEvents();

        QPointer<QcwTabFolder> createdFolder;
        QObject::connect(container, &QcwTabFolderContainer::folderCreated,
                         [&createdFolder](QcwTabFolder* folder) { createdFolder = folder; });

        DragProbe probe = dragTab(&incoming, 0, container, QPoint(container->width() / 2, 5));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();

        check(probe.move && probe.drop.accepted, "the leading perpendicular drop is accepted");
        check(container->rootSplitter() != root && container->rootSplitter()->orientation() == Qt::Vertical,
              "the leading perpendicular drop wraps the root vertically");
        check(createdFolder && container->rootSplitter()->widget(0) == createdFolder->parentWidget() &&
                  container->rootSplitter()->widget(1) == root,
              "the new folder lands before the old layout");
    }

    // Inside a multi-site root the two container edges are a no-op for a folder
    // that already sits there, and a reorder for one in the middle.
    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    QApplication::processEvents();

    auto* f0 = container->createFolder(nullptr, qcw::LeftDockArea);
    auto* f1 = container->createFolder(f0, qcw::RightDockArea);
    auto* f2 = container->createFolder(f1, qcw::RightDockArea);
    auto* f3 = container->createFolder(f2, qcw::RightDockArea);
    f0->addTab(new QWidget, "Tab0");
    f1->addTab(new QWidget, "Tab1");
    f2->addTab(new QWidget, "Tab2");
    f3->addTab(new QWidget, "Tab3");
    QApplication::processEvents();

    QSplitter* root = container->rootSplitter();
    check(root->count() == 4, "the root holds four sites");

    const QPoint leftEdge(5, container->height() / 2);
    const QPoint rightEdge(container->width() - 5, container->height() / 2);

    DragProbe leading = dragTab(f0, 0, container, leftEdge);
    check(leading.move && leading.drop.accepted, "the leading folder is accepted at the leading edge");
    check(container->folders().size() == 4 && root->count() == 4 && root->widget(0) == f0->parentWidget(),
          "a folder already at the leading edge stays put");

    DragProbe trailing = dragTab(f3, 0, container, rightEdge);
    check(trailing.move && trailing.drop.accepted, "the trailing folder is accepted at the trailing edge");
    check(container->folders().size() == 4 && root->count() == 4 && root->widget(3) == f3->parentWidget(),
          "a folder already at the trailing edge stays put");

    QPointer<QcwTabFolder> createdFolder;
    QObject::connect(container, &QcwTabFolderContainer::folderCreated,
                     [&createdFolder](QcwTabFolder* folder) { createdFolder = folder; });

    DragProbe middle = dragTab(f1, 0, container, leftEdge);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    check(middle.move && middle.drop.accepted, "the middle folder is accepted at the leading edge");
    check(createdFolder && createdFolder->tabText(0) == "Tab1", "the middle folder's tab moves to a new folder");
    check(container->folders().size() == 4 && root->count() == 4 &&
              root->widget(0) == createdFolder->parentWidget(),
          "the new folder takes the front of the root");

    // Dropping on an edge perpendicular to the root wraps the whole layout in a
    // new splitter instead of squeezing a site in.
    QPointer<QcwTabFolder> sourceGuard = f2;
    createdFolder = nullptr;
    const QPoint bottomEdge(container->width() / 2, container->height() - 5);
    DragProbe below = dragTab(f2, 0, container, bottomEdge);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    check(below.move && below.drop.accepted, "the perpendicular edge drop is accepted");
    check(container->rootSplitter() != root, "a perpendicular edge drop replaces the root splitter");
    check(container->rootSplitter()->orientation() == Qt::Vertical, "the new root splits vertically");
    check(createdFolder && createdFolder->tabText(0) == "Tab2", "the perpendicular drop creates a folder");
    check(container->rootSplitter()->count() == 2 && container->rootSplitter()->widget(0) == root &&
              container->rootSplitter()->widget(1) == createdFolder->parentWidget(),
          "the old layout and the new folder stack in the new root");
    check(sourceGuard.isNull() || sourceGuard->count() == 0, "the emptied source folder is gone");
}

void testCenterDropInVerticalRoot()
{
    std::fprintf(stderr, "--- center drop, vertical root ---\n");

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    QApplication::processEvents();

    auto* top = container->createFolder(nullptr, qcw::TopDockArea);
    auto* bottom = container->createFolder(top, qcw::BottomDockArea);
    top->addTab(new QWidget, "TopTab");
    bottom->addTab(new QWidget, "BottomTab");
    QApplication::processEvents();

    QSplitter* root = container->rootSplitter();
    check(root->orientation() == Qt::Vertical && root->count() == 2, "the root stacks two folders");

    QPointer<QcwTabFolder> topGuard = top;
    const QPoint center = folderPointIn(container, bottom, bottom->rect().center());
    DragProbe probe = dragTab(top, 0, container, center);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();

    check(probe.move && probe.drop.accepted, "the center drop is accepted");
    check(container->folders().size() == 1 && root->count() == 1, "the center drop leaves one folder in the root");
    check(bottom->count() == 2 && bottom->tabText(1) == "TopTab", "the tab joins the folder under the cursor");
    check(topGuard.isNull() || topGuard->count() == 0, "the emptied source folder is gone");
}

void testLockedTabAgainstContainer()
{
    std::fprintf(stderr, "--- locked tab vs container ---\n");

    QWidget host;
    host.resize(900, 600);
    auto* container = makeContainer(&host);
    host.show();
    QApplication::processEvents();

    auto* hostFolder = container->createFolder(nullptr, qcw::LeftDockArea);
    hostFolder->addTab(new QWidget, "HostTab");
    QApplication::processEvents();

    // A first, allowed edge drop leaves the container with a dock target. The
    // next drag may only reach the container's drop handler through that
    // remembered target, since every move with the locked tab is refused.
    QcwTabFolder first;
    first.resize(400, 200);
    first.addTab(new QWidget, "Scratch");
    first.show();
    QApplication::processEvents();

    const QPoint leftEdge(5, container->height() / 2);
    DragProbe scratch = dragTab(&first, 0, container, leftEdge);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QApplication::processEvents();
    check(scratch.move && scratch.drop.accepted, "the first edge drop is accepted");

    const qsizetype foldersBefore = container->folders().size();
    int createdCount = 0;
    QObject::connect(container, &QcwTabFolderContainer::folderCreated,
                     [&createdCount](QcwTabFolder*) { ++createdCount; });

    QcwTabFolder locked;
    locked.resize(400, 200);
    auto* lockedPane = new QWidget;
    locked.addTab(lockedPane, "Locked");
    locked.addTab(new QWidget, "Movable");
    locked.setTabDetachable(0, false);
    locked.show();
    QApplication::processEvents();

    const QPoint rightEdge(container->width() - 5, container->height() / 2);
    DragProbe probe = dragTab(&locked, 0, container, rightEdge);

    check(probe.enter, "the container sees the enter for a locked tab");
    check(!probe.move, "the container refuses to move a tab that is not detachable");
    check(!probe.drop.accepted, "the container refuses to drop a tab that is not detachable");
    check(locked.count() == 2 && locked.tabText(0) == "Locked", "the locked tab stays in its folder");
    check(lockedPane->parentWidget() == &locked, "the locked pane keeps its folder");
    check(container->folders().size() == foldersBefore && createdCount == 0,
          "the refused drop creates no folder");
}

void testRejectsForeignInput()
{
    std::fprintf(stderr, "--- foreign input ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    auto* pane = new QWidget;
    folder.addTab(pane, "Only");
    folder.show();
    QApplication::processEvents();

    const QPoint pos = tabRect(&folder, 0).center();
    DragMime plain("text/plain", "hello");

    check(!sendDragEnter(&folder, pos, plain), "a folder ignores a drag without the tab format");
    check(!sendDragMove(&folder, pos, plain), "a folder ignores a move without the tab format");
    DropResult plainDrop = sendDrop(&folder, pos, plain);
    check(!plainDrop.accepted && folder.count() == 1, "a drop without the tab format changes nothing");

    QWidget host;
    host.resize(600, 400);
    auto* container = makeContainer(&host);
    host.show();
    QApplication::processEvents();
    const QPoint containerPos(5, container->height() / 2);

    check(!sendDragEnter(container, containerPos, plain), "the container ignores a drag without the tab format");
    check(!sendDragMove(container, containerPos, plain), "the container ignores a move without the tab format");
    check(container->folders().isEmpty(), "the container creates nothing for foreign data");

    // A drag from something that is not a folder: the folder treatments accept it
    // through the proposed action, the container refuses to guess.
    QWidget foreignSource;
    foreignSource.resize(100, 100);
    foreignSource.show();
    QApplication::processEvents();

    bool foreignMoveOk = false;
    withDragFrom(&foreignSource, 0,
                 [&]
                 {
                     DragMime mime(0);
                     check(sendDragEnter(&folder, pos, mime), "a folder accepts the enter from a non-folder source");
                     foreignMoveOk = sendDragMove(&folder, pos, mime);
                 });
    check(foreignMoveOk, "a folder accepts a tab-format move from a non-folder source");

    bool containerEnterOk = false;
    bool containerMoveOk = true;
    withDragFrom(&foreignSource, 0,
                 [&]
                 {
                     DragMime mime(0);
                     containerEnterOk = sendDragEnter(container, containerPos, mime);
                     containerMoveOk = sendDragMove(container, containerPos, mime);
                 });
    check(containerEnterOk, "the container accepts the enter");
    check(!containerMoveOk, "the container ignores a non-folder source");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    testSelfReorder();
    testReorderAdjustments();
    testCrossFolderAdopt();
    testSameSplitterShortcut();
    testOrientationGuard();
    testContainerEdgeDrop();
    testContainerRootEdges();
    testCenterDropInVerticalRoot();
    testLockedTabAgainstContainer();
    testRejectsForeignInput();

    if (failures == 0)
    {
        std::printf("docking_drag_drop_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "docking_drag_drop_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
