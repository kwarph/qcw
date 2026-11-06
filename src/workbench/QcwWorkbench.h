// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPersistable.h"
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE

class QcwWindow;
class QcwWorkbenchPrivate;
class QcwCommandHandler;
class QcwTopicService;

/**
 * @class QcwWorkbench
 * @brief Owns the application's workbench windows and the session that survives restarts.
 *
 * @details Subclass QcwWorkbench and implement createWindow(); the base class does the rest:
 * application metadata, directories, logging, themes, plugins, the global command handlers and the
 * per-window state files. The usual main() is:
 * @code
 * QApplication app(argc, argv);
 * Workbench workbench;
 * if (!workbench.initialize(...)) return 1;
 * if (!workbench.start()) return 1;
 * return app.exec();
 * @endcode
 *
 * @par Startup
 * initialize() runs first, needs a constructed QApplication and may be called once; it
 * applies everything that is process-wide. start() runs the hooks, restores the session and
 * may also be called once. preStart() runs before any window exists; postStart() runs after
 * the session windows are up and its result becomes start()'s result.
 *
 * @par Windows and session
 * Every window comes from createWindow() and belongs to the workbench. The workbench keeps
 * the windows by id, writes that id list on shutdown, and next start asks each window to
 * restore its own state file. When the list cannot be read or none of its windows can be
 * restored, one fresh window is created instead. Closing a window keeps it in the session
 * (Window > Remove This Window is the command that forgets it and deletes its state file);
 * the workbench deletes the remaining window objects in its destructor.
 *
 * @par Shutdown
 * Both the Exit command and QCoreApplication::quit() first ask the views of every window
 * through QcwView::canClose(); the Exit command does it directly and every quit that reaches
 * the application as a QEvent::Quit does it through eventFilter(). One refusal keeps every
 * window open and cancels the quit. When all views agree the workbench closes the windows —
 * each saves its own state and closes its floating folders, without asking again — and lets
 * the quit proceed. The QApplication::aboutToQuit handler then saves the session and calls
 * cleanupBeforeQuitting(); it also closes the windows on shutdown paths that bypass the
 * question, such as QCoreApplication::exit() or a destructor, where nothing can be cancelled.
 *
 * @see QcwWindow, QcwPluginLoader, QcwBuiltinCommands, QcwSavedPerspectives
 */
class QCW_EXPORT QcwWorkbench : public QObject, public QcwPersistable
{
    Q_OBJECT
public:
    /**
     * @brief Builds the workbench object.
     * @details Only the private state is allocated here. Application metadata, directories,
     * logging, plugins and command handlers come with initialize(); windows come with start().
     */
    QcwWorkbench();

    /**
     * @brief Destroys the workbench, its remaining windows and the global handlers it registered.
     * @note The session is written in the aboutToQuit handler. A workbench destroyed while
     *       the application keeps running closes and deletes its windows — which saves each
     *       window's own state file — but does not write the session file.
     */
    ~QcwWorkbench() override;

    /**
     * @brief Applies the process-wide workbench setup.
     * @param appName Value for QCoreApplication::applicationName().
     * @param appDisplayName Value for QCoreApplication::applicationDisplayName().
     * @param orgDomain Value for QCoreApplication::organizationDomain().
     * @param orgName Value for QCoreApplication::organizationName().
     * @param appVersion Value for QCoreApplication::applicationVersion().
     * @param quitOnLastWindowClosed Passed to QApplication::setQuitOnLastWindowClosed().
     * @param loadPlugins Whether to load the plugins found in
     *                    QcwPluginLoader::defaultPluginDirectories() — next to the
     *                    executable and, on Unix, in the installation's private library
     *                    directory.
     * @return False when no QApplication exists yet, when initialize() has already run, or
     *         when the config or log directory cannot be created; true otherwise.
     *
     * @details Sets the application and organisation identity first, because the settings
     * provider and both directories are derived from it. It then installs the workbench's INI
     * settings provider (unless the application installed one first), creates the directories,
     * prunes old log files, installs QcwLogger, initializes the theme manager, applies the
     * stored appearance settings (UI language, font and followed system theme), adds the
     * executable directory to the library path, loads plugins, enables shortcut mnemonics in
     * context menus, registers the standard global command handlers and connects the aboutToQuit
     * handler that performs the shutdown.
     *
     * Plugin failures are reported by the loader and logged, but they do not fail this call:
     * a workbench without its optional plugins is still usable.
     *
     * @note Call it after the QApplication object exists and before start(); a second call is
     *       refused.
     * @see start(), registerGlobalActions()
     */
    bool initialize(const QString& appName,
                    const QString& appDisplayName,
                    const QString& orgDomain,
                    const QString& orgName,
                    const QString& appVersion,
                    bool quitOnLastWindowClosed = true,
                    bool loadPlugins = true);

    /**
     * @brief The application-scope topic service.
     * @return The service, which is a child of the workbench and lives as long as it does;
     *         never null.
     * @details Topics published here are visible to every subscriber of this service, whatever
     *          window it lives in; per-window topics belong to QcwWindow::topicService().
     *          QcwTopicService::forApplication() is the static shortcut for code that has no
     *          workbench pointer. The service caches the last payload per topic and replays it
     *          to late subscribers; see QcwTopicService for the full contract.
     * @see QcwTopicService, QcwTopicService::forApplication(), QcwWindow::topicService()
     */
    QcwTopicService* topicService() const;

    /**
     * @brief Registers @p handler for @p commandId at the process scope.
     * @details Replaces an existing handler and notifies windows, so their command actions pick
     * up the new handler's state. The workbench borrows the handler: it is not reparented or
     * deleted, replacing it leaves the previous handler with its owner, and a handler destroyed
     * elsewhere unregisters itself.
     */
    void registerGlobalHandler(const QString& commandId, QcwCommandHandler* handler);

    /// Removes the process-scope handler for @p commandId and notifies windows, which re-resolve
    /// the command and grey it out when no other scope answers. The handler keeps its owner and
    /// is not deleted.
    void unregisterGlobalHandler(const QString& commandId);

    /// The process-scope handler for @p commandId, or nullptr.
    QcwCommandHandler* globalHandler(const QString& commandId) const;

    /**
     * @brief Restores the saved session and hands control to the application.
     * @return False when called before initialize() or a second time, when preStart()
     *         returns false, or whatever postStart() returns; true when the session is up
     *         and postStart() succeeded.
     *
     * @details Runs preStart(), registers the global command handlers, the built-in commands, the
     * folder renderer creators and the preferences pages, initializes the window styler and the
     * saved-perspective store, recreates the windows listed in the session state (falling back to
     * one new window when none can be restored) and finishes with postStart(). Setting
     * @c QCW_DUMP_ID_CATALOG=1 makes it end with a dump of the registered command, action set
     * and contribution ids, for comparing a missing menu entry against the catalogs.
     *
     * @note Call it only once; there is no second session restore.
     * @see initialize()
     */
    bool start();

    /**
     * @brief Answers the application's quit request on the views' behalf.
     * @return True when @p event is a quit that the workbench turned into a question to the
     *         windows; the quit that follows their approval passes through.
     *
     * @details QApplication closes the windows as part of a quit and abandons the quit as
     * soon as one window defers its close, which is what a view with an asynchronous
     * confirmation does. The workbench therefore filters QEvent::Quit, asks every window
     * through QcwWindow::canClose(), and only when all of them agree closes the windows and
     * lets the quit proceed. A refusal cancels the quit, exactly as it cancels the Exit
     * command. QCoreApplication::exit() does not send the event and is not intercepted.
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    /// Emitted when the process-scope answer for @p commandId may have changed: the handler was
    /// registered, unregistered, destroyed, or reported stateChanged(). Windows refresh that
    /// command's QAction.
    void globalHandlerStateChanged(const QString& commandId);

protected:
    /**
     * @brief Creates one empty workbench window.
     * @return The new window, or nullptr when the application cannot create one; the caller
     *         then skips that session entry and logs a warning.
     *
     * @details Called once per restored window, once for the fresh window when no session
     * can be restored, and from the New Window command. Implementations must return a new
     * top-level QcwWindow without a parent. For a new window the workbench assigns its id
     * and calls setupUi(); for a restored window the id and the contents come from its state
     * file. Either way the workbench then shows the window and owns it.
     */
    virtual QcwWindow* createWindow() = 0;

    /**
     * @brief Registers the workbench's global command handlers.
     * @details Called once from start(), after preStart(). The default handles New Window,
     * Remove Window, Command Palette, Exit, Preferences and Themes at the workbench scope, the
     * last stop of QcwWindow::resolveHandler(); a view, perspective or window that handles the
     * same id wins. The commands themselves are catalogued by QcwBuiltinCommands, and an override
     * that wants the defaults must call the base implementation.
     * @see registerGlobalHandler()
     */
    virtual void registerGlobalActions();

    /**
     * @brief Re-applies the texts of the commands the workbench itself catalogued.
     * @details Called when the UI language changes. The built-in labels need nothing here: every
     * window re-registers them through QcwBuiltinCommands::registerCommands() as it handles the
     * change, and commandsChanged() refreshes the command QActions that show them. An override
     * that registered commands of its own re-registers them here.
     * @see registerGlobalActions()
     */
    virtual void retranslateGlobalActions();

    /**
     * @brief Registers the workbench's built-in preferences pages.
     * @details Called once from start(), after registerFolderRendererCreators(). The default registers
     * the Appearance and Notifications pages. An override that wants those entries must call
     * the base implementation.
     */
    virtual void registerPreferencesPages();

    /**
     * @brief Registers the renderer creators that window style files can refer to.
     * @details Called once from start(), right after registerGlobalActions(). The default registers
     * QcwDefaultTabFolderRenderer and QcwStyledTabFolderRenderer under their class names, which
     * are the values a <tt>QcwWindow.<i>style</i>.theme</tt> file uses for
     * @c folderRenderer. An override that wants those entries must call the base
     * implementation, then register its own creators.
     */
    virtual void registerFolderRendererCreators();

    /**
     * @brief Hook run at the beginning of start(), before any window exists.
     * @return False to abort the start; start() then returns false without restoring the
     *         session.
     * @details Use it for application setup that must precede the windows, such as
     * registering application preferences pages or settings defaults.
     */
    virtual bool preStart();

    /**
     * @brief Hook run at the end of start(), after the session windows are shown.
     * @return The value start() returns; false leaves the windows open but reports the start
     *         as failed to the caller.
     * @details Use it to open initial views or to report the state of the loaded application.
     */
    virtual bool postStart();

    /**
     * @brief Hook run on application shutdown.
     * @details Called from the aboutToQuit handler after the session state has been written
     * and the windows have been closed, so the windows and their floating folders are already
     * gone.
     */
    virtual void cleanupBeforeQuitting();

private:
    QcwWorkbenchPrivate* impl;
    friend class QcwWorkbenchPrivate;
};

QT_END_NAMESPACE
