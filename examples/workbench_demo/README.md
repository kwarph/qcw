# workbench_demo

The library used as an **application framework**: a `QcwWorkbench` subclass starts a `QcwWindow`
that restores the previous session, and everything the user sees — the project tree, the text
editor, the outline, the log, the two perspectives — arrives from a **plugin**.

This is the demo to read once the folder and renderer mechanics make sense: it is the only one that
shows how the pieces are supposed to be assembled, and how an application stays out of the way of
its own views.

## Components demonstrated

| Component | What the demo uses it for |
|---|---|
| `QcwWorkbench` | `Workbench` overrides `createWindow()` (which window class, what size) and `preStart()` (which plugin directories to scan). `initialize()` sets the application/organisation identity that `QcwSettings` and `QcwAppDirectories` then resolve; `start()` restores the session |
| `QcwWindow` | `MainWindow` overrides `postSetupUi()` (placeholder, first perspective) and `createSharedFolder()` (one editor folder shared by every perspective). It uses `sharedFolder()`, `openPerspective()`, `getMenu()`, `registeredMenus()`, `commandAction()` |
| `QcwWindowActionIds` | The ids of the menus the window builds itself — `MenuFile`, `MenuEdit`, `MenuWindow`, and the `Window > Appearance`, `Window > New View` and `Window > Perspective` submenus that come with it — which double as the contribution targets the demo aims its own entries at, and the ids of the built-in commands the demo's views handle |
| Commands and contributions | `MainWindow::registerFileCommands()` and `registerViewCommands()` put each demo command in `commandRegistry` and place it with a `QcwContribution` — the project commands into the window's `File` menu, the View ones into the `View` menu the demo owns. `populateToolBar()` then reuses the window's command `QAction`s, so the tool bars and the menus show the same state |
| `QcwPerspective` | `ProjectPerspective` and `FocusPerspective` each build a folder layout in `createInitialLayout()`; switching perspectives keeps the shared editor folder and swaps everything around it |
| `QcwView` | `ProjectView`, `TextEditor` and `LogView` answer commands through `commandHandlers()`; the views implement `statusBarWidget()`, `topRightToolBar()`, `saveExtraState()`/`restoreExtraState()`. `ProjectView` and `TextEditor` publish their current subject and the editor announcements through `owningWindow()->topicService()`, while `ProjectView` and `OutlineView` consume window topics through `subscribeWindowTopic()` and hold no window pointer at all |
| `QcwViewCreator`, `QcwPerspectiveCreator` | One plugin each: `doCreate()` builds the view or perspective, `defaultWindowTitle()`/`defaultViewIcon()` say what its tab looks like |
| `QcwPlugin` | `QCW_PLUGIN_INTERFACE` and `QCW_PLUGIN_IMPLEMENT(Creator, "Type")` write the seven metadata accessors from a `PluginInfo.h`; `Q_PLUGIN_METADATA(IID …)` plus `Q_INTERFACES()` are what make `qobject_cast` to an interface work. The notes plugin subclasses it **without** a creator — see below |
| `QcwPluginLoader`, `QcwPluginManager` | `preStart()` calls `loadPlugins(directories)`; consumers look a creator up by its `type()` string through `getPluginT<QcwViewCreator>()` |
| `QcwCommandRegistry`, `QcwContributionRegistry`, `QcwCommandPalette` | The notes plugin publishes `Notes/AddNote` to the catalog under its own `"Notes"` category and registers a window-scope handler on each window as it appears, with no contribution at all; `Ctrl+Shift+P` lists it beside the window's File and Edit commands |
| `QcwTabFolderContainer`, `QcwTabFolder` | `addSharedFolder()`, `createFolder()`, `setCentralFolder()`, `setEmptyFolderAction(…, Keep)`, `parentSplitter()`, `addTab()` — the same container as `folder_container_demo`, now driven by a perspective instead of a window |
| `QcwSettings` | Read-only legacy key migration for the remembered project roots, plus the view filters |
| `QcwServiceRegistry` | The process-wide service map, keyed by the interface id: `demo::IEditorService` (a plain `Q_DECLARE_INTERFACE` on `QObject`) is registered by the text editor plugin and looked up by the project view and the outline, so none of the three plugins links to another |
| `QcwTopicService` | Publish-subscribe with a cached last payload. Window-scoped: the editor publishes `win/selection/editor` (path, cursor position, anchor, revision, selected text) and the `win/editor/opened`, `closed`, `saved` and `pathChanged` announcements; the project tree publishes `win/selection/project` and follows the four announcements through
`subscribeWindowTopic()`, and the outline follows the editor selection topic the same way. Application-scoped: the notes store publishes `app/notes/changed` (note count) through `QcwTopicService::forApplication()` after every add, edit and removal, and every notes view subscribes and re-reads the store through `INotesService`. A subscriber receives the cached payload immediately, so a view opened later still shows the current subject; a publisher of a subject topic only publishes while it is the window's current view and clears the topic when it hides |
| `QcwTopicRegistry` | The optional catalog of topic metadata. Every demo topic is registered from the publisher's `doRegisterContributions()` with its description, payload keys and whether it is stateful; tooling reads it through `topicRegistry->allTopics()` |
| `QcwPersistable` | Inherited through `QcwView`/`QcwPerspective`; the editor persists its file path and the project view its filter, both travelling in the saved session as the `extraState` attribute |
| `QcwPreferencesManager`, `QcwPreferencesPage` | The text editor plugin contributes a font and tab-width page and re-applies it on `settingsChanged()` |
| `QcwLogger`, `QcwLoggerSink` | `LogView` installs itself as a sink, so the log pane is fed from the same file the library writes, including from other threads |
| `QcwThemeManager`, `QcwWidgetTheme`, `QcwThemeUtils`, `qcw::themeIcon()` | `LogView`'s colours come from `themes/QcwLight/LogView.theme` and `themes/QcwDark/LogView.theme`, re-read on `themeChanged()`; the perspective and view icons come from the theme |

## Layout

```
workbench_demo/
├── main.cpp                 initialize() + start(), then the Qt event loop
├── Workbench.cpp            QcwWorkbench: window class, plugin directories
├── MainWindow.cpp           QcwWindow: View menu, shared folder, first perspective
└── plugins/
    ├── common/DemoServices.h  type ids, the View menu id, the demo's command ids, IEditorService,
    │                        topic ids, payload keys, settings keys
    ├── common/DemoNotes.h   the notes contract: INotesService, the app-scope topic, the ids
    ├── projectview/         QcwView + QcwViewCreator  (multi-root project tree, filter, file actions)
    ├── texteditor/          QcwView + QcwViewCreator + IEditorService + topics + preferences page
    ├── outlineview/         QcwView + QcwViewCreator  (symbol tree fed by the editor selection topic)
    ├── logview/             QcwView + QcwViewCreator + QcwLoggerSink + widget theme
    ├── notes/               notesplugin: plain QcwPlugin + INotesService + app-scope topic, no view
    │                        notesview:   QcwView + QcwViewCreator (subscribes to the notes topic)
    ├── projectperspective/  QcwPerspective + QcwPerspectiveCreator
    ├── focusperspective/    QcwPerspective + QcwPerspectiveCreator
    └── CMakeLists.txt       qcw_demo_add_plugin(): qt_add_plugin + the POST_BUILD copy
```

`plugins/common/DemoServices.h` is the contract between plugins: type ids, the menu id,
`IEditorService`, the topic ids and their payload keys. It is a header, not a library, so each
plugin compiles it in, and only the IID, the topic strings and the payload keys have to stay in
agreement. `plugins/common/DemoNotes.h` does the same for the two notes plugins.

### The plugin that is not a view

Every other plugin here derives from `QcwViewCreator` or `QcwPerspectiveCreator`, because what it
adds is something a window shows in a tab. **`notesplugin` is different**: it derives from
`QcwPlugin` alone, reports the category `"Tool"`, and creates no widget at all. It exists to show
the rest of what a plugin can contribute before any window exists:

- a **service** — `demo::INotesService` in `QcwServiceRegistry`, which holds the notes and outlives
  any window or view;
- a **command** — `Notes/AddNote` catalogued in `QcwCommandRegistry` under its own `"Notes"`
  category, with a `QcwLambdaCommandHandler` registered at the window scope for every window the
  plugin sees. No menu and no contribution holds it, so the command palette is where it appears;
- a **preferences page** — the log flag, through `QcwPreferencesManager`.

The pane that shows the notes is a *second, ordinary* plugin, `notesview`, and the two meet only
through `DemoNotes.h`: the view reaches the store through the service, and the store announces
changes on the application-scope topic `app/notes/changed`, which every notes view subscribes to
and answers by re-reading the store. Close the notes tab and the notes are still there; open it
from another window and it shows the same list. Reading `NotesPlugin::doRegisterContributions()` is the
short answer to "what can a plugin do without a view?".

## Startup sequence

1. `main()` calls `Workbench::initialize(appName, displayName, orgDomain, orgName, version, quitOnLastWindowClosed, loadPlugins = false)`.
2. `start()` → `preStart()`, which scans `bin/plugins/workbench_demo` plus `QcwPluginLoader::defaultPluginDirectories()` (minus the shared `bin/plugins`, which belongs to the other examples).
3. The workbench restores the saved session; a window from a previous run comes back with its
   perspectives, folders and views, and each view's `restoreExtraState()` runs.
4. `postSetupUi()` runs for a fresh **and** a restored window — that is where the shared folder's
   placeholder belongs. It only builds the demo layout when there is nothing to restore.

## Adding one of your own

- **A view**: subclass `QcwView` and a `QcwViewCreator`, add `QCW_PLUGIN_INTERFACE` in the header and
  `QCW_PLUGIN_IMPLEMENT(MyViewCreator, "MyView")` in the source next to a `PluginInfo.h` that defines
  the five metadata literals, then `qt_add_plugin` it through `qcw_demo_add_plugin()`. A perspective
  puts it in a folder with `creator->create(folder, /*assignId=*/true, args)` and `addTab()`; the
  creating arguments are remembered on the view and saved with it, so the restore rebuilds it
  without the application doing any bookkeeping.
- **A perspective**: the same shape around `QcwPerspective`/`QcwPerspectiveCreator`; build the folder
  tree in `createInitialLayout()` and add views by their type string through `pluginManager`.
- **A service**: declare an interface with `Q_DECLARE_INTERFACE`; create the implementing object
  in `doRegisterContributions()` and pass it to
  `serviceRegistry->registerService<MyService>(service)`, deleting it if the call returns false.
  A successful registration hands the service to the registry, which owns it for the rest of the
  run. Look it up with `serviceRegistry->service<MyService>()` — see `demo::IEditorService`, which
  lets the project view and the outline ask the editor to open a file and reveal a position.
- **Something with no view — a store, a command, a settings page**: subclass `QcwPlugin` itself and
  fill `doRegisterContributions()`; several contributions in one plugin is the normal case, as
  `notesplugin` does. Only add a creator subclass when the plugin has a widget to show.
- **A publisher or consumer of a topic**: put the topic id and payload keys in a shared header,
  register the topic in `QcwTopicRegistry` from the publisher's `doRegisterContributions()`,
  publish with `owningWindow()->topicService()->publish(...)` for window scope (or
  `QcwTopicService::forApplication()->publish(...)` for application scope, as the notes store
  does), and subscribe — for a window topic, with `QcwView::subscribeWindowTopic()`, which keeps
  the subscription on the window the view is in and needs no cached window of the view's own, as
  `OutlineView` and `ProjectView` do; `subscribe()` on the service directly is the application-scope
  route (`NotesView`). Follow the demo's rule: a subject topic is published
  only while `window->currentView() == this`, it is cleared when the view hides, and consumers
  apply the payload they are handed — the cached one on attach and every later one — rather than
  polling for the current state.

## Gotchas

- **The `POST_BUILD` copy is not decoration.** `workbench_demo` loads from `bin/plugins/workbench_demo`,
  and nothing fills that directory on its own. Add a plugin without the copy step and the build
  succeeds while the run either misses it or loads a stale library whose interfaces no longer match.
- **`type()` is a persisted identifier.** `QCW_PLUGIN_IMPLEMENT`'s second argument is what the saved
  session refers to, so renaming it orphans every window that stored it.
- **The creator interfaces carry no `Q_OBJECT` on purpose.** `Q_DECLARE_INTERFACE` supplies the
  `qobject_cast` specialization that matches on the IID; the plugin's own class needs `Q_OBJECT` plus
  `Q_INTERFACES()`.
- **`QCW_PLUGIN_DESCRIPTION`/`QCW_PLUGIN_DETAILS` go through `tr()`** in the macro, so write them as
  untranslated source strings.
- **A view's `commandHandlers()` runs on every view change**, so it must return the handlers it
  already owns, in a stable order — building new `QcwLambdaCommandHandler`s there leaks, and a
  handler that changes identity on each call re-resolves the whole menu for nothing.
- **A menu entry is the command's `QAction`, and the handler behind it is looked up when the menu
  is built or the current view changes.** A contribution whose command the catalog does not list is
  skipped; a catalogued command that no scope handles keeps its entry but is hidden and disabled, so
  it reappears the moment a view that handles it becomes current. The project commands are a demo
  concept, so their ids live in `DemoServices.h` and `MainWindow` registers the info plus the
  contribution itself; `QcwWindowActionIds` keeps only the commands the framework offers.
- **Only the current view can answer at the view scope.** There is no fallback that searches the
  other views of the window, which is why `ProjectViewCreator` is a per-window singleton: a second
  project tree would show the same list twice and the idle one's commands would go unanswered.
- **Views own their handlers and their status bar widget**; the window only borrows them, and
  switching away detaches rather than deletes.
- **A view re-translates its own strings, never its window title.** `QcwView::changeEvent()` is where
  `NotesView` re-texts its buttons, but the title belongs to the window: `QcwWindow::retranslateUi()`
  re-resolves it from the creator, and `(type(), windowTitle())` is the pair the saved session
  restores a view by. A view that re-applies its own translated title fights that pass and breaks the
  restore.
- **Catalog entries are strings, not widgets, so Qt's language event never re-texts them.**
  `NotesPlugin` filters `qApp` for `QEvent::LanguageChange` to re-register its catalog entry, and
  `MainWindow` re-registers the command infos it owns from `retranslateUi()` — registering the same
  id again is what emits `commandsChanged()`, which every window answers by refreshing its command
  `QAction`s. Otherwise the palette and the menus keep the old words.
- **A plugin's destructor runs at process exit, not before.** Plugins are never unloaded, so by the
  time a plugin's static destructor runs the `QCoreApplication` and every window are gone. Notes'
  store keeps its notes in memory and publishes nothing to clean up; a plugin that would have to
  unregister something on destruction cannot rely on that moment arriving.

## What this demo does not cover

The renderer-level customization lives in
[`../folder_renderer_demo/README.md`](../folder_renderer_demo/README.md), and the raw container
operations — folder minimize/restore, the layout XML written by hand — in
[`../folder_container_demo/README.md`](../folder_container_demo/README.md). Here the workbench writes
and reads that document for you, which is why no file in this directory mentions
`QcwTabFolderContainer::saveState()`.

## Build and run

```sh
cmake --build build -j"$(nproc)"     # the plugins are separate targets
./build/bin/workbench_demo
```

Open a file in the project tree to get an editor in the shared folder and its symbols in the
outline; moving the cursor highlights the enclosing symbol and double-clicking a symbol jumps back
into the editor. Switch perspectives from `Window > Perspective > Open Perspective`, edit both the
filter and the editor font, then close and restart: the layout, the open editors and the per-view
state come back.

The notes pane opens beside the editor once a note exists or `Window > Command Palette`
(`Ctrl+Shift+P`) runs `Add Note`; typing `notes` in the palette shows just that category, which the
window does not get from any menu.
