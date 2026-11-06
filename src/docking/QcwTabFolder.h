// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwDockingTypes.h"
#include "QcwGlobal.h"
#include <QFrame>

QT_BEGIN_NAMESPACE

class QXmlStreamWriter;
class QXmlStreamReader;
class QMenu;
class QWindow;
class QcwTabFolderPrivate;
class QcwTabFolderRenderer;

/**
 * @class QcwTabFolder
 * @brief A tab container widget that supports docking, floating and reordering.
 *
 * @details QcwTabFolder extends QFrame to provide a tabbed interface similar to QTabWidget,
 * but with enhanced support for dockable layouts (via QcwTabFolderContainer), custom rendering,
 * and dynamic state management (Minimized/Maximized/Normal).
 *
 * @par Tab Bar Layout
 * The folder lays out and paints its own tab bar. Only the tabs that fit the current width
 * are shown; the rest are hidden and stay reachable through the tab list button, so
 * isTabVisible() reports the outcome of the last layout pass rather than a stored preference.
 *
 * @par Appearance
 * All tab drawing is delegated to the QcwTabFolderRenderer returned by renderer();
 * setRenderer() installs another one. The folder itself exposes no styling properties.
 *
 * @par State Management
 * The folder can exist in three states defined by qcw::FolderState:
 * - Normal: Standard tabbed view.
 * - Minimized: Collapsed by the container into a tool button on the window edge.
 * - Maximized: Expands to fill the available parent container space, with the sibling
 *   folders hidden.
 * state() and setState() only record this value; the layout change itself belongs to
 * QcwTabFolderContainer, which reacts to minimizeRequested(), maximizeRequested() and
 * restoreRequested().
 *
 * @par Pane Sharing
 * A folder shows a given pane at most once, so adding a pane it already holds is
 * refused (addTab()/insertTab() return -1). One pane may still be shared by
 * several folders; QcwTabPaneManager releases it once its last holder drops it.
 *
 * @par Drag And Drop
 * Dropping a tab inside its own folder reorders it when the TabsMovable feature is set.
 * Dropping it onto another folder needs AcceptDroppedTabs on that folder and moves the
 * tab through takeTab(), so the rule above still applies. A drag that nothing accepts
 * detaches the tab: a new floating folder is created for the pane instead of moving the
 * folder it came from, unless the tab is not detachable.
 *
 * @par Context Menu
 * The folder builds one menu at construction and shows it on a right click in the tab bar,
 * deciding each time which of its entries apply: @c Close and @c Detach follow the tab under
 * the cursor through isTabClosable()/isTabDetachable(), @c Close Others, @c Close Tabs to
 * the Left, @c Close Tabs to the Right and @c Close All follow how many closable tabs are
 * around it, and a click outside any tab hides all six. The five close entries do nothing
 * but emit tabCloseRequested() — the folder holds no receiver of its own and removes no tab.
 * A QcwWindow installs a receiver for every folder of its own: it asks the QcwView under the
 * request through QcwView::canClose() and calls removeTab() when the view agrees, so saving,
 * discarding and refusal stay the view's decision. A pane that is not a QcwView, and any
 * folder outside a QcwWindow, is left to the application, which calls removeTab() itself.
 * @c Detach is the exception: it moves its pane into a new floating folder by itself.
 *
 * The last entry, @c Folder Settings..., opens an editable view of the features set, the
 * tab icon size and the minimum tab text width, i.e. of setFeatures(), setIconSize() and
 * setMinimumTextRectWidth(). It is hidden while the folder is parked in a
 * QcwFastViewToolBar: that host is a popup the user dismisses by clicking away, and it
 * forces Minimizable off, so editing those two flags there would be undone on restore.
 * Nothing is signalled when the dialog applies, so an application that mirrors these values
 * in its own UI or settings has to re-read them. All three are written by saveState() and
 * read back by restoreState(), which is how a user's choice survives with the layout.
 *
 * @see QcwTabFolderContainer
 * @see QcwTabFolderRenderer
 * @see qcw::FolderState
 * @see qcw::FolderFeatures
 */
class QCW_EXPORT QcwTabFolder : public QFrame
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a QcwTabFolder widget.
     * @param parent The parent widget, which takes ownership of the folder. A null
     *               @p parent makes the folder a top-level window, which is how a
     *               detached folder is hosted; see setFloating().
     */
    explicit QcwTabFolder(QWidget* parent = nullptr);

    /** @brief Destructs the QcwTabFolder widget and releases every pane it holds. */
    ~QcwTabFolder() override;

    /**
     * @brief Writes the folder as a @c folder element, for restoreState() to read back.
     * @param writer The stream to write into. The element is written inside whatever
     *               element the caller has open; the folder writes no document header.
     * @details Records the folder id, current index, features, tab position, floating
     *          flag, icon size, minimum text rect width, state and MRU order, plus one
     *          @c tab element per tab holding its text, tooltip, data map, closable and
     *          detachable flags. Panes are referenced by their id only.
     * @see restoreState()
     */
    void saveState(QXmlStreamWriter& writer) const;

    /**
     * @brief Restores a folder element written by saveState().
     * @param reader Positioned on a @c folder start element.
     * @param tabPanes Maps a saved pane id to the widget to re-attach and its
     *                 current id. A tab whose pane is missing is restored
     *                 without a widget.
     * @return False if @p reader is not on a @c folder element or ends up in an
     *         error state; the tabs parsed so far stay in the folder.
     * @note Restored tabs whose MRU indexes or whose current index no longer
     *       exist are dropped, so a truncated stream cannot select a missing tab.
     */
    bool restoreState(QXmlStreamReader& reader, const QMap<int, QPair<QWidget*, int>>& tabPanes);

    /**
     * @brief A fixed convenience hint: 200 px wide and the tab bar height plus 100 px.
     * @details The height is not derived from the panes, because a pane is sized by the
     *          dock layout rather than by its content.
     */
    QSize sizeHint() const override;

    /** @brief Wide enough for the tab bar, high enough for the tab bar plus the placeholder. */
    QSize minimumSizeHint() const override;

    /**
     * @brief Checks if the widget is the active one in its parent container (e.g., QcwTabFolderContainer).
     * @return True if the folder is active.
     * @note The active state is only used to visually distinguish other inactive folders in the folder container.
     *       It does not affect other behaviors of the folder.
     * @see setActive()
     */
    bool isActive() const;

    /**
     * @brief Manually sets the active state.
     * @param active True to turn on active state, false to clear it.
     * @note The container normally drives this; setting it only repaints the tab bar.
     */
    void setActive(bool active);

    /**
     * @brief Checks if the folder is currently detached/floating.
     * @return True if the folder is hosted by a top-level window instead of the dock
     *         layout; false otherwise.
     */
    bool isFloating() const;

    /**
     * @brief Records the floating state of the folder.
     * @param floating True while the folder is hosted by a floating window.
     * @note This flag alone does not detach or re-attach anything: the container moves
     *       the folder into a window and then sets it. It decides whether the current
     *       tab's text and icon become the window title, and whether an emptied folder
     *       closes itself (see tabsCleared()).
     */
    void setFloating(bool floating);

    /**
     * @brief Adds a new tab at the end of the tab bar.
     * @param widget The pane the tab shows, or nullptr for a tab without one.
     *               Ownership is transferred: the folder becomes its parent as soon
     *               as the tab is shown.
     * @param text The label text for the tab. It also becomes @p widget's window title.
     * @return The index of the newly added tab, or -1 if @p widget is already
     *         shown by a tab of this folder; then ownership stays with the caller.
     * @note On success the new tab becomes the current tab and tabAdded() and
     *       tabsChanged() are emitted.
     */
    int addTab(QWidget* widget, const QString& text);

    /**
     * @brief Adds a new tab with an icon at the end of the tab bar.
     * @param widget The pane the tab shows, or nullptr for a tab without one.
     *               Ownership is transferred.
     * @param text The label text for the tab, also set as the pane's window title.
     * @param icon The icon to display alongside the text, drawn when the
     *             ShowIcon feature is enabled.
     * @return The index of the newly added tab, or -1 if @p widget is already
     *         shown by a tab of this folder; then ownership stays with the caller.
     * @note On success the new tab becomes the current tab.
     * @see addTab()
     */
    int addTab(QWidget* widget, const QString& text, const QIcon& icon);

    /**
     * @brief Inserts a new tab at the specified index.
     * @param index The position to insert the tab (0-based). An index outside the
     *              valid range appends the tab at the end.
     * @param widget The pane the tab shows, or nullptr for a tab without one.
     *               Ownership is transferred.
     * @param text The label text for the tab, also set as the pane's window title.
     * @return The index where the tab was inserted, or -1 if @p widget is already
     *         shown by a tab of this folder; then ownership stays with the caller.
     * @note On success the new tab becomes the current tab and tabAdded() and
     *       tabsChanged() are emitted.
     */
    int insertTab(int index, QWidget* widget, const QString& text);

    /**
     * @brief Inserts a new tab with an icon at the specified index.
     * @param index The position to insert the tab. An index outside the valid range
     *              appends the tab at the end.
     * @param widget The pane the tab shows, or nullptr for a tab without one.
     *               Ownership is transferred.
     * @param text The label text for the tab.
     * @param icon The icon to display alongside the text.
     * @return The index where the tab was inserted, or -1 if @p widget is already
     *         shown by a tab of this folder; then ownership stays with the caller.
     * @see insertTab()
     */
    int insertTab(int index, QWidget* widget, const QString& text, const QIcon& icon);

    /**
     * @brief Removes the tab at the given index.
     * @param index The index of the tab to remove. Ignored when out of range.
     * @note The associated widget is hidden. It is also scheduled for deletion when
     *       this folder was its last remaining holder, so callers must not keep using
     *       the pointer after a call that removes the last reference.
     * @note Emits tabAboutToBeRemoved(), then tabRemoved() and tabsChanged(). If the
     *       folder became empty, a floating folder closes itself and a docked one
     *       emits tabsCleared().
     * @see takeTab()
     */
    void removeTab(int index);

    /**
     * @brief Moves a tab from one index to another.
     * @param indexFrom The current index of the tab.
     * @param indexTo The target index.
     * @note Ignored unless both indices are valid. Emits tabMoved() and tabsChanged().
     * @note The current tab stays current: its index is remapped together with the
     *       tab list. Selecting the moved tab is the caller's choice, e.g. via
     *       setCurrentIndex(indexTo) for a user-initiated reorder.
     */
    void moveTab(int indexFrom, int indexTo);

    /**
     * @brief Transfers a tab to another folder, or reorders it within this one.
     * @param adoptor The target folder that will receive the tab. Passing this folder
     *                itself performs a move instead, and the moved tab becomes current.
     * @param index The index of the tab in this folder to transfer.
     * @param[in,out] indexInAdopter The position the tab should take in @p adoptor;
     *             an out-of-range value means "append". On return, the index the tab
     *             was placed at, or -1 when @p adoptor already shows this tab's pane
     *             or @p index is out of range.
     * @note A rejected transfer leaves both folders untouched. On success the tab is
     *       removed here (tabRemoved(), tabsChanged()) and added there, and
     *       tabTransferred() is emitted.
     */
    void takeTab(QcwTabFolder* adoptor, int index, int& indexInAdopter);

    /**
     * @brief Retrieves the pane shown by the tab at the specified index.
     * @param index The tab index.
     * @return Pointer to the pane, or nullptr if index is invalid or the tab has no pane.
     * @see indexOf()
     */
    QWidget* tabPane(int index) const;

    /**
     * @brief Replaces the pane shown by the tab at the specified index.
     * @param index The tab index. Ignored when out of range.
     * @param widget The new pane, or nullptr to leave the tab without one.
     *               Ownership is transferred.
     * @note Ignored when another tab of this folder already shows @p widget, or
     *       when the tab already has it; then ownership stays with the caller.
     * @note The displaced pane is hidden and released: it survives only while another
     *       folder still holds it, otherwise it is scheduled for deletion.
     */
    void setTabPane(int index, QWidget* widget);

    /**
     * @brief Retrieves the session id of the pane shown by a tab.
     * @param index The tab index.
     * @return The pane's @c ViewId dynamic property, or -1 when the index is invalid,
     *         the tab has no pane, or the pane carries no id.
     * @note The same id is what saveState() stores per tab and what restoreState()
     *       looks a pane up by, so it identifies the view across sessions. The key
     *       holding it in tabData() is qcw::TabPaneIdPropertyName (@c ViewId).
     */
    int tabPaneId(int index) const;

    /**
     * @brief Gets the text label of a tab.
     * @param index The tab index.
     * @return The tab text, or an empty string if index is invalid.
     */
    QString tabText(int index) const;

    /**
     * @brief Sets the text label of a tab.
     * @param index The tab index. Ignored when out of range.
     * @param text The new text. The leading mnemonic, if any, is re-grabbed as the
     *             tab's shortcut and the tab is re-measured.
     * @note Also sets @p text as the window title of the tab's pane, which a floating
     *       folder shows as its window title. Emits tabTextChanged() on a real change.
     */
    void setTabText(int index, const QString& text);

    /**
     * @brief Gets the icon of a tab.
     * @param index The tab index.
     * @return The tab icon, or a null icon if index is invalid.
     */
    QIcon tabIcon(int index) const;

    /**
     * @brief Sets the icon of a tab.
     * @param index The tab index. Ignored when out of range.
     * @param icon The new icon, drawn when the ShowIcon feature is enabled.
     */
    void setTabIcon(int index, const QIcon& icon);

    /**
     * @brief Retrieves user data associated with a tab.
     * @param index The tab index.
     * @return The stored data, or an empty map if index is invalid.
     * @note The map carries the tab's @c ViewId entry among the caller's keys;
     *       see setTabData().
     */
    const QMap<QString, QVariant>& tabData(int index) const;

    /**
     * @brief Retrieves user data associated with a tab.
     * @param index The tab index.
     * @param key The key from the data map.
     * @return The data value associated with the key @c key, or an invalid QVariant
     *         if index is invalid or the key is absent.
     */
    QVariant tabData(int index, const QString& key) const;

    /**
     * @brief Stores user data associated with a tab.
     * @param index The tab index. Ignored when out of range.
     * @param data The data set to store, replacing any previous value.
     * @note The whole map is serialized by saveState(), so a value that cannot be
     *       converted by QcwSerializeUtils will not survive a session. Keeping the
     *       @c ViewId entry is the caller's job once it replaces the map.
     */
    void setTabData(int index, const QMap<QString, QVariant>& data);

    /**
     * @brief Stores user data associated with a tab.
     * @param index The tab index. Ignored when out of range.
     * @param key The key from the data map.
     * @param value The data value associated with the key @c key.
     */
    void setTabData(int index, const QString& key, const QVariant& value);

    /**
     * @brief Checks if the tab at the specified index can be closed.
     * @param index The index of the tab to check.
     * @return true if the tab is closable, false otherwise.
     * @note If the index is out of range, returns false. New tabs are closable.
     * @note A false value hides every close affordance and makes the context menu
     *       skip the tab; it does not prevent removeTab()/takeTab().
     */
    bool isTabClosable(int index) const;

    /**
     * @brief Sets whether the tab at the specified index can be closed.
     * @param index     The index of the tab to modify. Ignored when out of range.
     * @param closable  true to enable closing (show close button), false to disable it.
     * @see isTabClosable()
     */
    void setTabClosable(int index, bool closable);

    /**
     * @brief Checks if the tab at the specified index can be detached from the folder.
     *
     * Detaching usually means dragging the tab out to create a new independent window.
     *
     * @param index The index of the tab to check.
     * @return true if the tab is detachable, false otherwise. Out of range is false.
     */
    bool isTabDetachable(int index) const;

    /**
     * @brief Sets whether the tab at the specified index can be detached.
     *
     * When enabled, users can drag the tab out of the QcwTabFolder to create
     * a new standalone window or folder. When disabled, the tab remains locked
     * within the current container.
     *
     * @param index       The index of the tab to modify. Ignored when out of range.
     * @param detachable  true to allow detaching, false to lock the tab in place.
     * @note New tabs are detachable.
     * @see isTabDetachable()
     */
    void setTabDetachable(int index, bool detachable);

    /**
     * @brief Checks if a tab is marked as 'dirty' (modified/unsaved).
     * @param index The tab index.
     * @return True if the tab is dirty, false also when index is invalid.
     * @note The renderer draws a dirty tab differently (and the text width is
     *       re-measured), but nothing sets this flag on its own; the view must.
     */
    bool isTabDirty(int index) const;

    /**
     * @brief Sets the dirty state of a tab.
     * @param index The tab index. Ignored when out of range.
     * @param dirty True to mark as modified, false to mark as clean.
     */
    void setTabDirty(int index, bool dirty);

    /**
     * @brief Gets the tooltip text for a tab.
     * @param index The tab index.
     * @return The tooltip string, or an empty string if index is invalid.
     */
    QString tabToolTip(int index) const;

    /**
     * @brief Sets the tooltip text for a tab.
     * @param index The tab index. Ignored when out of range.
     * @param text The tooltip text.
     */
    void setTabToolTip(int index, const QString& text);

    /**
     * @brief Checks if the tab is currently drawn in the tab bar.
     * @param index The tab index.
     * @return True if the tab is visible, false also when index is invalid.
     * @note Purely a layout result: the folder shows the tabs that fit the width and
     *       hides the rest, so this changes with the size, the features and the tab
     *       count. A hidden tab is still a normal tab: it can be selected with
     *       setCurrentIndex() and is listed by the tab list button.
     * @see tabBarFreeSpace()
     */
    bool isTabVisible(int index) const;

    /**
     * @brief Gets the widget shown at the right end of the tab bar.
     * @return Pointer to the right-side widget, or nullptr.
     * @details Lets a user of the folder put custom content there: a tool bar, a composite
     *          widget with labels, or any other widget. The widget stays owned by its
     *          caller; the folder only hosts it (see setRightWidget()).
     * @note The tabs always win: the widget sits in the gap between the tab list and the
     *       folder buttons, and while any tab is hidden behind that list, or while
     *       tabBarFreeSpace() leaves less than its size hint plus one tab spacing there,
     *       the widget moves into a strip at the top of the current pane instead of being
     *       hidden, and the pane is laid out below that strip. Moving it never hides a tab.
     * @see setRightWidget(), tabBarFreeSpace()
     */
    QWidget* rightWidget() const;

    /**
     * @brief Sets the widget shown at the right end of the tab bar.
     * @param widget The widget, or nullptr to remove the current one. Any widget works,
     *               including one with a layout and several children.
     *
     * @details The caller keeps ownership. The folder reparents the widget while it is
     *          installed and hands it back, hidden, to the parent it had when installed
     *          (or leaves it parentless when it had none) when it is replaced, removed, or
     *          the folder is destroyed. A widget whose original parent no longer exists is
     *          deleted. While the tab bar cannot hold the widget, the folder lays it out
     *          at the top of the current pane, right-aligned, and lays the pane out below
     *          it, so the widget never covers the view. It is never hidden for want of
     *          room; only a widget whose hint leaves no room for the pane stays hidden.
     *
     * @note The widget's sizeHint() is measured on every layout, and the tab bar reserves
     *       room for the whole hint, so no content is squeezed out. Keep the hint cheap
     *       and consistent.
     * @see rightWidget(), tabBarFreeSpace(), setRightWidget(QWidget*, QWidget*)
     */
    void setRightWidget(QWidget* widget);

    /**
     * @brief Sets the widget shown at the right end of the tab bar, naming its owner.
     * @param widget The widget, or nullptr to remove the current one.
     * @param owner The widget owning @p widget, usually the view it belongs to, or nullptr.
     *
     * @details This form keeps the owner stable when several folders install the same
     *          widget - a view shown by more than one perspective, for example. The folder
     *          hands the widget back to @p owner only while it is the folder currently
     *          hosting it; when another folder has installed it since, that folder keeps it.
     *          A destroyed @p owner takes the widget with it from the folder hosting it.
     *          setRightWidget(QWidget*) infers the owner from the widget's parent instead,
     *          which is only correct for a widget no other folder displays.
     * @see rightWidget(), setRightWidget(QWidget*)
     */
    void setRightWidget(QWidget* widget, QWidget* owner);

    /**
     * @brief Minimizes the folder content area.
     * @details Clicks the minimize button, i.e. emits minimizeRequested() and lets
     *          QcwTabFolderContainer collapse the folder into a window tool button.
     * @note Ignored unless the Minimizable feature is enabled.
     * @see minimizeRequested(), qcw::Minimizable
     */
    void minimize();

    /**
     * @brief Maximizes the folder to fill the parent container.
     * @details Clicks the maximize button: from the normal state that emits
     *          maximizeRequested(), which hides the sibling folders, and from any
     *          other state restoreRequested(), which brings them back.
     * @note Ignored unless the Maximizable feature is enabled.
     * @see maximizeRequested(), restoreRequested(), qcw::Maximizable
     */
    void maximize();

    /**
     * @brief Gets the current state of the folder.
     * @return The current qcw::FolderState enum value.
     * @note This is the value the container recorded; it does not have to match what
     *       the layout is currently doing.
     */
    qcw::FolderState state() const;

    /**
     * @brief Sets the state of the folder.
     * @param state The target state (Normal, Minimized, Maximized).
     * @note Only stores the value and refreshes the maximize button icon and tooltip.
     *       The layout change is QcwTabFolderContainer's job; call maximize() or
     *       minimize() to request one.
     */
    void setState(qcw::FolderState state);

    /**
     * @brief Calculates the remaining free space in the tab bar.
     * @return Available pixel width for additional tabs; negative when the visible
     *         tabs already overflow the bar.
     * @note This is measured before the folder buttons and the right widget claim the
     *       strip's end, so it is also what setRightWidget() is compared against.
     * @note The folder keeps this in step with the layout, so it is only meaningful
     *       after the widget has been sized.
     */
    int tabBarFreeSpace() const;

    /**
     * @brief Finds the index of a specific widget.
     * @param widget The widget to search for.
     * @return The index of the widget, or -1 if not found.
     * @note With a null @p widget this returns the first tab that has no pane.
     */
    int indexOf(QWidget* widget) const;

    /**
     * @brief Finds the tab index at a specific local position.
     * @param pos A point in the folder's own coordinates, i.e. mapped through
     *            mapFromGlobal() for a global position.
     * @return The tab index at the position, or -1 if no tab is there.
     * @note Only tabs that are currently drawn in the bar are hit-tested.
     */
    int indexAt(const QPoint& pos) const;

    /**
     * @brief Gets the index of the currently active tab.
     * @return Current tab index, or -1 when no tab is current (empty folder).
     */
    int currentIndex() const;

    /**
     * @brief Sets the currently active tab by index.
     * @param index The index to activate. -1 hides every pane and shows the
     *              placeholder; any other out-of-range index is ignored.
     * @note The newly shown pane takes the focus, and the index moves to the back of
     *       the MRU list. currentChanged() is emitted for every accepted call, even
     *       when @p index is already the current tab.
     */
    void setCurrentIndex(int index);

    /**
     * @brief Gets the widget of the currently active tab.
     * @return Pointer to the current widget, or nullptr when no tab is current or its
     *         pane is null. Never returns the placeholder.
     */
    QWidget* currentWidget() const;

    /**
     * @brief Validates if an index is within the valid range.
     * @param index The index to check.
     * @return True if 0 <= index < count(). Note that a valid index can still name a
     *         tab that is hidden because the tab bar has no room for it.
     */
    bool isValidIndex(int index) const;

    /**
     * @brief Gets the placeholder widget shown when no tabs are present or the current tab's widget is null.
     * @return Pointer to the placeholder widget. A folder always has one: a plain
     *         empty QWidget named @c placeholder unless setPlaceholder() replaced it.
     */
    QWidget* placeholder() const;

    /**
     * @brief Sets a placeholder widget to display when the folder is empty or the current tab's widget is null.
     * @param widget The placeholder widget. Ownership is transferred.
     * @note Ignored when @p widget is null or already the placeholder. The widget it
     *       replaces is hidden and scheduled for deletion.
     */
    void setPlaceholder(QWidget* widget);

    /**
     * @brief Gets the context menu for the tab bar.
     * @return Pointer to the QMenu object.
     * @note The folder owns the menu and rebuilds its entries on every right click,
     *       hiding the ones that do not apply to the tab under the cursor. Add your
     *       own actions here; they are kept across those updates.
     */
    QMenu* tabMenu() const;

    /**
     * @brief Gets the total number of tabs.
     * @return Tab count, including the tabs hidden because the bar is too narrow.
     */
    int count() const;

    /**
     * @brief Gets the current tab bar position.
     * @return The qcw::TabPosition enum value.
     */
    qcw::TabPosition tabPosition() const;

    /**
     * @brief Sets the tab bar position.
     * @param pos The new position.
     * @note Relays out the tab bar and the pane area, and repaints the tabs.
     */
    void setTabPosition(qcw::TabPosition pos);

    /**
     * @brief Gets the enabled features of the tab bar.
     * @return The qcw::FolderFeatures flag set.
     */
    qcw::FolderFeatures features() const;

    /**
     * @brief Sets the enabled features of the tab bar.
     * @param features The feature flags to enable, replacing the current set.
     * @note Relays out the tab bar: Minimizable and Maximizable add or remove their
     *       buttons, MultiTabs and UseMRU change how tabs are shown, and ShowIcon,
     *       ShowFullTitle and the close-button flags change what is drawn per tab.
     */
    void setFeatures(qcw::FolderFeatures features);

    /**
     * @brief Enables or disables a specific tab feature.
     * @param feature The specific feature flag.
     * @param on True to enable, false to disable.
     * @see setFeatures()
     */
    void setFeature(qcw::FolderFeature feature, bool on);

    // --- Renderer and tab bar metrics ---

    /**
     * @brief Gets the custom renderer instance.
     * @return Pointer to the QcwTabFolderRenderer. Never null: the folder falls back
     *         to the default renderer.
     */
    QcwTabFolderRenderer* renderer() const;

    /**
     * @brief Sets a custom renderer for advanced drawing control.
     * @param renderer The new renderer instance. Ownership is transferred; the
     *                 previous renderer is deleted. Passing nullptr installs a fresh
     *                 default renderer.
     * @note The renderer is queried while measuring and painting tabs, so a folder
     *       that is already visible is measured and repainted right away.
     * @see QcwTabFolderRenderer::createDefaultRenderer()
     */
    void setRenderer(QcwTabFolderRenderer* renderer);

    /**
     * @brief Gets the size of tab icons.
     * @return The edge length in pixels.
     * @note Only applied when the ShowIcon feature is enabled.
     */
    int iconSize() const;

    /** @brief Sets the size of tab icons, in pixels. Relays out the tab bar. */
    void setIconSize(int size);

    /**
     * @brief Gets the minimum width reserved for tab text.
     * @return The width in pixels a shrunken tab still gives its label.
     * @note Ignored while ShowFullTitle is enabled, since then tabs get their full
     *       text width.
     */
    int minimumTextRectWidth() const;

    /**
     * @brief Sets the minimum width reserved for tab text.
     * @note Re-measures the tabs unless ShowFullTitle is enabled, which ignores this
     *       width.
     */
    void setMinimumTextRectWidth(int textRectWidth);

signals:
    /**
     * @brief Emitted when the current tab index changes.
     * @param index The new current tab index, -1 when no tab is current.
     * @note Also emitted when the current tab is re-selected, see setCurrentIndex().
     */
    void currentChanged(int index);

    /**
     * @brief Emitted when a new tab is added.
     * @param index The index of the added tab.
     * @note Also emitted for the tabs restoreState() re-adds, so a listener that must
     *       not react during session restore has to guard itself.
     */
    void tabAdded(int index);

    /**
     * @brief Emitted when the user requests to close a tab (e.g., clicks 'x').
     * @param index The index of the tab to close.
     * @note The folder does not remove anything itself: the receiver decides, usually
     *       by calling removeTab() after the pane has been saved or discarded. For a folder
     *       of a QcwWindow that receiver is the window, which asks the QcwView and removes
     *       its tab once it agrees; panes that are not QcwViews still need the application's
     *       own receiver.
     * @see removeTab(), QcwView::canClose()
     */
    void tabCloseRequested(int index);

    /**
     * @brief Emitted before a tab is removed.
     * @param index The index of the tab being removed.
     * @note The tab is still valid here; the pane is released right after this signal.
     */
    void tabAboutToBeRemoved(int index);

    /**
     * @brief Emitted after a tab has been removed.
     * @param index The previous index of the removed tab.
     * @param widget Pointer to the removed widget, or nullptr when the tab had no pane.
     * @note The pane is already hidden and may be scheduled for deletion; use it only
     *       for matching, not beyond this signal.
     */
    void tabRemoved(int index, QWidget* widget);

    /**
     * @brief Emitted when a tab is moved.
     * @param from The original index.
     * @param to The new index.
     * @note Emitted by moveTab() and therefore also by a user-initiated reorder.
     */
    void tabMoved(int from, int to);

    /**
     * @brief Emitted when the list of tabs changes structurally.
     * @note Follows tabAdded(), tabRemoved() and tabMoved(); a tab's text, icon or
     *       other attribute changing does not emit it.
     */
    void tabsChanged();

    /**
     * @brief Emitted when tab text is modified.
     * @param index The tab index.
     * @param newText The new text.
     * @param oldText The previous text.
     */
    void tabTextChanged(int index, const QString& newText, const QString& oldText);

    /**
     * @brief Emitted when a tab is dragged and dropped into another folder.
     * @param source The source folder.
     * @param target The target folder.
     * @param widget The transferred widget, or nullptr when the tab had no pane.
     * @note Emitted by the source folder after both folders are updated.
     */
    void tabTransferred(QcwTabFolder* source, QcwTabFolder* target, QWidget* widget);

    /**
     * @brief Emitted when a tab bar label is clicked.
     * @param index The clicked tab index.
     * @note Also emitted when focus or a plain mouse press lands inside an inactive
     *       folder, using its current index, so that the folder can be activated.
     */
    void tabBarClicked(int index);

    /**
     * @brief Emitted when a tab bar label is double-clicked.
     * @param index The double-clicked tab index.
     * @note A double click outside the close button also clicks the maximize button,
     *       so maximizeRequested() or restoreRequested() is emitted with it whenever
     *       the Maximizable feature is on.
     */
    void tabBarDoubleClicked(int index);

    /**
     * @brief Request to maximize the folder.
     * @details Emitted when the maximize button is clicked in the normal state.
     *          QcwTabFolderContainer handles it by hiding the sibling folders.
     */
    void maximizeRequested();

    /**
     * @brief Request to restore the folder from maximized state.
     * @details Emitted when the maximize button is clicked while the folder is not in
     *          the normal state; the container then brings back what it hid.
     */
    void restoreRequested();

    /**
     * @brief Emitted immediately before restoring state.
     * @details Emitted by QcwTabFolderContainer before it puts a folder it had taken
     *          out of the layout back into it, so a holder such as the fast view tool
     *          bar can drop its stand-in for the folder first.
     */
    void aboutToRestore();

    /**
     * @brief Request to minimize the folder.
     * @details Emitted when the minimize button is clicked; the container stores the
     *          folder's features and turns it into a tool button.
     */
    void minimizeRequested();

    /**
     * @brief Emitted when the last tab is removed and the folder becomes empty.
     * @note A floating folder closes itself instead of emitting this.
     * @see QcwTabFolderContainer::emptyFolderAction()
     */
    void tabsCleared();

    /**
     * @brief Emitted before the folder is deleted.
     * @details Emitted by QcwTabFolderContainer when an emptied folder is taken out
     *          of the layout (its empty folder action is Delete). The folder and the
     *          widget hosting it are scheduled for deletion right after, except for
     *          the shared folder, which is only hidden and reused.
     */
    void aboutToDelete();

    /**
     * @brief Emitted when the folder receives a close event.
     * @param event The close event object, allowing acceptance/rejection.
     * @note The folder closes only if @p event is still accepted afterwards, so a
     *       receiver can veto the close, e.g. to ask about unsaved panes.
     */
    void aboutToClose(QCloseEvent* event);

protected:
    // The tab bar is not a child widget: it is painted, measured and hit-tested here,
    // so the event overrides below carry the folder's own interaction logic.

    /** Serves the tab tooltips (including the elided label) and the mnemonic shortcut that selects a tab. */
    bool event(QEvent* event) override;

    /** Draws the frame, then the tabs and the tab bar through renderer(), including the drop indicator. */
    void paintEvent(QPaintEvent* event) override;

    /** Records the pressed tab and, with TabsMovable, the point a reorder drag starts from. */
    void mousePressEvent(QMouseEvent* event) override;

    /** Tracks hover, then starts the tab drag; a drag nothing accepts detaches the tab. */
    void mouseMoveEvent(QMouseEvent* event) override;

    /** Closes the tab on its close button, selects it otherwise, and emits tabBarClicked(). */
    void mouseReleaseEvent(QMouseEvent* event) override;

    /** Double-clicking a tab toggles maximize/restore; see tabBarDoubleClicked(). */
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    /**
     * @brief Accepts a tab drag over the tab bar.
     * @details A drag from this folder is always accepted for reordering; one from
     *          another folder needs the AcceptDroppedTabs feature.
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /** Places the drop indicator on the near side of the hovered tab, or after the last one. */
    void dragMoveEvent(QDragMoveEvent* event) override;

    /** Clears the hover and drop-indicator state. */
    void dragLeaveEvent(QDragLeaveEvent* event) override;

    /** Reorders the tab with TabsMovable, or takes it from the source folder. */
    void dropEvent(QDropEvent* event) override;

    /** Measures the tab bar and the pane area for the size the folder is shown at. */
    void showEvent(QShowEvent* event) override;

    /** Emits aboutToClose() first, so a receiver can veto the close. */
    void closeEvent(QCloseEvent* event) override;

    /** Recomputes the tab bar and pane geometry. */
    void resizeEvent(QResizeEvent* event) override;

    /** Clears the tab hover state. */
    void leaveEvent(QEvent* event) override;

    /** Re-measures the tabs after a style or font change. */
    void changeEvent(QEvent* event) override;

    /** Installed on the application object: a press inside this folder activates it. */
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    /// Pointer to the private implementation, created by the constructor and deleted
    /// by the destructor. It owns the tab list and the tab bar's child widgets.
    QcwTabFolderPrivate* impl;
};

QT_END_NAMESPACE
