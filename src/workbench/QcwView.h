// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPersistable.h"
#include <QWidget>
#include <QHash>
#include <QList>
#include <QLatin1String>
#include <QMap>
#include <QVariant>
#include <QVariantMap>
#include <functional>

QT_BEGIN_NAMESPACE

class QAction;
class QEvent;
class QMenu;
class QShowEvent;
class QToolBar;
class QcwCommandHandler;
class QcwViewPrivate;
class QcwWindow;

/**
 * @class QcwView
 * @brief Base class for the widgets that fill a tab pane.
 *
 * @details A view is the content of one tab. It is normally made by a QcwViewCreator, which is what
 * lets a saved session be rebuilt after a restart, but an application can also construct a subclass
 * itself and add it to a QcwTabFolder directly — only the identity below is then its own business.
 * QcwWindow takes the title, icon, command handlers and status bar widget of whatever is current.
 *
 * @par Identity
 * Three values say what a view is, and all three end up in the saved state: type() is the name a
 * creator answers to, id() is the tab pane id the layout refers to a pane by, and creatingArguments()
 * is what that creator was handed, so restoring means calling it again with the same list. A view
 * built by hand simply leaves them unset. They are dynamic properties rather than members because a
 * pane can be any QWidget and QcwTabFolder has to read the id off a widget it cannot assume anything
 * about.
 *
 * @par Identity is write-once
 * setId() and setCreatingArguments() ignore the call once the value exists. A pane's id is copied
 * into the tab's data the moment the view is added to a folder, and a value that changed afterwards
 * would leave the copy and the widget disagreeing, so the first write is the one that counts. A
 * restore that has to impose a saved id therefore creates the view with assignId == false and calls
 * setId() itself, before the view goes into any folder.
 *
 * @par Topics
 * Subscribe to window-scope topics through subscribeWindowTopic(): the view follows its window
 * through attach, moves and teardown on its own, so a view never has to cache a QcwWindow* or
 * reach for a topic service. Publishing stays manual: the framework does not relay a selection
 * on the view's behalf — a view that describes the current subject publishes while it is the
 * window's currentView() and clears its topic when it stops being current (the workbench demo
 * does this from event() on QEvent::Show and QEvent::Hide).
 * Topic ids and payload keys are the contract between the publisher and its subscribers;
 * register them in QcwTopicRegistry when other plugins should discover them.
 *
 * @note id() is 0 for a view that was never given an id — which is also the first id
 * QcwTabPaneManager hands out. Give a view its id at creation time so the two cannot be confused.
 *
 * @see QcwViewCreator, QcwTabFolder, QcwPersistable, QcwTopicService
 */
class QCW_EXPORT QcwView : public QWidget, public QcwPersistable
{
    Q_OBJECT
public:
    explicit QcwView(QWidget* parent = nullptr);
    ~QcwView() override;

    /// The QcwViewCreator::type() that built this view; saved state names views by it.
    QString type() const;
    void setType(const QString& type);

    /**
     * @brief The tab title this view's creator hands out, in the language that was installed when
     *        it was assigned.
     * @details QcwViewCreator::create() stamps it, so every view carries the name its creator asked
     * for from birth; a caller that names a view something of its own clears it again. The window
     * reads it back when the UI language changes: a tab whose text still equals it, with or without
     * the ordinal QcwWindow::makeNewViewName() appends, is re-resolved from the creator, and
     * anything else is a rename that is left alone.
     *
     * @note A rename that reads exactly like an auto ordinal — "Notes 2" for a view whose default
     *       title is "Notes" — cannot be told from one, so it is re-derived with the rest.
     * @note Unlike id(), this is written again after a retranslation, which is what lets the next
     *       language change see the new baseline.
     * @note Saved state writes the value even when it is empty, so a restored document can tell a
     *       caller-chosen title (an empty baseline) from one it simply never recorded (no baseline
     *       at all). Only the latter is guessed at: its own text becomes the baseline, which is how
     *       a session written before this attribute existed follows the language it opens in.
     */
    QString defaultTitle() const;
    void setDefaultTitle(const QString& title);

    /// The tab pane id, unique per process. See the write-once note above; 0 while none was given.
    int id() const;
    void setId(int id);

    /**
     * @brief The arguments this view was created with.
     * @details Remembered so the workbench can write them into the saved state and rebuild the view
     * through its creator after a restart. Write-once, like id().
     */
    QList<QVariant> creatingArguments() const;
    void setCreatingArguments(const QList<QVariant>& args);

    /**
     * @brief The handlers this view provides, keyed by command id.
     * @details QcwWindow collects them while this view is current and disconnects them when
     * another view takes over. Returning a handler here is what makes the same command run
     * different code in different views (Edit/copy copies text here, a file path there).
     *
     * @note The view owns the handlers; the window only borrows them. A null entry is
     *       ignored. Return the same handler on every call.
     */
    virtual QHash<QString, QcwCommandHandler*> commandHandlers() const
    {
        return {};
    }

    /**
     * @brief A widget to show in the window's status bar while this view is current.
     * @return The widget, or @c nullptr for none.
     *
     * @details QcwWindow installs it whenever this view becomes current and hands it back,
     * hidden, when another view takes over. The view keeps ownership: while the widget is
     * installed the status bar reparents it, and on the way out it returns the widget to the
     * parent the widget had when it was installed (or leaves it parentless when it had none).
     * A widget whose original parent is destroyed while the widget is installed is deleted.
     *
     * @note Return the same widget on every call so the window can tell it is unchanged, and
     *       do not assume a particular parentWidget() while it is installed. Deleting the
     *       widget while it is installed is safe: the status bar drops it automatically.
     *       Create the widget hidden: until this view becomes current it is an ordinary child
     *       of the view, and a visible one would paint at its default geometry over the view's
     *       content.
     */
    virtual QWidget* statusBarWidget() const;

    /**
     * @brief A tool bar this view wants at the top right of its tab folder while it is current.
     * @return The tool bar, or @c nullptr for none.
     *
     * @details QcwPerspective installs it whenever this view becomes a folder's current tab.
     * The folder shows it at the right end of the tab bar while there is room for it, and
     * otherwise lays it out at the top-right corner of this view with the view's content
     * below it, on the model of an Eclipse view toolbar; the toolbar never covers the view.
     * The view keeps ownership: while the toolbar is installed the folder reparents
     * it, and when the view stops being current the folder hands it back, hidden, to the
     * view. Return the same toolbar on every call so the folder can tell it is unchanged.
     *
     * @note Do not assume a particular parentWidget() for the toolbar, and do not delete it
     *       while it is installed.
     * @see QcwTabFolder::setRightWidget()
     */
    virtual QToolBar* topRightToolBar() const;

    /**
     * @brief The window this view is laid out in.
     * @return The owning QcwWindow, or @c nullptr when the view is not in one — detached,
     *         still waiting for a folder, or parented outside the workbench.
     *
     * @details Walks the parent chain. A view in a floating folder has no window among its
     * ancestors, because the folder is a top-level widget, so the folder's @c WindowId is
     * matched against the top-level windows instead. Use this rather than a plugin-local
     * lookup to reach the window, its perspectives and its QcwTopicService.
     *
     * @note The pointer is not tracked. The window owns this view and destroys it first, so
     *       a held pointer is no safer than reading it back when needed — and holding one
     *       across events is worse than unsafe: during the window's teardown the folder
     *       destructors release the panes with events of their own, and by then the window's
     *       private is gone. Read the window when you need it (publishing, commands), and let
     *       subscribeWindowTopic() keep subscriptions alive across moves instead.
     */
    QcwWindow* owningWindow() const;

    /**
     * @brief Asks the view whether it may close, and calls back exactly once.
     * @param callback The answer: true lets the close proceed, false stops it. Call it once,
     *                 synchronously or later; a repeated call is ignored.
     *
     * @details QcwWindow calls this before a user-requested tab close removes the view,
     * before it closes a perspective or the window, and QcwWorkbench calls it before it
     * quits or removes a window. The framework never prompts on the view's behalf: a view
     * with unsaved work asks the user itself, saves or discards, and answers accordingly.
     * The default answers true.
     *
     * The answer may take its time — a save that talks to a server, for instance. The close
     * waits while the callback is outstanding, and the framework asks one view at a time, so
     * later requests queue behind it. Calling the callback before canClose() returns keeps a
     * view without unsaved work synchronous. A view destroyed while its answer is
     * outstanding no longer blocks the close: there is nothing left to keep open.
     *
     * @note Removing a tab with QcwTabFolder::removeTab(), deleting a window, or
     *       QCoreApplication::exit() — which does not send the quit event QcwWorkbench
     *       answers — does not ask. The protocol covers the tab close button and context
     *       menu, perspective and window closes, the floating folder's close button, Remove
     *       Window, and every quit() of a workbench application.
     */
    virtual void canClose(const std::function<void(bool)>& callback);

    /**
     * @brief Follows a window-scope topic for as long as this view lives.
     * @param topicId The topic to follow; an empty id or handler is ignored.
     * @param handler Invoked with every payload; must apply state, not count events — a
     *                (re-)attach can deliver the same payload again.
     *
     * @details The framework holds the handler and (un)subscribes it from the owning window's
     *          QcwTopicService as the view attaches, moves between windows, detaches, or
     *          survives its window's teardown, so the view never caches a window or a service
     *          of its own — the pattern whose misuse caused the use-after-free this API exists
     *          to replace. On (re-)attach to a window the handler runs once with the topic's
     *          current payload — an empty map when it was cleared or never published — and on
     *          a detach from a living window once with the empty map; a window that is being
     *          destroyed delivers nothing further. Subscribing again for the same topic
     *          replaces the handler (and re-reads the current payload).
     *
     * @note Call from the view's constructor or any later point; before the view is in a
     *       window nothing is subscribed yet, and the attach itself performs the first
     *       delivery. A handler may subscribe and unsubscribe while it runs, even for the topic
     *       it was called for, and it may move the view: the ids are taken from a snapshot and
     *       re-read per id, a nested transition request is answered once the running one is
     *       through, and what is left is at least one delivery per followed topic and no
     *       subscription on a window the view no longer belongs to. Deliveries are therefore
     *       counted in the contract as state to apply, never as events to tally.
     * @see unsubscribeWindowTopic(), currentWindowTopic(), QcwTopicService
     */
    void subscribeWindowTopic(const QString& topicId,
                              std::function<void(const QVariantMap&)> handler);

    /**
     * @brief Stops following a window-scope topic and forgets its handler.
     * @param topicId The topic to drop; unsubscriptions for topics not followed do nothing.
     * @see subscribeWindowTopic()
     */
    void unsubscribeWindowTopic(const QString& topicId);

    /**
     * @brief The owning window's cached payload for @p topicId, without subscribing.
     * @return current() from the window's QcwTopicService, or an empty map when the view is
     *         not in a window (or the topic has no cache).
     * @see subscribeWindowTopic(), QcwTopicService::current()
     */
    QVariantMap currentWindowTopic(const QString& topicId) const;

    /**
     * @brief Appends a context-menu section with @p title containing @p actions.
     * @param title The submenu title, or an empty string to append the actions directly.
     * @param actions The actions to place; an empty list does nothing.
     *
     * @details Profiles and plugins own their commands, so a view's context menu stays generic
     * and another component attaches its own section without duplicating the actions. A separator
     * is placed before the section. The caller keeps ownership of the actions and they keep their
     * enabled state.
     * @see contextMenu(), insertContextMenuSection()
     */
    void addContextMenuSection(const QString& title, const QList<QAction*>& actions);

    /**
     * @brief Inserts a context-menu section relative to an action named in namedActions().
     * @param before True places the section before @p refActionName, false places it after.
     * @param refActionName The view-local name of the anchor action.
     * @param title The submenu title, or an empty string to insert the actions directly.
     * @param actions The actions to place; an empty list does nothing.
     *
     * @details When @p refActionName is not registered the section is appended at the end, as
     * addContextMenuSection() would. A separator is placed on both sides of the inserted section.
     * The caller keeps ownership of the actions and they keep their enabled state.
     * @see addContextMenuSection(), namedActions()
     */
    void insertContextMenuSection(bool before,
                                  const QLatin1String& refActionName,
                                  const QString& title,
                                  const QList<QAction*>& actions);

protected:
    /**
     * @brief Routes QEvent::LanguageChange to retranslateUi() and QEvent::ParentChange to the
     *        window-topic subscriptions, then passes every event on.
     *
     * @details Override this only for events of its own; a language change is handled here, so a
     * subclass no longer needs a changeEvent() of its own just to resolve its texts again. An
     * override must call the base implementation, or the view stops following the language and
     * stops following its window.
     *
     * @note Qt delivers the event to every widget, hidden ones and views in hidden perspectives
     *       included, so retranslateUi() runs there too.
     * @see retranslateUi(), subscribeWindowTopic()
     */
    void changeEvent(QEvent* event) override;

    /**
     * @brief Re-reads the owning window, because restoring completes the parent chain after the
     *        reparenting event.
     * @details An override must call the base implementation, or the view stops following its
     *          window topics.
     * @see subscribeWindowTopic()
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief The view's context menu, created with the view.
     * @return The menu, never @c nullptr.
     *
     * @details The menu is a child of this view and is not shown by QcwView: a view that wants
     * a context menu selects a context-menu policy and calls exec() at the position it chooses.
     * Subclasses add their actions here; nothing is added by default.
     *
     * @note Mutate it only on the GUI thread. The view owns the menu, so a pointer stays valid
     *       for as long as the view lives.
     * @see namedActions(), addContextMenuSection(), insertContextMenuSection()
     */
    QMenu* contextMenu() const;

    /**
     * @brief The actions this view exposes under stable, view-local names.
     * @return A mutable map from a view-local name to the action registered for it.
     *
     * @details The map lets a view find one of its own actions again — for example to place a
     * context-menu section relative to it, or to update its state — without storing a member for
     * every action. The names are local to the view and are not command ids; a view that needs
     * to answer window commands does that through commandHandlers().
     *
     * @note The view does not own the actions and never deletes them: the map only borrows the
     *       pointers, so remove an entry before its action is destroyed. Do not insert a null
     *       action. Mutate the map only on the GUI thread.
     * @see contextMenu(), commandHandlers()
     */
    QMap<QLatin1String, QAction*>& namedActions();

    /**
     * @brief Re-resolves the texts this view holds as strings, whenever the UI language changes.
     *
     * @details A string resolved once — @c tr("...") in the constructor — is a QString, not a
     * lookup, so a widget built in one language keeps that language. Re-apply every such text
     * here. The default does nothing, for a view that holds none.
     *
     * The tab title is not one of them: QcwWindow retranslates it from the creator's
     * defaultWindowTitle() before the views hear of the change, so a view that names its own tab
     * here would overwrite that answer.
     *
     * @note A command the view registered with QcwCommandInfo carries a label captured the same
     *       way; re-register it here too, because the command palette shows what was registered
     *       rather than what the view resolves now.
     * @see changeEvent(), QcwWindow::retranslateUi()
     */
    virtual void retranslateUi();

private:
    friend class QcwViewPrivate;

    QcwViewPrivate* impl;
};

QT_END_NAMESPACE
