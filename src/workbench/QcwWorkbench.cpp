// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwWorkbench.h"
#include "QcwTopicService.h"
#include "QcwActionSetRegistry.h"
#include "QcwCommandRegistry.h"
#include "QcwContributionRegistry.h"
#include "QcwWindow.h"
#include "QcwWindowStyler.h"
#include "QcwSettings.h"
#include "QcwSavedPerspectives.h"
#include "QcwPluginLoader.h"
#include "QcwThemeManager.h"
#include "QcwThemeDialog.h"
#include "QcwSerializeUtils.h"
#include "QcwBuiltinCommands.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwLogger.h"
#include "QcwPathUtils.h"
#include "QcwPreferencesDialog.h"
#include "QcwPreferencesManager.h"
#include "QcwAppearancePreferencesPage.h"
#include "QcwNotificationPreferencesPage.h"
#include "QcwDefaultTabFolderRenderer.h"
#include "QcwStyledTabFolderRenderer.h"
#include "QcwTabFolderRenderer.h"
#include "QcwTabFolderRendererCreators.h"
#include "QcwThemeIcons.h"
#include "QcwTranslations.h"
#include "QcwWindowActionIds.h"
#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QKeySequence>
#include <QPointer>
#include <QSaveFile>
#include <QSet>
#include <QStyleHints>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <ctime>
#include <memory>

QT_BEGIN_NAMESPACE

namespace
{
constexpr int kWorkbenchStateVersion = 1;

// Window ids double as file names, so reject anything that could escape the configuration
// directory or name a file that the platform cannot create.
bool isValidWindowId(const QString& id)
{
    if (id.isEmpty() || id == "." || id == "..")
    {
        return false;
    }

    for (const auto ch : id)
    {
        if (ch == '/' || ch == '\\' || ch == ':' || ch == '<' || ch == '>' || ch == '"' || ch == '|' || ch == '?' ||
            ch == '*' || ch.unicode() < 0x20)
        {
            return false;
        }
    }

    return true;
}

// QCW_DUMP_ID_CATALOG=1 ends start() with every id the three registries hold, which is what
// turns "the menu is missing an entry" into a comparison against the catalog. Ids only: the
// registries do not record who registered an entry, so an owner column would be a guess.
void dumpRegisteredIds()
{
    qInfo().noquote() << "QCW command catalog (registered):";
    for (const auto& info : commandRegistry->commands())
    {
        qInfo().noquote() << QStringLiteral("  %1").arg(info.id);
    }

    qInfo().noquote() << "QCW action set catalog (registered):";
    for (const auto& actionSet : actionSetRegistry->allActionSets())
    {
        qInfo().noquote() << QStringLiteral("  %1").arg(actionSet.id);
    }

    qInfo().noquote() << "QCW contribution catalog (registered):";
    for (const auto& contribution : contributionRegistry->allContributions())
    {
        qInfo().noquote() << QStringLiteral("  %1").arg(contribution.id);
    }
}

} // namespace

class QcwWorkbenchPrivate
{
public:
    explicit QcwWorkbenchPrivate(QcwWorkbench* interface);

    void createNewWindow();
    bool trackWindow(QcwWindow* window);
    QcwWindow* currentWindow() const;
    void removeWindow(const QString& windowId);
    void requestExit();
    void closeAllWindows();

    void saveState() const;
    void restoreState();

    /// Disconnects both connections registerGlobalHandler() made for @p commandId.
    void dropHandlerConnections(const QString& commandId);

    QString stateFilePath() const;
    QString winStateFilePath(const QString& winId) const;
    QString makeWindowId();

public:
    QcwWorkbench* owner;
    QMap<QString, QcwWindow*> windows;
    QcwTopicService* topicService { nullptr };
    QPointer<QcwPreferencesDialog> preferencesDialog;
    QSet<QString> pendingRemovals;
    QMap<QString, QcwCommandHandler*> globalHandlers;
    // Both connections of one command id, so unregisterGlobalHandler() takes the destroyed() one
    // out with the stateChanged() one; leaving it behind let a handler destroyed after the
    // workbench free its impl call into freed memory. See the same arrangement in QcwWindowPrivate.
    struct GlobalHandlerConnections
    {
        QMetaObject::Connection stateChanged;
        QMetaObject::Connection destroyed;
    };
    QMap<QString, GlobalHandlerConnections> globalHandlerConnections;
    bool initialized { false };
    bool started { false };
    bool exitPending { false };
    bool exitApproved { false };
    bool windowsClosed { false };
    bool shuttingDown { false };
};

// -----------------------------------------------------------------------------

QcwWorkbench::QcwWorkbench() :
    impl { new QcwWorkbenchPrivate { this } }
{
}

QcwWorkbench::~QcwWorkbench()
{
    impl->shuttingDown = true;

    // Global handlers are children of this workbench; QObject::~QObject() destroys them
    // after this body runs, and their destroyed() callback reaches into impl.
    for (auto it = impl->globalHandlerConnections.begin(); it != impl->globalHandlerConnections.end(); ++it)
    {
        disconnect(it.value().stateChanged);
        disconnect(it.value().destroyed);
    }
    impl->globalHandlerConnections.clear();
    impl->globalHandlers.clear();

    impl->closeAllWindows();

    // Windows are deleted before the private object so the destroyed handlers still find a
    // valid map. Clearing it first keeps those handlers from mutating the map being walked.
    const auto remainingWindows = impl->windows;
    impl->windows.clear();
    for (auto window : remainingWindows)
    {
        delete window;
    }

    // Closing a window detaches its tab panes and hands them to the manager, which only
    // schedules their deletion. Those events are never delivered once exec() has returned,
    // and a pane that waits for QApplication's own teardown destroys its text layouts after
    // the font cache has been cleared, leaking every fallback font pattern. Deliver them now,
    // while the application is still whole.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    delete impl;
}

bool QcwWorkbench::initialize(const QString& appName,
                              const QString& appDisplayName,
                              const QString& orgDomain,
                              const QString& orgName,
                              const QString& appVersion,
                              bool quitOnLastWindowClosed,
                              bool loadPlugins)
{
    if (!qobject_cast<QApplication*>(QCoreApplication::instance()))
    {
        qWarning() << "QcwWorkbench::initialize(): a QApplication must exist before the workbench is initialized";
        return false;
    }

    if (impl->initialized)
    {
        qWarning() << "QcwWorkbench::initialize(): the workbench is already initialized";
        return false;
    }

    // QcwTopicService::forApplication() finds the workbench through the application's children;
    // without this, application-scope topics would use the fallback service even with a workbench.
    setParent(QCoreApplication::instance());

    // The directories are derived from the identity, so it has to be set first.
    QApplication::setApplicationName(appName);
    QApplication::setApplicationDisplayName(appDisplayName);
    QApplication::setOrganizationDomain(orgDomain);
    QApplication::setOrganizationName(orgName);
    QApplication::setApplicationVersion(appVersion);
    QApplication::setQuitOnLastWindowClosed(quitOnLastWindowClosed);

    const auto logPath = QcwAppDirectories::logDirectory();
    if (!QDir().mkpath(logPath))
    {
        qWarning() << "QcwWorkbench::initialize(): cannot create the log directory" << logPath;
        return false;
    }

    // One log per run: keep the nine newest and drop the rest. Only this application's own
    // files are touched, never whatever else shares the directory.
    QDir logDir(logPath);
    const auto logFiles = logDir.entryList({ QStringLiteral("*.log") }, QDir::Files, QDir::Name | QDir::Reversed);
    for (qsizetype i = 9; i < logFiles.size(); ++i)
    {
        logDir.remove(logFiles.at(i));
    }

    QcwLogger::install();

    // prepare configuration directory
    if (!setConfigDirectory())
    {
        return false;
    }

    // QcwSettings defaults to the platform's native QSettings, which has no known file. The
    // workbench is the kind of application that needs one, so it installs the INI provider;
    // an application that installed a provider earlier keeps it, because initialize() honors
    // only the first call.
    QcwSettings::initialize([]()
    {
        return QSettings(QDir(QcwAppDirectories::configDirectory()).filePath(QStringLiteral("QcwSettings.ini")),
                         QSettings::IniFormat);
    });

    const auto configPath = QcwAppDirectories::configDirectory();
    if (!QDir().mkpath(configPath))
    {
        qWarning() << "QcwWorkbench::initialize(): cannot create the configuration directory" << configPath;
        return false;
    }

    qDebug() << "The configuration directory:" << configPath;

    themeManager->initialize();
    qInfo() << "Current theme:" << themeManager->currentTheme();

    // The docking catalog ships with qcwdocking and installs beside qcw's own. Registering it
    // here, before the stored language is applied, keeps folder strings translated on the first
    // window.
    QcwTranslations::registerCatalogBase(QStringLiteral("qcwdocking"));

    // Language, font and the followed system theme must be in place before plugins and
    // windows create their UI.
    QcwAppearancePreferencesPage::applyStoredSettings();

    auto appDir = QApplication::applicationDirPath();
    QApplication::addLibraryPath(appDir);

    // A plugin's catalog base is only registered while the plugin loads, so a base that appears
    // now would otherwise stay untranslated until the user touched the language selector.
    const QStringList basesBeforePlugins = QcwTranslations::catalogBases();

    if (loadPlugins && !QcwPluginLoader::loadPlugins(QcwPluginLoader::defaultPluginDirectories()))
    {
        qWarning() << "Errors occurred while loading plugins";
    }

    if (QcwTranslations::catalogBases() != basesBeforePlugins)
    {
        QcwTranslations::applyStoredLanguage();
    }

    QApplication::styleHints()->setShowShortcutsInContextMenus(true);

    // Every quit path sends QEvent::Quit to the application, so this filter is what turns
    // them all into a question to the views; without it a quit could drop a deferred close.
    qApp->installEventFilter(this);

    impl->initialized = true;

    // The approved exits have closed the windows before the quit gets here. This handler
    // covers the paths that never asked — QCoreApplication::exit() and the workbench's own
    // destructor — by saving the session and closing what is still open.
    connect(qApp, &QApplication::aboutToQuit, this, [this]()
    {
        qDebug() << "QApplication::aboutToQuit()";
        impl->saveState();
        impl->closeAllWindows();
        cleanupBeforeQuitting();
    });

    return true;
}

QcwTopicService* QcwWorkbench::topicService() const
{
    return impl->topicService;
}

bool QcwWorkbench::start()
{
    if (!impl->initialized)
    {
        qWarning() << "QcwWorkbench::start(): initialize() has not succeeded yet";
        return false;
    }

    if (impl->started)
    {
        qWarning() << "QcwWorkbench::start(): the workbench has already been started";
        return false;
    }

    impl->started = true;

    if (!preStart())
    {
        return false;
    }

    registerGlobalActions();
    QcwBuiltinCommands::registerCommands();
    registerFolderRendererCreators();
    windowStyler->initialize();
    registerPreferencesPages();

    savedPerspectives->initialize();

    impl->restoreState();

    const bool started = postStart();

    // Last, so commands an application registers while its windows come up are included.
    if (qEnvironmentVariableIntValue("QCW_DUMP_ID_CATALOG") > 0)
    {
        dumpRegisteredIds();
    }

    return started;
}

bool QcwWorkbench::setConfigDirectory()
{
    return QcwAppDirectories::setConfigDirectory(
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
}

void QcwWorkbench::registerGlobalActions()
{
    auto* newWindow = new QcwLambdaCommandHandler(this);
    newWindow->onExecute([this](const QVariantMap&)
    {
        impl->createNewWindow();
    });
    registerGlobalHandler(QcwWindowActionIds::ActionNewWindow, newWindow);

    auto* removeWindow = new QcwLambdaCommandHandler(this);
    removeWindow->onExecute([this](const QVariantMap& params)
    {
        // The window whose UI ran the command carries its own id in it; without a target this
        // call says nothing, and an explicit id is how one window removes another.
        const auto windowId = params.value(QString(QcwWindowActionIds::ParamWindowId)).toString();
        if (windowId.isEmpty())
        {
            return;
        }
        impl->removeWindow(windowId);
    });
    registerGlobalHandler(QcwWindowActionIds::ActionRemoveWindow, removeWindow);

    auto* exit = new QcwLambdaCommandHandler(this);
    exit->onExecute([this](const QVariantMap&)
    {
        impl->requestExit();
    });
    registerGlobalHandler(QcwWindowActionIds::ActionExit, exit);

    auto* preferences = new QcwLambdaCommandHandler(this);
    preferences->onExecute([this](const QVariantMap&)
    {
        if (impl->preferencesDialog)
        {
            impl->preferencesDialog->raise();
            impl->preferencesDialog->activateWindow();
            return;
        }

        impl->preferencesDialog = new QcwPreferencesDialog;
        impl->preferencesDialog->setAttribute(Qt::WA_DeleteOnClose);
        impl->preferencesDialog->resize(900, 640);
        impl->preferencesDialog->show();
    });
    registerGlobalHandler(QcwWindowActionIds::ActionPreferences, preferences);

    auto* palette = new QcwLambdaCommandHandler(this);
    palette->onExecute([this](const QVariantMap&)
    {
        if (auto* window = impl->currentWindow())
        {
            window->openCommandPalette();
        }
    });
    registerGlobalHandler(QcwWindowActionIds::ActionCommandPalette, palette);

    auto* themes = new QcwLambdaCommandHandler(this);
    themes->onExecute([](const QVariantMap&)
    {
        auto* dialog = new QcwThemeDialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    registerGlobalHandler(QcwWindowActionIds::ActionThemes, themes);
}

void QcwWorkbench::retranslateGlobalActions()
{
    // The built-in command labels are re-registered by QcwBuiltinCommands::registerCommands()
    // when each window handles its own QEvent::LanguageChange, and the registry's
    // commandsChanged() updates the command QActions that display them. A subclass that
    // registered commands of its own re-registers them here.
}

void QcwWorkbench::registerGlobalHandler(const QString& commandId, QcwCommandHandler* handler)
{
    if (commandId.isEmpty() || !handler)
    {
        return;
    }

    impl->globalHandlers.insert(commandId, handler);
    impl->dropHandlerConnections(commandId);

    QcwWorkbenchPrivate::GlobalHandlerConnections connections;
    connections.stateChanged = connect(handler, &QcwCommandHandler::stateChanged, this, [this, commandId]()
    {
        emit globalHandlerStateChanged(commandId);
    });
    connections.destroyed = connect(handler, &QObject::destroyed, this, [this, commandId]()
    {
        impl->dropHandlerConnections(commandId);
        impl->globalHandlers.remove(commandId);
        emit globalHandlerStateChanged(commandId);
    });
    impl->globalHandlerConnections.insert(commandId, connections);

    emit globalHandlerStateChanged(commandId);
}

void QcwWorkbench::unregisterGlobalHandler(const QString& commandId)
{
    if (impl->globalHandlers.remove(commandId) == 0)
    {
        return;
    }

    impl->dropHandlerConnections(commandId);
    emit globalHandlerStateChanged(commandId);
}

void QcwWorkbenchPrivate::dropHandlerConnections(const QString& commandId)
{
    auto it = globalHandlerConnections.find(commandId);
    if (it == globalHandlerConnections.end())
    {
        return;
    }

    const auto connections = it.value();
    globalHandlerConnections.erase(it);

    QObject::disconnect(connections.stateChanged);
    QObject::disconnect(connections.destroyed);
}

QcwCommandHandler* QcwWorkbench::globalHandler(const QString& commandId) const
{
    return impl->globalHandlers.value(commandId, nullptr);
}

void QcwWorkbench::registerPreferencesPages()
{
    QcwPreferencesPage* page = new QcwAppearancePreferencesPage;
    if (!preferencesManager->registerPage(page))
    {
        delete page;
    }

    page = new QcwNotificationPreferencesPage;
    if (!preferencesManager->registerPage(page))
    {
        delete page;
    }
}

void QcwWorkbench::registerFolderRendererCreators()
{
    // The docking library falls back to the style's standard icons; the workbench keeps its
    // palette-tinted set by providing them here, before any folder asks for one.
    QcwTabFolderRenderer::setIconProvider([](QcwTabFolderRenderer::IconKind kind)
    {
        switch (kind)
        {
        case QcwTabFolderRenderer::IconKind::Maximize:
            return qcw::themeIcon(qcw::Icon::FolderMaximize);
        case QcwTabFolderRenderer::IconKind::Restore:
            return qcw::themeIcon(qcw::Icon::FolderRestore);
        case QcwTabFolderRenderer::IconKind::Minimize:
            return qcw::themeIcon(qcw::Icon::FolderMinimize);
        case QcwTabFolderRenderer::IconKind::View:
            return qcw::themeIcon(qcw::Icon::View);
        }
        return QIcon();
    });

    folderRendererCreators->registerCreator(QLatin1String("QcwDefaultTabFolderRenderer"),
                                            [](QcwTabFolder* folder) -> QcwTabFolderRenderer*
    {
        return new QcwDefaultTabFolderRenderer(folder);
    });

    folderRendererCreators->registerCreator(QLatin1String("QcwStyledTabFolderRenderer"),
                                            [](QcwTabFolder* folder) -> QcwTabFolderRenderer*
    {
        return new QcwStyledTabFolderRenderer(folder);
    });
}

bool QcwWorkbench::preStart()
{
    return true;
}

bool QcwWorkbench::postStart()
{
    return true;
}

void QcwWorkbench::cleanupBeforeQuitting()
{
}

// -----------------------------------------------------------------------------

QcwWorkbenchPrivate::QcwWorkbenchPrivate(QcwWorkbench* interface) :
    owner { interface }
{
    topicService = new QcwTopicService { interface };
}

void QcwWorkbenchPrivate::createNewWindow()
{
    auto window = owner->createWindow();
    if (!window)
    {
        qWarning() << "QcwWorkbench: createWindow() returned nullptr; no window was created";
        return;
    }

    window->setId(makeWindowId());
    window->setupUi();
    windowStyler->applyStyle(window);

    if (!trackWindow(window))
    {
        window->deleteLater();
        return;
    }

    window->resize(1024, 768);
    window->show();
}

QcwWindow* QcwWorkbenchPrivate::currentWindow() const
{
    const auto owningWindow = [this](QWidget* widget) -> QcwWindow*
    {
        for (auto* current = widget; current; current = current->parentWidget())
        {
            if (auto* window = qobject_cast<QcwWindow*>(current))
            {
                return window;
            }

            // A floating folder is a top-level of its own; its WindowId names the window that
            // owns it.
            const auto windowId = current->property("WindowId").toString();
            if (!windowId.isEmpty())
            {
                if (auto* window = windows.value(windowId))
                {
                    return window;
                }
            }
        }

        return nullptr;
    };

    // The active window may be a dialog of the window (the command palette itself, for
    // example), so the parent chain decides which window the user is working in.
    if (auto* window = owningWindow(QApplication::activeModalWidget()))
    {
        return window;
    }

    if (auto* window = owningWindow(QApplication::activeWindow()))
    {
        return window;
    }

    if (windows.size() == 1)
    {
        return windows.constBegin().value();
    }

    return nullptr;
}

bool QcwWorkbenchPrivate::trackWindow(QcwWindow* window)
{
    const auto windowId = window->id();
    if (!isValidWindowId(windowId) || windows.contains(windowId))
    {
        qWarning() << "QcwWorkbench: refusing a window with an empty, invalid or duplicate id:" << windowId;
        return false;
    }

    windows.insert(windowId, window);

    window->setWorkbench(owner);

    QObject::connect(window, &QObject::destroyed, owner, [this, window]()
    {
        for (auto it = windows.cbegin(); it != windows.cend(); ++it)
        {
            if (it.value() == window)
            {
                windows.erase(it);
                break;
            }
        }
    });

    return true;
}

void QcwWorkbenchPrivate::removeWindow(const QString& windowId)
{
    if (shuttingDown || pendingRemovals.contains(windowId))
    {
        return;
    }

    auto it = windows.find(windowId);
    if (it == windows.end())
    {
        return;
    }

    auto window = it.value();
    pendingRemovals.insert(windowId);

    QPointer<QcwWindow> windowGuard { window };
    window->canClose([this, windowId, windowGuard](bool allowed)
    {
        pendingRemovals.remove(windowId);

        if (shuttingDown || !allowed || !windowGuard)
        {
            return;
        }

        const auto entry = windows.find(windowId);
        if (entry == windows.end() || entry.value() != windowGuard.data())
        {
            return;
        }

        auto* window = windowGuard.data();
        windows.erase(entry);

        // The views have answered, and a removed window leaves no state file behind.
        window->setSaveStateOnClose(false);
        window->forceClose();

        // Called from the window's own menu action, so the window cannot be deleted while
        // the trigger is still on the stack.
        window->deleteLater();

        QFile::remove(winStateFilePath(windowId));
    });
}

bool QcwWorkbench::eventFilter(QObject* watched, QEvent* event)
{
    // Qt sends this event on qApp before forwarding it to the top-level windows, which is the
    // order the retranslation needs: retranslateGlobalActions() lets the application re-register
    // its own command texts before each window resolves the built-ins and refreshes its actions.
    if (watched == qApp && event->type() == QEvent::LanguageChange)
    {
        retranslateGlobalActions();
        return false;
    }

    // QApplication answers QEvent::Quit by closing every window, and drops the quit as soon
    // as one window defers its close — which a view with an asynchronous confirmation always
    // does. The workbench therefore stops the quit here, asks the views itself, and lets the
    // quit that follows their approval through.
    if (watched == qApp && event->type() == QEvent::Quit)
    {
        if (impl->shuttingDown || impl->exitApproved)
        {
            return false;
        }

        impl->requestExit();
        return true;
    }

    return QObject::eventFilter(watched, event);
}

void QcwWorkbenchPrivate::requestExit()
{
    if (exitPending || shuttingDown)
    {
        return;
    }

    exitPending = true;

    struct ExitState
    {
        int pending { 0 };
        bool refused { false };
    };

    const auto windowsToAsk = windows.values();
    if (windowsToAsk.isEmpty())
    {
        // Last window removed: there is nothing to ask and nothing to close.
        exitApproved = true;
        QCoreApplication::quit();
        return;
    }

    const auto state = std::make_shared<ExitState>();
    state->pending = windowsToAsk.size();

    // Called once per window: its views' answer, or its destruction while the answer was
    // outstanding. The quit waits for all of them so one refusal keeps every window open.
    auto answer = [this, state](bool allowed)
    {
        if (!allowed)
        {
            state->refused = true;
        }

        if (--state->pending > 0)
        {
            return;
        }

        if (!state->refused && !shuttingDown)
        {
            // Every window answered; the close that follows must not ask again, and must be
            // synchronous so the quit reaches QApplication with no window left to close.
            exitApproved = true;
            closeAllWindows();
            QCoreApplication::quit();
        }

        exitPending = false;
    };

    for (auto window : windowsToAsk)
    {
        if (!window)
        {
            answer(true);
            continue;
        }

        auto answered = std::make_shared<bool>(false);
        auto once = [answer, answered](bool allowed)
        {
            if (*answered)
            {
                return;
            }

            *answered = true;
            answer(allowed);
        };

        // A window that disappears while its views are being asked has nothing left to save.
        QObject::connect(window, &QObject::destroyed, owner, [once]()
        {
            once(true);
        });

        window->canClose([once](bool allowed)
        {
            once(allowed);
        });
    }
}

void QcwWorkbenchPrivate::closeAllWindows()
{
    // An approved exit closes the windows before the quit leaves the workbench, and a second
    // close would save their state and emit aboutToClose a second time.
    if (windowsClosed)
    {
        return;
    }

    windowsClosed = true;

    const auto windowsToClose = windows;
    for (auto window : windowsToClose)
    {
        // The shutdown path cannot cancel, and an approved close has already asked.
        if (window)
        {
            window->forceClose();
        }
    }
}

void QcwWorkbenchPrivate::saveState() const
{
    QByteArray xmlData;
    QXmlStreamWriter writer(&xmlData);
    writer.writeDTD(R"(<?xml version="1.0" encoding="UTF-8"?>)");

    writer.writeStartElement("workbench"); // --> workbench begin
    writer.writeAttribute("version", QString::number(kWorkbenchStateVersion));
    writer.writeAttribute("extraState", owner->extraStateToHex());

    writer.writeStartElement("windows"); // --> windows begin
    writer.writeAttribute("count", QString::number(windows.size()));
    for (auto it = windows.cbegin(); it != windows.cend(); ++it)
    {
        writer.writeStartElement("window"); // --> window begin
        writer.writeAttribute("id", it.key());
        writer.writeEndElement(); // --> window end
    }
    writer.writeEndElement(); // --> windows end

    writer.writeEndElement(); // --> workbench end
    writer.writeEndDocument();

    QSaveFile file(stateFilePath());
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "QcwWorkbench: cannot write the session state to" << stateFilePath() << ":" << file.errorString();
        return;
    }

    if (file.write(xmlData) != xmlData.size() || !file.commit())
    {
        qWarning() << "QcwWorkbench: cannot save the session state to" << stateFilePath() << ":" << file.errorString();
    }
}

void QcwWorkbenchPrivate::restoreState()
{
    QList<QString> windowIds;
    QMap<QString, QVariant> extraState;

    QFile file(stateFilePath());
    if (file.open(QIODevice::ReadOnly))
    {
        QXmlStreamReader reader(&file);
        if (reader.readNextStartElement() && reader.name() == QStringLiteral("workbench"))
        {
            const int version = reader.attributes().value("version").toInt();
            if (version > kWorkbenchStateVersion)
            {
                qWarning() << "QcwWorkbench: ignoring the session state in" << stateFilePath()
                           << "written with unsupported version" << version;
            }
            else
            {
                const auto extraStateHex = reader.attributes().value("extraState").toString();
                if (!extraStateHex.isEmpty())
                {
                    extraState = QcwSerializeUtils::deserialize<QMap<QString, QVariant>>(
                        QByteArray::fromHex(extraStateHex.toLatin1()));
                }

                while (reader.readNextStartElement())
                {
                    if (reader.name() != QStringLiteral("windows"))
                    {
                        reader.skipCurrentElement();
                        continue;
                    }

                    while (reader.readNextStartElement())
                    {
                        if (reader.name() == QStringLiteral("window"))
                        {
                            const auto windowId = reader.attributes().value("id").toString();
                            if (isValidWindowId(windowId) && !windowIds.contains(windowId))
                            {
                                windowIds.append(windowId);
                            }
                            else
                            {
                                qWarning()
                                    << "QcwWorkbench: ignoring an invalid or duplicate window id in the session state:"
                                    << windowId;
                            }
                        }
                        reader.skipCurrentElement();
                    }
                }
            }
        }

        if (reader.hasError())
        {
            qWarning() << "QcwWorkbench: cannot read the session state from" << stateFilePath() << ":"
                       << reader.errorString();
        }
    }

    for (const auto& windowId : std::as_const(windowIds))
    {
        auto window = owner->createWindow();
        if (!window)
        {
            qWarning() << "QcwWorkbench: createWindow() returned nullptr while restoring window" << windowId;
            continue;
        }

        const auto stateFile = winStateFilePath(windowId);
        if (!window->restoreStateFromFile(stateFile))
        {
            qWarning() << "QcwWorkbench: cannot restore window" << windowId << "from" << stateFile;
            window->deleteLater();

            // Keep an unusable file out of the way. The workbench state is rewritten on exit,
            // so this also keeps a corrupt file from being retried on the next start.
            const QString corruptFile = stateFile + QStringLiteral(".corrupt");
            QFile::remove(corruptFile);
            if (QFile::rename(stateFile, corruptFile))
            {
                qWarning() << "QcwWorkbench: moved the unusable state file to" << corruptFile;
            }
            else if (QFile::exists(stateFile))
            {
                qWarning() << "QcwWorkbench: cannot move the unusable state file" << stateFile;
            }
            continue;
        }

        // QcwWindow adopts the id stored in its own file; the workbench follows that id so the
        // next session and the window's next state file agree.
        if (window->id() != windowId)
        {
            qWarning() << "QcwWorkbench: the state file" << stateFile << "belongs to the window" << window->id()
                       << "instead of" << windowId;
        }

        if (!trackWindow(window))
        {
            window->deleteLater();
            continue;
        }

        windowStyler->applyStyle(window);
        window->show();
    }

    if (windows.isEmpty())
    {
        createNewWindow();
    }

    owner->restoreExtraState(extraState);
}

QString QcwWorkbenchPrivate::stateFilePath() const
{
    return QString("%1/QcwWorkbench.xml").arg(QcwAppDirectories::configDirectory());
}

QString QcwWorkbenchPrivate::winStateFilePath(const QString& winId) const
{
    return QString("%1/%2.xml").arg(QcwAppDirectories::configDirectory(), winId);
}

QString QcwWorkbenchPrivate::makeWindowId()
{
    auto now = std::time(nullptr);
    auto id = QString("QcwWindow-%1").arg(now);
    while (windows.contains(id))
    {
        id = QString("QcwWindow-%1").arg(++now);
    }
    return id;
}

QT_END_NAMESPACE
