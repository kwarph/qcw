# QCW Command, Handler, Action Set and Menu Contribution Design

- Status: Confirmed design
- Scope: `workbench` module, plus the `view` / `perspective` / `plugin` interfaces it interacts with
- Reference: Eclipse IDE command / handler, menu contribution, and action set mechanisms
- Goal: Refactor QCW's command, menu, toolbar, shortcut and action set system while preserving Qt conventions

---

## Table of Contents

1. [Background and Goals](#1-background-and-goals)
2. [Current State Analysis](#2-current-state-analysis)
3. [Overall Model](#3-overall-model)
4. [Command Declaration Layer](#4-command-declaration-layer)
5. [Handler Layer](#5-handler-layer)
6. [Command QAction](#6-command-qaction)
7. [Action Set Layer](#7-action-set-layer)
8. [Contribution Layer](#8-contribution-layer)
9. [Shortcuts](#9-shortcuts)
10. [Naming Conventions](#10-naming-conventions)
11. [Runtime Contracts and Cross-Plugin Extension](#11-runtime-contracts-and-cross-plugin-extension)
12. [Diagnostics](#12-diagnostics)
13. [Comparison with Existing Code](#13-comparison-with-existing-code)
14. [Migration Examples](#14-migration-examples)
15. [Implementation Phases](#15-implementation-phases)
16. [Design Decision Summary](#16-design-decision-summary)

---

## 1. Background and Goals

QCW is currently `QAction`-centric: `QcwView::actionSets()` lets the current view dynamically contribute menu actions, `QcwCommandSpec` adds application commands to the File/Edit skeleton, `QcwGlobalActions` publishes process-wide actions, and `QcwWindow::resolveCommand()` looks up `QAction`s in a fixed order.

This mechanism is simple and Qt-friendly, but has the following limitations:

1. `QAction` simultaneously carries identity, metadata, handler and UI, so command metadata and UI presentation can drift apart;
2. There is no formal carrier for "UI-less commands";
3. The same command cannot be handled by multiple handlers competing by context;
4. Menu contribution position relies on `*Start` / `*End` anchors, which have limited expressiveness;
5. There is no action set concept, so a group of commands cannot be shown/hidden by perspective;
6. Command parameters can only be passed temporarily through `QAction::data()`.

### Design Goals

1. Separate four layers: command declaration, handler implementation, menu contribution, action set;
2. Support UI-less commands;
3. Support multiple handlers for the same command id, competing by scope;
4. Support inserting menu contributions before or after any command;
5. Support automatic action set visibility switching by perspective;
6. Use `QVariantMap` uniformly for command parameters;
7. Remove `QcwView::actionSets()`; ordinary views no longer dynamically contribute to the main menu or main toolbar;
8. Preserve Qt conventions and avoid Eclipse's full expression engine.

### Non-Goals

- Do not introduce Eclipse's `enabledWhen` / `visibleWhen` expression engine;
- Do not require plugins to link to one another or publish public headers;
- Do not change `QcwView`'s role as tab content or `QcwPerspective`'s role as layout container.

---

## 2. Current State Analysis

### 2.1 Existing Chain

| Layer | Existing Entity | Responsibility |
|---|---|---|
| Command identity | `QcwWindowActionIds`, string ids | Name table for built-in commands, menus and anchors |
| Command metadata | `QcwCommandInfo` + `QcwCommandRegistry` | Process-wide catalog for palette listing and search |
| Menu slot declaration | `QcwCommandSpec` + `QcwWindow::registerCommand()` | Adds application commands to the File/Edit skeleton |
| Handler carrier | `QAction` | Returned by views via `actionSets()`, or published by `QcwGlobalActions` |
| Action set | `QcwView::actionSets()` | Grouped per view; contributed as a whole when the view becomes current |
| Menu contribution position | `*Start` / `*End` anchors; fixed slots in standard menus | Contributions sit between anchors; standard slots are filled by id |
| Handler resolution | `QcwWindow::resolveCommand()`, `composeCommandMenu()` | Current view → other views of current perspective → other views of window → `globalActions` |
| Visibility/enabled rules | `QcwCommandScope` + `QAction::setEnabled()` | Simplified expression |
| Parameters | Temporary use of `QAction::data()` | No formal parameter model |
| Shortcuts | `QAction::setShortcut()` | No independent key binding layer |

### 2.2 Key Trade-offs

- **QAction-centric**: Qt-friendly; views own actions, lifetime is clear. The cost is that metadata and UI can drift, and multiple handlers cannot be expressed.
- **Fixed lookup order**: Predictable, no expressions, but "the same command executed by different objects in different contexts" can only be simulated by different views returning different actions.
- **Anchors**: Lightweight, but cannot express "insert after Save".
- **No action set visibility**: View switching naturally switches actions, but cross-view logical grouping cannot be controlled by perspective.

---

## 3. Overall Model

### 3.1 Four-Layer Model

| Layer | Entity | Responsibility | Registrant |
|---|---|---|---|
| Command declaration | `QcwCommandInfo` + `QcwCommandRegistry` | id, label, icon, description, category, default shortcut, checkable; placement and action-set membership belong to `QcwContribution` | workbench / application / plugin |
| Handler | `QcwCommandHandler` | `execute(params)`, `isEnabled()`, `isVisible()`, `isChecked()` | view / perspective / window / workbench |
| Menu and toolbar contribution | `QcwContribution` + `QcwContributionRegistry` | command id, target, before/after, grouping, action set | workbench / application / plugin |
| Action set | `QcwActionSet` + `QcwActionSetRegistry` | logical grouping of contributions, visibility switched by perspective | workbench / application / plugin |

### 3.2 Handler Resolution Order

For the same command id, handlers are resolved in this order:

```text
current view's handler
→ current perspective's handler
→ window's handler
→ workbench global handler
```

The first existing handler wins. `accepts(params)` is used only for parameter validation and does not participate in handler competition.

### 3.3 Core Principles

1. **A command is a declaration, a handler is an implementation, a contribution is a position.** The three are linked by command id and do not reference each other directly.
2. **Views no longer dynamically contribute menus.** `QcwView::actionSets()` is removed; views only provide handlers.
3. **Menus are built from declarative contributions.** Contributions are positioned by before/after, no longer relying on `*Start` / `*End` anchors.
4. **Action sets switch automatically by perspective.** Perspectives declare visible action sets; the window rebuilds menus/toolbars on switch.
5. **Execution always goes through `executeCommand(id, params)`.** Shortcuts, palette, toolbar and programmatic calls share one path.
6. **An id is an organizational identifier internal to its owner and does not serve as a cross-plugin contract.** Cross-plugin extension goes only through interfaces.

---

## 4. Command Declaration Layer

### 4.1 `QcwCommandInfo`

```cpp
struct QCW_EXPORT QcwCommandInfo
{
    /// Command id; format described in Section 10.
    QString id;

    /// Palette category, e.g. "File", "Edit", "Window"; empty means "Application".
    QString category;

    /// Command display name.
    QString label;

    /// One-line description shown in the palette.
    QString description;

    /// Default shortcut; UnknownKey means none.
    QKeySequence defaultShortcut;

    /// Command icon; reused by menu, toolbar and palette.
    QIcon icon;

    /// Whether this is a checkable command; the command QAction calls setCheckable() accordingly.
    bool checkable { false };

    bool operator==(const QcwCommandInfo&) const;
};
```

The struct deliberately carries neither a scope nor an action set. Which handler answers is the
resolution order `QcwWindow::resolveHandler()` walks; what a perspective hides is the
`actionSetId` of the `QcwContribution` that places the command (Section 8). A command can be
placed by several contributions with different action sets, so a single field here could not be
authoritative.

### 4.2 `QcwCommandRegistry`

```cpp
class QCW_EXPORT QcwCommandRegistry : public QObject
{
    Q_OBJECT
public:
    static QcwCommandRegistry* instance();

    /// Register or replace; returns false if the id is invalid or empty.
    bool registerCommand(const QcwCommandInfo& info);

    /// Unregister; the window deletes the corresponding command QAction.
    bool unregisterCommand(const QString& id);

    std::optional<QcwCommandInfo> command(const QString& id) const;
    bool contains(const QString& id) const;

    /// Sorted by category, label, id.
    QList<QcwCommandInfo> commands() const;

signals:
    void commandsChanged();
};
```

**Registration semantics:** replace. Re-registering the same id emits a `qWarning` once indicating that the previous metadata was replaced.

**Format validation:** `registerCommand()` validates the id character set at the entry point (see Section 10). Invalid ids are rejected with a `qWarning`.

---

## 5. Handler Layer

### 5.1 `QcwCommandHandler`

```cpp
class QCW_EXPORT QcwCommandHandler : public QObject
{
    Q_OBJECT
public:
    using Params = QVariantMap;

    explicit QcwCommandHandler(QObject* parent = nullptr);

    /// Whether the command can execute in the current state; menu items, toolbar buttons
    /// and the palette all read it.
    virtual bool isEnabled() const { return true; }

    /// Whether the command should appear in menus, toolbars and the palette. Visible by default.
    virtual bool isVisible() const { return true; }

    /// Whether the command is checked; only checkable commands read it.
    virtual bool isChecked() const { return false; }

    /// Called when the user toggles the checked state; checkable handlers should override it
    /// and update their internal state. It should not emit stateChanged() here, to avoid
    /// refresh loops.
    virtual void setChecked(bool checked) { Q_UNUSED(checked); }

    /// Execute the command; checkable commands are called after setChecked().
    virtual void execute(const Params& params) = 0;

    /// Whether this handler accepts the given parameters; accepts everything by default.
    /// Used only for caller-side validation and does not participate in handler competition.
    virtual bool accepts(const Params& params) const
    {
        Q_UNUSED(params);
        return true;
    }

signals:
    /// isEnabled(), isVisible() or isChecked() may have changed.
    void stateChanged();
};
```

### 5.2 Scope Registration

```cpp
class QcwView : public QWidget, public QcwPersistable
{
public:
    /// Handlers provided by this view, keyed by command id. The window collects them when the
    /// view becomes current.
    virtual QHash<QString, QcwCommandHandler*> commandHandlers() const { return {}; }

    // Removed:
    // virtual QMap<QString, QList<QPair<QString, QAction*>>> actionSets() const;
};

class QcwPerspective : public QObject, public QcwPersistable
{
public:
    void registerHandler(const QString& commandId, QcwCommandHandler* handler);
    void unregisterHandler(const QString& commandId);
    QcwCommandHandler* handler(const QString& commandId) const;
};

class QcwWindow : public QMainWindow, public QcwPersistable
{
public:
    void registerHandler(const QString& commandId, QcwCommandHandler* handler);
    void unregisterHandler(const QString& commandId);
    QcwCommandHandler* resolveHandler(const QString& commandId) const;

    bool executeCommand(const QString& commandId, const QVariantMap& params = {});
    bool isCommandEnabled(const QString& commandId) const;
};

class QcwWorkbench : public QObject, public QcwPersistable
{
public:
    void registerGlobalHandler(const QString& commandId, QcwCommandHandler* handler);
    void unregisterGlobalHandler(const QString& commandId);
    QcwCommandHandler* globalHandler(const QString& commandId) const;
};
```

### 5.3 Execution Semantics

```cpp
bool QcwWindow::executeCommand(const QString& commandId, const Params& params)
{
    auto* handler = resolveHandler(commandId);
    if (!handler || !handler->isEnabled() || !handler->accepts(params))
    {
        return false;
    }
    handler->execute(params);
    return true;
}
```

**Registration semantics:** replace. Registering the same id again on perspective / window / global replaces the handler and returns whether a replacement occurred.

**Ownership:**

- View handlers are owned by the view; the window only borrows them.
- Perspective / window / global handlers are owned by the registrant. It is recommended to use the registrant as the parent, or to call `unregisterHandler()` explicitly.
- On registration, connect to `QObject::destroyed` to auto-unregister and avoid dangling pointers.

### 5.4 Execution Order for Checkable Commands

```text
User clicks a menu item / toolbar button
→ the command QAction toggles checked automatically
→ the window receives QAction::toggled(bool)
→ handler->setChecked(action->isChecked())   // pre-sync, no stateChanged
→ executeCommand(id)
→ handler executes its business logic
→ if the business state differs from the action, the handler emits stateChanged()
→ the window refreshes action->setChecked(handler->isChecked())
```

Destruction order. Handlers are QObjects and are usually children of the scope that registered them: the window, the perspective or the workbench. Qt destroys those children from QWidget::~QWidget() / QObject::~QObject(), which run after the scope's own destructor body. Each registration records both connections — `stateChanged` and `destroyed` — and one helper drops them together: when the handler is unregistered, when its registration is replaced, when the handler itself is destroyed, and in the scope's destructor body. That last step is what keeps a child handler's `destroyed()` callback from reaching into an impl that is already gone.

---

## 6. Command QAction

### 6.1 Ownership and Creation Timing

**Each `QcwWindow` lazily creates one command QAction per command, owned by the window.** Menu items and toolbar buttons do not create their own QActions; they reference this shared QAction.

```cpp
class QcwWindow : public QMainWindow, public QcwPersistable
{
public:
    /// The shared QAction for a command; created on first request, the same object afterwards.
    /// Returns nullptr if the command is not registered in QcwCommandRegistry.
    QAction* commandAction(const QString& commandId);
};
```

**Why per-window rather than per-process:** The enabled / checked state of a command depends on the current view, and each window has its own current view. Per-window also lets the shortcut `shortcutContext` naturally fall on `Qt::WindowShortcut`, so two open windows do not interfere with each other.

**Lazy creation timing:**

- `buildMenus()` and `buildToolBars()` both ask `placedEntries()` for a target, and it calls
  `commandAction(id)` for each contribution's `commandId`;
- Programmatic calls to `executeCommand()` do not need a QAction; they look up the handler directly;
- Shortcut registration needs a QAction, so the window creates one for every catalogued command
  with a `defaultShortcut` when it is prepared, and `commandsChanged()` does the same for commands
  registered later (Section 6.3).

**Destruction:**

- When a command is unregistered from `QcwCommandRegistry`, the window removes its QAction (first from menus/toolbars);
- On window destruction, all command QActions are deleted;
- If a contribution still references an unregistered command, `placedEntries()` skips that
  contribution and emits a `qWarning`.

### 6.2 Property Sources

| Property | Source | Update Timing |
|---|---|---|
| `text` | `QcwCommandInfo::label` | `commandsChanged()` or language change |
| `icon` | `QcwCommandInfo::icon` | Same |
| `shortcut` | `QcwCommandInfo::defaultShortcut` | Same |
| `checkable` | `QcwCommandInfo::checkable` | Set once at command registration |
| `enabled` | `resolveHandler(id)->isEnabled()` | handler `stateChanged()`, handler registration/unregistration, current view change |
| `visible` | `resolveHandler(id)->isVisible()` | Same; action sets filter placement, not visibility (Section 7.5) |
| `checked` | `resolveHandler(id)->isChecked()` | handler `stateChanged()` |
| `shortcutContext` | `Qt::WindowShortcut` | Set once at creation |

### 6.3 UI-less Commands

UI-less commands have no contributions, so they do not appear in menus or toolbars. The window
therefore creates a command QAction for every catalogue entry that carries a default shortcut —
at construction and again on every `commandsChanged()` — and calls `addAction()` on itself, since
the shortcut is the only way to reach such a command. Nobody has to ask for the action first.

Setting `visible` to `false` also stops the shortcut: Qt disables an invisible QAction, so its
binding no longer fires. **Whether a QAction exists is determined by command registration; whether it is
shown is determined by contributions; whether it can run is determined by the handler.**

### 6.4 Menu and Toolbar Reuse

- `QMenu::addAction(QAction*)`: reused directly;
- `QToolBar::addAction(QAction*)`: reused directly;
- When the same command appears in multiple menus, the same QAction is shared, so state is naturally consistent;
- "The same command showing different text at different positions" is not supported.

---

## 7. Action Set Layer

### 7.1 `QcwActionSet`

```cpp
struct QCW_EXPORT QcwActionSet
{
    QString id;
    QString label;   // shown in Preferences or the Action Set dialog
};

class QCW_EXPORT QcwActionSetRegistry
{
public:
    static QcwActionSetRegistry* instance();

    void registerActionSet(const QcwActionSet& set);
    void unregisterActionSet(const QString& id);
    const QcwActionSet* actionSet(const QString& id) const;
    QList<QcwActionSet> allActionSets() const;
};
```

**Registration semantics:** replace; re-registration emits `qWarning`.

### 7.2 Perspective Visibility Declaration

```cpp
class QcwPerspective
{
public:
    /// Action sets visible while this perspective is active.
    /// Empty means all visible; non-empty means only these are visible.
    QStringList visibleActionSetIds() const;
    void setVisibleActionSetIds(const QStringList& ids);
};

class QcwPerspectiveCreator
{
public:
    virtual QStringList defaultActionSetIds() const { return {}; }
};
```

### 7.3 Visibility Semantics

| Scenario | Behavior |
|---|---|
| `visibleActionSetIds()` is empty | All action sets visible |
| `visibleActionSetIds()` is non-empty | Only the listed action sets are visible |
| Contribution's `actionSetId` is empty | Always visible, unaffected by perspective |
| Perspective references an unregistered action set | `qWarning` once, does not block construction, equivalent to "no contributions" |
| Contribution references an unregistered action set | That contribution is invisible, `qWarning` once |
| Multiple contributions for the same command id | Visibility judged independently; command visible if at least one is visible |

### 7.4 `visible` of the Command QAction

```text
visible = handler == nullptr || handler->isVisible()
```

Whether the entry appears at a position is a second, independent question: which entries exist
at all is what the sorted contributions of a target decide, and each menu and tool bar is
rebuilt from that list.

- A command no scope currently handles stays **visible and greyed out**, so a tool bar does not
  collapse and a menu keeps its shape when the current view changes;
- A handler that reports `isVisible() == false` withdraws the entry everywhere, and with it its
  shortcut, because Qt does not activate the shortcut of a hidden QAction;
- A command whose contributions are all filtered out by the perspective's action sets stops
  being placed; its QAction is left alone;
- UI-less commands have no contributions, so they appear nowhere and keep only their shortcut —
  which requires their QAction to exist, see Section 6.3.

### 7.5 Perspective Switch

`setCurrentPerspective()` performs:

1. Read the new `visibleActionSetIds()`;
2. Re-sort contributions for each target (filtering out contributions of invisible action sets);
3. Rebuild menus and tool bars from the sorted result — `rebuildTargets()` does both, so no
   target can keep the previous perspective's list.

The `visible` of a command QAction is not part of this: action-set filtering decides only whether
an entry is *placed*, and the QAction keeps the visibility its handler reported (Section 7.4).

A perspective changing its own list while it is current reaches the same rebuild through
`QcwPerspective::visibleActionSetIdsChanged()`, which `setVisibleActionSetIds()` emits when the
list actually differs.

**Optimization:** `setCurrentPerspective()` returns early when the id already names the current
perspective; switching to a different perspective always rebuilds, since comparing the two action-set
lists would miss a view or handler change that happened in between.

---

## 8. Contribution Layer

### 8.1 `QcwContribution`

```cpp
struct QCW_EXPORT QcwContribution
{
    /// Unique contribution id, used for unregistration and before/after references.
    QString id;

    /// Owning action set; empty means always visible.
    QString actionSetId;

    /// Target menu or toolbar, e.g. "File", "Edit", "ToolBar/Main".
    QString target;

    /// Command id to place.
    QString commandId;

    /// Place before this contribution id or command id; empty means unconstrained.
    QString before;

    /// Place after this contribution id or command id; empty means unconstrained.
    QString after;

    /// Group id; a separator is inserted when groupId changes.
    QString groupId;

    /// Programmatic visibility switch.
    bool visible { true };
};
```

### 8.2 `QcwContributionRegistry`

```cpp
class QCW_EXPORT QcwContributionRegistry
{
public:
    static QcwContributionRegistry* instance();

    void registerContribution(const QcwContribution& contribution);
    void unregisterContribution(const QString& id);
    void unregisterContributionsOf(const QString& actionSetId);

    QList<QcwContribution> contributionsFor(const QString& target) const;
    QList<QcwContribution> allContributions() const;

signals:
    void contributionsChanged();
};
```

**Registration semantics:** replace; re-registration emits `qWarning`.

### 8.3 Target Naming

| Type | Format | Example |
|---|---|---|
| Menu | No prefix, PascalCase hierarchical path | `File`, `Edit`, `Window/Perspective`, `Window/Appearance` |
| Toolbar | `ToolBar/` prefix + logical name | `ToolBar/Main`, `ToolBar/Debug` |

Toolbars are created on demand and owned by the window; a bar with nothing placed in it hides itself.
Menus and toolbars use exactly the same sorting algorithm.

### 8.4 Reference Resolution

The value of `before` / `after` may be a contribution id or a command id:

```text
1. Look up by contribution id within the same target's contribution list;
2. If not found, look up by command id and take the first matching contribution in that target;
3. If still not found, ignore the reference, treat the contribution as unconstrained,
   and emit a qWarning.
```

**Narrowed cross-plugin references:** the range in which an id can be referenced is "the same owner". Built-in workbench ids are a public owner and can be referenced by applications; plugin-internal ids are not cross-plugin contracts, and cross-plugin positional dependencies should use interfaces (see Section 11). Runtime behavior is unchanged: a missing target causes the contribution to be skipped with a `qWarning`.

### 8.5 Sorting Algorithm

For a target's contributions:

1. Filter to contributions with `visible == true` and whose action set is currently visible;
2. Build a dependency graph where nodes are contributions:
   - `C.before` resolves to `B`: edge `C → B`;
   - `C.after` resolves to `A`: edge `A → C`;
   - Self-references are ignored with a `qWarning`;
3. Kahn topological sort, with in-degree-0 nodes enqueued in registration order; A node participates in the graph when it has an edge in or out. A contribution that only carries an unresolved reference has no edge and falls through to the unconstrained append.
4. Unconstrained contributions (no `before` / `after`) do not participate in the graph and are appended in registration order after the topological sort;
5. Cycle detection: nodes still unoutput after the topological sort are appended in registration order with a `qWarning` listing the contribution ids and command ids on the cycle;
6. `@start` reserved anchor: contributions with `before = "@start"` are placed first in registration order; `@end` is not used, since appending unconstrained contributions to the end is equivalent.

### 8.6 Separators

Rule: **iterate the sorted contributions; if the `groupId` of the current contribution differs from the next, insert a separator between them.**

- Empty groupId → non-empty: insert;
- Non-empty → empty: insert;
- Different non-empty groupIds: insert;
- Two consecutive empty groupIds: do not insert (empty is not a group);
- When several separators are inserted consecutively, Qt's menu/toolbar displays only one; no manual merging is needed.

A new separator QAction is created on each menu build and owned by the menu.

### 8.7 Integration with Built-in Menus

`QcwWindow` still creates the File / Edit / Window / Help menu skeleton at construction, but no longer pre-creates `commandSpecs`, `commandPlaceholders` or `commandActions`. Instead:

- Built-in commands are registered as `QcwContribution`s in `QcwContributionRegistry`, with an empty `actionSetId` meaning always visible;
- `QcwWindow::buildMenus()` and `buildToolBars()` ask `placedEntries()` for
  `contributionsFor(target)` sorted, create or reuse the command QActions it returns, and insert
  them into the target;
- Each command has one command QAction per window, responsible for shortcut, enabled state and icon;
- Menu items and toolbar buttons reuse that QAction.

### 8.8 Full Algorithm Pseudocode

```cpp
QList<QcwContribution> sortContributions(
    const QList<QcwContribution>& contributions)
{
    QMap<QString, int> indexById;
    QMap<QString, int> firstIndexByCommand;
    for (int i = 0; i < contributions.size(); ++i)
    {
        indexById.insert(contributions[i].id, i);
        if (!firstIndexByCommand.contains(contributions[i].commandId))
            firstIndexByCommand.insert(contributions[i].commandId, i);
    }

    auto resolve = [&](const QString& ref) -> int
    {
        if (ref.isEmpty() || ref == "@start") return -1;
        if (indexById.contains(ref)) return indexById.value(ref);
        if (firstIndexByCommand.contains(ref)) return firstIndexByCommand.value(ref);
        qWarning() << "unresolved contribution reference" << ref;
        return -1;
    };

    QList<QSet<int>> successors(contributions.size());
    QList<int> indegree(contributions.size(), 0);
    QList<int> pinnedFirst;
    QSet<int> constrained;

    for (int i = 0; i < contributions.size(); ++i)
    {
        const auto& c = contributions[i];

        if (c.before == "@start") { pinnedFirst.append(i); continue; }

        if (!c.before.isEmpty())
        {
            const int b = resolve(c.before);
            if (b >= 0 && b != i && !successors[i].contains(b))
            {
                successors[i].insert(b);
                ++indegree[b];
            }
            constrained.insert(i);
        }

        if (!c.after.isEmpty())
        {
            const int a = resolve(c.after);
            if (a >= 0 && a != i && !successors[a].contains(i))
            {
                successors[a].insert(i);
                ++indegree[i];
            }
            constrained.insert(i);
        }
    }

    QList<int> queue;
    for (int i = 0; i < contributions.size(); ++i)
        if (constrained.contains(i) && indegree[i] == 0)
            queue.append(i);
    std::sort(queue.begin(), queue.end());

    QList<int> result;
    while (!queue.isEmpty())
    {
        const int n = queue.takeFirst();
        result.append(n);
        for (int s : successors[n])
            if (--indegree[s] == 0)
                queue.append(s);
        std::sort(queue.begin(), queue.end());
    }

    QList<int> cycle;
    for (int i = 0; i < contributions.size(); ++i)
        if (constrained.contains(i) && !result.contains(i))
            cycle.append(i);
    if (!cycle.isEmpty())
    {
        qWarning() << "cycle detected; falling back to registration order";
        std::sort(cycle.begin(), cycle.end());
        result.append(cycle);
    }

    for (int i = 0; i < contributions.size(); ++i)
        if (!constrained.contains(i) && !pinnedFirst.contains(i))
            result.append(i);

    QList<int> finalOrder;
    finalOrder.append(pinnedFirst);
    finalOrder.append(result);

    QList<QcwContribution> sorted;
    for (int i : finalOrder)
        sorted.append(contributions[i]);
    return sorted;
}
```

---

## 9. Shortcuts

Each command has one command QAction per window:

```cpp
QAction* QcwWindow::commandAction(const QString& commandId) const;
```

- The shortcut comes from `QcwCommandInfo::defaultShortcut`;
- Triggering calls `executeCommand(commandId)`;
- The enabled state comes from `resolveHandler(id)->isEnabled()`;
- Refreshed on handler `stateChanged()` and current view changes;
- Menu items and toolbar buttons reuse the QAction directly to synchronize text / icon / enabled.

UI-less commands also get a QAction added with `addAction()` on the window so their shortcut works.

---

## 10. Naming Conventions

### 10.1 Character Set and Separators

All ids use only:

```text
A-Z a-z 0-9 . / _ -
```

- `/` denotes hierarchy;
- `.` separates words within a segment;
- `-` is used for hyphenated words;
- Spaces, colons, backslashes and control characters are not allowed.

### 10.2 Namespaces

| Source | Namespace | Example |
|---|---|---|
| QCW built-in | No prefix | `File/Save`, `Edit/Copy`, `Window/CommandPalette` |
| Application | Application short name + `/` | `myapp/Project/Open` |
| Plugin | Plugin name + `/` | `myplugin/Project/Open` |

**Rule: built-in ids have no namespace prefix; third-party ids must have one.**

### 10.3 Format of Each Id Type

| Type | Format | Example |
|---|---|---|
| Command id | `<namespace>/<Menu>/<CommandName>` or `<namespace>/<Domain>/<CommandName>` | `File/Save`, `myplugin/Project/Open` |
| Action set id | `<namespace>/<SetName>`, lowercase + dots or slashes | `debug`, `myplugin/project` |
| Contribution id | `<namespace>/<target-path>/<contributionName>`, lowercase + slashes | `file/save`, `myplugin/project/open` |
| Menu target | `<MenuPath>`, PascalCase | `File`, `Window/Perspective` |
| Toolbar target | `ToolBar/<ToolBarName>` | `ToolBar/Main`, `ToolBar/Debug` |

### 10.4 Constant Organization

- **Built-in ids** stay in `QcwWindowActionIds`;
- **Built-in action sets** go in `QcwActionSetIds` (not created unless there are built-in action sets);
- **Application ids** go in the application's own header (e.g. `AppCommands.h`);
- **Plugin ids** go in the plugin's internal header and are **not published**;
- `*Start` / `*End` are removed.

### 10.5 Runtime Validation

- Each registry validates the id character set at the entry point; invalid ids are rejected with a `qWarning`;
- Re-registration of the same id replaces the entry, and warns when the content differs; re-declaring
  an identical contribution is how a language change re-registers the built-ins, so it stays silent;
- The first version does not track origins, only format and duplicate validation.

### 10.6 Documentation

Add a section **Command and contribution id naming** to README / developer guide, covering:

- Character set and separators;
- Namespace rules;
- Format of each id type;
- Constant organization recommendations;
- Documentation covers only the ids of **public owners** (workbench built-in, application's own); plugin-internal ids are not part of QCW documentation.

---

## 11. Runtime Contracts and Cross-Plugin Extension

### 11.1 Principle

**Cross-plugin contracts come only from interfaces; ids are organizational identifiers internal to each owner.**

| Layer | Owner | Cross-plugin visible |
|---|---|---|
| Command id | Registrant | Only referencable within the same owner |
| Contribution id | Registrant | Same as above |
| Action set id | Registrant | Same as above |
| Cross-plugin contract | Plugin-implemented interface | Discovered via `qobject_cast` |

**"Referencable id range" = "same owner"**:

- Built-in workbench commands can reference each other via `before` / `after`;
- The application's own commands can;
- Commands within the same plugin can;
- **No cross-plugin references.**

### 11.2 Plugins Do Not Publish Headers

The only interface a plugin exposes is its binary library and the `QcwPlugin` metadata. **No headers, no contracts files, no `publishedIds()`.**

Why contracts files are removed:

- A contracts header can only guarantee spelling consistency, not that the target exists;
- The existence and semantics of an id are runtime properties, not compile-time properties;
- The distribution cost is the same as distributing the header directly, with no cost reduction;
- It creates the illusion of a contract, hiding runtime failure paths.

### 11.3 Correct Form of Cross-Plugin Extension

Through interfaces, not ids. Following the `AIModelConnectorCreator` pattern from the README:

```cpp
// Plugin A defines the interface (shipped with QCW or an independent interface
// library, not as a plugin header).
class IProjectCommandProvider
{
public:
    virtual ~IProjectCommandProvider() = default;
    virtual QString openCommandId() const = 0;
    virtual QString closeCommandId() const = 0;
};
Q_DECLARE_INTERFACE(IProjectCommandProvider, "com.example.IProjectCommandProvider")

// Application or plugin B consumes it.
for (auto* plugin : pluginManager->getPluginsByCategory("Project"))
{
    if (auto* provider = qobject_cast<IProjectCommandProvider*>(plugin))
    {
        contributionRegistry->registerContribution({
            "app/project/open", {}, "File", provider->openCommandId(),
            {}, {}, "project", true });
    }
}
```

The interface provides the id; the consumer does not guess. Plugin-internal id strings are invisible to consumers.

### 11.4 Role of the Registries

`QcwCommandRegistry` / `QcwContributionRegistry` are **declaration aggregates**, not guessing mechanisms:

- Plugins register their own command metadata and contributions in `doRegisterContributions()`;
- The palette reads `QcwCommandRegistry` and sees what was "declared";
- Contribution sorting reads `QcwContributionRegistry` and sees "declared" positions.

Consumers reading from the registry does **not** mean they know the semantics of plugin-internal ids. Registries are runtime catalogs, not cross-plugin contracts.

### 11.5 Perspective Action Set Visibility

Perspective creators may only write action set ids they know; cross-plugin action set combinations are composed by the application, either by creating the perspective in the application or by calling `setVisibleActionSetIds()` in `postStart()`.

---

## 12. Diagnostics

| Mechanism | Description |
|---|---|
| Contribution references nonexistent command | `placedEntries()` skips the contribution with a `qWarning`; primary diagnostic |
| Contribution references unregistered action set | `placedEntries()` drops it, with a `qWarning` once per action set id — the sorter stays pure and never reads a registry |
| Perspective references unregistered action set | `qWarning` once per id, does not block construction; the id matches nothing, so the same rule hides the contributions that named it |
| Duplicate id registration | replace, with `qWarning` when the new entry says something different — re-declaring an identical contribution is the language-change path and stays silent |
| Invalid id format | Rejected with a `qWarning` |
| Startup id catalog dump | Optional, enabled by `QCW_DUMP_ID_CATALOG=1`; at the end of `QcwWorkbench::start()` it prints the ids the command, action set and contribution registries hold. Ids only — the registries do not track origins, so no owner is claimed |

**Example id catalog dump:**

```text
QCW command catalog (registered):
  Edit/Copy
  File/Save
  myplugin/Project/Open
QCW action set catalog (registered):
  myplugin/project
QCW contribution catalog (registered):
  myplugin/project/open
  file.save
```

Origins are inferred from `QObject::parent()` or the registration `sender`, used only for diagnostics, not discovery.

---

## 13. Comparison with Existing Code

| Existing Mechanism | New Mechanism | Handling |
|---|---|---|
| `QcwView::actionSets()` | Views no longer contribute UI | Remove; use `commandHandlers()` |
| `QcwWindowActionIds` `Action*` constants | Command id constants | Keep |
| `QcwWindowActionIds` `*Start` / `*End` | `QcwContribution` before/after + `@start` | Remove |
| `QcwCommandSpec` + `QcwWindow::registerCommand()` | `QcwCommandInfo` + `QcwContribution` | Retire |
| `QcwGlobalActions` | Global command handlers | Fully retired, no compatibility layer |
| `QcwCommandPalette::buildCommands()` reads `commandRegistry` + `globalActions` | Reads only `commandRegistry` | Remove the globalActions merge logic |
| `QcwWindow::resolveCommand()` returns `QAction*` | `resolveHandler()` + `executeCommand()` | Replace |
| `composeCommandMenu()` / `applyMenuContributions()` | `buildMenus()` + contribution sorting | Remove old implementations |
| `commandPlaceholders` / `commandActions` | Command QAction | Remove |
| `QcwGlobalActions::ActionThemes` | `QcwWindowActionIds::ActionThemes` | Pure programmatic command, no menu contribution |
| `QcwViewCloseCoordinator` etc. | Unchanged | Keep |

### Migration for `QcwGlobalActions` Retirement

1. **Unify id constants in `QcwWindowActionIds`**
   - `ActionNewWindow` → `Window/NewWindow`
   - `ActionRemoveWindow` → `Window/RemoveWindow`
   - `ActionExit` → `File/Exit`
   - `ActionPreferences` → `Edit/Preferences`
   - `ActionCommandPalette` → `Window/CommandPalette`
   - `ActionAbout` → `Help/About`
   - `ActionAboutPlugins` → `Help/AboutPlugins`
   - `ActionThemes` → `Window/Appearance/Themes`

2. **Built-in global commands are registered as global handlers by `QcwWorkbench`**, along with their `QcwCommandInfo` in `QcwCommandRegistry`.

3. **About / About Plugins stay the library's commands and contributions**: the dialogs are still to come,
   and until a handler answers for them the entries are placed and greyed out, which is Section 7.4's
   state for a command nobody handles.

4. **`ActionThemes` is a pure programmatic command**: no menu contribution; it can be executed only via the palette or programmatically. The `themesButton` click calls `executeCommand(ActionThemes)`.

5. **Remove Window parameter handling**:
   - When triggered from a menu item, the window-level handler knows its own `windowId` and does not need parameters;
   - When triggered from the palette or programmatically, use `executeCommand(ActionRemoveWindow, { { "windowId", id } })`.

6. **`ActionShowToolBar` / `ActionShowStatusBar`** become commands + window-level handlers + contributions with target `"Window/Appearance"`, `groupId` `"appearance"`, and `checkable = true`.

---

## 14. Migration Examples

### 14.1 Copy Command

```cpp
// Command declaration
commandRegistry->registerCommand({
    QcwWindowActionIds::ActionCopy,
    "Edit",
    tr("Copy"),
    tr("Copy the selection"),
    QKeySequence::Copy,
    QIcon::fromTheme("edit-copy"),
    false
});

// editor plugin
class EditorView : public QcwView
{
public:
    QHash<QString, QcwCommandHandler*> commandHandlers() const override
    {
        return { { QcwWindowActionIds::ActionCopy, mCopyHandler } };
    }
private:
    QcwCommandHandler* mCopyHandler;
};

// project plugin
class ProjectView : public QcwView
{
public:
    QHash<QString, QcwCommandHandler*> commandHandlers() const override
    {
        return { { QcwWindowActionIds::ActionCopy, mCopyHandler } };
    }
private:
    QcwCommandHandler* mCopyHandler;
};

// Menu contribution registered once
contributionRegistry->registerContribution({
    "edit.copy", {}, "Edit", QcwWindowActionIds::ActionCopy,
    {}, {}, "edit-clipboard", true });
```

### 14.2 Perspective Switches Action Set

```cpp
actionSetRegistry->registerActionSet({ "debug", tr("Debug") });

perspective->setVisibleActionSetIds({ "debug", "project" });

contributionRegistry->registerContribution({
    "debug.stepOver", "debug", "ToolBar/Debug", "debug.stepOver",
    {}, {}, "debug-controls", true });
```

### 14.3 UI-less Command with Parameters

```cpp
class RemoveWindowHandler : public QcwCommandHandler
{
public:
    explicit RemoveWindowHandler(QcwWorkbench* workbench)
        : mWorkbench(workbench) {}

    void execute(const Params& params) override
    {
        mWorkbench->removeWindow(params.value("windowId").toString());
    }

private:
    QcwWorkbench* mWorkbench;
};

window->executeCommand(
    QcwWindowActionIds::ActionRemoveWindow,
    { { "windowId", window->id() } });
```

### 14.4 Checkable Command

```cpp
class ShowStatusBarHandler : public QcwCommandHandler
{
public:
    explicit ShowStatusBarHandler(QcwWindow* window) : mWindow(window) {}

    bool isChecked() const override
    {
        return mWindow->statusBar()->isVisible();
    }

    void setChecked(bool checked) override
    {
        mWindow->statusBar()->setVisible(checked);
    }

    void execute(const Params&) override {}

private:
    QcwWindow* mWindow;
};

commandRegistry->registerCommand({
    QcwWindowActionIds::ActionShowStatusBar,
    "Window",
    tr("Show Status Bar"),
    tr("Show status bar"),
    QKeySequence(),
    {},
    true  // checkable
});

contributionRegistry->registerContribution({
    "window.appearance.showStatusBar", {}, "Window/Appearance",
    QcwWindowActionIds::ActionShowStatusBar,
    {}, "window.appearance.showToolBar", "appearance", true });
```

---

## 15. Implementation Phases

### 15.1 PR Breakdown

**PR 1: Add foundation types without touching existing code**

- Add `QcwCommandHandler.h/.cpp`;
- Add `QcwActionSet.h`, `QcwActionSetRegistry.h/.cpp`;
- Add `QcwContribution.h`, `QcwContributionRegistry.h/.cpp`;
- Add `icon` and `checkable` to `QcwCommandInfo`; a command carries no scope or action set — the handler is found by resolution order and the contribution owns the action set;
- Add unit tests: contribution sorting, cycle detection, action set filtering;
- Existing `actionSets()` and `QcwGlobalActions` remain unchanged.

**PR 2: Wire handlers and command QActions into the window**

- `QcwWindow` gains `registerHandler()` / `unregisterHandler()` / `resolveHandler()` / `executeCommand()` / `commandAction()`;
- `QcwView` gains `commandHandlers()`;
- `QcwWindow` creates command QActions and wires shortcuts;
- `resolveCommand()` prefers handlers, falls back to the existing `QAction` lookup for compatibility;
- Existing `actionSets()` still works.

**PR 3: Switch menu construction to contributions and add action sets**

- `QcwWindow::buildMenus()` uses `QcwContributionRegistry` sorting results;
- Built-in File/Edit commands become contribution registrations;
- Standard menu slots, `commandSpecs`, `commandPlaceholders`, `commandActions` are removed;
- `QcwPerspective` gains `visibleActionSetIds()`;
- Non-standard menu `*Start` / `*End` anchors are removed and replaced by contributions;
- `QcwView::actionSets()`, `QcwCommandSpec`, `*Start/*End`, and `QcwGlobalActions` are deleted directly (no transitional shim).

**PR 4: Clean up palette, documentation, Doxygen, README**

- `QcwCommandPalette` reads only `QcwCommandRegistry`;
- Update `workbench_demo` and the qcw-docking examples (`folder_container_demo`,
  `folder_renderer_demo`);
- Update README and Doxygen.

### 15.2 stateChanged() Refresh Granularity

**(a) Current view's handlers**

- On `currentViewChanged`, disconnect all handlers of the old view and connect those of the new view;
- Only the current view's handlers are connected; hidden views' handlers are not.

**(b) Perspective handlers**

- All registered perspective handlers are connected to `stateChanged()`;
- On arrival, check whether that perspective is the current perspective; refresh if so, otherwise ignore.

**(c) Window / global handlers**

- Connected once at registration, disconnected on unregistration;
- Refresh on every arrival;
- Registering, unregistering or destroying a handler also refreshes that command immediately, so a
  command whose scope lost its handler greys out instead of waiting for the next `stateChanged()`.

**Refresh content:**

```text
action = commandAction(commandId)
if (!action) return
handler = resolveHandler(commandId)
action->setEnabled(handler && handler->isEnabled())
action->setVisible(handler == nullptr || handler->isVisible())
action->setChecked(handler && handler->isChecked())
```

Presence is not refreshed here: which entries a menu or tool bar holds comes from the sorted
contributions, and a handler appearing or disappearing does not change that list.

---

## 16. Design Decision Summary

| # | Decision |
|---|---|
| 1 | `QcwCommandHandler::execute()` returns `void` |
| 2 | Keep `accepts(params)`, used only for parameter validation, not competition |
| 3 | `checkable` goes in `QcwCommandInfo`; `isChecked` / `setChecked` go in the handler |
| 4 | Handler registration semantics: replace |
| 5 | Handlers need `Q_OBJECT` and a QObject parent |
| 6 | `QcwCommandInfo` gains `QIcon icon` |
| 7 | Command QAction owned by the window, per-window; created on first use, and eagerly for commands with a default shortcut — a UI-less command must answer its shortcut without anyone asking for its action |
| 8 | UI-less commands still get a QAction added to the window for shortcuts |
| 9 | When a command is unregistered the window deletes its QAction; contributions still referencing it emit `qWarning` and are skipped |
| 10 | Menus and toolbars reuse the command QAction directly; no per-position text |
| 11 | Unconstrained contributions are appended to the end |
| 12 | Contributions on a cycle are appended to the end in registration order with `qWarning`; not dropped |
| 13 | `before` / `after` first by contribution id, then by command id |
| 14 | `@start` anchor is introduced; `@end` is not used |
| 15 | Separators: inserted when groupId changes; Qt merges consecutive separators |
| 16 | A new separator QAction is created on each menu build and owned by the menu |
| 17 | Perspective references an unregistered action set: `qWarning`, does not block construction |
| 18 | Contribution references an unregistered action set: invisible + `qWarning` |
| 19 | Command QAction `visible = no handler, or a handler that reports itself visible`; which entries exist at all is left to the contributions, so an unhandled command greys out instead of vanishing |
| 20 | Multiple contributions for the same command id are judged independently; the command is placed where at least one of them survives filtering |
| 21 | Switching to the perspective that is already open does nothing; switching to another one always rebuilds, since comparing the two action-set lists would miss a view or handler change in between |
| 22 | Toolbar targets use the `"ToolBar/"` prefix; created on demand, owned by the window; a bar with nothing placed in it hides itself |
| 23 | Only current view handlers are connected; perspective handlers are all connected but checked against the current perspective; window/global handlers always refresh |
| 24 | Four PRs; no transitional shim in PR 3 |
| 25 | Id character set and separator rules; namespace rules; format of each id type |
| 26 | Built-in ids go in `QcwWindowActionIds`; plugin ids go in plugin-internal headers and are not published |
| 27 | Runtime format validation, plus a `qWarning` when a duplicate id replaces an entry with different content; first version does not track origins |
| 28 | Documentation covers public-owner ids; plugin-internal ids are not in QCW documentation |
| 29 | Contracts file concept removed; pure runtime contracts |
| 30 | Plugins do not publish public headers; cross-plugin extension only through interfaces |
| 31 | `QcwPublishedIds` and `QcwPlugin::publishedIds()` are deleted |
| 32 | Referencable id range = same owner; cross-plugin positional dependencies use interfaces |
| 33 | `QcwCommandRegistry` / `QcwContributionRegistry` are declaration aggregates |
| 34 | Perspective's `visibleActionSetIds()` only contains ids it knows |
| 35 | Startup id catalog dump dumps only registered content |
| 36 | `QcwGlobalActions` is fully retired; no compatibility layer |
| 37 | `ActionThemes` is a pure programmatic command; no menu contribution |
| 38 | `ActionShowToolBar` / `ActionShowStatusBar` become checkable commands + contributions |
