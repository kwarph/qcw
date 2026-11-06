// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwCommand.h"
#include "QcwPersistable.h"
#include <QMainWindow>
#include <QList>
#include <QVariantMap>
#include <functional>

QT_BEGIN_NAMESPACE

class QAction;
class QCloseEvent;
class QEvent;
class QMenu;
class QToolBar;
class QXmlStreamReader;
class QXmlStreamWriter;
class QcwPerspective;
class QcwTabFolder;
class QcwView;
class QcwWindowPrivate;
class QcwCommandHandler;
class QcwWorkbench;
class QcwTopicService;

/**
 * @class QcwWindow
 * @brief A workbench window that shows one perspective at a time.
 *
 * @details A window owns a central area into which the folder container of the current
 * perspective is installed, a perspective tool bar for switching between them, and the
 * standard File/Edit/Window/Help menus built from the contributions registered for them.
 * A dummy perspective, created during initialization and never closed, holds the
 * views that belong to no real perspective; when a perspective is closed it is the
 * fallback. The shared folder, if the window has one, is a single QcwTabFolder handed to
 * every perspective's container, so it survives perspective switches.
 *
 * @par Initialization
 * Call exactly one of setupUi() (fresh window) or restoreStateFromFile() (saved session).
 * Both build the UI, run postSetupUi() once and mark the window initialized; the other
 * path is then refused. QcwWorkbench, for example, calls restoreStateFromFile() on the
 * windows it recreates and setupUi() on a new one. A window creates its widgets, menus and
 * the perspective tool bar in the constructor, so those exist before either call.
 *
 * @par Menus and actions
 * The menus are built from QcwContributionRegistry: each holds the contributions whose target
 * is its id, ordered by QcwContributionSorter and divided by a separator wherever a groupId
 * changes. An entry is the window's command QAction for that command id — the very object the
 * shortcut, the tool bar and the palette share, not a copy — so the same command answers
 * identically wherever it appears. A command the catalog does not list cannot be placed at all.
 *
 * The command QAction mirrors QcwCommandInfo (label, icon, shortcut, checkable) and the handler
 * resolveHandler() finds for the current view, else the current perspective, else the window,
 * else the workbench. Which entries exist is the contributions' decision and whether they can run
 * is the handler's, so a command with no handler stays in place, greyed out; a tab switch then
 * moves the states without rebuilding anything. Only a handler that reports itself invisible
 * withdraws the entry, and since Qt disables a hidden QAction, its shortcut is silent as well.
 * Applications add commands by registering QcwCommandInfo in QcwCommandRegistry and placing it
 * with a contribution; ids that belong to one application stay out of QcwWindowActionIds.
 *
 * getMenu() and registeredMenus() read the menu table, and getToolBar() and registeredToolBars()
 * the tool bar table; the entries in both are command QActions and answer to commandAction().
 *
 * @par State
 * saveStateToFile() writes the complete window state — geometry, view identity,
 * shared folder, dummy perspective and perspectives — to an XML file. What the window
 * itself does not know is delegated to saveMoreState() and restoreMoreState(). The
 * default closeEvent() saves when saveStateOnClose() is true and then closes the floating
 * folders, which are top-level windows independent of this one.
 *
 * @see QcwPerspective, QcwTabFolder, QcwView, QcwWindowActionIds
 */
class QCW_EXPORT QcwWindow : public QMainWindow, public QcwPersistable
{
    Q_OBJECT
public:
    /**
     * @brief Builds the window shell: central widget, status bar, menus, actions and the
     *        perspective tool bar.
     * @param parent The parent widget, as usual for a QWidget.
     * @note No perspective or shared folder exists until setupUi() or
     *       restoreStateFromFile() has run.
     */
    explicit QcwWindow(QWidget* parent = nullptr);

    /**
     * @brief Destroys the window and its private implementation.
     * @details The floating folders are independent top-level windows, so they are closed
     *          here first — without asking their views, which cannot keep a window that is
     *          already being destroyed open. The shared folder is deleted with the window.
     */
    ~QcwWindow() override;

    /**
     * @brief Builds the initial UI for a window without saved state.
     *
     * @details Creates the shared folder through createSharedFolder() (if any), creates the
     * dummy perspective with its initial layout, installs that layout in the central area,
     * makes the dummy current and calls postSetupUi(). This is the fresh-session
     * alternative to restoreStateFromFile().
     *
     * @note Ignored once the window is initialized, either by an earlier setupUi() or by a
     *       successful restoreStateFromFile().
     */
    void setupUi();

    /**
     * @brief The window identifier.
     * @return The id set with setId(), empty until the owner assigns one.
     * @details The id names the window's state file (@c id.xml) and is stamped on every
     *          folder that reports back through the floating folder monitor.
     * @see setId()
     */
    QString id() const;

    /**
     * @brief Sets the window identifier.
     * @param id The new identifier.
     * @note Set it before setupUi()/restoreStateFromFile(): the dummy perspective's folders
     *       are stamped with it while those run. Changing it later does not re-stamp
     *       folders that already exist.
     */
    void setId(const QString& id);

    /**
     * @brief Adds @p perspective to the window and shows it.
     * @param perspective A perspective created for this window, or nullptr.
     * @details A perspective that has no id yet — the default of a freshly created one — is
     *          assigned the next free id here before it is added. A perspective whose id is
     *          already registered is not installed: the existing perspective is shown and
     *          @p perspective is left untouched, so the caller keeps ownership of it.
     * @note Does nothing for nullptr, for the window's dummy perspective, or before
     *       setupUi()/restoreStateFromFile() has created the dummy perspective.
     */
    void openPerspective(QcwPerspective* perspective);

    /**
     * @brief Removes the perspective with @p perspectiveId from the window.
     * @param perspectiveId The id of the perspective to close.
     * @details First asks the views this close actually ends through QcwView::canClose() —
     *          the views of its docked and floating folders whose tabs no other perspective
     *          holds. A view another perspective still shows, and one the shared folder
     *          holds, stays open with its work, so it is not asked. Once they agree, clears
     *          the perspective (which closes its floating folders and deletes its fast view
     *          tool bars), removes its tool bar button, and destroys it with deleteLater().
     *          If it was current, another registered perspective becomes current, or the
     *          dummy perspective when none is left. A view may answer asynchronously, so the
     *          removal can happen after this call has returned; a refusal leaves the
     *          perspective untouched.
     * @note Ignored for unknown ids and for the dummy perspective.
     * @see QcwView::canClose()
     */
    void closePerspective(int perspectiveId);

    /**
     * @brief Makes the perspective with @p perspectiveId the visible one.
     * @param perspectiveId The id of the perspective to show.
     * @details Swaps the perspective's folder container into the central layout, updates
     *          the tool bar selection, shows the perspective (including its floating
     *          folders) and re-asserts its active folder.
     * @note An unknown id falls back to the dummy perspective; before initialization, when
     *       there is no dummy perspective yet, the call does nothing. Re-selecting the
     *       perspective that is already current re-asserts its active folder instead of
     *       doing nothing; that is how the status bar and the command actions are rebuilt
     *       after a restore. Switching to a different perspective also rebuilds the menus,
     *       because the new perspective lists different visible action sets.
     */
    void setCurrentPerspective(int perspectiveId);

    /**
     * @brief The perspective currently installed in the central area.
     * @return The current perspective, or nullptr before initialization succeeds.
     */
    QcwPerspective* currentPerspective() const;

    /**
     * @brief The view currently on screen: the current perspective's active tab.
     * @return The current QcwView, or nullptr when there is none — before initialization,
     *         when the active folder is empty, or when its current tab is not a view.
     * @details Only this view owns the status bar, and only its command handlers take part in
     *          resolution, so which entries the menus show and what they do follows the view on
     *          screen. It is also the only view that should publish window-scoped topic data
     *          (for example its selection): background tabs and hidden perspectives stay silent.
     * @see topicService(), currentViewChanged()
     */
    QcwView* currentView() const;

    /**
     * @brief The window-scope topic service.
     * @return The service, which is a child of the window and lives as long as it does; nullptr
     *         only once the window's destructor has freed its private.
     * @details Topics published here are visible only to subscribers of this window; topics
     *          shared by the process belong to QcwWorkbench::topicService(). A view reaches this
     *          service through owningWindow()->topicService() when it publishes "win/..." topics,
     *          normally only while it is current; a view that consumes them calls
     *          QcwView::subscribeWindowTopic() instead of holding on to this pointer. The service
     *          caches the last payload per topic and replays it to late subscribers; see
     *          QcwTopicService for the full contract.
     * @see QcwTopicService, QcwView::subscribeWindowTopic(), currentView(),
     *      QcwWorkbench::topicService()
     */
    QcwTopicService* topicService() const;

    /**
     * @brief Every perspective of the window, the dummy perspective included.
     * @return The registered perspectives, followed by the dummy perspective; empty before
     *         setupUi() has installed the dummy one.
     * @details The window keeps ownership of every entry. The dummy perspective is not part of
     *          the registered set, so this accessor is the only way to reach it together with
     *          the others. The list is a snapshot: mutating the window's perspectives while
     *          iterating does not change it.
     */
    QList<QcwPerspective*> allPerspectives() const;

    /**
     * @brief The folder shared by all perspectives.
     * @return The shared folder, or nullptr before setupUi()/restoreStateFromFile() or when
     *         createSharedFolder() returns none.
     * @details The widget is reused across perspective switches; its container is the only
     *          one that ever shows it. The window owns it: it is created before any
     *          perspective, survives having no tabs and no open perspective, and is deleted
     *          only with the window.
     */
    QcwTabFolder* sharedFolder() const;

    /**
     * @brief Every view the window holds, in each perspective and in the dummy one.
     * @return The live views; empty before setupUi() has installed the dummy perspective.
     * @details A hidden perspective contributes its views just as the current one does, so this
     *          is the whole window rather than what is on screen. The result is a snapshot:
     *          a view removed by an event in between is still named here.
     * @see QcwPerspective::getOrCreateView()
     */
    QSet<QcwView*> allViews() const;

    /**
     * @brief A view name that no tab pane of this process uses yet.
     * @param defaultViewName The name to start from, normally a
     *                        QcwViewCreator::defaultWindowTitle().
     * @return @p defaultViewName when it is free, otherwise it with " 1", " 2"... appended
     *         until the result is unused.
     * @details Titles are counted over every tab pane of every window, not just this window's:
     *          a view is identified by its type and its title, and a saved perspective maps
     *          that pair to one view, so a repeated pair makes two views restore as one.
     *          A caller who wants to *reuse* a view therefore has to look for it before asking
     *          here, because this returns a different name whenever the wanted one is taken.
     * @see allViews()
     */
    static QString makeNewViewName(const QString& defaultViewName);

    /**
     * @brief Creates a floating folder that hosts a new view.
     * @param type The QcwViewCreator type of the view to create.
     * @param title The tab text and the title of the floating window.
     * @param args Arguments passed to the view creator.
     * @return The new floating folder, not shown yet, or nullptr when @p type is unknown,
     *         the creator failed, a per-window singleton of that type already exists, or the
     *         window has no current perspective.
     *
     * @details The view is created by QcwViewCreator::create() without a parent, the folder
     *          is given a new floating folder id and stamped with this window's id, and it
     *          is registered with the current perspective so that it is saved and hidden
     *          with that perspective. The folder has Qt::WA_DeleteOnClose set; the caller
     *          only has to resize and show it.
     */
    QcwTabFolder* createDetachedView(const QString& type, const QString& title, const QList<QVariant>& args = {});

    /**
     * @brief Looks up a registered menu.
     * @param menuId The menu id, e.g. QcwWindowActionIds::MenuFile.
     * @return The menu, or nullptr for an unknown id.
     * @details The menus the window builds itself are registered here, so this is how a subclass
     *          or a test reaches a built menu; the entries in it are command QActions.
     * @see commandAction()
     */
    QMenu* getMenu(const QString& menuId) const;

    /**
     * @brief Looks up the tool bar built for a contribution target.
     * @param target The target id, e.g. QcwWindowActionIds::ToolBarMain.
     * @return The tool bar, or nullptr when no contribution named that target and none was
     *         registered under it.
     * @details A tool bar appears for every target a contribution names, built at the window's
     *          first target build in setupUi() or restoreStateFromFile(), so this is how a
     *          subclass or a test reaches one the contribution system filled; the entries in it
     *          are command QActions, exactly as in a menu.
     * @see registeredToolBars(), commandAction()
     */
    QToolBar* getToolBar(const QString& target) const;

    /**
     * @brief The command QAction that carries @p commandId in this window.
     * @param commandId The command id, e.g. QcwWindowActionIds::ActionSave or an application id.
     * @return The window's command QAction, or nullptr when @p commandId is not in the catalog.
     *
     * @details The action is created on first use and mirrors the catalogued QcwCommandInfo and
     *          the handler resolveHandler() finds, so its text, icon, shortcut, enabled, visible
     *          and checked states are what the menus, toolbars and palette show. It is not the
     *          handler: asking for the handler itself is resolveHandler()'s job, and running the
     *          command is executeCommand()'s. Look the command up again when it is about to be
     *          used rather than caching this pointer, because an unregistered command drops its
     *          action.
     * @see QcwCommandRegistry, commandAction(), resolveHandler(), executeCommand()
     */
    QAction* resolveCommand(const QString& commandId);

    // --- Handler and command execution ------------------------------------------------

    /**
     * @brief Registers @p handler for @p commandId at the window scope.
     * @details Replaces an existing handler. The window borrows the handler: it is not
     * reparented or deleted, replacing it leaves the previous handler with its owner, and a
     * handler destroyed elsewhere unregisters itself.
     */
    void registerHandler(const QString& commandId, QcwCommandHandler* handler);

    /// Removes the window-scope handler for @p commandId. The handler keeps its owner and is
    /// not deleted.
    void unregisterHandler(const QString& commandId);

    /**
     * @brief Resolves @p commandId to a handler.
     * @details Current view's handler first, then the current perspective's, then the
     * window's, then the workbench's global handler. The first non-null wins.
     */
    QcwCommandHandler* resolveHandler(const QString& commandId) const;

    /**
     * @brief Runs @p commandId through its resolved handler.
     * @param commandId The command to run.
     * @param params Arguments passed to the handler; keys are agreed per command.
     * @return True when a handler was found, enabled and accepted the parameters.
     * @details A command this window's own UI runs arrives with
     * QcwWindowActionIds::ParamWindowId set to id(), because "this window" in a command label
     * means the one the user was looking at. Calling this directly passes whatever the caller
     * agrees with the handler, which is how one window addresses another.
     */
    bool executeCommand(const QString& commandId, const QVariantMap& params = {});

    /// True when the resolved handler reports isEnabled().
    bool isCommandEnabled(const QString& commandId) const;

    /**
     * @brief The command QAction for @p commandId; created lazily and owned by the window.
     * @details The QAction mirrors QcwCommandInfo (text, icon, shortcut, checkable) and the
     * resolved handler (enabled, visible, checked). It is the action menus, toolbars and
     * shortcuts share, and triggering it runs the command with this window as its target.
     * Returns nullptr when @p commandId is not registered.
     */
    QAction* commandAction(const QString& commandId);

    /// The workbench this window belongs to, set by QcwWorkbench::trackWindow().
    void setWorkbench(QcwWorkbench* workbench);
    QcwWorkbench* workbench() const;

    /**
     * @brief Opens the command palette for this window.
     *
     * @details Creates the palette if it is not open and raises it otherwise. The palette is
     *          parented to this window, is window-modal and deletes itself on close.
     * @see QcwCommandPalette, QcwWindowActionIds::ActionCommandPalette
     */
    void openCommandPalette();

    /**
     * @brief Whether closeEvent() persists the window state.
     * @return True by default.
     * @see setSaveStateOnClose()
     */
    bool saveStateOnClose() const;

    /**
     * @brief Enables or disables saving the state when the window closes.
     * @param save False for a window that is being removed on purpose and whose state file
     *             should not be written or kept.
     * @see saveStateOnClose()
     */
    void setSaveStateOnClose(bool save);

    /**
     * @brief Writes the window state to @p stateFile.
     * @param stateFile The XML file to write.
     * @return True when the file was written completely; false when @p stateFile or the
     *         window id is empty, the window has no perspective to save, or the file could
     *         not be written.
     *
     * @details The document records the state format version, the window id, current
     *          perspective, geometry and extra state, the views with their creating arguments,
     *          the shared folder, the dummy perspective and every other perspective. Floating
     *          folders are part of their perspective's state. saveMoreState() appends subclass
     *          data.
     *
     * @note Saving does not close the floating folders; closeEvent() does that after this
     *       returns.
     * @see restoreStateFromFile()
     */
    bool saveStateToFile(const QString& stateFile);

    /**
     * @brief Rebuilds the window from a file written by saveStateToFile().
     * @param stateFile The XML file to read.
     * @return False when the file is missing or malformed, the document has no dummy
     *         perspective, or the window was already initialized; true on success.
     *
     * @details This is the saved-session alternative to setupUi(). The window id from the
     *          document replaces id(). Views are recreated through their creators and
     *          their saved ids; folders reuse those views. Unknown views and perspectives are
     *          skipped; the document itself is refused when it is malformed or was written by
     *          a newer state format version. A document without a `<sharedFolder>` element
     *          leaves the window with one built by createSharedFolder(), as setupUi() would
     *          have done, so editors still have a folder to open in. A document saved while
     *          the shared folder was minimized restores it in its site, not parked in a fast
     *          view: the folder is one widget shared by every perspective, and a minimized
     *          state is transient by design. On success postSetupUi() runs once. On failure
     *          every object created during the attempt is discarded, the window id is put
     *          back, and the window is left uninitialized, so the caller can delete it or fall
     *          back to setupUi().
     *
     * @note restoreMoreState() is called for top-level elements the window does not
     *       recognize. It must consume the element; returning false aborts the restore.
     * @see saveStateToFile(), setupUi()
     */
    bool restoreStateFromFile(const QString& stateFile);

    /**
     * @brief Whether the window was rebuilt from a saved state file.
     * @return True after a successful restoreStateFromFile(); false for a window built by
     *         setupUi().
     * @note postSetupUi() runs on both paths, after this flag is set. It is the only sign a
     *       restored window gives when the saved session kept just the dummy perspective,
     *       which otherwise looks exactly like a fresh one.
     * @see setupUi(), restoreStateFromFile()
     */
    bool isRestoredFromState() const;

    /**
     * @brief Asks every view of the window whether the window may close.
     * @param callback Called exactly once: true when every view agreed, false as soon as one
     *                 refused. An empty callback is ignored.
     *
     * @details Asks the views one after another through QcwView::canClose(), the views of
     *          hidden perspectives and floating folders included. It only collects answers —
     *          nothing is closed and no state is saved; call close() when @p callback says
     *          true. The exception is forceClose(), for a close whose answer is already known.
     *          closeEvent() uses this same question, so a close requested by the user and one
     *          requested by the application agree.
     *
     * @note A confirmation that is already running comes first: another request is queued
     *       behind it and its callback runs when its turn comes. A view destroyed while its
     *       answer is outstanding is skipped.
     * @see QcwView::canClose(), closeEvent(), forceClose()
     */
    void canClose(const std::function<void(bool)>& callback);

    /**
     * @brief Closes the window without asking its views.
     *
     * @details Runs the normal close — aboutToClose(), the state save when
     *          saveStateOnClose() is true, and closing the floating folders — but skips
     *          QcwView::canClose(). It exists for a close whose answer has already been
     *          obtained (QcwWorkbench quits and removes windows this way) and for the
     *          shutdown path, where a quit can no longer be cancelled. An application that
     *          calls it takes the views' unsaved work into its own hands.
     * @see canClose(), closeEvent()
     */
    void forceClose();

signals:
    /**
     * @brief Emitted at the start of closeEvent().
     * @details Fires before the state is saved and before the floating folders are closed,
     *          so receivers still see the complete window state here.
     */
    void aboutToClose();

    /**
     * @brief Emitted at the very start of the destructor.
     * @details Every accessor of the window still works here, which is the point: the widget
     *          tree is torn down only after the destructor body has deleted the private, and
     *          the QcwTabFolder destructors in that teardown detach their panes with
     *          setParent(), whose ParentChange events reach the views still attached. A view
     *          that caches this window - or its topicService() - must drop that cache here,
     *          because those later events must not reach back into the gone private.
     *          QcwView::subscribeWindowTopic() does exactly that for window topics, so a view
     *          that holds no window pointer of its own needs no signal at all; id(),
     *          currentView() and topicService() additionally answer their empty value once the
     *          private is gone, which turns a missed connection into nothing shown rather than
     *          into freed memory read.
     * @see QcwView::subscribeWindowTopic(), topicService()
     */
    void aboutToBeDestroyed();

    /**
     * @brief The view on screen changed, or the current one re-announced itself.
     * @param view The new currentView(), or nullptr when there is none.
     * @details Emitted after the status bar and the command actions have been updated for
     *          @p view, so receivers may read both here. The window does not publish or clear
     *          topics on the view's behalf: a view that publishes its subject does so itself
     *          while it is current. The current view may be re-announced without changing —
     *          when a perspective is switched to itself, for example — so receivers that cannot
     *          tolerate a repeat should remember the previous pointer.
     * @see currentView(), topicService()
     */
    void currentViewChanged(QcwView* view);

    /**
     * @brief A tab folder was added to one of the window's perspectives.
     * @param folder The folder, already installed in its perspective's container.
     * @details Emitted when a container creates a folder or hosts the shared folder, and for
     *          every folder a perspective already holds when it is added to the window. A
     *          reset perspective announces the folders of its rebuilt layout this way.
     *          Floating folders belong to no container and are not announced; use
     *          createDetachedView() to keep their look in step with the window.
     * @see QcwWindowStyler::applyStyle(QcwWindow*)
     */
    void folderAdded(QcwTabFolder* folder);

protected:
    /**
     * @brief Handles the close of the window.
     * @param event The close event.
     * @details First asks every view through canClose(). Views that answer on the spot
     *          decide this event: a refusal ignores it, an approval goes on below. An answer
     *          that arrives later cannot resume an event that has returned, so an approval
     *          closes the window on a following event-loop turn and a refusal closes nothing.
     *          Once the views have agreed — or forceClose() supplied the answer — emits
     *          aboutToClose(), saves the state when saveStateOnClose() is true, closes the
     *          floating folders (which are independent top-level windows), then forwards to
     *          QMainWindow. The views' approval covers the floating folders closed here, so
     *          closing them does not ask a second time. Keeping the synchronous answers
     *          synchronous matters to QApplication, which abandons a quit whose window stays
     *          open; QcwWorkbench answers the quit itself so a late answer keeps working too.
     */
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief Retranslates the menu titles and action texts on a language change.
     * @param event The change event.
     */
    void changeEvent(QEvent* event) override;

    /**
     * @brief Hook for subclasses, called once after the UI is initialized.
     * @details Runs at the end of a successful setupUi() and restoreStateFromFile(), after
     *          the perspectives exist but before the window is shown. The default does
     *          nothing. Use it to add tool bars, open initial views or connect signals.
     */
    virtual void postSetupUi();

    /**
     * @brief Hook for subclasses, called every time the UI language changes.
     * @details Qt delivers QEvent::LanguageChange to every widget of the window, so a string a
     *          widget put on itself at construction is re-resolved by that widget's own
     *          changeEvent() — an application's views need nothing from here. This hook is for what
     *          a widget cannot do by itself: text the *window* holds, a tool bar built from strings
     *          the window chose, and a tab title the window derived from something else.
     *          QcwWindow's implementation re-resolves the view titles and the perspective buttons
     *          and re-texts its own menus and actions; an override calls it first, then re-applies
     *          what its subclass baked, then re-registers its QcwCommandInfo specs so the command
     *          palette shows them in the new language too.
     * @see QcwView::defaultTitle(), QcwPerspective::displayName()
     */
    virtual void retranslateUi();

    /**
     * @brief Hook for subclasses: append extra state to the window document.
     * @param writer The writer, positioned inside the @c window element.
     * @details Write well-formed child elements only; the base class closes the element
     *          afterwards. Read the data back in restoreMoreState().
     * @see restoreMoreState()
     */
    virtual void saveMoreState(QXmlStreamWriter& writer);

    /**
     * @brief Hook for subclasses: read extra state written by saveMoreState().
     * @param reader The reader, positioned on a top-level child of @c window that the base
     *               class did not recognize.
     * @return True to continue restoring; false to abort, in which case the partially
     *         built window is discarded.
     * @details The default skips the element. An override must consume the element as well,
     *          usually with reader.skipCurrentElement(), or the restore will not advance.
     * @see saveMoreState()
     */
    virtual bool restoreMoreState(QXmlStreamReader& reader);

    /**
     * @brief The menu table backing getMenu().
     * @return A mutable reference to the id-to-menu map.
     * @details Keys are a QcwWindowActionIds::Menu* value, and a menu is filled from the
     *          contributions whose target equals its key, so a subclass that adds a menu here
     *          gets one the sorter places entries in. The standard menus are inserted while the
     *          window is constructed.
     */
    QMap<QString, QMenu*>& registeredMenus();

    /**
     * @brief The tool bar table backing getToolBar().
     * @return A mutable reference to the target-to-tool-bar map.
     * @details Keys are contribution targets ("ToolBar/Main"), and a tool bar is filled from the
     *          contributions whose target equals its key. Registering one here is how a subclass
     *          says what title, icon size or button style its tool bar should have; the window
     *          then fills that one instead of building a plain tool bar of its own. Add it under
     *          its target and place the widget with addTopToolBar(), in the constructor body: the
     *          first build of the targets happens in setupUi() or restoreStateFromFile(), and a
     *          bar registered after it would sit beside the window's own for that target.
     */
    QMap<QString, QToolBar*>& registeredToolBars();

    /**
     * @brief Adds @p topToolBar to the top tool bar area.
     * @param topToolBar The tool bar to add; ignored when null.
     * @details Inserts it before the perspective tool bar and applies the current
     *          "Show Toolbar" visibility.
     */
    void addTopToolBar(QToolBar* topToolBar);

    /**
     * @brief Creates the folder shared by all perspectives.
     * @return The new folder, or nullptr for no shared folder.
     * @details Called once by setupUi() before the dummy perspective is built. The window
     *          keeps a pointer for its lifetime and only references the widget, so give it
     *          a parent — usually the window itself. Not called when restoring: the saved
     *          document supplies its own shared folder.
     */
    virtual QcwTabFolder* createSharedFolder();

private:
    friend class QcwWindowPrivate;

    QcwWindowPrivate* impl;
};

QT_END_NAMESPACE
