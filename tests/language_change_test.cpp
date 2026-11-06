// Switching the UI language re-texts what a widget baked at construction time.
//
// A string resolved once is a QString, not a lookup, so a class only follows the language when it
// re-applies its texts on QEvent::LanguageChange - which Qt delivers to every widget, recursively,
// hidden ones included. What that delivery cannot reach is a value the framework *stores*: a tab's
// text is the title the view's creator handed out at the time the tab was made, and a saved session
// writes titles as literals. Those re-resolve through QcwView::defaultTitle(), the baseline a tab is
// compared against before it is rewritten, so an untouched tab follows the language and a renamed one
// does not. This test swaps the language with a translator that answers in capitals instead of a
// catalog, and checks the three cases that decide the feature: delivery reaches a view nested in a
// folder of a window, an automatic title (with or without its ordinal) re-resolves while a rename
// stays, and the baseline survives a save and a restore. A tab a plugin opened by itself, with no
// perspective call to record its baseline, is in the layout too, because the creator is what stamps it.

#include "QcwPerspective.h"
#include "QcwPerspectiveCreator.h"
#include "QcwPluginManager.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>
#include <cstdio>

namespace
{
constexpr QLatin1String ViewType { "LanguageChangeTestView" };
constexpr QLatin1String PerspectiveType { "LanguageChangeTestPerspective" };
constexpr QLatin1String WindowId { "language-change-test" };

// Every translatable string of this test lives in one context, which is how the translator below
// tells them from Qt's own.
constexpr const char* Context { "QcwLanguageChangeTest" };

int gFailures = 0;
int gViewLanguageChanges = 0;

void check(bool ok, const char* what, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!ok)
    {
        ++gFailures;
    }
}

QString caption(const char* text)
{
    return QCoreApplication::translate(Context, text);
}

/// Answers the test's own strings in capitals, so a language change is visible without a catalog.
class CapsTranslator : public QTranslator
{
public:
    // installTranslator() sends no LanguageChange at all when it believes a translator holds
    // nothing, and one with no .qm behind it is empty by default.
    bool isEmpty() const override
    {
        return false;
    }

    QString translate(const char* context, const char* sourceText, const char* = nullptr, int = -1) const override
    {
        if (!context || !sourceText || qstrcmp(context, Context) != 0)
        {
            return {}; // null: fall through to the source text
        }

        return QString::fromUtf8(sourceText).toUpper();
    }
};

/// A view with one label whose text is baked at construction, the way an application's view has.
class TestView : public QcwView
{
public:
    explicit TestView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
        details = new QLabel { caption("Details"), this };
    }

    QLabel* details { nullptr };

protected:
    void changeEvent(QEvent* event) override
    {
        if (event->type() == QEvent::LanguageChange)
        {
            ++gViewLanguageChanges;
            details->setText(caption("Details"));
        }

        QcwView::changeEvent(event);
    }
};

// Not a per-window singleton: the test needs three views of one type to tell an automatic title,
// an automatic title with an ordinal, and a rename apart.
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
        return caption("Notes");
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

/// Lays three tabs of one creator into the central folder.
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
        auto* folder = container->createFolder(nullptr, qcw::LeftDockArea);
        container->setCentralFolder(folder);

        for (int i = 0; i < 3; ++i)
        {
            auto viewInfo = getOrCreateView(QString(ViewType));
            if (viewInfo.first)
            {
                folder->addTab(viewInfo.first, viewInfo.first->windowTitle(), viewInfo.second);
            }
        }

        // The route a plugin takes when it opens a view of its own: the creator and addTab(), with
        // no getOrCreateView() in between. The tab still follows the language, because it is the
        // creator that records the baseline, not the perspective.
        auto* creator = pluginManager->getPluginT<QcwViewCreator>(QString(ViewType));
        auto* made = creator ? creator->create(folder, true) : nullptr;
        if (made)
        {
            folder->addTab(made, QcwWindow::makeNewViewName(creator->defaultWindowTitle()), {});
        }
    }
};

/// Hosts the view another perspective already laid out — the window's shared folder, in the demo.
class SharingPerspective : public QcwPerspective
{
public:
    using QcwPerspective::QcwPerspective;

    void createInitialLayout() override
    {
        ownFolder = folderContainer()->createFolder(nullptr, qcw::LeftDockArea);
        folderContainer()->setCentralFolder(ownFolder);

        auto viewInfo = getOrCreateView(QString(ViewType));
        if (ownFolder && viewInfo.first)
        {
            ownFolder->addTab(viewInfo.first, viewInfo.first->windowTitle(), viewInfo.second);
        }
    }

    QcwTabFolder* ownFolder { nullptr };
};

class TestPerspectiveCreator : public QcwPerspectiveCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwPerspectiveCreator)
public:
    QString type() const override
    {
        return PerspectiveType;
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
        return PerspectiveType;
    }
    QString details() const override
    {
        return {};
    }
    QString defaultName() const override
    {
        return caption("Layout");
    }
    QIcon defaultPerspectiveIcon() const override
    {
        return {};
    }

protected:
    QcwPerspective* doCreate(QcwWindow* window) override
    {
        return new TestPerspective { window };
    }
};

QcwTabFolder* centralFolder(QcwWindow& window)
{
    auto* perspective = window.currentPerspective();
    return perspective && perspective->folderContainer() ? perspective->folderContainer()->centralFolder() : nullptr;
}

QString tabTitle(const QcwTabFolder* folder, int index)
{
    return folder && index < folder->count() ? folder->tabText(index) : QString {};
}

TestView* tabView(QcwTabFolder* folder, int index)
{
    auto* pane = folder && index < folder->count() ? folder->tabPane(index) : nullptr;
    // The only views this test puts in a folder are TestViews, and the creators make nothing else.
    return pane ? static_cast<TestView*>(pane) : nullptr;
}

QString baseline(QcwTabFolder* folder, int index)
{
    auto* view = qobject_cast<QcwView*>(folder && index < folder->count() ? folder->tabPane(index) : nullptr);
    return view ? view->defaultTitle() : QString {};
}

QString detailsText(QcwTabFolder* folder, int index)
{
    auto* view = tabView(folder, index);
    return view && view->details ? view->details->text() : QString {};
}

/// The text of the tool bar button that switches to @p perspectiveId.
QString perspectiveButtonText(QcwWindow& window, int perspectiveId)
{
    for (auto* button : window.findChildren<QToolButton*>())
    {
        auto* action = button->defaultAction();
        if (action && action->data().toInt() == perspectiveId)
        {
            return action->text();
        }
    }

    return {};
}

void installLanguage(QTranslator& translator, bool on)
{
    if (on)
    {
        QCoreApplication::installTranslator(&translator);
    }
    else
    {
        QCoreApplication::removeTranslator(&translator);
    }

    // The event is posted to every top-level, so the widgets see it on the next turn.
    QCoreApplication::processEvents();
}

/// Writes @p document to the temporary directory as @p fileName and returns the path.
QString writeDocument(const QTemporaryDir& dir, const QString& fileName, const QString& document)
{
    const QString path = dir.filePath(fileName);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        check(false, "a test document can be written", fileName);
    }
    file.write(document.toUtf8());
    file.close();

    return path;
}

/// @p document with every @p before swapped for @p after, which the test aims at one element by
/// naming an attribute pair no other element carries.
QString rewriteLiteral(const QString& document, const QString& before, const QString& after)
{
    if (!document.contains(before))
    {
        check(false, "the document holds what the test meant to rewrite", before);
    }

    auto rewritten = document;
    rewritten.replace(before, after);

    return rewritten;
}
} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    pluginManager->registerPlugin(new TestViewCreator);
    pluginManager->registerPlugin(new TestPerspectiveCreator);

    CapsTranslator caps;

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        std::fprintf(stderr, "cannot create the temporary directory\n");
        return 1;
    }
    const QString stateFile = tempDir.filePath(QStringLiteral("language-change.xml"));

    {
        QcwWindow window;
        window.setId(WindowId);
        window.setupUi();
        window.resize(800, 500);
        window.show();
        QCoreApplication::processEvents();

        auto* creator = pluginManager->getPluginT<QcwPerspectiveCreator>(QString(PerspectiveType));
        auto* perspective = creator->create(&window);
        perspective->createInitialLayout();
        window.openPerspective(perspective);
        QCoreApplication::processEvents();

        auto* folder = centralFolder(window);
        check(folder && folder->count() == 4, "the layout holds four tabs of one view type",
              QStringLiteral("tabs=%1").arg(folder ? folder->count() : 0));
        check(tabTitle(folder, 0) == caption("Notes") && tabTitle(folder, 1) == QStringLiteral("Notes 1") &&
                  tabTitle(folder, 2) == QStringLiteral("Notes 2") &&
                  tabTitle(folder, 3) == QStringLiteral("Notes 3"),
              "the titles are the creator's default, made unique",
              QStringLiteral("%1 | %2 | %3 | %4").arg(tabTitle(folder, 0), tabTitle(folder, 1), tabTitle(folder, 2),
                                                      tabTitle(folder, 3)));
        check(baseline(folder, 0) == caption("Notes") && baseline(folder, 1) == caption("Notes") &&
                  baseline(folder, 2) == caption("Notes") && baseline(folder, 3) == caption("Notes"),
              "every tab records the creator's default as its baseline, the plugin-made one too",
              QStringLiteral("%1 | %2").arg(baseline(folder, 2), baseline(folder, 3)));

        // A rename is exactly what the tab's only writer does: the text is replaced, the baseline
        // is not.
        folder->setTabText(2, QStringLiteral("My Note"));

        check(window.saveStateToFile(stateFile), "the session is saved in the language it was laid out in");

        check(perspective->name() == caption("Layout") && perspective->displayName() == caption("Layout") &&
                  perspectiveButtonText(window, perspective->id()) == caption("Layout"),
              "the perspective is named by its creator", perspective->name());

        gViewLanguageChanges = 0;
        installLanguage(caps, true);

        check(tabTitle(folder, 0) == QStringLiteral("NOTES"), "an untouched tab title re-resolves from its creator",
              tabTitle(folder, 0));
        check(tabTitle(folder, 1) == QStringLiteral("NOTES 1"), "the ordinal is kept through the re-resolve",
              tabTitle(folder, 1));
        check(tabTitle(folder, 2) == QStringLiteral("My Note"), "a renamed tab keeps its name",
              tabTitle(folder, 2));
        check(baseline(folder, 0) == QStringLiteral("NOTES") && baseline(folder, 1) == QStringLiteral("NOTES"),
              "the new default becomes the next baseline, without its ordinal", baseline(folder, 1));
        check(tabTitle(folder, 3) == QStringLiteral("NOTES 3"),
              "a tab a plugin made without the perspective re-resolves as well", tabTitle(folder, 3));
        check(gViewLanguageChanges == 4, "the event reaches a view nested in a folder of the window",
              QStringLiteral("delivered=%1").arg(gViewLanguageChanges));
        check(detailsText(folder, 0) == QStringLiteral("DETAILS"),
              "a view that re-applies its texts follows the language", detailsText(folder, 0));
        // name() is read back through the *source* literal: it is the identity the saved file is
        // named by, so a language change must not have touched it.
        check(perspective->name() == QStringLiteral("Layout"),
              "the perspective name, a saved file's identity, is left alone", perspective->name());
        check(perspective->displayName() == QStringLiteral("LAYOUT") &&
                  perspectiveButtonText(window, perspective->id()) == QStringLiteral("LAYOUT"),
              "the perspective button shows the current language",
              perspectiveButtonText(window, perspective->id()));

        installLanguage(caps, false);

        check(tabTitle(folder, 0) == caption("Notes") && tabTitle(folder, 1) == QStringLiteral("Notes 1") &&
                  tabTitle(folder, 2) == QStringLiteral("My Note") &&
                  tabTitle(folder, 3) == QStringLiteral("Notes 3"),
              "the titles come back with the language",
              QStringLiteral("%1 | %2 | %3 | %4").arg(tabTitle(folder, 0), tabTitle(folder, 1), tabTitle(folder, 2),
                                                      tabTitle(folder, 3)));
    }

    {
        QcwWindow restored;
        restored.setId(QStringLiteral("language-change-test-restored"));

        // The document holds the titles as English literals and the baselines they were derived from.
        if (!restored.restoreStateFromFile(stateFile))
        {
            check(false, "the saved session restores");
        }
        QCoreApplication::processEvents();

        auto* folder = centralFolder(restored);
        check(folder && folder->count() == 4, "the restored layout holds its four tabs",
              QStringLiteral("tabs=%1").arg(folder ? folder->count() : 0));
        check(baseline(folder, 0) == caption("Notes") && baseline(folder, 2) == caption("Notes") &&
                  baseline(folder, 3) == caption("Notes"),
              "the baseline survives saveState() and restoreState()",
              QStringLiteral("%1 | %2").arg(baseline(folder, 2), baseline(folder, 3)));
        check(tabTitle(folder, 2) == QStringLiteral("My Note"), "the restored rename is still on screen",
              tabTitle(folder, 2));

        installLanguage(caps, true);

        check(tabTitle(folder, 0) == QStringLiteral("NOTES") && tabTitle(folder, 1) == QStringLiteral("NOTES 1") &&
                  tabTitle(folder, 3) == QStringLiteral("NOTES 3"),
              "a session opened in another language flips its untouched tabs",
              QStringLiteral("%1 | %2 | %3").arg(tabTitle(folder, 0), tabTitle(folder, 1), tabTitle(folder, 3)));
        check(tabTitle(folder, 2) == QStringLiteral("My Note"),
              "and leaves the tab that was saved under a rename", tabTitle(folder, 2));

        installLanguage(caps, false);
    }

    {
        // One view, laid out by two perspectives in two folders: a language change has to leave the
        // pair exactly where it found it. Reading the title off every tab instead of every view made
        // the second reading compare an already rewritten baseline against the first tab's title and
        // number it, so the view came back as "Notes 1".
        QcwWindow window;
        window.setId(QStringLiteral("language-change-test-shared"));
        window.setupUi();
        window.show();
        QCoreApplication::processEvents();

        auto* creator = pluginManager->getPluginT<QcwPerspectiveCreator>(QString(PerspectiveType));
        auto* first = creator->create(&window);
        first->createInitialLayout();
        window.openPerspective(first);
        QCoreApplication::processEvents();

        auto* second = new SharingPerspective { &window };
        second->createInitialLayout();
        window.openPerspective(second);
        QCoreApplication::processEvents();

        auto* firstFolder = centralFolder(window);
        auto* secondFolder = second->ownFolder;
        check(firstFolder && secondFolder && firstFolder->tabPane(0) == secondFolder->tabPane(0),
              "the two perspectives show one and the same view");

        installLanguage(caps, true);

        check(tabTitle(firstFolder, 0) == QStringLiteral("NOTES"), "the first perspective keeps the plain title",
              tabTitle(firstFolder, 0));
        check(tabTitle(secondFolder, 0) == QStringLiteral("NOTES"),
              "a view laid out twice is retitled once, so it does not number itself", tabTitle(secondFolder, 0));

        installLanguage(caps, false);

        check(tabTitle(firstFolder, 0) == caption("Notes") && tabTitle(secondFolder, 0) == caption("Notes"),
              "and the pair is back where it started",
              QStringLiteral("%1 | %2").arg(tabTitle(firstFolder, 0), tabTitle(secondFolder, 0)));
    }

    QFile saved(stateFile);
    if (!saved.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        check(false, "the saved document can be read back");
    }
    const auto savedDocument = QString::fromUtf8(saved.readAll());
    saved.close();

    {
        // What every session written before the baseline existed looks like: the tab texts are
        // English literals and nothing says which title they were derived from. saveState() writes
        // the attribute even when it has nothing to record, so its absence — not its emptiness —
        // is what marks such a document. The restore then pairs each text with a baseline of its
        // *own* language, because the creator's default in the language installed right now is a
        // different string, and a text that does not match its baseline reads as a rename that
        // nothing may rewrite. Pairing them wrongly is what leaves an old session speaking the old
        // language forever.
        auto legacyDocument = savedDocument;
        legacyDocument.remove(QRegularExpression(QStringLiteral(" defaultText=\"[^\"]*\"")));
        const QString legacyFile = writeDocument(tempDir, QStringLiteral("legacy.xml"), legacyDocument);

        installLanguage(caps, true);

        QcwWindow legacyWindow;
        legacyWindow.setId(QStringLiteral("language-change-test-legacy"));
        if (!legacyWindow.restoreStateFromFile(legacyFile))
        {
            check(false, "the legacy session restores");
        }
        QCoreApplication::processEvents();

        auto* folder = centralFolder(legacyWindow);
        check(tabTitle(folder, 0) == QStringLiteral("NOTES") && tabTitle(folder, 1) == QStringLiteral("NOTES 1") &&
                  tabTitle(folder, 3) == QStringLiteral("NOTES 3"),
              "a document with no recorded baseline re-derives its titles as it is restored",
              QStringLiteral("%1 | %2 | %3").arg(tabTitle(folder, 0), tabTitle(folder, 1), tabTitle(folder, 3)));
        // The one thing such a document cannot say is whether "My Note" was typed by a user, so the
        // upgrade re-derives it too; the next save records the baseline and the rename survives.
        check(tabTitle(folder, 2) != QStringLiteral("My Note"),
              "and re-derives the tab it cannot tell apart from an untouched one", tabTitle(folder, 2));

        installLanguage(caps, false);
    }

    {
        // An empty recorded baseline is a different answer from a missing one: the document was
        // written knowing the tab has no creator default behind it, because the view was made and
        // then named by whoever made it — a perspective layout, another plugin. That name is not
        // the creator's to re-resolve, so the tab keeps it through a language change.
        const QString namedFile = writeDocument(tempDir, QStringLiteral("named.xml"),
                                                rewriteLiteral(savedDocument, QStringLiteral(" text=\"Notes\" defaultText=\"Notes\""),
                                                               QStringLiteral(" text=\"Notes\" defaultText=\"\"")));

        installLanguage(caps, true);

        QcwWindow namedWindow;
        namedWindow.setId(QStringLiteral("language-change-test-named"));
        if (!namedWindow.restoreStateFromFile(namedFile))
        {
            check(false, "the session whose first tab is named by its caller restores");
        }
        QCoreApplication::processEvents();

        auto* folder = centralFolder(namedWindow);
        check(tabTitle(folder, 0) == QStringLiteral("Notes") && baseline(folder, 0).isEmpty(),
              "a tab with no baseline to re-resolve keeps the title it was given",
              QStringLiteral("%1 | %2").arg(tabTitle(folder, 0), baseline(folder, 0)));
        check(tabTitle(folder, 1) == QStringLiteral("NOTES 1"),
              "while its neighbours, which do have one, still follow the language", tabTitle(folder, 1));

        installLanguage(caps, false);
    }

    {
        // The same three readings for the perspective button: saveState() always writes autoName,
        // so a document that says "not automatic" keeps its stored name on screen, and one that
        // predates the attribute is taken for an automatic name — the reading that follows the
        // language. name() answers the stored literal whichever way, because it is the file name
        // the saved perspective is stored under.
        const QString chosenFile = writeDocument(tempDir, QStringLiteral("chosen.xml"),
                                                 rewriteLiteral(savedDocument,
                                                                QStringLiteral(" name=\"Layout\" autoName=\"1\""),
                                                                QStringLiteral(" name=\"Layout\" autoName=\"0\"")));
        auto unmarkedDocument = savedDocument;
        unmarkedDocument.remove(QRegularExpression(QStringLiteral(" autoName=\"[^\"]*\"")));
        const QString unmarkedFile = writeDocument(tempDir, QStringLiteral("unmarked.xml"), unmarkedDocument);

        installLanguage(caps, true);

        for (const auto& file : { chosenFile, unmarkedFile })
        {
            QcwWindow window;
            window.setId(QStringLiteral("language-change-test-perspective"));
            if (!window.restoreStateFromFile(file))
            {
                check(false, "the session whose perspective flag was rewritten restores",
                      QFileInfo(file).fileName());
            }
            QCoreApplication::processEvents();

            auto* perspective = window.currentPerspective();
            const bool chosen = file == chosenFile;
            const auto shown = chosen ? QStringLiteral("Layout") : QStringLiteral("LAYOUT");
            check(perspective && perspective->name() == QStringLiteral("Layout"),
                  "the perspective keeps the name the document stored, which is its file name",
                  perspective ? perspective->name() : QString {});
            check(perspective && perspective->displayName() == shown,
                  chosen ? "a perspective the document calls hand-named shows that name"
                         : "a perspective from before the flag follows the language",
                  perspective ? perspective->displayName() : QString {});
            // The button is built while the document is read, so it has to be right from the start
            // rather than waiting for a language change that may never come.
            check(perspectiveButtonText(window, perspective->id()) == shown,
                  "the button the restored window builds shows the same text at once",
                  perspectiveButtonText(window, perspective->id()));
        }

        installLanguage(caps, false);
    }

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    delete pluginManager->removePlugin(QString(ViewType));
    delete pluginManager->removePlugin(QString(PerspectiveType));

    return gFailures == 0 ? 0 : 1;
}

#include "language_change_test.moc"
