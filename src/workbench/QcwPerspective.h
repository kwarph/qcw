// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPersistable.h"
#include <QAnyStringView>
#include <QObject>
#include <QStringList>

QT_BEGIN_NAMESPACE

class QcwCommandHandler;
class QWindow;
class QXmlStreamWriter;
class QXmlStreamReader;
class QcwWindow;
class QcwView;
class QcwTabFolder;
class QcwTabFolderContainer;
class QcwPerspectivePrivate;

/**
 * @class QcwPerspective
 * @brief One arrangement of tab folders inside a QcwWindow.
 *
 * @details A window keeps several perspectives at the same time and shows one of them:
 * - the *dummy* perspective, created by QcwWindow::setupUi() and never closed, which holds
 *   the views that belong to no real perspective;
 * - the perspectives the user opened, either from a QcwPerspectiveCreator plugin's initial
 *   layout or from saved state.
 *
 * Each perspective owns a QcwTabFolderContainer of its own — that is the widget
 * QcwWindow swaps into its central area on a switch — plus the floating folders detached
 * from it and the fast view tool bars its minimized folders produced. Hidden perspectives
 * keep all of that intact, which is why the visual state has to be reconciled on a switch
 * (see activateFolder()).
 *
 * @par Identity
 * Three strings and a number, all persisted by QcwWindow's state:
 * - id() — unique among the perspectives of one window, assigned by the window;
 * - name() — what the perspective tool bar button and the save dialog show;
 * - type() — the QcwPlugin::type() of the creator plugin that built it, and the value a
 *   saved perspective is matched by when it is opened again.
 *
 * @par Ownership
 * The perspective is a child of its QcwWindow, so closing the window disposes of it; a
 * perspective closed on its own is removed with closePerspective()-style cleanup: clear()
 * then deleteLater(). It owns neither the folders' tab panes (views are reused from the
 * window's registry when restoring) nor the folders it parks in fast view tool bars, which
 * QcwTabFolderSite documents.
 *
 * @see QcwPerspectiveCreator, QcwWindow, QcwTabFolderContainer
 */
class QCW_EXPORT QcwPerspective : public QObject, public QcwPersistable
{
    Q_OBJECT
public:
    /**
     * @brief Builds the perspective and its folder container inside @p parent.
     * @param parent The window this perspective belongs to. Required: the container is
     *               created inside the window's central widget and the folder bookkeeping
     *               reads the window's id, so a null parent cannot work.
     */
    explicit QcwPerspective(QcwWindow* parent);

    ~QcwPerspective();

    /**
     * @brief The window's stand-in perspective: type @c "DummyPerspective", name @c "Dummy".
     * @details QcwWindow keeps exactly one alive for its whole lifetime and uses it as the
     * destination for views opened outside any perspective.
     */
    static QcwPerspective* createDummyPerspective(QcwWindow* parent);

    /// Icon used for the dummy perspective, until a @c QCW_THEME provides a better one.
    static QIcon dummyPerspectiveIcon();

    /**
     * @brief Fill an empty perspective with its starting layout.
     * @details The base does nothing, which is the right answer for a perspective whose whole
     * layout comes from saved state. QcwPerspectiveCreator::create() does not call this
     * either: the caller decides whether a new perspective starts from an initial layout or
     * from a state document.
     * @see resetLayout()
     */
    virtual void createInitialLayout();

    /**
     * @brief Drop everything and run createInitialLayout() again: the "reset perspective"
     *        command.
     * @details The old folders are destroyed before the new layout is built, not just queued
     *          for deletion, so the views a reset recreates carry their default titles rather
     *          than a " 1" suffix that only made room for a view that was on its way out.
     * @note The window's shared folder belongs to the window, and only the perspective on
     *       screen may show it. Resetting a hidden perspective therefore hands it back to
     *       the current one right after the rebuilt layout has claimed it.
     * @see clear()
     */
    void resetLayout();

    /**
     * @brief Write the layout — docked folders, floating folders and the perspective's own
     *        identity — as one XML element.
     * @param writer Where to write.
     * @param elemName Name of the element, e.g. @c "perspective" or @c "dummyPerspective".
     * @see restoreState()
     */
    void saveState(QXmlStreamWriter& writer, QAnyStringView elemName);

    /**
     * @brief Rebuild the layout written by saveState().
     * @param reader Positioned on the element saveState() wrote.
     * @param sharedFolder The window's shared folder to install, or nullptr when the saved
     *                     layout had none.
     * @param tabPanes The view widgets the window already restored, by id. Folders re-add
     *                 these instead of creating new views.
     * @param minFloatingFolderId In/out: the lowest floating folder id seen so far. Restored
     *                            folders are typically negative (see
     *                            qcw::FolderStartingIds::Floating); the window keeps handing
     *                            out ids below this one so it never collides with a restored
     *                            folder.
     * @return @c false when the layout could not be rebuilt — the container or one of the
     *         floating folders refused its element. The perspective is left half-filled, so
     *         the caller is expected to discard it.
     *
     * @details id(), name() and the extra state are applied after the layout, and every
     * restored folder — docked and floating — is stamped with this window's id() so the
     * floating ones keep reporting to the right window. Floating folders stay hidden; the
     * window shows those of the current perspective only after the restore is committed.
     * @see saveState()
     */
    bool restoreState(QXmlStreamReader& reader,
                      QcwTabFolder* sharedFolder,
                      const QMap<int, QPair<QWidget*, int>>& tabPanes,
                      int& minFloatingFolderId);

    /// The creator plugin's type(), empty for a perspective nothing built through a creator.
    QString type() const;

    /// Unique among the perspectives of one window; QcwWindow assigns one when it is unset.
    int id() const;
    void setId(int id);

    /**
     * @brief The name shown to the user, in the perspective tool bar and the save dialog.
     * @details This name is also the perspective's identity: it is the file name
     * QcwSavedPerspectives writes it under and the key it looks it up by, so nothing that only
     * affects presentation may rewrite it. A tool bar button shows displayName() instead.
     */
    QString name() const;
    void setName(const QString& name);

    /**
     * @brief The name to show for this perspective, in the UI language now installed.
     * @details While the perspective still carries the name its creator handed out
     * (QcwPerspectiveCreator::defaultName()), that default is resolved afresh and returned; a name
     * somebody set by hand comes back unchanged, as does name() for a perspective with no creator.
     * @note Saved state writes the autoName flag whether or not the name is automatic, so a document
     * that carries no flag at all predates it. Such a document is read as an automatic name — the
     * reading that follows a later language change — which costs a hand-named one its own button
     * text from the first start after upgrading; name() keeps the stored literal, so the
     * perspective is still the same saved layout.
     * @see setName()
     */
    QString displayName() const;

    /// The icon shown to the user; the creator's default until something replaces it.
    QIcon icon() const;
    void setIcon(const QIcon& icon);

    /**
     * @brief The action sets visible while this perspective is active.
     * @details An empty list means every action set is visible; a non-empty list means only
     * the listed ones are. Contributions whose actionSetId is empty are always visible and
     * are not affected by this list. The list is persisted with the perspective's state.
     *
     * The list decides which contributions a window places, so setting it on the perspective
     * that is already current re-places its menus and tool bars at once — the window learns
     * through visibleActionSetIdsChanged(), which is emitted only when the list really
     * changed. Setting it before opening the perspective works the same way; nothing about
     * this list touches a command's own state, it only decides what is placed.
     */
    QStringList visibleActionSetIds() const;
    void setVisibleActionSetIds(const QStringList& ids);

    /**
     * @brief Add @p floatingFolder to the folders this perspective owns outside the window.
     * @details Called when a tab is detached into its own top-level window and when a saved
     * layout puts folders back. Ignored when @p floatingFolder is null or already registered.
     * The folder stays registered until it is destroyed — its destroyed() signal drops it out
     * of the set, and clears currentFolder() too when it was the active one.
     * @see floatingFolders()
     */
    void registerFloatingFolder(QcwTabFolder* floatingFolder);

    /// The folders currently floating for this perspective, in no particular order.
    QSet<QcwTabFolder*> floatingFolders() const;

    /**
     * @brief Make @p folder the active one, and the only folder drawn as active.
     *
     * @details The single entry point for activation: a docked folder clicked in the
     * container, a floating folder clicked in its own title bar, a focus change and a
     * perspective switch all end up here. It records currentFolder(), moves the active
     * highlight across the docked and the floating folders and emits currentViewChanged().
     *
     * @note Calling it again with the folder that is already current is not a no-op — that is
     * how the highlight is re-asserted after a switch or a restore.
     * @note The highlight is only touched while this is the window's visible perspective. The
     * shared folder is one widget reused by every perspective's container, so a hidden
     * perspective toggling it would corrupt the one on screen; show() and
     * QcwWindow::setCurrentPerspective() reconcile it instead.
     * @see currentFolder(), currentViewChanged()
     */
    void activateFolder(QcwTabFolder* folder);

    /// The active folder, docked or floating; nullptr when there is none.
    QcwTabFolder* currentFolder() const;

    /// The window this perspective lives in.
    QcwWindow* window() const;

    /// The perspective's own folder container, the widget the window shows for it.
    QcwTabFolderContainer* folderContainer() const;

    /// The view in the active folder, nullptr when that folder is empty or holds a non-view.
    QcwView* currentView() const;

    /**
     * @brief Whether @p folder belongs to this perspective rather than to the window or another one.
     * @param folder The folder to test; nullptr is never owned.
     * @return True for a docked folder of folderContainer(), for a floating folder and for a
     *         folder parked in one of this perspective's fast view tool bars; false for the
     *         window's shared folder, which every perspective borrows but none owns, and for
     *         the folders of another perspective.
     *
     * @details Clearing or closing the perspective releases the tab panes of exactly these
     *          folders. QcwWindow::closePerspective() uses this to ask only the views that the
     *          close actually ends: a view another perspective still shows stays open, so its
     *          QcwView::canClose() is not consulted.
     */
    bool ownsFolder(QcwTabFolder* folder) const;

    /**
     * @brief Registers @p handler for @p commandId at the perspective scope.
     * @details The perspective owns neither the handler nor its lifetime; destroyed() is
     * watched so a handler that goes away is dropped automatically. Registering an id again
     * replaces the previous handler. QcwWindow asks the current perspective's handlers after
     * the current view's and before its own.
     */
    void registerHandler(const QString& commandId, QcwCommandHandler* handler);

    /// Removes the perspective-scope handler for @p commandId.
    void unregisterHandler(const QString& commandId);

    /// The perspective-scope handler for @p commandId, or nullptr.
    QcwCommandHandler* handler(const QString& commandId) const;

    /**
     * @brief Add every view of this perspective — docked folders, floating ones and folders
     *        parked in a fast view tool bar — to @p views.
     * @details The window calls this to build its registry of live views and to ask the views
     * before it closes a perspective. Passing a set it already filled is how several
     * perspectives are merged without copying sets around.
     */
    void collectViews(QSet<QcwView*>& views) const;

    /**
     * @brief Hide the container, the floating folders and the fast view tool bars.
     * @details A minimized shared folder is restored first, as its fast view's Restore action
     *          would: it is one widget that the perspective shown next takes into its own
     *          layout, so leaving it parked would empty this perspective's tool bar and hand
     *          the next one a minimized folder it cannot minimize again.
     */
    void hide();

    /**
     * @brief Show the container, the floating folders and the fast view tool bars again.
     * @details Also takes the window's shared folder back into this perspective's site: the
     * shared folder is a single widget, so the perspective that was visible before this one
     * is currently holding it. hide() has already restored a minimized shared folder, so the
     * site receives a normal one.
     */
    void show();

    /**
     * @brief Empty the perspective: close the floating folders, delete the fast view tool
     *        bars and clear the container.
     * @details The tool bars own the folders they park, so a minimized folder goes with them;
     * what remains is an unusable perspective, which is why callers either rebuild its layout
     * (resetLayout()) or discard it right after. The window's shared folder is the exception:
     * a fast view that parked it hands it back before the tool bar goes, because the window
     * owns the folder, not this perspective.
     * @see resetLayout()
     */
    void clear();

signals:
    /**
     * @brief The active folder was set or its current tab changed, so the active view may
     *        have changed.
     * @details Emitted by activateFolder(), including the re-asserting calls, and when the
     *          current folder's tab changes without activation (a shortcut, a tab removal).
     *          Receivers re-read currentView() rather than comparing against a previous value.
     */
    void currentViewChanged();

    /// Emitted when a registered handler reports stateChanged().
    void handlerStateChanged(const QString& commandId);

    /// Emitted when setVisibleActionSetIds() stored a different list; a window whose current
    /// perspective announces this re-places its menus and tool bars.
    void visibleActionSetIdsChanged();

protected:
    /**
     * @brief The window's existing view of @p viewType, creating one only when there is none.
     * @param viewType The QcwViewCreator type() to look the view up and create by.
     * @param viewName The title to match, or empty to use the creator's defaultWindowTitle().
     *                 Ignored for a creator whose singleInstancePerWindow() is true.
     * @param args Arguments for the creator; only used when a view is actually created, so a
     *             reused view keeps the state it was first created with.
     * @return The view and the creator's defaultViewIcon(); `{ nullptr, {} }` when @p viewType
     *         has no creator or the creator produced no view.
     *
     * @details This exists to lay out a view the window already has, not to make views: two
     * perspectives that ask for the same type and name share one instance, each folder getting
     * a tab and QcwTabPaneManager counting the view as referenced by both. A layout that wants a
     * fresh view of its own, never reusing one, should call QcwViewCreator::create() instead —
     * with a distinct name it would also get one here, but asking to reuse is the point.
     *
     * Views are matched by type and title over QcwWindow::allViews(), the dummy perspective
     * included. A view of *this* perspective is never a candidate — it is already laid out
     * here — so calling twice in one layout does get a second view. A creator that is a
     * per-window singleton is the exception: the type alone identifies it, a view of this
     * perspective is a candidate too, and the returned view is always the one the window has.
     *
     * A created view takes a name no other tab pane holds (QcwWindow::makeNewViewName()), which
     * keeps the (type, title) pair the saved session restores by unambiguous; a reused view keeps
     * the name it answers to, even when @p viewName differs from what it was created with.
     *
     * @note The view is detached, like QcwViewCreator::create()'s result: the caller owns it
     *       until a folder takes it, and QcwTabFolder::addTab() renames the tab it is given, so
     *       pass the view's own windowTitle() as the tab text to keep the identity intact.
     * @see QcwWindow::allViews(), QcwViewCreator::create()
     */
    QPair<QcwView*, QIcon>
    getOrCreateView(const QString& viewType, const QString& viewName = {}, const QList<QVariant>& args = {});

private:
    /**
     * @details The shared folder is not part of this perspective's layout, so clear() leaves
     *          its tabs alone. A per-window singleton dragged into it would then survive the
     *          reset as a second tab of a view the new layout already shows. Only such a
     *          singleton loses its shared-folder tab: one the rebuilt layout does not place in
     *          a folder of this perspective stays where it is, because the shared folder then
     *          holds the window's only instance and removing its tab would destroy it. Runs
     *          after createInitialLayout(), when the view is in its rebuilt folder, so
     *          dropping the extra tab cannot end the view's life.
     */
    void removeTransferredSingletonTabs();

    QcwPerspectivePrivate* impl;
};

QT_END_NAMESPACE
