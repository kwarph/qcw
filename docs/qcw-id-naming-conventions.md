# QCW Identifier Naming Conventions

- Status: Adopted — normative for every new id; the one-time migration in §10 has been applied
- Adopted: 2026-11-18
- Scope: every stable identifier published by QCW, applications built on QCW, plugins, examples,
  and every id that is persisted in session or project state
- Authority: normative for new ids and the reference for migrating existing ones. This document
  supersedes the naming rules in
  [`qcw-command-menu-design.md`](qcw-command-menu-design.md) §10.
- Companion documents: [`qcw-command-menu-design.md`](qcw-command-menu-design.md),
  [`qcw-topic-service-design.md`](qcw-topic-service-design.md),
  [`qcw-pimpl-conventions.md`](qcw-pimpl-conventions.md)

---

## 1. Why the convention has two shapes

Ids cross component boundaries and some of them are persisted. A single scheme that mixes
qualification and hierarchy makes it impossible to tell from a string whether it names a type, a
menu, a command or a group. The convention therefore uses exactly two **separator shapes**, plus
single-word local names that carry neither separator:

| Shape | Means | Example |
|---|---|---|
| `<owner>.<TypeName>` | **Qualified name**: a type or interface, scoped to its publisher | `demo.ProjectView` |
| `<path>/<path>/...` | **Path**: a place or an operation inside a window | `File/save`, `demo/File/newProject` |
| `<word>` | **Local name**: a category, named folder or payload key | `Tool`, `Central`, `windowId` |

The rule in one sentence:

> **A dot qualifies a name; a slash walks a hierarchy. Never mix the two.**

As a consequence, a string can be classified by its punctuation alone:

- starts with a reverse-DNS domain (`org.`, `com.`, ...) → interface IID;
- exactly one `.` and no `/` → concrete type;
- contains `/` → path id (menu, toolbar, command, contribution, action set, topic);
- no separator → single-word key (category, named folder, payload key).

The rest of this document defines each kind precisely.

---

## 2. Vocabulary

| Term | Meaning |
|---|---|
| **Owner** | The short name of the publishing component: an application (`demo`) or a plugin (`acme`). Lowercase, one segment. |
| **Built-in id** | An id owned by QCW itself. Built-in ids have no owner prefix. |
| **Type id** | The string returned by `QcwPlugin::type()`: the identity a session or project file uses to find a plugin again. |
| **Interface IID** | The string declared by `Q_DECLARE_INTERFACE`: the identity of a C++ interface across every project. |
| **Path id** | An id whose segments are separated by `/`: menus, toolbars, commands, contributions, action sets, topics. |
| **Registry** | `QcwCommandRegistry`, `QcwContributionRegistry`, `QcwActionSetRegistry`, `QcwTopicRegistry`, `QcwPluginManager`. |
| **Persisted id** | An id written into a session file, a perspective file or an application's project file. |
| **Label** | User-visible text (menu title, command label, action set label). A label is not an id. |

---

## 3. Universal rules

### 3.1 Character set

New ids use letters and digits only, plus the separators defined below. QCW's runtime check
(`qcw::isValidId()` in [`QcwIdUtils.cpp`](../src/workbench/QcwIdUtils.cpp)) accepts the wider
legacy set `A-Z a-z 0-9 . / _ -` so that third-party ids are not rejected, but **QCW and its
projects do not introduce `-` or `_`**. Acronyms and compound words are joined by camel case
instead.

### 3.2 Separators

| Separator | Meaning | Allowed in |
|---|---|---|
| `.` | qualifies a name with its owner | concrete type ids, interface IIDs |
| `/` | walks a hierarchy | menu targets, toolbar targets, commands, contributions, action sets, topics |
| `@` | reserved anchor keyword (`@start`) | a contribution's `before` value only, never in an id |

- A dot is **never** used inside a path id.
- A slash is **never** used inside a concrete type id.
- No leading or trailing `/`, no empty segments, no spaces, no control characters.

### 3.3 Case

| Position | Case | Examples |
|---|---|---|
| Owner segment | lowercase | `demo`, `acme` |
| Path segment naming a place, category, type or set | UpperCamelCase | `File`, `Window/Appearance`, `ProjectView`, `Build` |
| Leaf naming an action, operation or data key | lowerCamelCase | `save`, `newWindow`, `buildTarget`, `windowId` |
| Interface name | UpperCamelCase | `QcwViewCreator`, `IProjectCommandProvider` |

Acronyms keep the spelling of the actual type name: `CMakeProject`, not `CmakeProject`. The type
id mirrors the type, not a re-spelling of it.

### 3.4 Ownership

- **Built-in QCW ids have no owner.** They are the framework's own: `File/save`,
  `Window/newWindow`, `ToolBar/Main`, `View`, `Perspective`.
- **Application and plugin ids carry their owner.** The owner is the short name of the publisher,
  never a reverse-DNS prefix: `demo/File/newProject`, `demo.ProjectView`. A plugin that is not
  shipped by the application uses its own short name: `acme.SomeView`, `acme/Project/open`.
- **A reverse-DNS prefix (`org.`, `com.`, ...) is reserved for interface IIDs.** A concrete type
  id must never start with one; it uses the short owner form instead.
- **Places and categories are not owner-prefixed.** Menu targets, toolbar targets and plugin
  categories are window-global or grouping names; an owner prefix there would fragment the very
  grouping they describe. The owner belongs to commands, contributions, action sets and types.

### 3.5 What is not an id

The following are **not** governed by this convention as ids, although they may look similar:

- user-visible labels (`C/C++`, `C++ CMake`, `Debug`);
- `QObject::objectName` values;
- `QSettings` group and key paths (`appearance/textEditor`, `executable`);
- contribution group tokens (`QcwContribution::groupId`), which only compare for equality inside
  one target to decide where a separator falls;
- file-system paths and file names;
- `Q_DECLARE_INTERFACE` **version suffixes** (`/1.0`), which belong to the IID, not to a type id.

Never put a label into an id field and never derive a label from an id.

---

## 4. Identifier kinds

### 4.1 Interface creator type (Qt IID)

**Format**

```text
<reverse-dns-domain>.<InterfaceName>[/<major>.<minor>]
```

- `<reverse-dns-domain>` is the domain the publisher owns, as at least two lowercase segments:
  `org.qcw`, `com.example`, `org.acme`.
- `<InterfaceName>` is the C++ interface class name in UpperCamelCase.
- The optional version suffix belongs to the IID and is not a separate id.

**Examples**

```text
org.qcw.QcwViewCreator
org.qcw.QcwPerspectiveCreator
com.example.IProjectCommandProvider
org.acme.SomeInterface/1.0
```

**Notes**

- This is the only place where the full reverse-DNS form is used. Concrete types use the short
  owner form from §4.2 — the scope is the application, not the internet.
- The IID therefore contains at least two dots; a concrete type id contains exactly one. That is
  also how the two are told apart without looking at the registry.
- QCW uses the version suffix when an interface is expected to evolve; be consistent per interface.

### 4.2 Concrete creator/type id

The string returned by `QcwPlugin::type()` for view creators, window creators, perspective
creators, project creators, build-system adapters and plain plugins.

**Format**

```text
<owner>.<TypeName>
```

- Exactly one dot: the owner, then the type name.
- `<owner>` is the publisher's short name (§3.4).
- `<TypeName>` is the name of the actual product type, in UpperCamelCase, mirroring the class
  name including any prefix the project puts on its types. Do not strip or re-spell that prefix:
  `FooProjectView` produces `demo.FooProjectView`, and `CMakeView` stays `demo.CMakeView`.
- When the product type has no separate class (project creators, build-system adapters), the type
  name is the class name with the `Creator` suffix removed, or the class name itself when it has
  none: `ProjectCreator` → `demo.Project`, `MakeBuildSystem` → `acme.MakeBuildSystem`.

**Examples — workbench_demo**

```text
demo.ProjectView
demo.TextEditor
demo.NotesView
demo.LogView
demo.OutlineView
demo.ProjectPerspective
demo.FocusPerspective
demo.Notes
```

**Examples — third-party plugin**

```text
acme.SomeView
acme.SomePerspective
acme.CMakeBuildSystem
```

**Notes**

- The id names the product, not the creator class: `demo.ProjectView`, not
  `demo.ProjectViewCreator`.
- The owner makes type ids globally unique among loaded plugins. Two plugins must never register
  the same type id.
- **Persistence:** view and perspective types are written into window and perspective state; an
  application may additionally persist project types in its project files. Treat these ids as
  frozen once a release ships them (see §6).

### 4.3 Menu target

**Format**

```text
<MenuPath>
```

`<MenuPath>` is one or more UpperCamelCase segments separated by `/`.

**Examples**

```text
File
Edit
Window
Window/Appearance
Window/Perspective
Window/NewView
File/New
```

**Notes**

- Menu targets are places, not operations: the leaf is UpperCamelCase (`File/New`).
- Built-in QCW menus have no owner. An application extends the window by registering its own menu
  in `registeredMenus()` and using that target; it still does not owner-prefix the target.
- A target is **not** persisted; only contributions and commands reference it at run time.
- `QcwCommandInfo::category` holds a menu target id, not a command id.

### 4.4 Tool bar target

**Format**

```text
ToolBar/<Name>
```

- The `ToolBar/` prefix is significant: `QcwWindow` builds and fills a tool bar only for a target
  with this prefix (`qcw::kToolBarTargetPrefix`).
- `<Name>` is UpperCamelCase. Do not put an owner before `ToolBar/`; that makes the string a menu
  target and the tool bar never appears.

**Examples**

```text
ToolBar/Main
ToolBar/Edit
ToolBar/Debug
```

**Notes**

- A tool bar target is not persisted.
- A tool bar is filled from the same contributions as a menu; the target path in the contribution
  id repeats it (§4.6).

### 4.5 Command

**Format**

```text
[<owner>/]<MenuPath>/<lowerCamelAction>
```

- `<owner>` is present for application and plugin commands and absent for QCW built-ins.
- `<MenuPath>` is one or more UpperCamelCase segments. It is normally the menu the command is
  catalogued under. A functional domain segment is acceptable for palette-only commands, but it is
  still UpperCamelCase.
- `<lowerCamelAction>` names what the command does: a verb phrase, not a copy of the menu title.

**Examples**

```text
File/save
File/saveAs
File/exit
Edit/copy
View/refresh
Window/newWindow
Window/Appearance/showToolBar
Window/Appearance/showStatusBar
Window/Perspective/openPerspective
Help/about
demo/File/newProject
demo/File/openProject
demo/Project/closeAll
```

**Notes**

- The command's `QcwCommandInfo::category` is a menu target (§4.3), not part of the command id.
- Commands are **not** persisted; the command registry is rebuilt at start-up. Shortcuts are
  defaults, not per-id settings.
- A command that is not placed in a menu still needs an id; use the functional-domain form and a
  lowerCamelCase action, e.g. `demo/Project/closeAll`.

### 4.6 Contribution

**Format**

```text
[<owner>/]<TargetPath>/<lowerCamelAction>
```

- `<TargetPath>` repeats `QcwContribution::target` verbatim, minus any owner (targets never carry
  an owner).
- `<lowerCamelAction>` is the action leaf of the command being placed.
- The owner is present for application and plugin contributions and absent for built-ins.

**Examples**

```text
File/save                              // target "File"
Edit/copy                              // target "Edit"
ToolBar/Main/save                      // target "ToolBar/Main"
Window/Appearance/showStatusBar        // target "Window/Appearance"
demo/File/New/newProject               // target "File/New"
demo/ToolBar/Main/newProject           // target "ToolBar/Main"
demo/View/filter                       // target "View"
```

**Notes**

- **A contribution id may equal its command id.** The two live in different registries, and
  `QcwContributionSorter` resolves contribution ids first, then command ids, within one target.
  Built-in contributions therefore equal their built-in commands (`File/save`).
- When one command is placed in several targets, the target path keeps the ids distinct:
  `File/save` (menu) and `ToolBar/Main/save` (tool bar).
- `before` and `after` may name a contribution id or a command id in the same target. The reserved
  `@start` value pins an entry to the front of its target; `@start` is a keyword, not an id.
- Contributions are not persisted.

### 4.7 Action set

**Format**

```text
[<owner>/]<UpperCamelSetName>
```

- An action set is a **named group** of contributions whose visibility a perspective switches. Like
  a menu, it groups a set of entries, and it is a noun rather than an action, so its name is
  UpperCamelCase.
- Built-in action sets have no owner; application and plugin sets carry theirs.

**Examples**

```text
Debug
demo/Debug
```

**Notes**

- **Persistence:** the ids a perspective lists in `QcwPerspective::visibleActionSetIds()` are
  written into the perspective state file (`visibleActionSets`). Once a release uses an action set,
  its id is frozen.
- An action set id may be the same string as a menu target (`Build`); the registries are separate,
  and the context tells them apart.
- There are no built-in action sets today.

### 4.8 Category

`QcwPlugin::category()` groups plugins for lists and menus.

**Format**

```text
<UpperCamelWord>
```

- One word, UpperCamelCase, no owner, no separator.
- Categories are open grouping lists, not globally unique names.

**Examples**

```text
View                        // QCW view creators
Perspective                 // QCW perspective creators
Tool                        // workbench_demo notes plugin
```

**Notes**

- Do not carry a plugin or class prefix into a category: `Tool`, not `NotesPluginCategory`.
- A category is not an id that is persisted or looked up by plugin type.
- A project-creation dialog may group creators by a display label (`C++ CMake`); that label is
  **label text**, not a category id.

### 4.9 Named folder

**Format**

```text
<UpperCamelWord>
```

- In-memory key of a perspective's folder, for example an entry in the named-folder table a
  perspective fills in `createInitialLayout()`.
- Not persisted; the folder layout is persisted, not these keys.
- Keys must be distinct; one key per folder.

**Examples**

```text
Central
Left
Right
Bottom
```

### 4.10 Payload key

**Format**

```text
<lowerCamelWord>
```

- Keys inside a `QVariantMap` payload (topics, command parameters).
- A key is a data field, not an id, but it follows the same lowerCamelCase rule.

**Examples**

```text
windowId
path
revision
```

### 4.11 View-local action name

**Format**

```text
<TypeName>/<lowerCamelAction>
```

- A per-view key an application gives one of its own actions: `QcwView::namedActions()` is the
  map a view registers them in, so one contribution can find an anchor another one placed.
- `<TypeName>` is the class that declares the action name: the view class itself, or a reusable
  view base that defines the action set.
- These names are not registry ids and never leave the view's own API; they do not need an owner.

**Examples**

```text
ProjectView/refresh
ProjectView/openProjects
ProjectView/expandAll
ProjectView/showFilterBar
```

### 4.12 Topic

Topic ids follow the topic service design, not the type/path split above:

**Format**

```text
app/[<lowercaseNamespace>/...]<lowerCamelName>
win/[<lowercaseNamespace>/...]<lowerCamelName>
```

- The scope keyword (`app`, `win`) is lowercase and fixed.
- Namespace segments after the scope keyword are lowercase; the leaf is lowerCamelCase.
- Topics are registered in `QcwTopicRegistry` when tooling should discover them.
- Payload keys follow §4.10.

**Examples**

```text
app/language
win/selection
app/notes/changed
```

### 4.13 Build-system adapter id

**Format**

```text
<lowercaseToolName>
```

- The id returned by a build-system adapter's id accessor selects that adapter; it is a tool name,
  not a plugin or creator type.
- Use the tool's established lowercase spelling (`cmake`, `meson`, `make`); do not UpperCamelCase a
  brand name and do not owner-prefix it.
- The id is not persisted. The adapter plugin's own type follows §4.2
  (`acme.CMakeBuildSystem`).

### 4.14 Free-form data keys

Some contracts need a producer key that is not a registry id, for example a problems `ownerId`.
Those keep their own grammar:

- use `/` for hierarchy, never `.`;
- define the grammar in the contract header;
- values interpolated into the key (project names, document paths) may contain arbitrary characters,
  so the separator must be a character those values cannot contain.

**Example — a problems producer key**

```text
build/<project>/<buildId>
```

---

## 5. Telling the kinds apart

| Shape | Kind | Example |
|---|---|---|
| `<reverse-dns-domain>.<Interface>[/<version>]` | interface IID | `org.qcw.QcwViewCreator` |
| `<owner>.<TypeName>` (exactly one dot) | concrete type | `demo.ProjectView` |
| `ToolBar/<Name>` | tool bar target | `ToolBar/Main` |
| `[owner/]<UpperCamelPath>` | menu target or action set | `File/New`, `demo/Debug` |
| `[owner/]<path>/<lowerCamelLeaf>` | command or contribution | `demo/File/newProject` |
| `app/...` or `win/...` (lowercase segments, lowerCamel leaf) | topic | `app/language` |
| one UpperCamel word | category or named folder | `Tool`, `Central` |
| one lowerCamel word | payload key, build-system id or owner | `windowId`, `cmake`, `demo` |
| anything else | free-form data key or invalid | `build/project/42` |

A menu target and an action set can share a shape (and even a string); they are distinguished by
the registry that holds them. A command and a contribution can share a string; the contribution
registry resolves first inside a target.

---

## 6. Persistence and compatibility

### 6.1 Persisted ids

| Id | Where it is written |
|---|---|
| Concrete view type | `QcwWindow-*.xml`, one `type` attribute per view |
| Concrete perspective type | `QcwWindow-*.xml` and saved perspective files |
| Action set id | `QcwWindow-*.xml` and saved perspective files, in `visibleActionSets` |
| Application-persisted type | the application's own project state, for example the project type stored in its project files |

QCW offers no alias or remap hook when it restores a saved type: it looks the type up in
`QcwPluginManager` and drops the view/perspective when the creator is gone.

### 6.2 Runtime-only ids

Commands, contributions, menu targets, tool bar targets, categories, payload keys, named folders
and view-local action names are rebuilt at run time and are not persisted. Renaming them is safe
when every reference is a compile-time constant.

### 6.3 Rename policy

- **Before the first release that can persist an id:** rename freely, with a coordinated change to
  every reference and to the saved-state format.
- **After release:** a persisted id is frozen. A rename requires a compatibility map at the layer
  that owns the persistence (the application for project files; QCW would need a new hook for
  session files), or an explicit reset of the saved state.
- Application-persisted types are frozen by the application, not by QCW; the application decides
  whether to migrate them.

### 6.4 Labels

Labels may change at any time and are never stored as ids. When an id has to change because a
label changed, the id and the label are being conflated.

---

## 7. Validation and tests

### 7.1 Runtime

`qcw::isValidId()` validates the **character set** only. The registries call it and reject a
malformed id with a `qWarning`. Runtime validation deliberately stays lenient: it must not reject
a third-party id that follows the character set but not every structural rule below.

### 7.2 Structural checks in tests

The structure is enforced by tests, not by the registries. The recommended patterns for ids defined
by QCW and its projects:

| Kind | Pattern | Example |
|---|---|---|
| Owner | `^[a-z][a-z0-9]*$` | `demo` |
| Concrete type | `^[a-z][a-z0-9]*\.[A-Z][A-Za-z0-9]*$` | `demo.ProjectView` |
| Interface IID | `^([a-z][a-z0-9]*\.){2,}[A-Z][A-Za-z0-9]*(/[0-9]+\.[0-9]+)?$` | `org.qcw.QcwViewCreator` |
| Menu target | `^[A-Z][A-Za-z0-9]*(/[A-Z][A-Za-z0-9]*)*$` | `Window/Perspective` |
| Tool bar target | `^ToolBar/[A-Z][A-Za-z0-9]*$` | `ToolBar/Main` |
| Command | `^([a-z][a-z0-9]*/)?([A-Z][A-Za-z0-9]*/)+[a-z][A-Za-z0-9]*$` | `demo/File/newProject` |
| Contribution | same as command | `demo/File/New/newProject` |
| Action set | `^([a-z][a-z0-9]*/)?[A-Z][A-Za-z0-9]*$` | `demo/Debug` |
| Category | `^[A-Z][A-Za-z0-9]*$` | `Tool` |
| Named folder | `^[A-Z][A-Za-z0-9]*$` | `Central` |
| Payload key | `^[a-z][A-Za-z0-9]*$` | `windowId` |
| View-local action | `^[A-Z][A-Za-z0-9]*/[a-z][A-Za-z0-9]*$` | `ProjectView/refresh` |

Two additional checks matter as much as the patterns:

- **Uniqueness:** type ids across loaded plugins; command, contribution and action-set ids within
  their registry.
- **Agreement:** every creator's `type()` equals the constant that the world looks it up by, and
  every registered command/contribution id has a matching constant in its owner's header.

### 7.3 Negative cases worth testing

- `demo/File/newProject` must not pass a concrete-type check.
- `demo.ProjectView` must not pass a command check.
- `demo/file/newProject` (lowercase menu segment) and `demo/File/NewProject` (uppercase action)
  must be rejected.
- `org.demo.ProjectView` must be rejected as a concrete type.
- `demo.Debug` must be rejected as an action set.
- `ToolBar/main` must be rejected.
- `C/C++` must be rejected as a category or id.
- `demo.file/newProject` (dot inside a path) must be rejected.

---

## 8. Checklist for a new id

1. Decide the kind: interface, concrete type, menu target, tool bar target, command, contribution,
   action set, category, payload key.
2. Apply the shape and case for that kind; use the owner only where §3.4 allows it.
3. Put the constant next to the component that owns it: built-ins in `QcwWindowActionIds`
   (and `QcwActionSetIds` when built-in action sets appear); application ids in the application's
   header; plugin ids inside the plugin.
4. If the id is a type, make `type()` return exactly that constant.
5. If the id is persisted (§6.1), check the rename policy before changing it.
6. Add the id to the owner's naming test: character set, shape, uniqueness and agreement.

---

## 9. Worked examples

### 9.1 QCW built-ins

Menus, tool bar targets and payload keys do not change under this convention; command leaves become
lowerCamelCase and contribution ids become target-path paths.

| Current | Convention |
|---|---|
| `File/Save` | `File/save` |
| `File/SaveAs` | `File/saveAs` |
| `Edit/Copy` | `Edit/copy` |
| `Window/NewWindow` | `Window/newWindow` |
| `Window/Appearance/ShowToolBar` | `Window/Appearance/showToolBar` |
| `Window/Perspective/OpenPerspective` | `Window/Perspective/openPerspective` |
| `Help/About` | `Help/about` |
| `file.save` (contribution) | `File/save` |
| `window.appearance.showStatusBar` (contribution) | `Window/Appearance/showStatusBar` |
| `File`, `Window/Perspective`, `ToolBar/Main` (targets) | unchanged |
| `windowId` (payload key) | unchanged |

### 9.2 workbench_demo

| Kind | Current | Convention |
|---|---|---|
| View type | `ProjectView` | `demo.ProjectView` |
| View type | `TextEditor` | `demo.TextEditor` |
| Command | `File/NewProject` | `demo/File/newProject` |
| Contribution | `demo/file/newProject` | `demo/File/newProject` |
| Contribution | `demo/toolbar/main/newProject` | `demo/ToolBar/Main/newProject` |
| Tool bar target | `demo/ToolBar/Edit` | `ToolBar/Edit` |
| Menu target | `View` | unchanged |

The tool bar target is a real defect, not only a naming issue: `demo/ToolBar/Edit` does not start
with `ToolBar/`, so `QcwWindow` treats it as a menu target and never fills the Edit tool bar.

---

## 10. Migration at adoption (2026-11, one-time)

This document is normative from its adoption date. The code base was migrated when the document was
adopted; the items below record what changed.

- 30 built-in command ids change their action leaf to lowerCamelCase.
- 28 built-in contribution ids change to target-path paths; `before`/`after` references follow.
- `workbench_demo` type, command and contribution ids gain the `demo` owner; the Edit tool bar
  target becomes `ToolBar/Edit`.
- Tests that spell ids out and the documentation examples are updated with the code.

Persistence impact: QCW stores no command, contribution or target id and has no built-in action
set, so nothing has to be migrated. Saved `workbench_demo` sessions reference the old demo type
ids; reset the demo's session state.

---

## 11. References

- Runtime character-set check: [`QcwIdUtils.cpp`](../src/workbench/QcwIdUtils.cpp)
- Built-in ids: [`QcwWindowActionIds.h`](../src/workbench/QcwWindowActionIds.h)
- Built-in commands and contributions: [`QcwBuiltinCommands.cpp`](../src/workbench/QcwBuiltinCommands.cpp)
- Id fields and their contracts: [`QcwCommand.h`](../src/workbench/QcwCommand.h),
  [`QcwContribution.h`](../src/workbench/QcwContribution.h),
  [`QcwActionSet.h`](../src/workbench/QcwActionSet.h)
- Contribution resolution order: [`QcwContributionSorter.cpp`](../src/workbench/QcwContributionSorter.cpp)
- Plugin type contract: [`QcwPluginTemplate.h`](../src/core/QcwPluginTemplate.h) and
  [`QcwViewCreator.h`](../src/workbench/QcwViewCreator.h)
- Persisted view types: [`QcwWindow.cpp`](../src/workbench/QcwWindow.cpp)
- Persisted perspective types and action sets: [`QcwPerspective.cpp`](../src/workbench/QcwPerspective.cpp)
- Topic ids: [`qcw-topic-service-design.md`](qcw-topic-service-design.md)
