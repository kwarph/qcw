// Reset Perspective replaces the whole layout in the same event-loop turn as the command.
// The folders it clears are only queued for deletion, and each folder releases its tab panes
// while it is destroyed - until then QcwTabPaneManager still reports their names, so building
// the initial layout right away would call every recreated view "<default> 1". This test runs
// a reset and checks that the rebuilt view answers to its default title, twice in a row.

#include "QcwPerspective.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QTimer>
#include <cstdio>

namespace
{
constexpr QLatin1String ViewType { "PerspectiveResetTestView" };
constexpr QLatin1String WindowId { "perspective-reset-test" };

int gFailures = 0;

void check(bool ok, const char* what, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!ok)
    {
        ++gFailures;
    }
}

class TestView : public QcwView
{
public:
    explicit TestView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
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
        return ViewType;
    }
    QString details() const override
    {
        return {};
    }
    QString defaultWindowTitle() const override
    {
        return ViewType;
    }
    QIcon defaultViewIcon() const override
    {
        return {};
    }
    bool singleInstancePerWindow() const override
    {
        return true;
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new TestView { parent };
    }
};

/// Lays one per-window singleton into its central folder, the way the demo perspectives do.
class ResetPerspective : public QcwPerspective
{
public:
    explicit ResetPerspective(QcwWindow* window) :
        QcwPerspective { window }
    {
    }

    void createInitialLayout() override
    {
        auto* container = folderContainer();
        auto* folder = container->createFolder(nullptr, qcw::LeftDockArea);
        container->setCentralFolder(folder);

        auto viewInfo = getOrCreateView(QString(ViewType));
        if (viewInfo.first)
        {
            folder->addTab(viewInfo.first, viewInfo.first->windowTitle(), viewInfo.second);
        }
    }
};

QcwView* resetView(QcwPerspective* perspective)
{
    auto* folder = perspective->folderContainer()->centralFolder();
    return folder ? qobject_cast<QcwView*>(folder->currentWidget()) : nullptr;
}

int viewCount(const QcwWindow& window)
{
    int count = 0;
    for (auto* view : window.allViews())
    {
        count += view->type() == ViewType ? 1 : 0;
    }

    return count;
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    pluginManager->registerPlugin(new TestViewCreator);

    {
        QcwWindow window;
        window.setId(WindowId);
        window.setupUi();
        window.resize(800, 500);
        window.show();
        QCoreApplication::processEvents();

        auto* perspective = new ResetPerspective { &window };
        perspective->createInitialLayout();
        window.openPerspective(perspective);
        QCoreApplication::processEvents();

        auto* initialView = resetView(perspective);
        check(initialView != nullptr, "initial layout lays out the view");
        check(initialView && initialView->windowTitle() == QString(ViewType), "initial view holds the default title",
              initialView ? initialView->windowTitle() : QString());

        // The command does exactly this, in the same event-loop turn: no processEvents() in between.
        perspective->resetLayout();
        auto* resetViewFirst = resetView(perspective);
        check(resetViewFirst != nullptr, "reset rebuilds the view");
        check(resetViewFirst && resetViewFirst->windowTitle() == QString(ViewType),
              "reset view keeps the default title", resetViewFirst ? resetViewFirst->windowTitle() : QString());
        check(resetViewFirst != nullptr && resetViewFirst != initialView, "reset creates a fresh view");

        perspective->resetLayout();
        auto* resetViewSecond = resetView(perspective);
        check(resetViewSecond && resetViewSecond->windowTitle() == QString(ViewType),
              "a second reset keeps the default title", resetViewSecond ? resetViewSecond->windowTitle() : QString());

        QCoreApplication::processEvents();
        check(viewCount(window) == 1, "only the reset view survives",
              QStringLiteral("views=%1").arg(viewCount(window)));
    }

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    delete pluginManager->removePlugin(QString(ViewType));

    return gFailures == 0 ? 0 : 1;
}

#include "perspective_reset_test.moc"
