// End-to-end check of QcwView::statusBarWidget() through QcwWindow: the current view's
// widget lands in the status bar, gives way when another view becomes current, and comes
// back unchanged when its own view does. The widget keeps its owner throughout: the status
// bar hands it back hidden, deletes it only when its owner is gone, and returns it on
// window destruction.

#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QStatusBar>
#include <QTimer>
#include <cstdio>

// A status widget with its own layout, like the line/column label pairs a real view uses.
class CompositeStatusWidget : public QWidget
{
public:
    explicit CompositeStatusWidget(QWidget* parent = nullptr) :
        QWidget { parent }
    {
        auto layout = new QHBoxLayout(this);
        layout->setContentsMargins(6, 0, 6, 0);
        layout->setSpacing(8);
        positionLabel = new QLabel("Ln 1, Col 1", this);
        eolLabel = new QLabel("LF", this);
        layout->addWidget(positionLabel);
        layout->addWidget(eolLabel);
    }

    QLabel* positionLabel { nullptr };
    QLabel* eolLabel { nullptr };
};

class StatusView : public QcwView
{
public:
    explicit StatusView(bool withWidget, QWidget* parent = nullptr) :
        QcwView { parent }
    {
        if (withWidget)
        {
            statusWidget = new CompositeStatusWidget(this);
        }
    }

    QWidget* statusBarWidget() const override
    {
        return statusWidget;
    }

    QWidget* statusWidget { nullptr };
};

class LabelView : public QcwView
{
public:
    explicit LabelView(const QString& text, QWidget* parent = nullptr) :
        QcwView { parent }
    {
        statusWidget = new QLabel(text, this);
    }

    QWidget* statusBarWidget() const override
    {
        return statusWidget;
    }

    QLabel* statusWidget { nullptr };
};

// A view that shows a widget it does not own: the status bar has to hand the widget back
// to its real owner, and delete it only when that owner goes away while it is installed.
class AttachedStatusView : public QcwView
{
public:
    explicit AttachedStatusView(QWidget* widget, QWidget* parent = nullptr) :
        QcwView { parent }, statusWidget { widget }
    {
        QObject::connect(widget, &QObject::destroyed, this, [this]()
        {
            statusWidget = nullptr;
        });
    }

    QWidget* statusBarWidget() const override
    {
        return statusWidget;
    }

    QWidget* statusWidget { nullptr };
};

// A second perspective with a folder of its own, so a perspective switch changes the
// current view and therefore the widget in the status bar.
class SecondPerspective : public QcwPerspective
{
public:
    explicit SecondPerspective(QcwWindow* parent) :
        QcwPerspective { parent }
    {
    }

    void createInitialLayout() override
    {
        auto container = folderContainer();
        auto folder = container->createFolder(nullptr, qcw::LeftDockArea);
        container->setCentralFolder(folder);

        view = new StatusView(true);
        folder->addTab(view, "Other");
        folder->setCurrentIndex(0);

        // A hidden perspective applies no active highlight, but its folder still has to be
        // current for setCurrentPerspective() to re-announce the view on the switch.
        activateFolder(folder);
    }

    StatusView* view { nullptr };
};

static bool isInStatusBar(QStatusBar* statusBar, QWidget* widget)
{
    return widget && statusBar->isAncestorOf(widget);
}

static bool isFullyInside(QStatusBar* statusBar, QWidget* widget)
{
    if (!isInStatusBar(statusBar, widget))
    {
        return false;
    }

    const QRect rect { widget->mapTo(statusBar, QPoint(0, 0)), widget->size() };
    return statusBar->rect().contains(rect);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QcwWindow window;
    window.setId("status-bar-widget-test");
    window.setupUi();
    window.resize(800, 500);
    window.show();
    app.processEvents();

    auto statusBar = window.statusBar();
    auto dummy = window.currentPerspective();
    auto folder = dummy->folderContainer()->centralFolder();

    auto view1 = new StatusView(true);
    auto view2 = new LabelView("View two");
    auto view3 = new StatusView(false);
    folder->addTab(view1, "One");
    folder->addTab(view2, "Two");
    folder->addTab(view3, "Three");
    folder->setCurrentIndex(0);
    app.processEvents();

    auto composite = static_cast<CompositeStatusWidget*>(view1->statusWidget);
    auto label = view2->statusWidget;

    // The current view's composite widget is installed, visible and whole: both labels fit
    // instead of being squeezed by the status bar.
    const bool statusBarVisibleOk = statusBar->isVisible();
    const bool installedOk = isInStatusBar(statusBar, composite) && composite->isVisible() &&
                             isFullyInside(statusBar, composite) &&
                             composite->width() >= composite->sizeHint().width() &&
                             composite->positionLabel->isVisible() && composite->eolLabel->isVisible();
    std::fprintf(stderr, "state: window=%dx%d statusBar=%d widgetParent=%p geom=%d,%d %dx%d hint=%d\n", window.width(),
                 window.height(), statusBarVisibleOk, static_cast<void*>(composite->parentWidget()), composite->x(),
                 composite->y(), composite->width(), composite->height(), composite->sizeHint().width());

    // Switching tabs gives the slot to the new view and hands the old widget back to the
    // parent it came from, hidden, like a folder's right widget.
    folder->setCurrentIndex(1);
    app.processEvents();

    const bool switchedOk = !composite->isVisible() && isInStatusBar(statusBar, label) && label->isVisible() &&
                            isFullyInside(statusBar, label) && label->width() >= label->sizeHint().width();
    const bool handedBackOk =
        composite->parentWidget() == view1 && !composite->isVisible() && !isInStatusBar(statusBar, composite);
    std::fprintf(stderr, "switch: oldVisible=%d newVisible=%d oldParentOk=%d newGeom=%d,%d %dx%d\n",
                 composite->isVisible(), label->isVisible(), handedBackOk, label->x(), label->y(), label->width(),
                 label->height());

    // A view with no status widget clears the slot and hands the previous widget back too.
    folder->setCurrentIndex(2);
    app.processEvents();

    const bool clearedOk = !composite->isVisible() && !label->isVisible() && composite->parentWidget() == view1 &&
                           label->parentWidget() == view2 && view3->statusBarWidget() == nullptr;
    std::fprintf(stderr, "cleared=%d compositeVisible=%d labelVisible=%d\n", clearedOk, composite->isVisible(),
                 label->isVisible());

    // Coming back shows the same instance again, labels included.
    folder->setCurrentIndex(0);
    app.processEvents();

    const bool restoredOk = view1->statusWidget == composite && isInStatusBar(statusBar, composite) &&
                            composite->isVisible() && composite->positionLabel->isVisible() &&
                            composite->eolLabel->isVisible();
    std::fprintf(stderr, "restored=%d sameInstance=%d parent=%p\n", restoredOk, view1->statusWidget == composite,
                 static_cast<void*>(composite->parentWidget()));

    // A perspective switch re-reads the status widget of whatever view is current in it.
    auto secondPerspective = new SecondPerspective(&window);
    secondPerspective->createInitialLayout();
    window.openPerspective(secondPerspective);
    app.processEvents();

    auto secondWidget = secondPerspective->view->statusWidget;
    const bool perspectiveSwitchOk = window.currentPerspective() == secondPerspective &&
                                     isInStatusBar(statusBar, secondWidget) && secondWidget->isVisible() &&
                                     !composite->isVisible();

    window.setCurrentPerspective(dummy->id());
    app.processEvents();

    const bool perspectiveBackOk = window.currentPerspective() == dummy && isInStatusBar(statusBar, composite) &&
                                   composite->isVisible() && secondWidget->parentWidget() == secondPerspective->view &&
                                   !secondWidget->isVisible();
    std::fprintf(stderr, "perspective: switch=%d back=%d secondParent=%p firstVisible=%d secondVisible=%d\n",
                 perspectiveSwitchOk, perspectiveBackOk, static_cast<void*>(secondWidget->parentWidget()),
                 composite->isVisible(), secondWidget->isVisible());

    // A view may delete its status widget while that widget is installed; the next view's
    // widget has to install normally, without a stale slot in between.
    QPointer<QWidget> oldWidget = composite;
    delete composite;
    view1->statusWidget = nullptr;
    app.processEvents();

    folder->setCurrentIndex(1);
    app.processEvents();

    const bool deleteWhileInstalledOk = oldWidget.isNull() && isInStatusBar(statusBar, label) && label->isVisible();
    std::fprintf(stderr, "deleted=%d nextInstalled=%d\n", oldWidget.isNull(), label->isVisible());

    // A widget deleted while installed leaves its owner connection behind. Destroying that
    // owner later must not reach the widget that replaced it.
    auto staleOwner = new QWidget;
    auto transient = new QLabel("Transient", staleOwner);
    auto attachedA = new AttachedStatusView(transient);
    folder->addTab(attachedA, "Attached A");
    folder->setCurrentIndex(3);
    app.processEvents();
    delete transient;
    app.processEvents();

    folder->setCurrentIndex(1);
    app.processEvents();
    QPointer<QWidget> keptLabel = label;
    delete staleOwner;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const bool staleOwnerOk = !keptLabel.isNull() && label->isVisible() && isInStatusBar(statusBar, label);
    std::fprintf(stderr, "staleOwner: labelAlive=%d visible=%d\n", !keptLabel.isNull(), label->isVisible());
    folder->removeTab(3);

    // The widget's real owner going away while the widget is installed deletes the widget
    // and clears the slot.
    auto goneOwner = new QWidget;
    auto orphan = new QLabel("Orphan", goneOwner);
    auto attachedB = new AttachedStatusView(orphan);
    folder->addTab(attachedB, "Attached B");
    folder->setCurrentIndex(3);
    app.processEvents();
    QPointer<QWidget> orphanPointer = orphan;
    const bool orphanInstalledOk = isInStatusBar(statusBar, orphan) && orphan->isVisible();

    delete goneOwner;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const bool ownerGoneOk = orphanInstalledOk && orphanPointer.isNull() && attachedB->statusWidget == nullptr;
    folder->setCurrentIndex(1);
    app.processEvents();
    const bool afterOwnerGoneOk = label->isVisible() && isInStatusBar(statusBar, label);
    std::fprintf(stderr, "ownerGone: installed=%d deleted=%d next=%d\n", orphanInstalledOk, orphanPointer.isNull(),
                 afterOwnerGoneOk);
    folder->removeTab(3);

    // The window disposes of an installed status widget whose owner lives inside the
    // window; QPointer turns null when it goes.
    QPointer<QWidget> windowOwnedWidget;
    {
        auto tempWindow = new QcwWindow;
        tempWindow->setId("status-bar-widget-lifetime-test");
        tempWindow->setupUi();

        auto tempFolder = tempWindow->currentPerspective()->folderContainer()->centralFolder();
        auto tempView = new StatusView(true);
        tempFolder->addTab(tempView, "Temp");
        tempFolder->setCurrentIndex(0);
        app.processEvents();

        windowOwnedWidget = tempView->statusWidget;
        std::fprintf(stderr, "lifetime: installed=%d inStatusBar=%d\n", !windowOwnedWidget.isNull(),
                     isInStatusBar(tempWindow->statusBar(), windowOwnedWidget));

        delete tempWindow;

        // The folder hands its panes to the pane manager, which deletes the last holder
        // with deleteLater(); the status widget went back to that view first.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    const bool windowDestroysWidgetOk = windowOwnedWidget.isNull();
    std::fprintf(stderr, "windowDestroysWidget=%d\n", windowDestroysWidgetOk);

    // An owner outside the window keeps its widget: destroying the window hands the widget
    // back, hidden, instead of destroying it.
    auto survivorOwner = new QWidget;
    QPointer<QWidget> survivor;
    bool windowHandedBackOk = false;
    {
        auto owned = new QLabel("Survivor", survivorOwner);
        survivor = owned;

        auto tempWindow = new QcwWindow;
        tempWindow->setId("status-bar-widget-handback-test");
        tempWindow->setupUi();

        auto tempFolder = tempWindow->currentPerspective()->folderContainer()->centralFolder();
        auto tempView = new AttachedStatusView(owned);
        tempFolder->addTab(tempView, "Temp");
        tempFolder->setCurrentIndex(0);
        app.processEvents();

        const bool installed = isInStatusBar(tempWindow->statusBar(), owned);
        delete tempWindow;
        windowHandedBackOk =
            installed && !survivor.isNull() && survivor->parentWidget() == survivorOwner && !survivor->isVisible();
    }

    const bool survivorStaysUntilOwnerGoneOk = !survivor.isNull();
    delete survivorOwner;
    const bool survivorDeletedWithOwnerOk = survivor.isNull();
    std::fprintf(stderr, "handback: windowHandedBack=%d survivorAlive=%d survivorDeleted=%d\n", windowHandedBackOk,
                 survivorStaysUntilOwnerGoneOk, survivorDeletedWithOwnerOk);

    // A fresh widget is picked up on the next view change, and leaves the composite in the
    // status bar for a manual run to inspect.
    view1->statusWidget = new CompositeStatusWidget(view1);
    folder->setCurrentIndex(1);
    folder->setCurrentIndex(0);
    app.processEvents();

    const bool recreatedOk = view1->statusWidget->isVisible() && isInStatusBar(statusBar, view1->statusWidget);
    std::fprintf(stderr, "recreated=%d widget=%p\n", recreatedOk, static_cast<void*>(view1->statusWidget));

    const bool allOk = statusBarVisibleOk && installedOk && switchedOk && handedBackOk && clearedOk && restoredOk &&
                       perspectiveSwitchOk && perspectiveBackOk && deleteWhileInstalledOk && staleOwnerOk &&
                       ownerGoneOk && afterOwnerGoneOk && windowDestroysWidgetOk && windowHandedBackOk &&
                       survivorStaysUntilOwnerGoneOk && survivorDeletedWithOwnerOk && recreatedOk;
    std::fprintf(stderr,
                 "results: statusBarVisible=%d installed=%d switched=%d handedBack=%d cleared=%d restored=%d "
                 "perspectiveSwitch=%d perspectiveBack=%d deleteWhileInstalled=%d staleOwner=%d ownerGone=%d "
                 "afterOwnerGone=%d windowDestroysWidget=%d windowHandedBack=%d survivorDeletedWithOwner=%d "
                 "recreated=%d\n",
                 statusBarVisibleOk, installedOk, switchedOk, handedBackOk, clearedOk, restoredOk, perspectiveSwitchOk,
                 perspectiveBackOk, deleteWhileInstalledOk, staleOwnerOk, ownerGoneOk, afterOwnerGoneOk,
                 windowDestroysWidgetOk, windowHandedBackOk, survivorDeletedWithOwnerOk, recreatedOk);

    // CTest runs headless and has to terminate on its own; a manual run keeps the window
    // open for inspection.
    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return allOk ? 0 : 1;
}
