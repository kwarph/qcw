// End-to-end check of QcwView::topRightToolBar() placement through QcwPerspective: the
// current view's tool bar lands in the tab bar when it fits, drops to the view's own
// top-right corner when it does not, and changes hands as the current tab changes.

#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QTimer>
#include <QToolBar>
#include <cstdio>

// A tool bar whose width can be overridden at runtime, to exercise the hint-driven
// re-layout without a minimum width (setMinimumWidth would defeat the folder's clamping).
// Width 0 means the natural hint, which is what a real tool bar reports.
class VariableToolBar : public QToolBar
{
public:
    explicit VariableToolBar(QWidget* parent = nullptr) :
        QToolBar { parent }
    {
    }

    void setHintWidth(int width)
    {
        hintWidth = width;
        updateGeometry();
    }

    QSize sizeHint() const override
    {
        auto hint = QToolBar::sizeHint();
        if (hintWidth > 0)
        {
            hint.setWidth(hintWidth);
        }
        return hint;
    }

private:
    int hintWidth { 0 };
};

class ToolBarView : public QcwView
{
public:
    explicit ToolBarView(bool withToolBar = true, QWidget* parent = nullptr) :
        QcwView { parent }
    {
        if (withToolBar)
        {
            toolBar = new VariableToolBar(this);
            toolBar->addAction("Action A");
            toolBar->addAction("Action B");
        }
    }

    QToolBar* topRightToolBar() const override
    {
        return toolBar;
    }

    VariableToolBar* toolBar { nullptr };
};

// The shared folder is the usual home of a view, and it reaches the container through
// addSharedFolder() rather than createFolder(), so this window makes sure it is covered.
class SharedFolderWindow : public QcwWindow
{
protected:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder(this);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    SharedFolderWindow window;
    window.setId("toolbar-integration-test");
    window.setupUi();
    window.resize(800, 500);
    window.show();
    app.processEvents();

    auto perspective = window.currentPerspective();
    auto container = perspective->folderContainer();

    // The dummy perspective hosts the shared folder as its central folder.
    auto folder = container->centralFolder();
    const bool sharedFolderOk = window.sharedFolder() == folder;

    auto view1 = new ToolBarView;
    auto view2 = new ToolBarView;
    auto view3 = new ToolBarView(false);
    folder->addTab(view1, "One");
    folder->addTab(view2, "Two");
    folder->addTab(view3, "Three");
    folder->setCurrentIndex(0);
    app.processEvents();

    auto toolbar1 = view1->toolBar;
    auto toolbar2 = view2->toolBar;

    const bool current1Ok = folder->currentWidget() == view1;
    // A width below the natural hint makes QToolBar fold actions into its extension menu,
    // so the bar placement must also leave every action visible.
    const bool tabBarOk = toolbar1->parentWidget() == folder && toolbar1->isVisible() &&
                          folder->rightWidget() == toolbar1 && toolbar1->width() >= toolbar1->sizeHint().width();
    std::fprintf(stderr, "state: window=%dx%d folder=%dx%d tabBar=%d parent=%p geom=%d,%d %dx%d\n", window.width(),
                 window.height(), folder->width(), folder->height(), tabBarOk,
                 static_cast<void*>(toolbar1->parentWidget()), toolbar1->x(), toolbar1->y(), toolbar1->width(),
                 toolbar1->height());
    const int barViewHeight = view1->height();

    // A hint wider than the tab bar lays the toolbar out at the top-right corner of the view,
    // with the view below it: the toolbar must not sit on top of the view's content.
    toolbar1->setHintWidth(2000);
    app.processEvents();

    const QRect toolbarRect = toolbar1->geometry();
    const QRect viewRect = view1->geometry();
    const bool fallbackOk = toolbar1->parentWidget() == folder && toolbar1->isVisible() &&
                            folder->rightWidget() == toolbar1 && toolbarRect.right() == viewRect.right() &&
                            toolbarRect.bottom() + 1 == viewRect.top() && !toolbarRect.intersects(viewRect) &&
                            viewRect.height() + toolbarRect.height() == barViewHeight;
    std::fprintf(stderr, "fallback=%d parent=%p viewRect=%d,%d %dx%d geom=%d,%d %dx%d\n", fallbackOk,
                 static_cast<void*>(toolbar1->parentWidget()), view1->rect().x(), view1->rect().y(),
                 view1->rect().width(), view1->rect().height(), toolbar1->x(), toolbar1->y(), toolbar1->width(),
                 toolbar1->height());

    // The natural hint lets it back into the tab bar, wide enough for all its actions, and the
    // view gets the strip back.
    toolbar1->setHintWidth(0);
    app.processEvents();

    const bool backInBarOk = toolbar1->parentWidget() == folder && toolbar1->isVisible() &&
                             toolbar1->width() >= toolbar1->sizeHint().width() && view1->height() == barViewHeight;
    std::fprintf(stderr, "backInBar=%d parent=%p viewHeight=%d\n", backInBarOk,
                 static_cast<void*>(toolbar1->parentWidget()), view1->height());

    // Switching tabs returns toolbar1 to view1 and installs toolbar2, which fits.
    folder->setCurrentIndex(1);
    app.processEvents();

    const bool handedBackOk = toolbar1->parentWidget() == view1 && !toolbar1->isVisible();
    const bool secondInstalledOk = folder->rightWidget() == toolbar2 && toolbar2->parentWidget() == folder &&
                                   toolbar2->isVisible() && toolbar2->width() >= toolbar2->sizeHint().width() &&
                                   view2->height() == barViewHeight;
    std::fprintf(stderr, "handedBack=%d secondInstalled=%d toolbar1Parent=%p visible=%d rightWidget=%p\n", handedBackOk,
                 secondInstalledOk, static_cast<void*>(toolbar1->parentWidget()), toolbar1->isVisible(),
                 static_cast<void*>(folder->rightWidget()));

    // A view without a toolbar clears the slot and hands toolbar2 back.
    folder->setCurrentIndex(2);
    app.processEvents();

    const bool noToolBarOk =
        folder->rightWidget() == nullptr && toolbar2->parentWidget() == view2 && !toolbar2->isVisible();
    std::fprintf(stderr, "noToolBar=%d rightWidget=%p toolbar2Parent=%p visible=%d\n", noToolBarOk,
                 static_cast<void*>(folder->rightWidget()), static_cast<void*>(toolbar2->parentWidget()),
                 toolbar2->isVisible());

    // Switching back restores toolbar1 from view1, without the perspective doing anything
    // different from the first time it was shown.
    folder->setCurrentIndex(0);
    app.processEvents();

    const bool restoredOk = folder->rightWidget() == toolbar1 && toolbar1->parentWidget() == folder &&
                            toolbar1->isVisible() && toolbar1->width() >= toolbar1->sizeHint().width() &&
                            view1->height() == barViewHeight;

    const bool allOk = sharedFolderOk && current1Ok && tabBarOk && fallbackOk && backInBarOk && handedBackOk &&
                       secondInstalledOk && noToolBarOk && restoredOk;
    std::fprintf(stderr,
                 "results: sharedFolder=%d current1=%d tabBar=%d fallback=%d backInBar=%d handedBack=%d "
                 "secondInstalled=%d noToolBar=%d restored=%d\n",
                 sharedFolderOk, current1Ok, tabBarOk, fallbackOk, backInBarOk, handedBackOk, secondInstalledOk,
                 noToolBarOk, restoredOk);

    // CTest runs headless and has to terminate on its own; a manual run keeps the window
    // open for inspection.
    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return allOk ? 0 : 1;
}
