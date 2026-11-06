// The shared folder belongs to the window that created it, not to the container that happens
// to host it. Closing its last tab may therefore never delete it, even though the container's
// default empty-folder action is Delete - the window starts with an empty shared folder, so
// that state is ordinary. Hiding it is still the container's business. This test docks the
// shared folder beside the central one, empties it, and checks that it survives; then it hides
// the emptied folder, closes the perspective, and checks that a later perspective re-adopts it.

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
constexpr QLatin1String WindowId { "shared-folder-ownership-test" };

int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

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

/// Docks the window's shared folder on the right and keeps a folder of its own as the centre.
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
        QcwTabFolder* shared = window()->sharedFolder();
        if (shared)
        {
            container->addSharedFolder(shared, nullptr, qcw::RightDockArea);
        }

        auto* central = container->createFolder(nullptr, qcw::LeftDockArea);
        container->setCentralFolder(central);
        container->setEmptyFolderAction(central, qcw::EmptyFolderAction::Keep);
        central->addTab(new QLabel(QStringLiteral("Project")), QStringLiteral("Project"));
    }
};

bool sharedIsHostedBy(QcwTabFolder* shared, QcwPerspective* perspective)
{
    return shared && perspective && perspective->folderContainer()->isAncestorOf(shared);
}

void addAndRemoveTab(QcwTabFolder* folder)
{
    folder->addTab(new QLabel(QStringLiteral("editor")), QStringLiteral("editor"));
    settle();
    folder->removeTab(0);
    settle();
}
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
        auto* container = first->folderContainer();
        check(sharedIsHostedBy(shared, first), "the perspective hosts the shared folder");

        // The shared folder keeps the container's default Delete action here: the created
        // folder is not the central one, so nothing exempts it by accident.
        check(container->emptyFolderAction(shared) == qcw::EmptyFolderAction::Delete,
              "the shared folder has the default Delete action");

        addAndRemoveTab(shared);
        check(!shared.isNull() && window.sharedFolder() == shared.data(),
              "emptying the shared folder does not delete it");
        check(sharedIsHostedBy(shared, first), "the emptied shared folder stays in the layout");
        check(shared && shared->isVisible(), "the emptied shared folder stays visible");

        container->setEmptyFolderAction(shared, qcw::EmptyFolderAction::Hide);
        addAndRemoveTab(shared);
        check(!shared.isNull() && window.sharedFolder() == shared.data(),
              "an emptied shared folder with the Hide action still exists");
        check(shared && !shared->isVisible(), "the Hide action hides the shared folder");

        // The dummy perspective stays and hosts the shared folder, so it is back on screen.
        window.closePerspective(firstId);
        settle();
        check(!shared.isNull() && window.sharedFolder() == shared.data(),
              "the shared folder survives its last real perspective");
        check(shared && shared->isVisible(), "the dummy perspective takes the shared folder over");

        auto* second = new TestPerspective { &window };
        second->createInitialLayout();
        window.openPerspective(second);
        settle();
        check(sharedIsHostedBy(shared, second), "a later perspective re-adopts the shared folder");
        check(shared && shared->isVisible(), "the re-adopted shared folder is visible again");
    }

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return gFailures == 0 ? 0 : 1;
}
