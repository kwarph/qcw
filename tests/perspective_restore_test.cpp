// Restores a window saved with two perspectives that both use the shared folder.
//
// Every perspective keeps its folders in its own container, but the shared folder is a single
// widget: only the current perspective's container holds it, and the others keep a site that
// names it. Restoring therefore has to move the folder through the containers while rebuilding
// them, and switching perspectives later keeps moving it. This test checks that the move never
// costs a container its splitter proportions, that a restored non-current perspective does not
// end up visible inside the current one, and that saving after a switch stores the sizes of the
// layout the user last saw rather than of the emptied site.

#include "QcwPerspective.h"
#include "QcwPerspectiveCreator.h"
#include "QcwPluginManager.h"
#include "QcwSerializeUtils.h"
#include "QcwFastViewToolBar.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabFolderSite.h"
#include "QcwTabPaneManager.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QPointer>
#include <QSplitter>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <cstdio>

namespace
{

constexpr QLatin1String ViewType { "PerspectiveRestoreTestView" };
constexpr QLatin1String FloatingType { "PerspectiveRestoreTestFloating" };
constexpr QLatin1String ProjectType { "PerspectiveRestoreTestProject" };
constexpr QLatin1String FocusType { "PerspectiveRestoreTestFocus" };
constexpr QLatin1String WindowId { "perspective-restore-test" };

int gFailures = 0;
int gCloseAsks = 0;
bool gRefuseClose = true;

void check(bool ok, const char* what, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!ok)
    {
        ++gFailures;
    }
}

bool sizesClose(const QList<int>& actual, const QList<int>& expected, int tolerance = 8)
{
    if (actual.size() != expected.size())
    {
        return false;
    }

    for (int i = 0; i < actual.size(); ++i)
    {
        if (qAbs(actual.at(i) - expected.at(i)) > tolerance)
        {
            return false;
        }
    }

    return true;
}

QString sizesText(const QList<int>& sizes)
{
    QStringList parts;
    for (int size : sizes)
    {
        parts.append(QString::number(size));
    }

    return QStringLiteral("(%1)").arg(parts.join(','));
}

void settle()
{
    for (int i = 0; i < 10; ++i)
    {
        QCoreApplication::processEvents();
        QThread::msleep(10);
    }
}

class TestView : public QcwView
{
public:
    explicit TestView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(new QLabel(QStringLiteral("view"), this));
    }
};

/// Answers every close request, refusing while the test says so.
class RefusingView : public TestView
{
public:
    using TestView::TestView;

    void canClose(const std::function<void(bool)>& callback) override
    {
        ++gCloseAsks;
        callback(!gRefuseClose);
    }
};

class TestViewCreator : public QcwViewCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwViewCreator)
public:
    QString type() const override
    {
        return ViewType;
    }
    QString version() const override
    {
        return QStringLiteral("1.0.0");
    }
    QString provider() const override
    {
        return QStringLiteral("test");
    }
    QString updateTime() const override
    {
        return {};
    }
    QString description() const override
    {
        return QStringLiteral("test view");
    }
    QString details() const override
    {
        return {};
    }
    QString defaultWindowTitle() const override
    {
        return QStringLiteral("Test View");
    }
    QIcon defaultViewIcon() const override
    {
        return {};
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new TestView { parent };
    }
};

class FloatingViewCreator : public TestViewCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwViewCreator)
public:
    QString type() const override
    {
        return FloatingType;
    }
    QString defaultWindowTitle() const override
    {
        return QStringLiteral("Floating View");
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new RefusingView { parent };
    }
};

class TestPerspective : public QcwPerspective
{
public:
    enum class Kind
    {
        Project,
        Focus
    };

    TestPerspective(QcwWindow* window, Kind kind) :
        QcwPerspective { window }, mKind { kind }
    {
    }

    void createInitialLayout() override
    {
        auto container = folderContainer();
        auto central = window()->sharedFolder();
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
        addView(mLeft, QStringLiteral("Project"));

        if (mKind == Kind::Project)
        {
            mBottom = container->createFolder(central, qcw::BottomDockArea);
            container->setEmptyFolderAction(mBottom, qcw::EmptyFolderAction::Keep);
            addView(mBottom, QStringLiteral("Log"));
        }
    }

    Kind kind() const
    {
        return mKind;
    }

    QcwTabFolder* leftFolder() const
    {
        return mLeft;
    }

private:
    static void addView(QcwTabFolder* folder, const QString& title)
    {
        auto creator = pluginManager->getPluginT<QcwViewCreator>(ViewType);
        if (!folder || !creator)
        {
            return;
        }

        if (auto view = creator->create(folder, true))
        {
            folder->addTab(view, title, creator->defaultViewIcon());
        }
    }

    Kind mKind;
    QcwTabFolder* mLeft { nullptr };
    QcwTabFolder* mBottom { nullptr };
};

class TestPerspectiveCreator : public QcwPerspectiveCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwPerspectiveCreator)
public:
    TestPerspectiveCreator(const QString& type, const QString& name, TestPerspective::Kind kind) :
        mType { type }, mName { name }, mKind { kind }
    {
    }

    QString type() const override
    {
        return mType;
    }
    QString version() const override
    {
        return QStringLiteral("1.0.0");
    }
    QString provider() const override
    {
        return QStringLiteral("test");
    }
    QString updateTime() const override
    {
        return {};
    }
    QString description() const override
    {
        return mName;
    }
    QString details() const override
    {
        return {};
    }
    QString defaultName() const override
    {
        return mName;
    }
    QIcon defaultPerspectiveIcon() const override
    {
        return {};
    }

protected:
    QcwPerspective* doCreate(QcwWindow* window) override
    {
        return new TestPerspective { window, mKind };
    }

private:
    QString mType;
    QString mName;
    TestPerspective::Kind mKind;
};

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

QcwPerspective* perspectiveOfType(QcwWindow& window, const QString& type)
{
    for (auto perspective : window.allPerspectives())
    {
        if (perspective && perspective->type() == type)
        {
            return perspective;
        }
    }

    return nullptr;
}

/// Writes @p source with the Focus splitters set to zeroes, the way the old restore path
/// saved a hidden perspective after the shared folder had left its site.
bool writeCorruptedFocusState(const QString& source, const QString& target)
{
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QString xml = QString::fromUtf8(in.readAll());
    const int focus = xml.indexOf(QStringLiteral("type=\"%1\"").arg(FocusType));
    const int sizesElement = focus < 0 ? -1 : xml.indexOf(QStringLiteral("<splitterSizes>"), focus);
    const int attribute = sizesElement < 0 ? -1 : xml.indexOf(QStringLiteral("sizes=\""), sizesElement);
    if (attribute < 0)
    {
        return false;
    }

    const QString marker = QStringLiteral("sizes=\"");
    const int valueStart = attribute + marker.size();
    const int valueEnd = xml.indexOf(QLatin1Char('"'), valueStart);
    if (valueEnd < 0)
    {
        return false;
    }

    const QByteArray zeroSizes = QcwSerializeUtils::serialize(QList<int> { 0, 0 }).toHex(' ');
    xml.replace(valueStart, valueEnd - valueStart, QString::fromLatin1(zeroSizes));

    QFile out(target);
    const QByteArray data = xml.toUtf8();
    return out.open(QIODevice::WriteOnly) && out.write(data) == data.size();
}

/// Writes @p source without its sharedFolder element, the way a session that had lost the
/// window's shared folder saved its state.
bool writeStateWithoutSharedFolder(const QString& source, const QString& target)
{
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QString xml = QString::fromUtf8(in.readAll());
    const int start = xml.indexOf(QStringLiteral("<sharedFolder"));
    const QString closing = QStringLiteral("</sharedFolder>");
    const int end = start < 0 ? -1 : xml.indexOf(closing, start);
    if (start < 0 || end < 0)
    {
        return false;
    }

    xml.remove(start, end + closing.size() - start);

    QFile out(target);
    const QByteArray data = xml.toUtf8();
    return out.open(QIODevice::WriteOnly) && out.write(data) == data.size();
}

/// Writes @p source cut off inside the perspectives element, after the views, the shared
/// folder and the dummy perspective have been written: a document that fails only once
/// restoring is well under way.
bool writeTruncatedState(const QString& source, const QString& target)
{
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly))
    {
        return false;
    }

    const QByteArray xml = in.readAll();
    const int start = xml.indexOf("<perspectives");
    if (start < 0)
    {
        return false;
    }

    QFile out(target);
    const QByteArray data = xml.left(start + QByteArrayLiteral("<perspectives").size());
    return out.open(QIODevice::WriteOnly) && out.write(data) == data.size();
}

bool sharedFolderIsHostedBy(QcwTabFolder* shared, QcwPerspective* perspective)
{
    if (!shared || !perspective || !perspective->folderContainer()->isVisible())
    {
        return false;
    }

    auto site = qobject_cast<QcwTabFolderSite*>(shared->parentWidget());
    return site && site->folder() == shared && perspective->folderContainer()->isAncestorOf(shared);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        std::fprintf(stderr, "cannot create the temporary directory\n");
        return 1;
    }

    pluginManager->registerPlugin(new TestViewCreator);
    pluginManager->registerPlugin(new FloatingViewCreator);
    TestPerspectiveCreator* projectCreator =
        new TestPerspectiveCreator(ProjectType, QStringLiteral("Project"), TestPerspective::Kind::Project);
    TestPerspectiveCreator* focusCreator =
        new TestPerspectiveCreator(FocusType, QStringLiteral("Focus"), TestPerspective::Kind::Focus);
    pluginManager->registerPlugin(projectCreator);
    pluginManager->registerPlugin(focusCreator);

    // The manager owns nothing; these test plugins are ours to delete, on every exit path.
    const auto removeTestPlugins = []()
    {
        delete pluginManager->removePlugin(QString(ViewType));
        delete pluginManager->removePlugin(QString(FloatingType));
        delete pluginManager->removePlugin(QString(ProjectType));
        delete pluginManager->removePlugin(QString(FocusType));
    };

    const QString stateFile = tempDir.filePath(QStringLiteral("window.xml"));

    QList<int> projectRootSizes;
    QList<int> projectNestedSizes;
    QList<int> focusRootSizes;
    int projectId = 0;
    int focusId = 0;

    {
        TestWindow window;
        window.setId(WindowId);
        window.setupUi();
        window.resize(1000, 720);
        window.show();
        settle();

        auto project = projectCreator->create(&window);
        project->createInitialLayout();
        window.openPerspective(project);
        settle();

        auto container = project->folderContainer();
        auto root = container->rootSplitter();
        auto nested = container->parentSplitter(window.sharedFolder());
        check(root != nullptr && nested != nullptr, "source layout has its splitters");
        if (!root || !nested)
        {
            removeTestPlugins();
            return 1;
        }

        root->setSizes({ 260, 740 });
        nested->setSizes({ 520, 220 });
        settle();
        projectRootSizes = root->sizes();
        projectNestedSizes = nested->sizes();
        projectId = project->id();

        auto focus = focusCreator->create(&window);
        focus->createInitialLayout();
        window.openPerspective(focus);
        settle();

        auto focusRoot = focus->folderContainer()->rootSplitter();
        check(focusRoot != nullptr, "focus layout has its splitter");
        if (!focusRoot)
        {
            removeTestPlugins();
            return 1;
        }

        focusRoot->setSizes({ 260, 740 });
        settle();
        focusRootSizes = focusRoot->sizes();
        focusId = focus->id();

        // Saving with Project current is the state the bug report starts from: Focus stays in
        // the document, but nothing of it is on screen.
        window.setCurrentPerspective(projectId);
        settle();

        check(root->sizes() == projectRootSizes, "project sizes survive the round trip",
              QStringLiteral("root=%1 expected=%2").arg(sizesText(root->sizes()), sizesText(projectRootSizes)));
        check(window.saveStateToFile(stateFile), "state file written");
    }

    {
        TestWindow window;
        const bool restored = window.restoreStateFromFile(stateFile);
        // The offscreen platform clamps a restored geometry to its screen, so put the source
        // size back: the splitter sizes are pixels and the comparison needs the same width.
        window.resize(1000, 720);
        window.show();
        settle();

        check(restored, "state file restored");
        auto project = perspectiveOfType(window, ProjectType);
        auto focus = perspectiveOfType(window, FocusType);
        check(project != nullptr && focus != nullptr, "both perspectives came back");
        if (!restored || !project || !focus)
        {
            removeTestPlugins();
            return 1;
        }

        check(window.currentPerspective() == project, "project is the current perspective");
        check(project->id() == projectId && focus->id() == focusId, "perspective ids are kept");

        QcwTabFolder* shared = window.sharedFolder();
        check(shared != nullptr && shared->isVisible(), "the shared folder is visible");
        check(sharedFolderIsHostedBy(shared, project), "the shared folder sits in the project container");

        // A restored perspective that is not current must stay hidden. Its container is a
        // sibling of the current one under centralWidget; left visible, it paints over the
        // current perspective at a default geometry.
        bool hiddenOk = true;
        for (auto perspective : window.allPerspectives())
        {
            if (perspective != project && perspective->folderContainer()->isVisible())
            {
                hiddenOk = false;
            }
        }
        check(hiddenOk, "non-current perspective containers stay hidden");

        check(project->folderContainer()->folders().size() == 3, "project holds its three folders",
              QString::number(project->folderContainer()->folders().size()));
        check(focus->folderContainer()->folders().size() == 2, "focus holds its two folders",
              QString::number(focus->folderContainer()->folders().size()));

        auto projectRoot = project->folderContainer()->rootSplitter();
        auto projectNested = project->folderContainer()->parentSplitter(shared);
        check(projectNested != nullptr, "the shared folder has a splitter again");
        if (projectNested)
        {
            check(sizesClose(projectRoot->sizes(), projectRootSizes), "project root proportions restored",
                  QStringLiteral("actual=%1 expected=%2")
                      .arg(sizesText(projectRoot->sizes()), sizesText(projectRootSizes)));
            check(sizesClose(projectNested->sizes(), projectNestedSizes), "project folder proportions restored",
                  QStringLiteral("actual=%1 expected=%2")
                      .arg(sizesText(projectNested->sizes()), sizesText(projectNestedSizes)));
        }

        check(shared->height() > 100, "the shared folder is not collapsed",
              QStringLiteral("size=%1x%2").arg(shared->width()).arg(shared->height()));

        window.setCurrentPerspective(focusId);
        settle();
        check(sharedFolderIsHostedBy(shared, focus), "the shared folder follows the switch to focus");
        check(sizesClose(focus->folderContainer()->rootSplitter()->sizes(), focusRootSizes),
              "focus keeps its proportions",
              QStringLiteral("actual=%1 expected=%2")
                  .arg(sizesText(focus->folderContainer()->rootSplitter()->sizes()), sizesText(focusRootSizes)));
        check(!project->folderContainer()->isVisible(), "project hides on the switch");

        window.setCurrentPerspective(projectId);
        settle();
        check(sharedFolderIsHostedBy(shared, project), "the shared folder follows the switch back");
        check(
            sizesClose(projectRoot->sizes(), projectRootSizes), "project keeps its proportions after two switches",
            QStringLiteral("actual=%1 expected=%2").arg(sizesText(projectRoot->sizes()), sizesText(projectRootSizes)));
        if (projectNested)
        {
            check(sizesClose(projectNested->sizes(), projectNestedSizes),
                  "project folders keep their proportions after two switches",
                  QStringLiteral("actual=%1 expected=%2")
                      .arg(sizesText(projectNested->sizes()), sizesText(projectNestedSizes)));
        }

        // Saving while Project is current must describe both perspectives as the user last saw
        // them, not the emptied site the shared folder left behind in Focus.
        const QString secondFile = tempDir.filePath(QStringLiteral("window2.xml"));
        check(window.saveStateToFile(secondFile), "state file written after the switches");

        TestWindow reread;
        const bool rereadOk = reread.restoreStateFromFile(secondFile);
        reread.resize(1000, 720);
        reread.show();
        settle();
        check(rereadOk, "the second state file restores");

        auto rereadProject = perspectiveOfType(reread, ProjectType);
        auto rereadFocus = perspectiveOfType(reread, FocusType);
        if (rereadOk && rereadProject && rereadFocus)
        {
            QcwTabFolder* rereadShared = reread.sharedFolder();
            auto rereadProjectNested = rereadProject->folderContainer()->parentSplitter(rereadShared);
            check(rereadProjectNested && sizesClose(rereadProjectNested->sizes(), projectNestedSizes),
                  "project folder proportions survive a save after switching",
                  rereadProjectNested ?
                      QStringLiteral("actual=%1 expected=%2")
                          .arg(sizesText(rereadProjectNested->sizes()), sizesText(projectNestedSizes)) :
                      QStringLiteral("no nested splitter"));

            // Focus was hidden when the file was written; its live splitter says nothing until
            // the perspective is shown and applies what was saved for it.
            reread.setCurrentPerspective(rereadFocus->id());
            settle();
            auto rereadFocusRoot = rereadFocus->folderContainer()->rootSplitter();
            check(sizesClose(rereadFocusRoot->sizes(), focusRootSizes),
                  "focus proportions survive a save after switching",
                  QStringLiteral("actual=%1 expected=%2")
                      .arg(sizesText(rereadFocusRoot->sizes()), sizesText(focusRootSizes)));
        }

        // A document written by the old restore path can carry a zero size for a live site of a
        // hidden perspective: the shared folder had left that site when the window was saved, so
        // the splitter measured it as empty. Restoring those numbers would collapse the
        // perspective into a column of nothing, so the document has to be treated as unset.
        const QString corruptFile = tempDir.filePath(QStringLiteral("window-corrupt.xml"));
        check(writeCorruptedFocusState(stateFile, corruptFile), "corrupted state file written");

        TestWindow healed;
        const bool healedRestored = healed.restoreStateFromFile(corruptFile);
        healed.resize(1000, 720);
        healed.show();
        settle();
        check(healedRestored, "the corrupted state file still restores");

        auto healedFocus = perspectiveOfType(healed, FocusType);
        check(healedFocus != nullptr, "the corrupted file keeps the focus perspective");
        if (healedRestored && healedFocus)
        {
            healed.setCurrentPerspective(healedFocus->id());
            settle();

            QcwTabFolder* healedShared = healed.sharedFolder();
            check(sharedFolderIsHostedBy(healedShared, healedFocus),
                  "the healed file hosts the shared folder in focus");
            check(healedShared && healedShared->width() > 100 && healedShared->height() > 100,
                  "the healed file does not collapse the central folder",
                  healedShared ? QStringLiteral("size=%1x%2").arg(healedShared->width()).arg(healedShared->height()) :
                                 QStringLiteral("no shared folder"));

            const QString healedFile = tempDir.filePath(QStringLiteral("window-healed.xml"));
            check(healed.saveStateToFile(healedFile), "the healed file can be saved again");
        }

        if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
        {
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
        }

        app.exec();
    }

    // A state file can have lost the window's shared folder: older builds deleted it when a
    // closed perspective's container saw it emptied. A window restored from such a file must
    // still have one, or every editor would dock into the folder of the view that opened it.
    {
        const QString damagedFile = tempDir.filePath(QStringLiteral("window-no-shared-folder.xml"));
        check(writeStateWithoutSharedFolder(stateFile, damagedFile), "state file without a shared folder written");

        TestWindow window;
        const bool restored = window.restoreStateFromFile(damagedFile);
        window.resize(1000, 720);
        window.show();
        settle();
        check(restored, "the file without a shared folder still restores");

        QcwTabFolder* shared = window.sharedFolder();
        check(shared != nullptr, "the window rebuilds the shared folder such a file lacks");

        if (restored && shared)
        {
            auto project = projectCreator->create(&window);
            project->createInitialLayout();
            window.openPerspective(project);
            settle();
            check(sharedFolderIsHostedBy(shared, project),
                  "a perspective opened later adopts the rebuilt shared folder");
        }
    }

    // A session can be saved while the shared folder is minimized. Restoring that state would
    // park the one shared widget in a fast view belonging to a single perspective, which can
    // only be hidden or shown with it; the restored window therefore starts with the folder
    // back in its site.
    {
        const QString minimizedFile = tempDir.filePath(QStringLiteral("window-minimized-shared.xml"));
        {
            TestWindow window;
            window.setId(QStringLiteral("perspective-restore-minimized"));
            window.setupUi();
            window.resize(1000, 720);

            auto project = projectCreator->create(&window);
            project->createInitialLayout();
            window.openPerspective(project);
            window.show();
            settle();

            QcwTabFolder* shared = window.sharedFolder();
            check(shared != nullptr, "the session has a shared folder to minimize");
            if (shared)
            {
                shared->minimize();
                settle();
                check(shared->state() == qcw::FolderState::Minimized,
                      "the shared folder is minimized for the save");
            }

            check(window.saveStateToFile(minimizedFile), "state file with a minimized shared folder written");
        }

        TestWindow window;
        const bool restored = window.restoreStateFromFile(minimizedFile);
        window.resize(1000, 720);
        window.show();
        settle();

        // The released fast view is only scheduled for deletion by its Restore path.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

        auto project = perspectiveOfType(window, ProjectType);
        QcwTabFolder* shared = window.sharedFolder();
        check(restored && project && shared, "the minimized session restores");
        if (restored && project && shared)
        {
            check(shared->state() == qcw::FolderState::Normal, "the restored shared folder is normal");
            check(shared->features().testFlag(qcw::Minimizable),
                  "the restored shared folder can be minimized again");
            check(window.currentPerspective() == project, "the saved current perspective is current again");
            check(project->folderContainer()->isAncestorOf(shared), "the restored shared folder is hosted by it");
            check(window.findChildren<QcwFastViewToolBar*>().isEmpty(),
                  "no fast view is left over from the saved minimized state");
        }
    }

    // A floating folder is saved as part of its perspective and rebuilt by restoreState(), not
    // by the container, so nothing announces it to the close protocol. Its close button must
    // still ask the view first, and a refusal must leave both open.
    {
        const QString floatingFile = tempDir.filePath(QStringLiteral("window-floating.xml"));
        {
            TestWindow window;
            window.setId(QStringLiteral("perspective-restore-floating"));
            window.setupUi();
            window.resize(1000, 720);
            window.show();
            settle();

            auto project = projectCreator->create(&window);
            project->createInitialLayout();
            window.openPerspective(project);
            settle();

            auto floating = window.createDetachedView(QString(FloatingType), QStringLiteral("Floating"));
            check(floating != nullptr, "the session has a floating folder to save");
            if (floating)
            {
                floating->resize(320, 240);
                floating->show();
                settle();
            }

            check(window.saveStateToFile(floatingFile), "state file with a floating folder written");
        }

        TestWindow window;
        const bool restored = window.restoreStateFromFile(floatingFile);
        window.resize(1000, 720);
        window.show();
        settle();

        auto project = perspectiveOfType(window, ProjectType);
        check(restored && project, "the floating session restores");
        if (restored && project)
        {
            const auto floatingFolders = project->floatingFolders();
            auto floating = floatingFolders.isEmpty() ? nullptr : *floatingFolders.constBegin();
            check(floating != nullptr, "the floating folder comes back",
                  QString::number(floatingFolders.size()));

            if (floating)
            {
                QPointer<QcwTabFolder> folderGuard { floating };
                QPointer<QcwView> viewGuard { qobject_cast<QcwView*>(floating->tabPane(0)) };
                check(viewGuard != nullptr, "the restored floating folder shows its view");

                gCloseAsks = 0;
                floating->close();
                settle();
                check(gCloseAsks == 1, "the restored floating folder's close asks its view",
                      QStringLiteral("asked=%1").arg(gCloseAsks));
                check(!folderGuard.isNull() && !viewGuard.isNull(),
                      "a refusal keeps the restored floating folder open");

                gRefuseClose = false;
                if (!folderGuard.isNull())
                {
                    folderGuard->close();
                }
                for (int i = 0; i < 5; ++i)
                {
                    QCoreApplication::processEvents();
                    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
                }
                gRefuseClose = true;

                check(gCloseAsks == 2, "the allowed close asks the view once more",
                      QStringLiteral("asked=%1").arg(gCloseAsks));
                check(folderGuard.isNull(), "the allowed close takes the restored floating folder");
                check(viewGuard.isNull(), "and the view it held");
            }
        }
    }

    // A document that breaks off while the perspectives are being read has already produced
    // views, a shared folder and the dummy perspective. They must all go, and the window must
    // still be able to build its own layout afterwards.
    {
        const QString truncatedFile = tempDir.filePath(QStringLiteral("window-truncated.xml"));
        check(writeTruncatedState(stateFile, truncatedFile), "truncated state file written");

        TestWindow window;
        const bool restored = window.restoreStateFromFile(truncatedFile);

        // The failed attempt schedules everything it built for deletion; purge it before the
        // window is set up, the way an event loop would.
        for (int i = 0; i < 5; ++i)
        {
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }

        check(!restored, "a truncated document is refused");
        check(!window.isRestoredFromState(), "a refused restore is not a restored one");
        check(window.sharedFolder() == nullptr, "the refused restore leaves no shared folder behind");
        check(window.allPerspectives().isEmpty(), "the refused restore leaves no perspective behind");
        check(window.allViews().isEmpty(), "the refused restore leaves no view behind");
        check(tabPaneManager->allTabPanes().isEmpty(), "the refused restore leaves no tab pane behind");

        window.setupUi();
        window.resize(1000, 720);
        window.show();
        settle();
        check(window.sharedFolder() != nullptr, "the window still builds its own shared folder");
        check(window.currentPerspective() != nullptr, "the dummy perspective comes up after the refusal");

        auto focus = focusCreator->create(&window);
        focus->createInitialLayout();
        window.openPerspective(focus);
        settle();
        check(window.currentPerspective() == focus && window.allViews().size() == 1,
              "a perspective opened after the refused restore works",
              QStringLiteral("views=%1").arg(window.allViews().size()));
    }

    // A session that ended with every perspective closed saves just the dummy one, and that
    // document looks exactly like a fresh window: one dummy perspective, current. An
    // application that builds its default layout in postSetupUi() needs isRestoredFromState()
    // to leave it alone, or closing every perspective would not survive a restart.
    {
        const QString dummyFile = tempDir.filePath(QStringLiteral("window-dummy-only.xml"));
        {
            TestWindow window;
            window.setId(WindowId);
            window.setupUi();
            window.resize(1000, 720);
            window.show();
            settle();
            check(!window.isRestoredFromState(), "a window built by setupUi() is not a restored one");
            check(window.saveStateToFile(dummyFile), "dummy-only state file written");
        }

        TestWindow window;
        const bool restored = window.restoreStateFromFile(dummyFile);
        window.resize(1000, 720);
        window.show();
        settle();
        check(restored, "the dummy-only state file restores");
        check(window.isRestoredFromState(), "a restored window reports itself as restored");
        check(window.allPerspectives().size() == 1, "only the dummy perspective comes back");
        check(window.currentPerspective() && window.currentPerspective()->type() == QLatin1String("DummyPerspective"),
              "the dummy perspective is the current one");
    }

    removeTestPlugins();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}

#include "perspective_restore_test.moc"
