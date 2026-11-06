// Checks QcwWindowStyler against temporary themes: it collects the QcwWindow.<style>.theme
// files of the current theme, installs the named renderer in every folder of every window,
// applies the window stylesheet, follows a style change, re-applies after a theme change,
// keeps the styled widgets following QApplication::setFont(), keeps an opaque page widget from
// covering the styled folder's rounded corners, styles the folders a reset perspective builds
// and reports a stored style name the current theme
// does not offer exactly once, as a card whose button runs the Preferences command.

#include "QcwCommandRegistry.h"
#include "QcwDefaultTabFolderRenderer.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwNotificationBox.h"
#include "QcwPerspective.h"
#include "QcwPluginManager.h"
#include "QcwSettings.h"
#include "QcwStyledTabFolderRenderer.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabFolderRenderer.h"
#include "QcwTabFolderRendererCreators.h"
#include "QcwThemeManager.h"
#include "QcwView.h"
#include "QcwViewCreator.h"
#include "QcwWindow.h"
#include "QcwWindowStyler.h"
#include "QcwWindowActionIds.h"

#include <QAction>
#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QMap>
#include <QPainter>
#include <QPointer>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>
#include <QToolButton>
#include <cstdio>

namespace
{

/// Opaque page, so a missing corner overlay would show this color in the corner.
class ColoredWidget : public QWidget
{
public:
    explicit ColoredWidget(const QColor& color) :
        mColor { color }
    {
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), mColor);
    }

private:
    QColor mColor;
};

/// Minimal view, so QcwWindow::createDetachedView() has a creator to work with.
class StylerTestView : public QcwView
{
    Q_OBJECT
public:
    explicit StylerTestView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
    }
};

class StylerTestViewCreator : public QcwViewCreator
{
    Q_OBJECT
    Q_INTERFACES(QcwViewCreator)
public:
    QString type() const override
    {
        return QStringLiteral("WindowStylerTestView");
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
        return new StylerTestView { parent };
    }
};

bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

QString mainCssFor(const QString& themeName)
{
    return QStringLiteral("QToolTip { color: #123456; } /* %1 */").arg(themeName);
}

QString writeTheme(const QString& root,
                   const QString& name,
                   const QString& renderer,
                   const QString& css,
                   bool writeRendererTheme = true,
                   bool writeAppCss = true)
{
    const QDir dir(QDir(root).filePath(name));
    if (!QDir().mkpath(dir.path()))
    {
        return {};
    }

    const bool paletteOk = writeFile(dir.filePath("QcwPalette.theme"), R"({"style":"Fusion","colors":{}})");
    const bool rendererOk =
        !writeRendererTheme ||
        writeFile(dir.filePath("QcwStyledTabFolderRenderer.theme"),
                  R"({"tabBarSpacing":6,"tabBarPadding":"6 6 6 6","pageMargins":"0 0 0 0",)"
                  R"("surroundColor":"#ff00ff",)"
                  R"("tabBorderRadius":"4 4 4 4","tabBorderColor":"#dddddd",)"
                  R"("dirtyTextColor":"#00c853","hoveredTabBackground":"#f1f1f1","selectedTabBackground":"#e3e3e3",)"
                  R"("activeTabBackground":"#b2ebf2",)"
                  R"("folderCss":"QcwTabFolder { border: 2px solid #3366cc; border-radius: 10px; }"})");
    const QByteArray windowTheme =
        QStringLiteral(R"({"folderRenderer":"%1","centralMargins":"6 6 6 6","windowCss":"%2"})")
            .arg(renderer, css)
            .toUtf8();
    const bool windowOk = writeFile(dir.filePath("QcwWindow.Cards.theme"), windowTheme);
    const bool appCssOk = !writeAppCss || writeFile(dir.filePath("main.css"), mainCssFor(name).toUtf8());
    return paletteOk && rendererOk && windowOk && appCssOk ? dir.absolutePath() : QString();
}

void settle()
{
    for (int i = 0; i < 5; ++i)
    {
        QCoreApplication::processEvents();
    }
}

bool isStyled(const QcwTabFolder* folder)
{
    return dynamic_cast<QcwStyledTabFolderRenderer*>(folder->renderer()) != nullptr;
}

bool isDefault(const QcwTabFolder* folder)
{
    return dynamic_cast<QcwDefaultTabFolderRenderer*>(folder->renderer()) != nullptr;
}

/// Warnings seen while the message handler installed by installWarningCollector() is active.
QStringList gWarnings;

void collectWarning(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    // Only the styler's own warnings, so unrelated noise cannot upset the count below.
    if (type == QtWarningMsg && message.contains(QStringLiteral("QcwWindowStyler")))
    {
        gWarnings.append(message);
    }
}

/// Installs collectWarning() and returns the handler it replaced.
QtMessageHandler installWarningCollector()
{
    gWarnings.clear();
    return qInstallMessageHandler(collectWarning);
}

/// The visible notification card with @p id, or nullptr when none is on screen.
QcwNotificationBox* notificationBox(const QString& id)
{
    for (auto* widget : QApplication::topLevelWidgets())
    {
        auto* box = qobject_cast<QcwNotificationBox*>(widget);
        if (box && box->id() == id)
        {
            return box;
        }
    }

    return nullptr;
}

/// The card's action button, or nullptr when the card offers none.
QToolButton* cardButton(QcwNotificationBox* box)
{
    return box ? box->findChild<QToolButton*>(QStringLiteral("notificationActionButton")) : nullptr;
}

QMargins centralMargins(const QcwWindow& window)
{
    if (auto central = window.centralWidget())
    {
        if (auto layout = central->layout())
        {
            return layout->contentsMargins();
        }
    }

    return {};
}

/// Page inset from the folder's contents, in left, top, right, bottom order.
QMargins pageInsets(const QcwTabFolder& folder)
{
    const QWidget* page = folder.currentWidget();
    if (!page)
    {
        return { -1, -1, -1, -1 };
    }

    const QRect pageRect = page->geometry();
    const QRect contents = folder.contentsRect();
    return QMargins { pageRect.left() - contents.left(), pageRect.top() - contents.top(),
                      contents.right() - pageRect.right(), contents.bottom() - pageRect.bottom() };
}

/// Number of visible corner overlays installed by the folder.
int visibleCornerMasks(const QcwTabFolder& folder)
{
    int count = 0;
    for (const auto* mask : folder.findChildren<QWidget*>(QStringLiteral("qcwCornerMask")))
    {
        count += mask->isVisible() ? 1 : 0;
    }

    return count;
}

/// Number of visible border strips installed by the folder.
int visibleBorderStrips(const QcwTabFolder& folder)
{
    int count = 0;
    for (const auto* strip : folder.findChildren<QWidget*>(QStringLiteral("qcwBorderStrip")))
    {
        count += strip->isVisible() ? 1 : 0;
    }

    return count;
}

/// Renders the folder with its children in stacking order.
QImage folderImage(QcwTabFolder& folder)
{
    return folder.grab().toImage();
}

/// True when two colors are close enough for an antialiased edge pixel.
bool nearColor(const QColor& actual, const QColor& expected)
{
    return qAbs(actual.red() - expected.red()) + qAbs(actual.green() - expected.green()) +
               qAbs(actual.blue() - expected.blue()) <=
           30;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        std::fprintf(stderr, "cannot create the temporary directory\n");
        return 1;
    }

    const QString lightCss = "QAbstractButton { color: #123456; }";
    const QString darkCss = "QAbstractButton { color: #654321; }";
    const QString plainCss = "QAbstractButton { color: #112233; }";
    const QString lightMainCss = mainCssFor("TestLight");
    const QString darkMainCss = mainCssFor("TestDark");

    const QString lightDir = writeTheme(tempDir.path(), "TestLight", "QcwStyledTabFolderRenderer", lightCss);
    const QString darkDir = writeTheme(tempDir.path(), "TestDark", "QcwDefaultTabFolderRenderer", darkCss);
    // Same window style as TestLight, but without the renderer's theme data file: the renderer
    // has to fall back to its built-in defaults instead of zeroing its values. It also ships no
    // main.css, so the application stylesheet an earlier theme applied has to be cleared.
    const QString plainDir =
        writeTheme(tempDir.path(), "TestPlain", "QcwStyledTabFolderRenderer", plainCss, false, false);
    if (lightDir.isEmpty() || darkDir.isEmpty() || plainDir.isEmpty())
    {
        std::fprintf(stderr, "cannot write the test themes\n");
        return 1;
    }

    const QString settingsPath = QDir(tempDir.path()).filePath("QcwSettings.ini");
    {
        QMap<QString, QString> themes;
        themes.insert("TestLight", lightDir);
        themes.insert("TestDark", darkDir);
        themes.insert("TestPlain", plainDir);

        QByteArray serialized;
        QDataStream stream(&serialized, QIODevice::WriteOnly);
        stream << themes;

        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue("appearance/themes/themeInfos", serialized);
        settings.setValue("appearance/themes/currentTheme", "TestLight");
        settings.setValue("appearance/windowStyle/currentStyle", "Cards");
        settings.sync();
    }

    QcwSettings::initialize([settingsPath]()
    {
        return QSettings(settingsPath, QSettings::IniFormat);
    });

    themeManager->initialize();

    folderRendererCreators->registerCreator("QcwStyledTabFolderRenderer", [](QcwTabFolder* folder)
    {
        return static_cast<QcwTabFolderRenderer*>(new QcwStyledTabFolderRenderer(folder));
    });
    folderRendererCreators->registerCreator("QcwDefaultTabFolderRenderer", [](QcwTabFolder* folder)
    {
        return static_cast<QcwTabFolderRenderer*>(new QcwDefaultTabFolderRenderer(folder));
    });

    pluginManager->registerPlugin(new StylerTestViewCreator);

    QcwWindow window;
    window.setId("window-styler-test");
    window.setupUi();
    window.resize(640, 480);

    auto fontProbe = new QLabel(QStringLiteral("font probe"), &window);
    window.show();
    settle();

    QcwTabFolder* folder = window.currentPerspective()->folderContainer()->centralFolder();
    if (!folder)
    {
        std::fprintf(stderr, "the window has no central folder\n");
        delete pluginManager->removePlugin(QStringLiteral("WindowStylerTestView"));
        return 1;
    }

    const QColor pageColor("#00ff00");
    const QColor surroundColor("#ff00ff");
    auto page = new ColoredWidget(pageColor);
    folder->addTab(page, QStringLiteral("Page"));
    folder->setCurrentIndex(0);

    windowStyler->initialize();
    settle();

    // The window stylesheet must not cut the widgets off from the application font, because
    // the appearance preferences change that font at run time.
    const QFont appFont = QApplication::font();
    QFont largerFont = appFont;
    largerFont.setPointSize(appFont.pointSize() + 4);
    QApplication::setFont(largerFont);
    settle();
    const bool fontOk = fontProbe->font().pointSize() == largerFont.pointSize();
    QApplication::setFont(appFont);
    settle();

    const auto styles = windowStyler->allStyles();
    const bool stylesOk = styles.size() == 2 && styles.first() == QString("Framed") && styles.contains("Cards");

    const QRect folderRect = folder->rect();
    const QColor borderColor("#3366cc");
    const QPoint bottomRight { folderRect.width() - 1, folderRect.height() - 1 };
    // The arc centre of the 10px corner: inside the disc, so the page shows there.
    const QPoint innerBottomRight { folderRect.width() - 10, folderRect.height() - 10 };
    // Middle of the 2px border arc, and of the restored straight edges.
    const QPoint arcBottomRight { folderRect.width() - 4, folderRect.height() - 4 };
    const QPoint bottomEdge { folderRect.width() / 2, folderRect.height() - 1 };
    const QPoint leftEdge { 0, folderRect.height() / 2 };
    const QPoint topRight { folderRect.width() - 1, 0 };
    const QPoint innerTopRight { folderRect.width() - 10, 10 };
    const QPoint arcTopRight { folderRect.width() - 4, 4 };
    const QPoint topEdge { folderRect.width() / 2, 0 };

    const QMargins initialInsets = pageInsets(*folder);
    const QImage initialImage = folderImage(*folder);
    const auto initialMask = folder->renderer()->frameMask();
    const bool initialOk =
        windowStyler->currentStyle() == "Cards" && isStyled(folder) && window.styleSheet() == lightCss &&
        qApp->styleSheet() == lightMainCss && folder->styleSheet() != QString() &&
        centralMargins(window) == QMargins(6, 6, 6, 6) && initialInsets.left() == 0 && initialInsets.right() == 0 &&
        initialInsets.bottom() == 0 && initialInsets.top() > 0 && initialMask.cornerRadius == 10 &&
        initialMask.surroundColor == surroundColor && initialMask.borderWidth == 2 &&
        initialMask.borderColor == borderColor && visibleCornerMasks(*folder) == 2 &&
        visibleBorderStrips(*folder) == 3 && initialImage.pixelColor(bottomRight) == surroundColor &&
        initialImage.pixelColor(innerBottomRight) == pageColor && initialImage.pixelColor(bottomEdge) == borderColor &&
        initialImage.pixelColor(leftEdge) == borderColor &&
        nearColor(initialImage.pixelColor(arcBottomRight), borderColor);

    // A view opened through the menu builds its own floating folder, and no style pass runs
    // for it afterwards: it has to adopt the renderer the folders around it already wear.
    auto detachedCards = window.createDetachedView(QStringLiteral("WindowStylerTestView"), QStringLiteral("Detached"));
    const bool detachedStyledOk = detachedCards && isStyled(detachedCards);
    delete detachedCards;

    windowStyler->setCurrentStyle("Framed");
    const QMargins defaultInsets = pageInsets(*folder);
    const QImage defaultImage = folderImage(*folder);
    const bool defaultOk = isDefault(folder) && window.styleSheet().isEmpty() && folder->styleSheet().isEmpty() &&
                           centralMargins(window) == QMargins() && defaultInsets.left() == 0 &&
                           defaultInsets.right() == 0 && defaultInsets.bottom() == 0 && defaultInsets.top() > 0 &&
                           visibleCornerMasks(*folder) == 0 && visibleBorderStrips(*folder) == 0 &&
                           defaultImage.pixelColor(bottomRight) == pageColor;

    auto detachedFramed = window.createDetachedView(QStringLiteral("WindowStylerTestView"), QStringLiteral("Detached"));
    const bool detachedDefaultOk = detachedFramed && isDefault(detachedFramed);
    delete detachedFramed;

    windowStyler->setCurrentStyle("Cards");
    const QMargins restoredInsets = pageInsets(*folder);
    const QImage restoredImage = folderImage(*folder);
    const bool restoredOk = isStyled(folder) && window.styleSheet() == lightCss &&
                            centralMargins(window) == QMargins(6, 6, 6, 6) && restoredInsets.left() == 0 &&
                            restoredInsets.right() == 0 && restoredInsets.bottom() == 0 && restoredInsets.top() > 0 &&
                            visibleCornerMasks(*folder) == 2 && visibleBorderStrips(*folder) == 3 &&
                            restoredImage.pixelColor(bottomRight) == surroundColor &&
                            restoredImage.pixelColor(innerBottomRight) == pageColor &&
                            restoredImage.pixelColor(bottomEdge) == borderColor &&
                            nearColor(restoredImage.pixelColor(arcBottomRight), borderColor);

    // The theme change is applied in a queued pass, after the renderers reload their data.
    themeManager->changeTheme("TestDark");
    settle();
    const QMargins darkInsets = pageInsets(*folder);
    const QImage darkImage = folderImage(*folder);
    const bool darkOk = isDefault(folder) && window.styleSheet() == darkCss && qApp->styleSheet() == darkMainCss &&
                        centralMargins(window) == QMargins(6, 6, 6, 6) && darkInsets.left() == 0 &&
                        darkInsets.right() == 0 && darkInsets.bottom() == 0 && darkInsets.top() > 0 &&
                        visibleCornerMasks(*folder) == 0 && visibleBorderStrips(*folder) == 0 &&
                        darkImage.pixelColor(bottomRight) == pageColor;

    themeManager->changeTheme("TestLight");
    settle();
    const QMargins lightInsets = pageInsets(*folder);
    const QImage lightImage = folderImage(*folder);
    const bool lightOk =
        isStyled(folder) && window.styleSheet() == lightCss && qApp->styleSheet() == lightMainCss &&
        centralMargins(window) == QMargins(6, 6, 6, 6) && lightInsets.left() == 0 && lightInsets.right() == 0 &&
        lightInsets.bottom() == 0 && lightInsets.top() > 0 && visibleCornerMasks(*folder) == 2 &&
        visibleBorderStrips(*folder) == 3 && lightImage.pixelColor(bottomRight) == surroundColor &&
        lightImage.pixelColor(innerBottomRight) == pageColor && lightImage.pixelColor(bottomEdge) == borderColor &&
        nearColor(lightImage.pixelColor(arcBottomRight), borderColor);

    // With the tab bar at the bottom the page covers the top corners and edges, so the overlays
    // move there while the page keeps its full area.
    folder->setTabPosition(qcw::TabPosition::Bottom);
    settle();
    const QMargins bottomInsets = pageInsets(*folder);
    const QImage bottomImage = folderImage(*folder);
    const bool bottomOk =
        isStyled(folder) && bottomInsets.left() == 0 && bottomInsets.right() == 0 && bottomInsets.top() == 0 &&
        bottomInsets.bottom() > 0 && visibleCornerMasks(*folder) == 2 && visibleBorderStrips(*folder) == 3 &&
        bottomImage.pixelColor(topRight) == surroundColor && bottomImage.pixelColor(innerTopRight) == pageColor &&
        bottomImage.pixelColor(topEdge) == borderColor && nearColor(bottomImage.pixelColor(arcTopRight), borderColor);
    folder->setTabPosition(qcw::TabPosition::Top);
    settle();

    // TestPlain ships no QcwStyledTabFolderRenderer.theme, so the renderer must restore its
    // built-in metrics and folder stylesheet instead of keeping zeroed values. Without a
    // surround color the corner mask stays off and the page margins keep the page clear.
    themeManager->changeTheme("TestPlain");
    settle();
    auto styledRenderer = dynamic_cast<QcwStyledTabFolderRenderer*>(folder->renderer());
    const QMargins plainInsets = pageInsets(*folder);
    const bool plainOk =
        styledRenderer && styledRenderer->tabBarSpacing() == 6 &&
        styledRenderer->tabBarPadding() == QMargins(6, 6, 6, 6) &&
        styledRenderer->pageMargins() == QMargins(6, 0, 6, 6) && !styledRenderer->frameMask().surroundColor.isValid() &&
        styledRenderer->frameMask().borderWidth == 0 && visibleCornerMasks(*folder) == 0 &&
        visibleBorderStrips(*folder) == 0 && folder->styleSheet().contains("QcwTabFolder") &&
        window.styleSheet() == plainCss && centralMargins(window) == QMargins(6, 6, 6, 6) && plainInsets.left() == 6 &&
        plainInsets.right() == 6 && plainInsets.bottom() == 6 && plainInsets.top() > 0 && qApp->styleSheet().isEmpty();

    // The fallback page margins follow the tab position as well.
    folder->setTabPosition(qcw::TabPosition::Bottom);
    settle();
    const QMargins plainBottomInsets = pageInsets(*folder);
    const bool plainBottomOk = styledRenderer && styledRenderer->pageMargins() == QMargins(6, 6, 6, 0) &&
                               plainBottomInsets.left() == 6 && plainBottomInsets.top() == 6 &&
                               plainBottomInsets.right() == 6 && plainBottomInsets.bottom() > 0;
    folder->setTabPosition(qcw::TabPosition::Top);
    settle();

    // A stored name no theme answers to any more - which is what every settings file written
    // before the Default/Styled rename holds - applies as the built-in style: the default
    // renderer, no window stylesheet and no central margins. allStyles() leaves it out too, so the
    // preferences combo shows its first entry while the window still wears the fallback look;
    // picking any style ends the disagreement, and nothing migrates the stored value by itself.
    // The styler says so once, in the log and as a card whose button runs the Preferences command.
    // The card's button runs the Preferences command through the active window, so the command
    // needs both a catalog entry and a handler for the button to appear and do anything.
    auto* preferencesHandler = new QcwLambdaCommandHandler(&window);
    int preferencesTriggered = 0;
    preferencesHandler->onExecute([&preferencesTriggered](const QVariantMap&)
    {
        ++preferencesTriggered;
    });
    if (!commandRegistry->contains(QcwWindowActionIds::ActionPreferences))
    {
        commandRegistry->registerCommand({ QString(QcwWindowActionIds::ActionPreferences), QStringLiteral("Test"),
                                           QStringLiteral("Preferences...") });
    }
    window.registerHandler(QcwWindowActionIds::ActionPreferences, preferencesHandler);

    auto previousHandler = installWarningCollector();
    windowStyler->setCurrentStyle("Styled");
    const bool removedOk = windowStyler->currentStyle() == "Styled" && isDefault(folder) &&
                           window.styleSheet().isEmpty() && centralMargins(window) == QMargins() &&
                           !windowStyler->allStyles().contains("Styled");
    const bool warnedOk =
        gWarnings.size() == 1 && gWarnings.first().contains("Styled") && gWarnings.first().contains("Cards");
    settle(); // The card is posted on the next event-loop pass, after the window is on screen.
    QPointer<QcwNotificationBox> box = notificationBox(QStringLiteral("window-style-unavailable"));
    auto* button = cardButton(box.data());
    const bool cardOk = box && box->isVisible() && button != nullptr;

    // Re-applying the same name stays quiet, and a name the theme answers to adds no report.
    windowStyler->applyStyle();
    windowStyler->setCurrentStyle("Framed");
    windowStyler->setCurrentStyle("Cards");
    const bool dedupedOk = gWarnings.size() == 1;

    // The button runs the Preferences command through the active window, which is what closes the
    // card; the action deletes itself once the deferred delete is due. The offscreen platform
    // leaves a shown window inactive, so the test has to activate it for the command to have a
    // window to route through.
    if (button)
    {
        window.activateWindow();
        settle();
        button->click();
    }
    const bool openedOk = preferencesTriggered == 1;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const bool cardGoneOk = box.isNull();

    // An application with no Preferences command in the catalog gets the card without the button,
    // and the next unresolved name is announced as its own report: the guard is per name, not per
    // process.
    window.unregisterHandler(QcwWindowActionIds::ActionPreferences);
    commandRegistry->unregisterCommand(QString(QcwWindowActionIds::ActionPreferences));
    windowStyler->setCurrentStyle("Default");
    settle();
    auto* plainBox = notificationBox(QStringLiteral("window-style-unavailable"));
    const bool noButtonOk = plainBox && cardButton(plainBox) == nullptr;
    const bool secondNameOk = gWarnings.size() == 2 && gWarnings.at(1).contains("Default");
    qInstallMessageHandler(previousHandler);
    if (plainBox)
    {
        plainBox->close();
    }

    // Reset Perspective drops the layout and builds a new one through the container. Those
    // folders are announced through QcwWindow::folderAdded(), so they must wear the current
    // style even though no style pass runs for the reset itself.
    windowStyler->setCurrentStyle("Cards");
    window.currentPerspective()->resetLayout();
    settle();
    auto* resetFolder = window.currentPerspective()->folderContainer()->centralFolder();
    const bool resetStyledOk = resetFolder && isStyled(resetFolder) && !resetFolder->styleSheet().isEmpty();

    const bool allOk = stylesOk && initialOk && defaultOk && restoredOk && darkOk && lightOk && plainOk &&
                       plainBottomOk && fontOk && bottomOk && removedOk && warnedOk && cardOk && dedupedOk &&
                       openedOk && cardGoneOk && noButtonOk && secondNameOk && detachedStyledOk && detachedDefaultOk &&
                       resetStyledOk;
    std::fprintf(stderr,
                 "styles=%d initial=%d default=%d restored=%d dark=%d light=%d plain=%d plainBottom=%d font=%d "
                 "bottom=%d removed=%d warned=%d card=%d deduped=%d opened=%d cardGone=%d noButton=%d second=%d "
                 "detachedStyled=%d detachedDefault=%d resetStyled=%d\n",
                 stylesOk, initialOk, defaultOk, restoredOk, darkOk, lightOk, plainOk, plainBottomOk, fontOk, bottomOk,
                 removedOk, warnedOk, cardOk, dedupedOk, openedOk, cardGoneOk, noButtonOk, secondNameOk,
                 detachedStyledOk, detachedDefaultOk, resetStyledOk);

    // The manager owns nothing; this test creator is ours to delete.
    delete pluginManager->removePlugin(QStringLiteral("WindowStylerTestView"));

    return allOk ? 0 : 1;
}

#include "window_styler_test.moc"
