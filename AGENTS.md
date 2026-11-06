# QCW — project conventions for agents

## Singletons

Fifteen classes are process-wide singletons, all built the same way:

| Class | Accessor macro |
|---|---|
| `QcwActionSetRegistry` | `actionSetRegistry` |
| `QcwCommandRegistry` | `commandRegistry` |
| `QcwContributionRegistry` | `contributionRegistry` |
| `QcwFloatingFolderMonitor` | `floatingFolderMonitor` |
| `QcwNotificationManager` | `notificationManager` |
| `QcwPluginManager` | `pluginManager` |
| `QcwPreferencesManager` | `preferencesManager` |
| `QcwSavedPerspectives` | `savedPerspectives` |
| `QcwServiceRegistry` | `serviceRegistry` |
| `QcwSessionData` | `sessionData` |
| `QcwTabFolderRendererCreators` | `folderRendererCreators` |
| `QcwTabPaneManager` | `tabPaneManager` |
| `QcwThemeManager` | `themeManager` |
| `QcwTopicRegistry` | `topicRegistry` |
| `QcwWindowStyler` | `windowStyler` |

The pattern is always:

```cpp
// header
class QCW_EXPORT QcwThemeManager : public QObject
{
    Q_OBJECT
public:
    static QcwThemeManager* instance();
    // ...
};

#ifndef themeManager
#define themeManager QcwThemeManager::instance()
#endif

// source
Q_GLOBAL_STATIC(QcwThemeManager, themeManagerInstance);

QcwThemeManager* QcwThemeManager::instance()
{
    return themeManagerInstance;
}
```

How to read and treat them:

- `instance()` returns a **valid object on every call**. `Q_GLOBAL_STATIC` builds it on
  first use and hands out that same object ever after, so call sites neither null-check
  the result nor document a null case.
- The lowerCamelCase macro next to `instance()` is the project's way of calling it, not an
  oversight. Keep using it; do not propose replacing it with `instance()` or with a
  dependency-injection refactor.
- Twelve of the fifteen declare no constructor at all and use the implicit default one, so
  the global is never parented and nobody can build a second instance by accident.
  `QcwFloatingFolderMonitor` declares one that takes a `parent`, but nothing passes one — the
  global instance has no parent, because it must outlive everything that listens to it.
  `QcwNotificationManager` and `QcwWindowStyler` declare constructors and destructors for
  their own setup, but `instance()` is still the only intended way to get them.
  The plain-data registries among them — `QcwActionSetRegistry`,
  `QcwTabFolderRendererCreators`, `QcwServiceRegistry` and `QcwTopicRegistry` — are not even
  `QObject`s: they are id-to-metadata, name-to-factory, interface-to-service and
  id-to-topic-metadata maps, and their globals must not be duplicated either.
- When documenting one of these classes, say it is the single global instance and that a
  second one is pointless — nothing connects to it, and lookups go through the macro, so a
  second registry would never be seen. Do not add shutdown/lifetime caveats.

## Registry ownership models

Every process-wide registry and every handler map follows one of three models. Match the model
of the class you are extending instead of inventing a fourth.

- **Copies — owns nothing.** `QcwActionSetRegistry`, `QcwCommandRegistry`,
  `QcwContributionRegistry`, `QcwSavedPerspectives`, `QcwSessionData`,
  `QcwTabFolderRendererCreators`, `QcwThemeManager`, `QcwTopicRegistry`. The registry stores
  values (a copy of a struct, string, path or `std::function`). The caller may drop the original
  as soon as the call returns, and removing an entry — where the class offers it — only forgets
  a copy; it never deletes anything. A later registration under the same id replaces the copy.
  Most removal APIs in this group exist for tests that share the process-wide instance (and for
  `QcwBuiltinCommands::unregisterCommands()`), because nothing unloads; production code
  registers once. `QcwThemeManager::removeTheme()` is the exception the domain needs: themes
  come and go through the theme dialog.
- **Borrowed — owned elsewhere.** `QcwPluginManager` (plugin objects), the handler maps of
  `QcwWindow`, `QcwPerspective` and `QcwWorkbench`, and the `QcwLogger` sinks. The registry
  keeps a raw pointer and never deletes it; the provider keeps ownership and must outlive the
  registration. The handler maps watch `destroyed()` and drop an entry automatically, and
  registering an id again leaves the previous handler untouched. Logger sinks are not
  QObjects, so the sink must call `QcwLogger::removeSink()` before it is destroyed. Removing a
  borrowed entry, including `QcwPluginManager::removePlugin()`, forgets the pointer and leaves
  the object with its owner.
- **Owned forever.** `QcwServiceRegistry` and `QcwPreferencesManager`. A successful
  registration transfers ownership: the registry detaches the service from any parent or keeps
  the raw page, and never deletes it — not even at shutdown. A refused registration returns
  false and leaves the object with the caller, who must delete it. There is no removal API.

`QcwTabPaneManager` is the one class that deletes what it tracks: a pane is reference-counted
across the folders that show it and `deleteLater()`ed when the last folder releases it;
`clear(deletePanes)` drops every entry at session teardown. `QcwTopicService` is scope-owned:
its lifetime belongs to the window or workbench that created it, not to a registry.

## Component interaction

Plugins are separate libraries and must not link to one another. The interaction classes live
in `src/workbench/` (`QcwServiceRegistry` and `QcwTopicService`/`QcwTopicRegistry`) and offer
two channels, in this order of preference:

- **Services — a verb.** `QcwServiceRegistry` maps an interface id to the object implementing
  it. The interface is declared with `Q_DECLARE_INTERFACE` (that is what supplies the id);
  the provider creates the service in `doRegisterContributions()` and calls
  `serviceRegistry->registerService<MyService>(service)`, deleting it when the call returns
  false. A successful registration transfers ownership to the registry: the service is
  detached from any parent and lives until the process exits, so it must never be deleted.
  The first registration wins; a refused service stays with the caller. Consumers call
  `serviceRegistry->service<MyService>()`; look a service up when it is needed, and never
  delete the returned pointer.
- **Topics — state and facts.** `QcwTopicService` carries a topic id string and a `QVariantMap`.
  It is **not** a singleton: `QcwWindow::topicService()` returns the window-scoped instance and
  `QcwWorkbench::topicService()` the application-scoped one; `QcwTopicService::forWindow(window)`
  and `QcwTopicService::forApplication()` are the static accessors. `publish()` caches the
  payload and calls subscribers
  synchronously, in subscription order; `subscribe()` replays the cached payload immediately, so
  a late consumer catches up without polling; `current()` reads it without subscribing, and
  `clear()` drops it. A subscription belongs to its receiver and disappears with it;
  `unsubscribe()` stops it earlier. Use `win/...` ids for window topics and `app/...` for
  application topics — `QcwTopicRegistry` warns about other prefixes, but the service neither
  routes nor validates them. Register ids there when tooling should discover them. The current
  implementation caches every topic, so even an announcement topic replays its last payload;
  handlers must not expect a result. Delivery is synchronous on the GUI thread and the service is
  not thread-safe; queue worker-thread publications with `QMetaObject::invokeMethod()`.
  A view **consumes** window topics through `QcwView::subscribeWindowTopic()` /
  `unsubscribeWindowTopic()` / `currentWindowTopic()`, never by caching a `QcwWindow*` and
  calling `window->topicService()` from its own event handlers: the base class (un)subscribes as
  the view attaches, moves between windows, detaches and survives its window's teardown, and
  `~QcwWindow` frees its private before the folder destructors release the panes — the pattern that
  use-after-free was. Publishing is the view's own business: it reads `owningWindow()` on the spot,
  publishes its current subject only while `window->currentView() == this` and clears the
  topic when it hides; the window does not publish or clear on the view's behalf — the demo's
  `TextEditor` and `ProjectView` show the window-scope pattern. The notes store shows the
  application-scope one: `NotesService` publishes `app/notes/changed` through `forApplication()`,
  and every `NotesView` subscribes and re-reads the store. `forApplication()` finds the workbench
  through the application's children; `QcwWorkbench::initialize()` attaches the workbench to the
  `QApplication` for that, and without an initialized workbench the accessor falls back to a
  process-wide service for tests and small applications.

Commands are the exception that needs no new channel: a view answers them by returning
handlers from `commandHandlers()`, an application places them with a `QcwContribution`, and
`QcwWindow::resolveHandler()` picks the handler — current view, current perspective, window,
then the workbench's global one. Payload keys and topic names are a contract between the
publisher and its subscribers, documented next to the interface, not by the framework.

## Settings access

`QcwSettings::get()` returns a `QSettings` **by value**. That is deliberate — do not
"optimise" it into a cached/shared instance. The object comes from a provider function:

```cpp
using QcwSettingsProvider = QSettings (*)();

struct QCW_EXPORT QcwSettings
{
    static void initialize(QcwSettingsProvider provider);  // first call wins
    static QSettings get();                                // fresh object per call, by value
};

// source
QSettings QcwSettings::get()
{
    return settingsProvider();
}
```

- Without `initialize()`, the provider returns a default-constructed `QSettings`; its
  location and format come from `QSettings::defaultFormat()` and the organization/application
  names. That is what a program gets when it never installs a provider.
- `initialize()` is optional and exists for programs that need a known store. QcwWorkbench
  installs a provider that keeps everything in `configDirectory()/QcwSettings.ini` in INI
  format. Only the first call has an effect, so an application may install its own provider
  before `QcwWorkbench::initialize()`, and `initialize(nullptr)` freezes the default.
- Qt documents `QSettings` as *reentrant*: **distinct** objects may be used from different
  threads at once, even when they wrap the same store. A single shared instance would be
  data-race territory.
- Constructing and destroying is cheap — Qt's own wording is "Constructing and destroying a
  QSettings object is very fast". Measured on QCW's real settings file: ~3 µs per call.
- Cheap because the parsed settings are shared process-wide (`QConfFile` objects live in a
  global cache behind `settingsGlobalMutex`); strace over 20 000 `QSettings` constructions
  shows ~25 reads. Re-parsing is not what happens.
- Writes still land: `~QSettings()` flushes pending changes, so a caller's temporary object
  persisting `setValue()` needs no explicit `sync()`.
- Changes made through one object are visible through the others in the process
  immediately, so nothing is lost by not sharing the wrapper.
- The provider is a plain global, so install it during startup, before other threads call
  `get()`. Set the application/organisation names before touching settings, since both the
  default store and the INI path derive from them.

## Translation catalogs

`QcwTranslations` is a static struct, not a singleton: no `instance()`, no accessor macro, nothing
to own. The registered bases and directories and the applied locale are file-scope data in its
`.cpp`, and the one `QTranslator` per base lives in a function-local `static std::map` of
`unique_ptr` — deliberately not parented to the `QCoreApplication`, because a test process creates
more than one application. It is the only code that loads a catalog:
`QcwAppearancePreferencesPage` reads the locale list from it and hands a user's choice to it.

Three rules the subsystem exists to enforce:

- **Never vendor Qt's catalogs.** `qtbase_<locale>.qm` is loaded from
  `QLibraryInfo::path(QLibraryInfo::TranslationsPath)`, which closes the search list and always is
  the Qt the application links against. A copy under `translations/` would freeze it at whichever
  Qt it was taken from, so that directory holds only `qcw_<locale>.ts` and `.qm`.
- **`qtbase` never advertises a language.** `availableLocales()` lists the application-owned bases
  — `qcw` plus the registered ones — and adds `en`, the language the sources are written in. A Qt
  installation carries a catalog for every language Qt supports, so listing its file names would
  offer a UI the library's own strings cannot follow.
- **`appearance/language` belongs to this subsystem.** A host reads `currentLanguage()` or
  subscribes to `QcwLanguageTopic` (`app/language`, payload key `locale`). The empty choice is
  "follow the system" and is stored as *no* value at all, so a moved system locale is followed on
  the next start.

A plugin registers its own base from `doRegisterContributions()`, which runs after the stored
language has been applied; `QcwWorkbench::initialize()` compares `catalogBases()` around the plugin
load and applies the language again when a base appeared. Keep that comparison — without it a
plugin's strings come up in the source language until the user touches the language selector.

## Plugin interfaces and `Q_OBJECT`

`QcwViewCreator` and `QcwPerspectiveCreator` are interface classes: they declare **no**
`Q_OBJECT` of their own, and that is correct rather than an omission — do not add it back
when "fixing" a missing metaobject.

```cpp
class QCW_EXPORT QcwViewCreator : public QcwPlugin   // no Q_OBJECT
{ /* ... */ };

#define QcwViewCreator_iid "org.qcw.QcwViewCreator"
Q_DECLARE_INTERFACE(QcwViewCreator, QcwViewCreator_iid)
```

- `Q_DECLARE_INTERFACE` (Qt sources: `qtbase/src/corelib/kernel/qobject.h`) supplies explicit
  specializations:
  ```cpp
  template <> inline IFace* qobject_cast<IFace*>(QObject* object)
  { return qobject_iid_cast<IFace>(object); }   // -> object->qt_metacast(IId)
  ```
  So `qobject_cast<QcwViewCreator*>` — which is what
  `QcwPluginManager::getPluginT<QcwViewCreator>()` runs — resolves to that specialization and
  matches on the IID string. It never touches a `staticMetaObject` of the interface, whether or
  not the interface has one.
- What *is* required is on the plugin side: its own class needs `Q_OBJECT` plus
  `Q_INTERFACES(QcwViewCreator)`, which is what puts the IID into `qt_metacast()`.
- moc links a plugin's metaobject to `QcwViewCreator::staticMetaObject`; without `Q_OBJECT` that
  name resolves to the inherited `QcwPlugin::staticMetaObject`, so the chain simply skips the
  interface level and still compiles.
- Consequence worth remembering: the interface's `Q_OBJECT` emitted
  `staticMetaObject`/`qt_metacast`/vtable symbols into the library, so dropping it is an ABI
  break — every plugin .so has to be rebuilt.
- `perspectives_demo` loads from `bin/plugins` beside the executable. Both sample plugins copy
  themselves there with a `POST_BUILD` `copy_if_different` command; a plugin added without that
  step would build fine and still be missing (or be a stale, unloadable copy) at run time.

## Private implementation and member naming

An interface that serves the outside world exposes as few private members as possible — member
functions and data members alike. There is no quantitative rule for when a class gets a pimpl; the
principle decides, and [`docs/qcw-pimpl-conventions.md`](docs/qcw-pimpl-conventions.md) records the
full set. The shape is fixed:

```cpp
// MyWidget.h  — forward declare only; the private is defined in MyWidget.cpp
class MyWidgetPrivate;

class MyWidget : public QWidget
{
public:
    explicit MyWidget(QWidget* parent = nullptr);
    ~MyWidget() override;              // declared AND defined: delete impl; in the body

private:
    // friend class MyWidgetPrivate;   // only when the private needs the host's protected members
    MyWidgetPrivate* impl;             // always in a private: section, always named impl
};

// MyWidget.cpp
class MyWidgetPrivate
{
public:
    explicit MyWidgetPrivate(MyWidget* interface);   // host argument named: interface

public:
    MyWidget* owner;        // back-pointer to the host named: owner
    QString imagesPath;     // every member of the private is lowerCamelCase
};
```

How to read and treat it:

- `impl` is built in the constructor's member-init list and is never null, so call sites neither
  null-check it nor document a null case.
- The host destructor is always user-declared and user-defined. A compiler-generated one leaks the
  private; a declaration without a definition does not compile, because `MyWidgetPrivate` is
  incomplete in the header.
- `MyWidgetPrivate` is a plain class: no `Q_OBJECT`, no signals or slots — those stay on the host,
  and the private reaches it through `owner`.
- `friend class QcwXxxPrivate;` exists in exactly three places (`QcwWindow`, `QcwWorkbench`,
  `QcwView`), because only those three privates touch protected host members. Do not add the
  friendship anywhere else, and do not "clean it up" where it is needed.
- A pimpl is what keeps internal changes from forcing every plugin to be rebuilt: private members
  are part of the object layout of a `QCW_EXPORT` class that plugins subclass and stack-allocate.
  Prefer it when a class has many private members, when declaring them would drag includes into
  the public header, or when the destructor has a real teardown. Skip it for two or three plain
  data members, for pure data structs, and for helpers used inside a single `.cpp`.

Naming when there is no pimpl:

- Pure data carriers (`struct`s with little or no behavior): every member lowerCamelCase —
  `struct SessionData { QString docRoot; QString cookie; };`.
- Classes with a few private data members: `m` + UpperCamelCase, never an underscore —
  `mDataPath`, `mTopic` (not `m_dataPath`).

The one exception to "never null-check `impl`" is Qt's destruction order. `delete impl;` does not
end destruction: `~QWidget` then tears the children down, and a `QcwTabFolder` releasing its panes
reaches the views of a window whose private is already gone. `~QcwWindow` therefore sets
`impl = nullptr;` after the delete and its accessors answer with an empty value during that span.
This does not contradict the singleton rule above — that one is about `Q_GLOBAL_STATIC` globals
that stay valid for the whole process, this one about a single destructor.

## Versioning

`CMakeLists.txt` holds the single version number, and a release tag is that number prefixed
with `v` (`v1.0.0`). Semantic versioning applies to the public API — the `QCW_EXPORT` classes
and functions, the installed headers and the plugin interfaces: fixes bump the patch digit,
additions the minor one, and removals, renamed or changed signatures, changed contracts and
every plugin-interface ABI change the major one. A feature change does not bump the version by
itself; the release step does. The table and the release checklist are in
[`README.md`](README.md#versioning-and-releases).

## Reading Qt documentation

Qt keeps its `\verbatim`-style doc comments next to the implementation, so grep the `.cpp` files,
not the headers — e.g. `widgets/qtoolbar.cpp` for `QToolBar::clear()`, `kernel/qwidget.cpp` for
`QWidget::insertAction()`.

Where those sources live is a property of the machine, not of the repository, so it is recorded in
[`AGENTS.local.md`](AGENTS.local.md) — a gitignored, per-developer file beside this one, which the
CLI loads together with `AGENTS.md`. Create it with your own qtbase checkout path.
