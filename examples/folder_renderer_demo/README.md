# folder_renderer_demo

Shows that **the whole look of a tab folder is replaceable**. One `QcwTabFolder` sits in a plain
main window; a preferences dialog edits two JSON theme documents and swaps between the library's
built-in renderer and the demo's own card-style renderer at run time, without the folder losing
its tabs, its dirty state or its behaviour.

Read this demo first if you want to change how tabs are painted. It deliberately contains no
docking container, so everything in it is about one folder and one renderer.

## Components demonstrated

| Component | What the demo uses it for |
|---|---|
| `QcwTabFolder` | Hosted directly in the window layout (no `QcwTabFolderContainer`), with panes added through `addTab()`, `setTabToolTip()`, `setTabData()`/`tabData()`, `setTabClosable()`, `setTabDetachable()`, `setTabDirty()`/`isTabDirty()`, `setTabPosition()`, `setActive()` |
| `QcwTabFolderRenderer` | The base class the demo subclasses: the pure virtuals `clone()`, `renderTabBarRegion()`, `renderTabRegion()`, `textColor()`, and the geometry hooks `tabBarSpacing()` and `tabBarPadding()` |
| `QcwDefaultTabFolderRenderer` | Kept as the alternative ("Framed") look, so the two can be compared live in one run |
| `QcwTabFolderRenderer::isLightTheme()` | Picks the `Light` or `Dark` entry of the theme document to match the application palette |
| `QcwTabFolder::tabTransferred()` | A tab dragged into another folder re-themes and re-wires the target, which is how the floating folder created by a detach is reached |
| `QcwTabFolder::tabCloseRequested()` | The save-or-discard prompt before a tab goes away |
| `QcwTabFolder::setRenderer()` | The swap itself, driven by the dialog |

Plain Qt does the rest: `QJsonDocument` for the theme text, `QFileDialog`, `QMessageBox`,
`QPlainTextEdit` as the pane.

## Files

| File | Role |
|---|---|
| `MainWindow.cpp` | Folder setup, `applyTheme()` (choose and configure the renderer), `connectFolder()`, `openFile()`, `saveFile()` |
| `PreferencesDialog.cpp` | Builds the two theme documents, parses the edited JSON, exposes `themeData()` |
| `StyledTabFolderRenderer.cpp/.h` | The card renderer: rounded tabs, per-state colours, the folder stylesheet |
| `DefaultTabFolderRenderer.cpp/.h` | A thin `QcwDefaultTabFolderRenderer` subclass that only takes theme data, to show the baseline is also configurable |

## Things worth knowing

- **`clone()` is not optional and not cosmetic.** A renderer belongs to one folder; when a tab is
  detached into a floating folder, the library clones the renderer onto the new folder. A `clone()`
  that forgets to copy the parsed theme leaves the floating folder painted with defaults.
- **The renderer and the stylesheet are two halves of one look.** `StyledTabFolderRenderer` keeps a
  `mFolderCss` string and applies it to its folder in `setThemeData()`, because the folder's
  background, its rounded frame and the corner buttons are widget-level CSS that no `QPainter` call
  in `renderTabRegion()` can reach. Whoever owns the stylesheet has to be the renderer, or a theme
  swap leaves the old CSS behind.
- **`TabState` is a bit set, and `Pressed` means *selected*.** Test it with `testFlag()`/`&`, and
  only compare with `==` against `Normal`. Painting `Pressed` as "mouse button down" gives tabs
  that change colour at the wrong moment.
- **The geometry hooks decide the space you get to paint in.** `tabBarSpacing()` and
  `tabBarPadding()` feed the folder's own layout maths, so a spacing that does not match what the
  renderer draws leaves tabs touching or visibly gapped.
- **The feature flags already have a dialog.** The folder's context menu ends in
  `Folder Settings...`, which edits `qcw::FolderFeatures` plus the tab icon size and the minimum tab
  text width — the same three setters an application would call — and its result is written by
  `saveState()` along with the layout. The demo deliberately ships no feature checkboxes of its own,
  because a second editor for those bits only drifts out of sync. Note that the dialog emits nothing
  when it applies: an application that mirrors the flags has to re-read them.
- **One `tabCloseRequested()` receiver per folder.** The signal carries an index, and every handler
  that removes a tab shifts that index under the others. `connectFolder()` guards against installing
  a second receiver on the same folder.

## What this demo does not cover

Folder arrangement, splitters, minimize/maximize, layout serialization, views, perspectives and
plugins all live elsewhere: see [`../folder_container_demo/README.md`](../folder_container_demo/README.md)
and [`../workbench_demo/README.md`](../workbench_demo/README.md). The theme documents here are the
dialog's own JSON, not the `themes/*.theme` files the library ships — the demo has no
`QcwThemeManager` dependency, which is what keeps it self-contained.

## Build and run

```sh
cmake --build build --target folder_renderer_demo
./build/bin/folder_renderer_demo
```

`File > Open Files…` takes several paths. Drag a tab out of the folder to get a floating folder and
watch the renderer follow it. Right-click the tab bar for the folder's own menu: its last entry,
`Folder Settings…`, is where the feature flags and the tab metrics are edited.
