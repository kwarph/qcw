// Checks the per-window singleton rule for view creators: the New View menu does not offer the
// type, createDetachedView() refuses a second instance, getOrCreateView() returns the window's
// existing view regardless of the title asked for, and a saved duplicate entry is dropped when
// the window restores.

#include "QcwPerspective.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

namespace
{
constexpr QLatin1String SingletonType { "SingletonTestView" };
constexpr QLatin1String PlainType { "PlainTestView" };

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
    explicit TestViewCreator(const QString& type, bool singleInstance) :
        mType { type }, mSingleInstance { singleInstance }
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
        return mType;
    }
    QString details() const override
    {
        return {};
    }
    QString defaultWindowTitle() const override
    {
        return mType;
    }
    QIcon defaultViewIcon() const override
    {
        return {};
    }
    bool singleInstancePerWindow() const override
    {
        return mSingleInstance;
    }

protected:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>&) override
    {
        return new TestView { parent };
    }

private:
    QString mType;
    bool mSingleInstance;
};

/// Exposes the protected lookup QcwPerspective offers to a layout.
class TestPerspective : public QcwPerspective
{
public:
    explicit TestPerspective(QcwWindow* window) :
        QcwPerspective { window }
    {
    }

    QPair<QcwView*, QIcon> viewOf(const QString& type, const QString& name)
    {
        return getOrCreateView(type, name);
    }
};

int viewCount(const QcwWindow& window, const QString& type)
{
    int count = 0;
    for (auto view : window.allViews())
    {
        if (view->type() == type)
        {
            ++count;
        }
    }

    return count;
}

QString viewTitle(const QcwWindow& window, const QString& type)
{
    for (auto view : window.allViews())
    {
        if (view->type() == type)
        {
            return view->windowTitle();
        }
    }

    return {};
}

bool duplicateFirstViewElement(QString& xml)
{
    const int viewsEnd = xml.indexOf(QStringLiteral("</views>"));
    const int viewStart = xml.indexOf(QStringLiteral("<view "));
    const int viewEnd = viewStart < 0 ? -1 : xml.indexOf(QStringLiteral("/>"), viewStart);

    if (viewsEnd < 0 || viewStart < 0 || viewEnd < 0 || viewEnd > viewsEnd)
    {
        return false;
    }

    xml.insert(viewsEnd, xml.mid(viewStart, viewEnd + 2 - viewStart));
    return true;
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    pluginManager->registerPlugin(new TestViewCreator(QString(SingletonType), true));
    pluginManager->registerPlugin(new TestViewCreator(QString(PlainType), false));

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        std::fprintf(stderr, "cannot create the temporary directory\n");
        return 1;
    }
    const QString stateFile = tempDir.filePath(QStringLiteral("singleton-window.xml"));

    bool menuOk = false;
    bool detachedRefused = false;
    bool reused = false;
    bool saved = false;

    {
        QcwWindow window;
        window.setId("singleton-view-test");
        window.setupUi();
        window.resize(800, 500);
        window.show();
        QCoreApplication::processEvents();

        bool offersSingleton = false;
        bool offersPlain = false;
        if (auto newViewMenu = window.getMenu(QcwWindowActionIds::MenuNewView))
        {
            for (auto* action : newViewMenu->actions())
            {
                offersSingleton = offersSingleton || action->text() == SingletonType;
                offersPlain = offersPlain || action->text() == PlainType;
            }
        }
        menuOk = !offersSingleton && offersPlain;

        auto firstFolder = window.createDetachedView(QString(SingletonType), QStringLiteral("Detached One"));
        auto secondFolder = window.createDetachedView(QString(SingletonType), QStringLiteral("Detached Two"));
        detachedRefused =
            firstFolder != nullptr && secondFolder == nullptr && viewCount(window, QString(SingletonType)) == 1;

        if (firstFolder)
        {
            firstFolder->close(); // the folder is top-level and deletes itself on close
            QCoreApplication::processEvents();
        }
    }

    {
        QcwWindow window;
        window.setId("singleton-view-test-restore");
        window.setupUi();
        window.resize(800, 500);
        window.show();
        QCoreApplication::processEvents();

        TestPerspective perspective { &window };
        auto first = perspective.viewOf(QString(SingletonType), QStringLiteral("Alpha"));
        if (first.first)
        {
            window.currentPerspective()->folderContainer()->centralFolder()->addTab(first.first,
                                                                                    QStringLiteral("Alpha"));
        }
        QCoreApplication::processEvents();

        // The title is an identity only for non-singletons; a singleton answers by type.
        auto second = perspective.viewOf(QString(SingletonType), QStringLiteral("Beta"));
        reused = first.first && second.first == first.first && viewCount(window, QString(SingletonType)) == 1;

        saved = window.saveStateToFile(stateFile);
        std::fprintf(stderr, "reuse: first=%p second=%p views=%d saved=%d\n", static_cast<void*>(first.first),
                     static_cast<void*>(second.first), viewCount(window, QString(SingletonType)), saved);
    }

    QFile state(stateFile);
    QString xml;
    if (state.open(QIODevice::ReadOnly))
    {
        xml = QString::fromUtf8(state.readAll());
        state.close();
    }
    const bool duplicated = duplicateFirstViewElement(xml);
    if (duplicated && state.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        state.write(xml.toUtf8());
        state.close();
    }

    bool restored = false;
    bool deduplicated = false;
    int restoredViews = 0;
    {
        QcwWindow restoredWindow;
        restored = duplicated && restoredWindow.restoreStateFromFile(stateFile);
        QCoreApplication::processEvents();
        restoredViews = viewCount(restoredWindow, QString(SingletonType));
        deduplicated = restored && restoredViews == 1 &&
                       viewTitle(restoredWindow, QString(SingletonType)) == QStringLiteral("Alpha");
    }

    std::fprintf(stderr, "menu=%d detached=%d reused=%d duplicated=%d restored=%d deduplicated=%d views=%d\n", menuOk,
                 detachedRefused, reused, duplicated, restored, deduplicated, restoredViews);

    const bool allOk = menuOk && detachedRefused && reused && saved && deduplicated;

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    // The manager owns nothing; these test creators are ours to delete.
    delete pluginManager->removePlugin(QString(SingletonType));
    delete pluginManager->removePlugin(QString(PlainType));

    return allOk ? 0 : 1;
}

#include "singleton_view_test.moc"
