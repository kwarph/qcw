// Drives the mouse paths of QcwTabFolder: selection and close on release,
// maximize and the double-click signal, hover bookkeeping, starting a real
// drag from a pressed tab (cancelled at once, so an ignored drag detaches the
// tab into a floating folder one second later), the refusal to drag a
// non-detachable tab out of the bar, and the event filter that promotes a
// click landing deep inside an inactive folder to a tabBarClicked().
//
// The drag is started by mouseMoveEvent itself. The offscreen platform's
// QOffscreenDrag::drag() returns IgnoreAction at once, which is exactly the
// "cancelled drag" branch; a timer ends a real drag on platforms that run a
// nested loop.

#include "QcwDockingTypes.h"
#include "QcwFloatingFolderMonitor.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderRenderer.h"

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>
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

void sendMouse(QcwTabFolder* folder, QEvent::Type type, const QPoint& pos, Qt::MouseButton button,
               Qt::MouseButtons buttons)
{
    QMouseEvent event(type, pos, folder->mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(folder, &event);
}

void clickTab(QcwTabFolder* folder, int index, int xOffsetFromRight = 0)
{
    QRect rect = tabRect(folder, index);
    check(!rect.isNull(), "tab rect is discoverable");
    const QPoint pos(rect.right() - xOffsetFromRight, rect.center().y());

    sendMouse(folder, QEvent::MouseButtonPress, pos, Qt::LeftButton, Qt::LeftButton);
    sendMouse(folder, QEvent::MouseButtonRelease, pos, Qt::LeftButton, Qt::NoButton);
}

/// A child that consumes presses the way a real editor widget does: the press
/// never bubbles up to the pane, so only the folder's application-wide filter
/// sees it.
class PressConsumer : public QWidget
{
public:
    using QWidget::QWidget;

protected:
    void mousePressEvent(QMouseEvent* event) override { event->accept(); }
};

void testSelectionOnRelease()
{
    std::fprintf(stderr, "--- selection on release ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.addTab(new QWidget, "Charlie");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    int clicked = -2;
    int closeRequests = 0;
    QObject::connect(&folder, &QcwTabFolder::tabBarClicked, [&clicked](int index) { clicked = index; });
    QObject::connect(&folder, &QcwTabFolder::tabCloseRequested, [&closeRequests](int) { ++closeRequests; });

    clickTab(&folder, 2);
    check(folder.currentIndex() == 2, "releasing on a tab selects it");
    check(clicked == 2, "a tab click reports its index through tabBarClicked");
    check(closeRequests == 0, "a tab click requests no close");

    // Releasing off the bar clears the press bookkeeping; a click on the empty
    // strip afterwards reports -1 instead of the tab pressed earlier.
    const QRect second = tabRect(&folder, 1);
    sendMouse(&folder, QEvent::MouseButtonPress, second.center(), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseButtonRelease, QPoint(folder.width() / 2, folder.height() - 20), Qt::LeftButton,
              Qt::NoButton);
    clicked = -2;

    const QRect last = tabRect(&folder, 2);
    const QPoint strip(last.right() + 20, last.center().y());
    sendMouse(&folder, QEvent::MouseButtonPress, strip, Qt::LeftButton, Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseButtonRelease, strip, Qt::LeftButton, Qt::NoButton);
    check(clicked == -1, "a stale press is not reused after a release outside the bar");

    // The right button is not a tab click.
    clicked = -2;
    const int before = folder.currentIndex();
    const QRect first = tabRect(&folder, 0);
    sendMouse(&folder, QEvent::MouseButtonPress, first.center(), Qt::RightButton, Qt::RightButton);
    sendMouse(&folder, QEvent::MouseButtonRelease, first.center(), Qt::RightButton, Qt::NoButton);
    check(folder.currentIndex() == before && clicked == -2, "a right click neither selects nor reports a tab click");

    // A press on the folder itself is not a deep-click promotion: in an inactive
    // folder only the release path names a tab, so the press must stay silent
    // instead of promoting the current index first.
    int clicks = 0;
    QObject::connect(&folder, &QcwTabFolder::tabBarClicked, [&clicks](int) { ++clicks; });
    clicked = -2;
    folder.setActive(false);
    const QRect firstRect = tabRect(&folder, 0);
    const QPoint firstPos = firstRect.center();
    sendMouse(&folder, QEvent::MouseButtonPress, firstPos, Qt::LeftButton, Qt::LeftButton);
    check(clicks == 0 && clicked == -2, "a press on the tab bar of an inactive folder promotes nothing");
    sendMouse(&folder, QEvent::MouseButtonRelease, firstPos, Qt::LeftButton, Qt::NoButton);
    check(clicked == 0, "the release still reports the clicked tab");
}

void testCloseButton()
{
    std::fprintf(stderr, "--- close button ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    int closeRequests = 0;
    int lastClosed = -1;
    int clicked = -2;
    QObject::connect(&folder, &QcwTabFolder::tabCloseRequested,
                     [&](int index)
                     {
                         ++closeRequests;
                         lastClosed = index;
                     });
    QObject::connect(&folder, &QcwTabFolder::tabBarClicked, [&clicked](int index) { clicked = index; });

    // The close button is the rightmost 16 px of a tab, followed by one spacing.
    const int closeX = QcwTabFolderRenderer::TabSpacing + QcwTabFolderRenderer::CloseRectSize / 2;
    clickTab(&folder, 1, closeX);

    check(closeRequests == 1 && lastClosed == 1, "clicking the close button requests that tab");
    check(clicked == -2, "the close button is an owned widget, not a tab-bar click");
    check(folder.currentIndex() == 0, "closing is a request only; the tab stays until the owner acts");
}

void testDoubleClick()
{
    std::fprintf(stderr, "--- double click ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    int maximizes = 0;
    int restores = 0;
    int doubleClicks = 0;
    int lastIndex = -2;
    QObject::connect(&folder, &QcwTabFolder::maximizeRequested, [&maximizes]() { ++maximizes; });
    QObject::connect(&folder, &QcwTabFolder::restoreRequested, [&restores]() { ++restores; });
    QObject::connect(&folder, &QcwTabFolder::tabBarDoubleClicked,
                     [&](int index)
                     {
                         ++doubleClicks;
                         lastIndex = index;
                     });

    const QRect second = tabRect(&folder, 1);
    sendMouse(&folder, QEvent::MouseButtonDblClick, second.center(), Qt::LeftButton, Qt::LeftButton);
    check(maximizes == 1, "double-clicking a tab asks to maximize");
    check(doubleClicks == 1 && lastIndex == 1, "double-clicking a tab reports its index");

    const QRect last = tabRect(&folder, 1);
    const QPoint strip(last.right() + 20, last.center().y());
    sendMouse(&folder, QEvent::MouseButtonDblClick, strip, Qt::LeftButton, Qt::LeftButton);
    check(maximizes == 2, "double-clicking the empty strip asks to maximize");
    check(doubleClicks == 2 && lastIndex == -1, "double-clicking the empty strip reports no tab");

    // The close button owns the double-click: neither maximize nor the signal.
    const int closeX = QcwTabFolderRenderer::TabSpacing + QcwTabFolderRenderer::CloseRectSize / 2;
    const QRect third = tabRect(&folder, 0);
    sendMouse(&folder, QEvent::MouseButtonDblClick, QPoint(third.right() - closeX, third.center().y()),
              Qt::LeftButton, Qt::LeftButton);
    check(maximizes == 2 && doubleClicks == 2, "a double-click on the close button does nothing");

    // Outside the tab bar it is not a tab-bar double-click at all.
    sendMouse(&folder, QEvent::MouseButtonDblClick, QPoint(folder.width() / 2, folder.height() - 20), Qt::LeftButton,
              Qt::LeftButton);
    check(maximizes == 2 && doubleClicks == 2, "a double-click outside the bar does nothing");

    // Without Maximizable the signal still goes out, the maximize request does not.
    folder.setFeature(qcw::Maximizable, false);
    sendMouse(&folder, QEvent::MouseButtonDblClick, second.center(), Qt::LeftButton, Qt::LeftButton);
    check(maximizes == 2, "a folder that cannot maximize ignores the double-click for maximizing");
    check(doubleClicks == 3, "the double-click signal still reports the tab");
    check(restores == 0, "the normal folder asks for no restore");
}

void testHoverAndCancelledPress()
{
    std::fprintf(stderr, "--- hover ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    // Hovering with no button down tracks the tab and its close button.
    const QRect second = tabRect(&folder, 1);
    sendMouse(&folder, QEvent::MouseMove, second.center(), Qt::NoButton, Qt::NoButton);
    sendMouse(&folder, QEvent::MouseMove, second.topRight(), Qt::NoButton, Qt::NoButton);
    sendMouse(&folder, QEvent::MouseMove, QPoint(5, folder.height() - 20), Qt::NoButton, Qt::NoButton);

    // A left-button move below the drag distance does not start a drag.
    sendMouse(&folder, QEvent::MouseButtonPress, second.center(), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseMove, second.center() + QPoint(2, 0), Qt::NoButton, Qt::LeftButton);
    check(folder.count() == 2, "a short move leaves the folder as it was");
}

void testDragDetachesTab()
{
    std::fprintf(stderr, "--- cancelled drag detaches ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    auto* movedPane = new QWidget;
    folder.addTab(movedPane, "Bravo");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    QcwTabFolder* floating = nullptr;
    auto connection = QObject::connect(floatingFolderMonitor, &QcwFloatingFolderMonitor::floatingFolderCreated,
                                       [&floating](QcwTabFolder* created) { floating = created; });

    const QRect second = tabRect(&folder, 1);
    const QPoint start = second.center();
    sendMouse(&folder, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);

    // A live drag would run a nested loop; end it if this platform has one.
    QTimer::singleShot(
        0, qApp,
        []
        {
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(qApp, &escape);
        });
    sendMouse(&folder, QEvent::MouseMove, start + QPoint(60, 0), Qt::NoButton, Qt::LeftButton);
    QObject::disconnect(connection);

    check(folder.count() == 1 && folder.tabText(0) == "Alpha", "the cancelled drag leaves the moved tab out of the folder");
    check(floating != nullptr, "the cancelled drag detaches the tab into a floating folder");
    if (floating)
    {
        check(floating->isFloating(), "the detached folder is floating");
        check(floating->count() == 1 && floating->tabText(0) == "Bravo", "the floating folder holds the dragged tab");
        check(floating->tabPane(0) == movedPane && movedPane->parentWidget() == floating,
              "the pane moves into the floating folder");
        check(!floating->features().testFlag(qcw::Maximizable) &&
                  !floating->features().testFlag(qcw::Minimizable),
              "a floating folder cannot minimize or maximize");

        floating->close();
        QApplication::processEvents();
    }
}

void testNonDetachableDrag()
{
    std::fprintf(stderr, "--- non-detachable drag ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    folder.addTab(new QWidget, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.setCurrentIndex(0);
    folder.setTabDetachable(1, false);
    folder.show();
    QApplication::processEvents();

    int floatingCount = 0;
    auto connection = QObject::connect(floatingFolderMonitor, &QcwFloatingFolderMonitor::floatingFolderCreated,
                                       [&floatingCount](QcwTabFolder*) { ++floatingCount; });

    // Moving out of the bar with a non-detachable tab must not even start a drag.
    const QRect second = tabRect(&folder, 1);
    sendMouse(&folder, QEvent::MouseButtonPress, second.center(), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseMove, QPoint(folder.width() / 2, folder.height() - 20), Qt::NoButton,
              Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseButtonRelease, QPoint(folder.width() / 2, folder.height() - 20), Qt::LeftButton,
              Qt::NoButton);

    check(folder.count() == 2, "a non-detachable tab cannot be dragged out of the bar");
    check(floatingCount == 0, "no floating folder is created for it");

    // Inside the bar a drag still starts (it may reorder), but a cancelled drag
    // must not detach the tab either.
    sendMouse(&folder, QEvent::MouseButtonPress, second.center(), Qt::LeftButton, Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseMove, second.center() + QPoint(40, 0), Qt::NoButton, Qt::LeftButton);
    sendMouse(&folder, QEvent::MouseButtonRelease, second.center() + QPoint(40, 0), Qt::LeftButton, Qt::NoButton);
    QObject::disconnect(connection);

    check(folder.count() == 2, "a cancelled non-detachable drag keeps its tab");
    check(floatingCount == 0, "a cancelled non-detachable drag detaches nothing");
}

void testEventFilterPromotion()
{
    std::fprintf(stderr, "--- deep click promotion ---\n");

    QcwTabFolder folder;
    folder.resize(700, 300);
    auto* firstPane = new QWidget;
    auto* deepChild = new PressConsumer(firstPane);
    folder.addTab(firstPane, "Alpha");
    folder.addTab(new QWidget, "Bravo");
    folder.setCurrentIndex(0);
    folder.show();
    QApplication::processEvents();

    int clicked = -2;
    QObject::connect(&folder, &QcwTabFolder::tabBarClicked, [&clicked](int index) { clicked = index; });

    // A press that lands on the pane itself names that tab.
    folder.setActive(false);
    QMouseEvent onPane(QEvent::MouseButtonPress, QPoint(5, 5), firstPane->mapToGlobal(QPoint(5, 5)), Qt::LeftButton,
                       Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(firstPane, &onPane);
    check(clicked == 0, "a press on the pane reports its tab");

    // A press consumed deep inside the view promotes the current tab only while
    // the folder is not active; an active folder's views need no promotion.
    clicked = -2;
    QMouseEvent onChild(QEvent::MouseButtonPress, QPoint(1, 1), deepChild->mapToGlobal(QPoint(1, 1)), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(deepChild, &onChild);
    check(clicked == 0, "a deep press reports the current tab of an inactive folder");

    clicked = -2;
    folder.setActive(true);
    QApplication::sendEvent(deepChild, &onChild);
    check(clicked == -2, "an active folder leaves deep presses alone");

    // Presses outside the folder are ignored by its filter.
    clicked = -2;
    QWidget outside;
    QMouseEvent outsider(QEvent::MouseButtonPress, QPoint(1, 1), outside.mapToGlobal(QPoint(1, 1)), Qt::LeftButton,
                         Qt::LeftButton, Qt::NoModifier);
    folder.setActive(false);
    QApplication::sendEvent(&outside, &outsider);
    check(clicked == -2, "a press outside the folder is not promoted");
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    testSelectionOnRelease();
    testCloseButton();
    testDoubleClick();
    testHoverAndCancelledPress();
    testDragDetachesTab();
    testNonDetachableDrag();
    testEventFilterPromotion();

    if (failures == 0)
    {
        std::printf("tab_folder_mouse_test: all checks passed\n");
    }
    else
    {
        std::fprintf(stderr, "tab_folder_mouse_test: %d failure(s)\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
