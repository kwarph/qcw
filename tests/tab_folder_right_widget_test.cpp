// Drives QcwTabFolder::setRightWidget() directly with a tool bar and with a composite
// widget: the widget sits in the tab bar while there is room, falls back to the pane's
// top-right corner when there is not, and goes back to its owner when replaced, removed,
// or when the folder is destroyed. Hosting one also makes the folder re-measure its tabs,
// so the settling of that re-measure and the click bookkeeping around it are checked here too.

#include "QcwTabFolder.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <cstdio>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QcwTabFolder folder;
    folder.resize(800, 500);

    auto pane1 = new QWidget;
    auto toolbar1 = new QToolBar(pane1);
    toolbar1->addAction("Alpha");
    toolbar1->addAction("Beta");

    auto pane2 = new QWidget;

    folder.addTab(pane1, "One");
    folder.addTab(pane2, "Two");
    folder.setCurrentIndex(0);
    folder.show();
    app.processEvents();

    const bool startOk = folder.currentWidget() == pane1;

    folder.setRightWidget(toolbar1);
    app.processEvents();

    const bool wideOk = toolbar1->parentWidget() == &folder && toolbar1->isVisible();
    std::fprintf(stderr, "wide ok=%d parent=%p folder=%p geom=%d,%d %dx%d folderSize=%dx%d hint=%d\n", wideOk,
                 static_cast<void*>(toolbar1->parentWidget()), static_cast<void*>(&folder), toolbar1->x(),
                 toolbar1->y(), toolbar1->width(), toolbar1->height(), folder.width(), folder.height(),
                 toolbar1->sizeHint().width());
    const int widePaneHeight = pane1->height();

    folder.resize(240, 500);
    app.processEvents();

    // Without room in the tab bar the widget is laid out at the top of the pane, right
    // aligned, and the pane is placed below it instead of underneath it.
    const QRect narrowToolbarRect = toolbar1->geometry();
    const QRect narrowPaneRect = pane1->geometry();
    const bool narrowOk = toolbar1->parentWidget() == &folder && toolbar1->isVisible() &&
                          narrowToolbarRect.right() == narrowPaneRect.right() &&
                          narrowToolbarRect.bottom() + 1 == narrowPaneRect.top() &&
                          !narrowToolbarRect.intersects(narrowPaneRect) &&
                          narrowToolbarRect.height() + narrowPaneRect.height() == widePaneHeight;
    std::fprintf(stderr, "narrow ok=%d parent=%p pane1=%p geom=%d,%d %dx%d pane=%d,%d %dx%d\n", narrowOk,
                 static_cast<void*>(toolbar1->parentWidget()), static_cast<void*>(pane1), toolbar1->x(), toolbar1->y(),
                 toolbar1->width(), toolbar1->height(), pane1->x(), pane1->y(), pane1->width(), pane1->height());

    folder.resize(800, 500);
    app.processEvents();
    // Back in the tab bar, the pane gets the strip back.
    const bool wideAgainOk =
        toolbar1->parentWidget() == &folder && toolbar1->isVisible() && pane1->height() == widePaneHeight;

    // A folder-level widget stays put across tab changes; replacing it is the caller's job.
    folder.setCurrentIndex(1);
    app.processEvents();
    const bool persistsOk =
        toolbar1->parentWidget() == &folder && toolbar1->isVisible() && folder.rightWidget() == toolbar1;

    auto toolbar2 = new QToolBar(pane2);
    toolbar2->addAction("Gamma");
    folder.setRightWidget(toolbar2);
    app.processEvents();
    const bool replacedOk = folder.rightWidget() == toolbar2 && toolbar2->parentWidget() == &folder &&
                            toolbar1->parentWidget() == pane1 && !toolbar1->isVisible();

    // Any widget works, including a composite one with its own layout and children.
    auto indicator = new QWidget(pane1);
    auto indicatorLayout = new QHBoxLayout(indicator);
    indicatorLayout->setContentsMargins(0, 0, 0, 0);
    indicatorLayout->setSpacing(4);
    auto positionLabel = new QLabel("12:34", indicator);
    auto eolLabel = new QLabel("LF", indicator);
    indicatorLayout->addWidget(positionLabel);
    indicatorLayout->addWidget(eolLabel);

    folder.setRightWidget(indicator);
    app.processEvents();
    const bool compositeOk = folder.rightWidget() == indicator && indicator->parentWidget() == &folder &&
                             indicator->isVisible() && positionLabel->isVisible() && eolLabel->isVisible();

    // Removal hands the old widget back hidden.
    folder.setRightWidget(nullptr);
    app.processEvents();
    const bool clearedOk = folder.rightWidget() == nullptr && toolbar2->parentWidget() == pane2 &&
                           !toolbar2->isVisible() && indicator->parentWidget() == pane1 && !indicator->isVisible();

    // Destroying the folder hands a parked widget back to its owner instead of destroying
    // it along with the folder's children.
    auto extraFolder = new QcwTabFolder;
    extraFolder->resize(800, 500);
    auto extraPane = new QWidget;
    auto extraToolBar = new QToolBar(extraPane);
    extraToolBar->addAction("Delta");
    extraFolder->addTab(extraPane, "Extra");
    extraFolder->show();
    extraFolder->setRightWidget(extraToolBar);
    app.processEvents();
    const bool extraParkedOk = extraToolBar->parentWidget() == extraFolder;
    delete extraFolder;
    const bool folderDestroyedOk = extraToolBar->parentWidget() == extraPane && !extraToolBar->isVisible();
    delete extraPane;

    // A view shown by two folders hands the same tool bar to both. Naming the pane as the
    // owner keeps the folders from treating each other as that owner: the one that loses the
    // widget must leave it with the folder still hosting it, and only the last one hands it
    // back to the pane.
    auto sharedPane = new QWidget;
    auto sharedToolBar = new QToolBar(sharedPane);
    sharedToolBar->addAction("Shared");

    auto firstFolder = new QcwTabFolder;
    firstFolder->resize(800, 500);
    firstFolder->addTab(sharedPane, "Shared");
    firstFolder->show();
    firstFolder->setRightWidget(sharedToolBar, sharedPane);
    app.processEvents();
    const bool firstHostedOk = sharedToolBar->parentWidget() == firstFolder;

    auto secondFolder = new QcwTabFolder;
    secondFolder->resize(800, 500);
    secondFolder->addTab(sharedPane, "Shared");
    secondFolder->show();
    secondFolder->setRightWidget(sharedToolBar, sharedPane);
    app.processEvents();
    const bool secondHostedOk = sharedToolBar->parentWidget() == secondFolder;

    delete firstFolder;
    app.processEvents();
    const bool keptBySecondOk =
        sharedToolBar->parentWidget() == secondFolder && secondFolder->rightWidget() == sharedToolBar;

    delete secondFolder;
    const bool handedBackToPaneOk = sharedToolBar->parentWidget() == sharedPane && !sharedToolBar->isVisible();
    delete sharedPane;

    // Tab widths come from the text measured against the folder font. A font change has to
    // measure the text again, or the renderer keeps the old width and elides the new text.
    auto tabRightEdge = [](QcwTabFolder& f)
    {
        for (int y = 0; y < f.height(); ++y)
        {
            int right = 0;
            for (int x = 0; x < f.width(); ++x)
            {
                if (f.indexAt(QPoint(x, y)) == 0)
                {
                    right = x + 1;
                }
            }
            if (right > 0)
            {
                return right;
            }
        }
        return 0;
    };

    QcwTabFolder fontFolder;
    fontFolder.resize(800, 500);
    fontFolder.addTab(new QWidget, "Font tab");
    fontFolder.setCurrentIndex(0);
    fontFolder.show();
    app.processEvents();
    const int widthBeforeFontChange = tabRightEdge(fontFolder);

    const QFont originalFont = QApplication::font();
    QFont largerFont = originalFont;
    largerFont.setPointSize(originalFont.pointSize() + 6);
    QApplication::setFont(largerFont);
    app.processEvents();
    const int widthAfterFontChange = tabRightEdge(fontFolder);
    QApplication::setFont(originalFont);
    app.processEvents();
    const bool fontOk = widthBeforeFontChange > 0 && widthAfterFontChange > widthBeforeFontChange;

    // A folder narrow enough to hide a tab keeps its tab list button, and a right widget that
    // has to move into the pane makes the folder measure the tabs again. Measuring must not hide
    // and re-show the button on the way there: a child's visibility change posts a layout request
    // to a parent that has no layout, and the folder answers that request by measuring the tabs
    // once more - so an answer that only held for one of the measuring passes re-armed the next
    // one forever, at the cost of a spinning event loop and a flickering button.
    struct Churn : public QObject
    {
        QcwTabFolder* folder { nullptr };
        QToolButton* button { nullptr };
        int layoutRequests { 0 };
        int buttonChanges { 0 };

        bool eventFilter(QObject* obj, QEvent* event) override
        {
            if (obj == button && (event->type() == QEvent::Show || event->type() == QEvent::Hide))
            {
                ++buttonChanges;
            }
            else if (obj == folder && event->type() == QEvent::LayoutRequest)
            {
                ++layoutRequests;
            }

            return false;
        }
    };

    auto* stormFolder = new QcwTabFolder;
    stormFolder->resize(320, 300);
    auto* stormPane = new QWidget;
    auto* stormBar = new QToolBar(stormPane);
    stormBar->addAction("Alpha");
    stormBar->addAction("Beta");
    stormBar->addAction("Gamma");
    stormFolder->addTab(stormPane, "Notes");
    stormFolder->addTab(new QWidget, "echo_server_with_default.cpp");
    stormFolder->addTab(new QWidget, "chat_server.cpp");
    stormFolder->setCurrentIndex(0);
    stormFolder->show();
    app.processEvents();
    stormFolder->setRightWidget(stormBar, stormPane);
    app.processEvents();

    auto* listButton = stormFolder->findChild<QToolButton*>(QStringLiteral("tabListButton"));
    Churn churn;
    churn.folder = stormFolder;
    churn.button = listButton;
    app.installEventFilter(&churn);
    churn.layoutRequests = 0;
    churn.buttonChanges = 0;
    for (int round = 0; round < 20; ++round)
    {
        app.processEvents();
    }
    app.removeEventFilter(&churn);

    const bool settledOk = listButton->isVisible() && churn.buttonChanges == 0 && churn.layoutRequests == 0;
    std::fprintf(stderr, "settled requests=%d changes=%d\n", churn.layoutRequests, churn.buttonChanges);
    delete stormFolder;

    // A release the folder handles itself is not a tab-bar click once it lands on one of the
    // folder's own widgets. The point comes in the folder's coordinates, so it has to be compared
    // against a child's geometry(): a child's rect() is local to that child, which made every
    // position answer "not on a widget".
    QcwTabFolder releaseFolder;
    releaseFolder.resize(900, 400);
    auto* releasePane = new QWidget;
    auto* releaseBar = new QToolBar(releasePane);
    releaseBar->addAction("Alpha");
    releaseBar->addAction("Beta");
    releaseFolder.addTab(releasePane, "One");
    releaseFolder.addTab(new QWidget, "Two");
    releaseFolder.setCurrentIndex(0);
    releaseFolder.show();
    app.processEvents();
    releaseFolder.setRightWidget(releaseBar, releasePane);
    app.processEvents();

    int tabBarClicks = 0;
    auto clickConnection = QObject::connect(&releaseFolder, &QcwTabFolder::tabBarClicked,
                                            [&tabBarClicks](int)
                                            { ++tabBarClicks; });

    auto sendRelease = [&releaseFolder](const QPoint& where)
    {
        QMouseEvent event(QEvent::MouseButtonRelease, where, releaseFolder.mapToGlobal(where), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&releaseFolder, &event);
    };

    sendRelease(releaseBar->geometry().center());
    const bool releaseOnWidgetOk = tabBarClicks == 0;

    // The same release in the empty strip beside the tabs still is a tab-bar click, so the check
    // above cannot pass by never reporting one.
    sendRelease(QPoint(releaseBar->x() - 40, releaseBar->y() + 4));
    const bool releaseOnEmptyOk = tabBarClicks == 1;
    QObject::disconnect(clickConnection);

    const bool allOk = startOk && wideOk && narrowOk && wideAgainOk && persistsOk && replacedOk && compositeOk &&
                       clearedOk && extraParkedOk && folderDestroyedOk && firstHostedOk && secondHostedOk &&
                       keptBySecondOk && handedBackToPaneOk && fontOk && settledOk && releaseOnWidgetOk &&
                       releaseOnEmptyOk;
    std::fprintf(stderr,
                 "results: start=%d wide=%d narrow=%d wideAgain=%d persists=%d replaced=%d composite=%d "
                 "cleared=%d extraParked=%d folderDestroyed=%d firstHosted=%d secondHosted=%d keptBySecond=%d "
                 "handedBackToPane=%d font=%d (%d -> %d) settled=%d onWidget=%d empty=%d\n",
                 startOk, wideOk, narrowOk, wideAgainOk, persistsOk, replacedOk, compositeOk, clearedOk, extraParkedOk,
                 folderDestroyedOk, firstHostedOk, secondHostedOk, keptBySecondOk, handedBackToPaneOk, fontOk,
                 widthBeforeFontChange, widthAfterFontChange, settledOk, releaseOnWidgetOk, releaseOnEmptyOk);

    // CTest runs headless and has to terminate on its own; a manual run keeps the window
    // open for inspection.
    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return allOk ? 0 : 1;
}
