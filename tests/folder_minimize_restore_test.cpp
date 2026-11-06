// Exercises the minimized-folder bookkeeping on a nested layout: minimizing every folder
// hides whole splitter subtrees, and restoring them in a different order has to bring back
// the proportions the splitters had while everything was visible.

#include "QcwFastViewToolBar.h"
#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabFolderSite.h"
#include "QcwThemeManager.h"
#include "QcwWindow.h"
#include "QcwWindowStyler.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLayout>
#include <QSplitter>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <cstdio>

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

class TestPerspective : public QcwPerspective
{
public:
    using QcwPerspective::QcwPerspective;

    void createInitialLayout() override
    {
        auto container = folderContainer();
        auto central = window()->sharedFolder();
        container->addSharedFolder(central, nullptr, qcw::LeftDockArea);
        container->setCentralFolder(central);
        shared = central;

        left = container->createFolder(central, qcw::LeftDockArea);
        container->setEmptyFolderAction(left, qcw::EmptyFolderAction::Keep);

        bottom = container->createFolder(central, qcw::BottomDockArea);
        container->setEmptyFolderAction(bottom, qcw::EmptyFolderAction::Keep);

        right = container->createFolder(central, qcw::RightDockArea);
        container->setEmptyFolderAction(right, qcw::EmptyFolderAction::Keep);

        left->addTab(new QTextEdit, "SampleView1");
        bottom->addTab(new QTextEdit, "SampleView2");
        right->addTab(new QTextEdit, "SampleView3");
    }

    QcwTabFolder* shared { nullptr };
    QcwTabFolder* left { nullptr };
    QcwTabFolder* bottom { nullptr };
    QcwTabFolder* right { nullptr };
};

static void settle()
{
    for (int i = 0; i < 10; ++i)
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
}

static void minimizeAll(TestPerspective* perspective, const QStringList& order)
{
    for (const auto& name : order)
    {
        auto folder = name == "left" ? perspective->left : name == "shared" ? perspective->shared : perspective->bottom;
        folder->minimize();
        settle();
    }
}

static void restoreAll(QcwWindow& window, TestPerspective* perspective, const QStringList& order)
{
    for (const auto& name : order)
    {
        auto folder = name == "left" ? perspective->left : name == "shared" ? perspective->shared : perspective->bottom;

        QcwFastViewToolBar* toolbar = nullptr;
        for (auto bar : window.findChildren<QcwFastViewToolBar*>())
        {
            if (bar->folder() == folder)
            {
                toolbar = bar;
                break;
            }
        }

        if (toolbar && !toolbar->actions().isEmpty())
        {
            toolbar->actions().first()->trigger();
        }
        settle();
    }
}

static bool failures = false;

static void checkOn(const char* when, bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s (%s)\n", message, when);
        failures = true;
    }
}

// The folder sits on the anchor's top-left corner with nothing above it, so it lines up with the
// folders around it; the margin on its window-facing side sets it apart from the page behind the
// popup - on the left for a fast view docked on the right, on the right for one docked on the left -
// and one below it closes the popup off. The margins lie inside the popup, so the folder keeps the
// size it was given.
static void checkPopupMargins(QcwWindow& window, QcwTabFolder* folder, qcw::DockArea dockArea, const char* when)
{
    QcwFastViewToolBar* toolbar = nullptr;
    for (auto bar : window.findChildren<QcwFastViewToolBar*>())
    {
        if (bar->folder() == folder)
        {
            toolbar = bar;
        }
    }

    if (!toolbar)
    {
        std::fprintf(stderr, "FAIL: no fast view toolbar for %s\n", when);
        failures = true;
        return;
    }

    QList<QAction*> actions = toolbar->actions();
    QAction* show = actions.value(actions.size() > 1 ? 1 : 0);
    if (show)
    {
        show->trigger();
    }
    settle();

    // The frame's margins are the site's own; QLayout reports nothing for them even though it lays
    // the folder out inside them.
    auto site = qobject_cast<QFrame*>(folder->parentWidget());
    if (!site)
    {
        std::fprintf(stderr, "FAIL: the fast view does not host the folder in a frame (%s)\n", when);
        failures = true;
        return;
    }

    const QMargins margins = site->contentsMargins();
    const QSize folderSize = folder->size();
    const QSize siteSize = site->size();
    const QPoint folderOffset = folder->mapTo(site, QPoint { 0, 0 });
    std::fprintf(stderr, "%s: folder=%dx%d at %d,%d popup=%dx%d margins=%d,%d,%d,%d\n", when, folderSize.width(),
                 folderSize.height(), folderOffset.x(), folderOffset.y(), siteSize.width(), siteSize.height(),
                 margins.left(), margins.top(), margins.right(), margins.bottom());

    checkOn(when, folderOffset == QPoint(margins.left(), margins.top()),
            "the folder sits on the popup's top-left corner, pushed in by those margins");
    checkOn(when, margins.top() == 0 && margins.bottom() > 0,
            "the popup keeps the folder on its top edge and gives it a margin below");
    if (dockArea == qcw::LeftDockArea)
    {
        checkOn(when, margins.left() == 0 && margins.right() > 0,
                "a left fast view keeps no margin on the window's side and one towards the window");
    }
    else
    {
        checkOn(when, margins.left() > 0 && margins.right() == 0,
                "a right fast view keeps a margin towards the window and none on the window's side");
    }
    checkOn(when, siteSize == folderSize.grownBy(margins), "the popup is the folder grown by its margins");
    checkOn(when, folderSize == QSize(600, 400), "the folder keeps the size the fast view was asked to show");
    checkOn(when, site->palette().color(QPalette::Window) == window.palette().color(QPalette::Window),
            "the margin is painted in the window color");

    // The popup is dismissed without restoring first, so the next opening moves the folder through
    // its original site and back - the round trip that used to leave the folder at 0x0, spilling
    // over the popup and painting its margin or frame away.
    site->hide();
    settle();
}

// The Framed style draws the boundary as a frame around the popup and keeps no margin, so the
// folder has to stay inside the frame's width on every side - in particular once it has been
// through its original site and back.
static void checkPopupFrame(QcwWindow& window, QcwTabFolder* folder, const char* when)
{
    QcwFastViewToolBar* toolbar = nullptr;
    for (auto bar : window.findChildren<QcwFastViewToolBar*>())
    {
        if (bar->folder() == folder)
        {
            toolbar = bar;
        }
    }

    if (!toolbar)
    {
        std::fprintf(stderr, "FAIL: no fast view toolbar for %s\n", when);
        failures = true;
        return;
    }

    QList<QAction*> actions = toolbar->actions();
    QAction* show = actions.value(actions.size() > 1 ? 1 : 0);
    if (show)
    {
        show->trigger();
    }
    settle();

    auto site = qobject_cast<QFrame*>(folder->parentWidget());
    if (!site)
    {
        std::fprintf(stderr, "FAIL: the fast view does not host the folder in a frame (%s)\n", when);
        failures = true;
        return;
    }

    const int frame = site->frameWidth();
    const QSize folderSize = folder->size();
    const QSize siteSize = site->size();
    const int margin = site->layout()->contentsMargins().top();
    std::fprintf(stderr, "%s: folder=%dx%d at %d,%d popup=%dx%d frame=%d\n", when, folderSize.width(),
                 folderSize.height(), folder->x(), folder->y(), siteSize.width(), siteSize.height(), frame);

    const QMargins insets { frame + margin, frame + margin, frame + margin, frame + margin };
    checkOn(when, site->frameShape() != QFrame::NoFrame, "the Framed style frames the fast view");
    checkOn(when, frame > 0, "the frame has a width");
    checkOn(when, folder->pos() == QPoint(insets.left(), insets.top()),
            "the folder starts on the popup's corner inside its frame");
    checkOn(when, folderSize == QSize(600, 400), "the folder keeps the size the fast view was asked to show");
    checkOn(when, siteSize == folderSize.grownBy(insets), "the popup is the folder grown by its frame");
    checkOn(when, site->rect().contains(folder->geometry()), "the folder does not spill over the frame");

    site->hide();
    settle();
}

static QString writeStyleTheme(const QString& root, const QString& name, const QString& styleName,
                               const QString& styleJson)
{
    const QDir dir(QDir(root).filePath(name));
    if (!QDir().mkpath(dir.path()))
    {
        return QString();
    }

    auto write = [](const QString& path, const QByteArray& data)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };

    const bool paletteOk = write(dir.filePath("QcwPalette.theme"), R"({"style":"Fusion","colors":{}})");
    const bool styleOk = write(dir.filePath(QStringLiteral("QcwWindow.%1.theme").arg(styleName)), styleJson.toUtf8());
    return paletteOk && styleOk ? dir.absolutePath() : QString();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    TestWindow window;
    window.setId("minimize_restore_test");
    window.setupUi();
    window.resize(900, 600);

    auto perspective = new TestPerspective { &window };
    perspective->createInitialLayout();
    window.openPerspective(perspective);
    window.show();
    settle();

    auto container = perspective->folderContainer();
    auto root = container->parentSplitter(perspective->left);
    auto right = container->parentSplitter(perspective->bottom);
    root->setSizes({ 240, container->width() - 240 });
    right->setSizes({ 280, 240 });
    settle();

    const auto expectedRoot = root->sizes();
    const auto expectedRight = right->sizes();

    // The orders that previously left the shared folder collapsed to its tab bar.
    const QList<QPair<QStringList, QStringList>> cycles {
        { { "left", "shared", "bottom" }, { "bottom", "shared", "left" } },
        { { "shared", "left", "bottom" }, { "left", "shared", "bottom" } },
        { { "bottom", "shared", "left" }, { "shared", "bottom", "left" } },
    };

    bool allOk = true;
    for (const auto& [minimizeOrder, restoreOrder] : cycles)
    {
        minimizeAll(perspective, minimizeOrder);
        const bool allMinimized = perspective->left->state() == qcw::FolderState::Minimized &&
                                  perspective->shared->state() == qcw::FolderState::Minimized &&
                                  perspective->bottom->state() == qcw::FolderState::Minimized;

        restoreAll(window, perspective, restoreOrder);
        const bool rootOk = root->sizes() == expectedRoot;
        const bool rightOk = right->sizes() == expectedRight;
        const bool visibleOk =
            perspective->left->isVisible() && perspective->shared->isVisible() && perspective->bottom->isVisible();

        std::fprintf(stderr,
                     "minimize=%s restore=%s allMinimized=%d root=%d right=%d visible=%d "
                     "rootSizes=%d,%d expected=%d,%d rightSizes=%d,%d expectedRight=%d,%d\n",
                     qUtf8Printable(minimizeOrder.join(',')), qUtf8Printable(restoreOrder.join(',')), allMinimized,
                     rootOk, rightOk, visibleOk, root->sizes().at(0), root->sizes().at(1), expectedRoot.at(0),
                     expectedRoot.at(1), right->sizes().at(0), right->sizes().at(1), expectedRight.at(0),
                     expectedRight.at(1));

        allOk = allOk && allMinimized && rootOk && rightOk && visibleOk;
    }

    // The popup is opened for a fast view docked on the left and for one docked on the right, on the
    // full window and on a narrowed one: the margin on the window's side stays inside the anchor's
    // area, while the one towards the window keeps the folder clear of the page behind the popup.
    // No window style is installed here, so the margin is the docking side's own built-in one.
    for (const auto dockArea : { qcw::LeftDockArea, qcw::RightDockArea })
    {
        QcwTabFolder* fastView = dockArea == qcw::LeftDockArea ? perspective->left : perspective->right;
        fastView->minimize();
        settle();
        checkPopupMargins(window, fastView, dockArea, dockArea == qcw::LeftDockArea ? "left" : "right");

        fastView->minimize();
        settle();
        window.resize(520, 460);
        settle();
        checkPopupMargins(
            window, fastView, dockArea, dockArea == qcw::LeftDockArea ? "left, narrowed" : "right, narrowed");
        window.resize(900, 600);
        settle();
        fastView->restoreRequested();
        settle();
    }

    allOk = allOk && !failures;

    // The Framed style draws the boundary as a frame around the popup instead of a margin. Each
    // fast view is opened repeatedly under it: the second opening is the one that moves the folder
    // through its original site and back, the round trip that used to leave it at 0x0.
    QTemporaryDir styleDir;
    const QString framedDir =
        styleDir.isValid() ? writeStyleTheme(styleDir.path(), "TestFramed", "Framed",
                                             R"({"folderRenderer":"QcwDefaultTabFolderRenderer","fastViewMargin":0,)"
                                             R"("fastViewHasBorder":true,"windowCss":""})")
                           : QString();
    if (framedDir.isEmpty() || !themeManager->registerTheme("TestFramed", framedDir) ||
        !themeManager->changeTheme("TestFramed"))
    {
        std::fprintf(stderr, "FAIL: cannot install the test theme\n");
        allOk = false;
    }
    else
    {
        // Applying the style is what loads its fast view values.
        windowStyler->setCurrentStyle(QString(QcwWindowStyler::FramedStyle));
        settle();

        for (const auto dockArea : { qcw::LeftDockArea, qcw::RightDockArea })
        {
            QcwTabFolder* fastView = dockArea == qcw::LeftDockArea ? perspective->left : perspective->right;
            const char* name = dockArea == qcw::LeftDockArea ? "framed, left" : "framed, right";
            fastView->minimize();
            settle();
            checkPopupFrame(window, fastView, name);
            checkPopupFrame(window, fastView, dockArea == qcw::LeftDockArea ? "framed, left, again"
                                                                            : "framed, right, again");
            checkPopupFrame(window, fastView, dockArea == qcw::LeftDockArea ? "framed, left, third"
                                                                            : "framed, right, third");
            fastView->minimize();
            settle();
            fastView->restoreRequested();
            settle();
        }

        allOk = allOk && !failures;
    }

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return allOk ? 0 : 1;
}
