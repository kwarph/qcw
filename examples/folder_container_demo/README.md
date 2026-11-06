# folder_container_demo

Shows the **docking container**: how folders are created, arranged and remembered. One
`QcwTabFolderContainer` holds three folders — a project list, a central editor area and a console —
split left/right and bottom, and the whole tree (plus the open files and the window geometry) is
written to and read back from an XML document.

Read this demo after `folder_renderer_demo` if the look is already settled and you want to know how
folders, panes and layout state are managed.

## Components demonstrated

| Component | What the demo uses it for |
|---|---|
| `QcwTabFolderContainer` | `createFolder(relativeTo, dockArea)` builds the tree, `setCentralFolder()` marks the editor area, `parentSplitter()` sets the ratios, `folders()`/`currentFolder()`/`centralFolder()`/`isCentralFolder()` query it, `parentFolder(pane)` resolves a pane back to its folder, `clear()` throws the layout away. `File > Docking in a Dialog...` puts a second container in a plain `QDialog` |
| `qcw::LeftDockArea`, `qcw::RightDockArea`, `qcw::BottomDockArea` | Where each new folder lands relative to the one it is split from |
| `QcwTabFolderContainer::folderCreated()` | The single hook that covers both `createFolder()` and the folders a restore recreates |
| `QcwTabFolderContainer::currentFolderChanged()` | Drives `QcwTabFolder::setActive()`, so exactly one folder looks active |
| `QcwTabFolderContainer::folderMinimized()` + `QcwFastViewToolBar` | A minimized folder leaves the layout and comes back as a window-edge tool bar |
| `QcwTabFolderContainer::saveState()` / `restoreState()` | The XML serialization of the splitter tree, the folder sites, the sizes and the folder settings |
| `QcwTabFolder` | `addTab()`, `tabPane()`, `indexOf()`, `setTabData()`/`tabData()`, `setTabToolTip()`, `setTabIcon()`, `setTabClosable()`, `setTabDetachable()`, `setTabDirty()`/`isTabDirty()`, `setPlaceholder()`, `setFeature(qcw::ShowFullTitle, false)`, `minimize()` |
| `QcwTabPaneManager` | `nextPaneId()` stamps every pane on creation, `allTabPanes()` lists them for the document, `resetStartingPaneId()` continues after a restore, `clear(true)` deletes them with the layout |
| `qcw::TabPaneIdPropertyName` | The id that ties a saved `<tabPane>` element back to the widget a folder re-attaches |
| `QcwSerializeUtils` | `serialize()`/`deserialize()` for the creating arguments, so a pane can be rebuilt from what created it the first time |

Plain Qt does the rest: `QXmlStreamReader`/`QXmlStreamWriter` for the document, `QSaveFile` for the
write, `QPlainTextEdit` as the editor pane, `QFileDialog` and `QMessageBox` for interaction.

## Files

| File | Role |
|---|---|
| `MainWindow.cpp` | Initial layout, the File menu actions, `openFile()`, `saveFile()`/`saveAll()`, `connectFolder()`, `saveState()`/`restoreState()`, `openDockingDialog()` |
| `MainWindow.ui` | The container as the window's central widget |
| `main.cpp` | The stylesheet that frames the folders and thins the splitter handles |

## How the state document is put together

```
<qcw version="1">
  <window geometry="<saveGeometry() hex>">
    <tabPanes count="N">
      <tabPane type="QPlainTextEdit" id="3" creatingArguments="<serialized args hex>"/>
    </tabPanes>
    <folderContainer> … written by QcwTabFolderContainer::saveState() … </folderContainer>
  </window>
</qcw>
```

Restoring is a two-layer job, and the split is worth understanding because it is the library's
design, not a quirk of the demo:

1. `MainWindow::restoreState()` reads the `<tabPanes>` element and **creates the widgets**, keyed by
   their saved id. Only the demo knows how to rebuild its own panes — that is what the
   `PaneType` property and the serialized creating arguments are for.
2. `QcwTabFolderContainer::restoreState()` then rebuilds the splitter tree and hands each folder its
   tabs, addressing panes by id. `QcwTabFolder::restoreState()` calls `addTab()` for every saved
   `<tab>` element and restores its tooltip, data map and closable/detachable flags.
3. The demo finishes the job: icons for the restored tabs (the library takes the icon from
   `QWidget::windowIcon()`, which a plain editor does not set), the central folder's placeholder,
   `resetStartingPaneId()` and `restoreGeometry()`.

## Docking inside a dialog

`File > Docking in a Dialog...` shows the same container in a `QDialog`. Nothing about the
container is window-specific: `openDockingDialog()` builds an ordinary `QVBoxLayout` with a new
`QcwTabFolderContainer`, creates the three folders with the same `createFolder()` calls, and calls
`exec()`. Tabs still dock, split, drag between folders and float into their own top-level windows,
because a floating folder is a window of its own rather than part of the host.

Two things do depend on the host:

- **Minimize needs a `QMainWindow`.** Minimizing a folder emits `folderMinimized()`, and the usual
  handler puts a `QcwFastViewToolBar` into the host's tool bar area — which a `QDialog` does not
  have. The dialog therefore switches `qcw::Minimizable` off on every folder (`setFeature()`), so
  the button is gone instead of being offered and doing nothing. A maximized folder only hides its
  siblings inside the container, so `qcw::Maximizable` stays as it is.
- **The container goes away with the dialog.** It is parented to the dialog, so `exec()` returning
  and the stack-allocated dialog being destroyed destroy the folders, and with them the panes. The
  pane ids they took from `QcwTabPaneManager` go with their widgets; the demo's own save/restore
  does not include the dialog's layout, which is what makes the dialog a self-contained example.

## Things worth knowing before you write your own

- **`restoreState()` can hand you a partial layout.** It returns `false` when a folder refuses its
  element, and the contract is that the caller discards what is left. Do not treat "the XML parsed"
  as "the layout came back".
- **A pane that cannot be rebuilt is not an error.** Its `<tab>` element still restores, so you get
  a tab with text and no pane. The demo skips those when it re-applies icons and rebuilds its open
  file set.
- **Dirty state and unsaved buffer content are not serialized.** `<tabPane>` stores the arguments
  that created the pane, so a restore re-reads the file from disk. Anything the user had not saved
  is gone, and every restored tab starts clean.
- **The pane, not the code that opened it, should track its own dirty flag.** Wiring the
  `textChanged` handler where the tab is added looks fine until a restore recreates the pane — then
  nothing marks it dirty — or until a pane is dragged into a floating folder, where a lookup
  against the original container finds nothing. `TextEditor` in this demo resolves its own folder
  from its parent chain.
- **One `tabCloseRequested()` receiver per folder.** The signal carries an index; every receiver
  that removes a tab shifts that index under the others. `folderCreated()` plus a per-folder
  install is the pattern that keeps it to one.
- **`QcwFastViewToolBar` manages its own lifetime.** It takes the minimized folder over, and
  `aboutToRestore()`/`aboutToDelete()` hand the folder back and schedule the tool bar for deletion,
  so the window does not need a folder → tool bar map. It does need to remove them before deleting
  the folders, which is what the restore path does first.
- **Saving and restoring here is explicit, by design.** Nothing is written on exit and nothing is
  read at start-up, so both directions stay visible as actions. The file is `state.xml` next to the
  executable; delete it by hand to start from the initial three folders again.

## What this demo does not cover

Custom rendering (see [`../folder_renderer_demo/README.md`](../folder_renderer_demo/README.md)) and
everything above the container level — views, perspectives, plugins, per-view state (see
[`../workbench_demo/README.md`](../workbench_demo/README.md)).

## Build and run

```sh
cmake --build build --target folder_container_demo
./build/bin/folder_container_demo
```

`File > Open Files…` puts the editors in the central folder; they stay closable and detachable, so
drag one out to see the floating-folder path. Minimize the console folder to get a fast-view tool
bar, then `File > Save State`, close every folder, and `File > Restore State` to bring the layout
back. `File > Docking in a Dialog...` opens the same container in a dialog, where a drag between
folders works and the minimize buttons are absent.
