// Closing a perspective must dispose of its folder container. The container is a widget of
// the window's central area, so nothing else deletes it, and one left behind keeps listening
// to the window's shared folder, its floating folders and its sites long after the perspective
// is gone. This test closes two perspectives, rebuilds one, and closes the last document of
// the shared folder: the shared folder itself is protected from containers (see
// shared_folder_ownership_test), but a container outliving its perspective is still a leak.

#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <cstdio>

namespace
{
constexpr QLatin1String WindowId { "perspective-close-test" };

int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

/// main() runs outside an event loop, where processEvents() alone would leave deferred
/// deletes queued: the closing perspectives only die once they are delivered.
void settle()
{
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

/// A perspective with the shared folder as its centre, plus a kept folder of its own.
class TestPerspective : public QcwPerspective
{
public:
    explicit TestPerspective(QcwWindow* window) :
        QcwPerspective { window }
    {
    }

    void createInitialLayout() override
    {
        auto* container = folderContainer();
        auto* central = window()->sharedFolder();
        if (central)
        {
            container->addSharedFolder(central, nullptr, qcw::RightDockArea);
        }
        else
        {
            central = container->createFolder(nullptr, qcw::LeftDockArea);
        }
        container->setCentralFolder(central);

        mLeft = container->createFolder(central, qcw::LeftDockArea);
        container->setEmptyFolderAction(mLeft, qcw::EmptyFolderAction::Keep);
        mLeft->addTab(new QLabel(QStringLiteral("Project")), QStringLiteral("Project"));
    }

private:
    QcwTabFolder* mLeft { nullptr };
};
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    {
        TestWindow window;
        window.setId(WindowId);
        window.setupUi();
        window.resize(800, 500);
        window.show();
        settle();

        QPointer<QcwTabFolder> shared { window.sharedFolder() };
        check(!shared.isNull(), "setupUi builds the shared folder");

        auto* first = new TestPerspective { &window };
        first->createInitialLayout();
        window.openPerspective(first);
        settle();
        const int firstId = first->id();
        QPointer<QcwTabFolderContainer> firstContainer { first->folderContainer() };

        auto* second = new TestPerspective { &window };
        second->createInitialLayout();
        window.openPerspective(second);
        settle();
        const int secondId = second->id();
        QPointer<QcwTabFolderContainer> secondContainer { second->folderContainer() };

        // Close All Perspectives closes every entry the same way; the dummy stays.
        window.closePerspective(firstId);
        window.closePerspective(secondId);
        settle();

        check(firstContainer.isNull(), "a closed perspective destroys its folder container");
        check(secondContainer.isNull(), "the last closed perspective destroys its folder container");
        check(window.sharedFolder() == shared.data(), "the shared folder survives the closed perspectives");

        auto* third = new TestPerspective { &window };
        third->createInitialLayout();
        window.openPerspective(third);
        settle();

        // Opening files and closing the last one is what made a leftover container act on the
        // shared folder.
        shared->addTab(new QLabel(QStringLiteral("one")), QStringLiteral("one"));
        shared->addTab(new QLabel(QStringLiteral("two")), QStringLiteral("two"));
        settle();
        shared->removeTab(1);
        shared->removeTab(0);
        settle();

        check(window.sharedFolder() == shared.data(), "closing the last file keeps the window's shared folder");
        check(!shared.isNull() && third->folderContainer()->isAncestorOf(shared.data()),
              "the shared folder stays in the current perspective");
    }

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return gFailures == 0 ? 0 : 1;
}
