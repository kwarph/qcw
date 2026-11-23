# Bringing a view forward

Status: implemented 2026-11-23.

## Why

A view can be out of sight in four different ways, and each one needs a different call:

| Situation | State | What makes it visible |
|---|---|---|
| Another tab is current | folder visible, another index | `QcwTabFolder::setCurrentIndex()` |
| Folder minimized into a fast view | folder parked in a `QcwFastViewToolBar` | `QcwTabFolder::restoreRequested()` |
| Folder hidden behind a maximized sibling | `container->isMinimizedFolder()` | `QcwTabFolder::restoreRequested()` |
| View lives in another perspective | perspective not current | `QcwWindow::setCurrentPerspective()` + folder/tab |

Every application that says "show the Build console" or "show the search results" would have to
repeat those four cases. `QcwWindow` owns the perspectives and folders, so it owns the sequence.

## API

```cpp
// QcwWindow
QcwView* viewOfType(const QString& viewType) const;  // current perspective first
bool revealView(QcwView* view);
QcwView* revealViewOfType(const QString& viewType);  // find + reveal
```

`revealView()`:

1. finds the perspective and folder that hold the view, current perspective first;
2. switches to that perspective only when it is not current;
3. when `isMinimizedFolder(folder)` is true (fast view or hidden behind a maximized sibling),
   emits `QcwTabFolder::restoreRequested()` — the same path the fast view's Restore action uses;
4. calls `QcwPerspective::activateFolder()` and `QcwTabFolder::setCurrentIndex()`.

It deliberately does **not** focus the view, create one, move it to another window or raise the
window. A read-only output view (a build console) can therefore be shown without pulling the
caret out of an editor; a view that wants focus (for example a rename field) does that itself
after the reveal.

`viewOfType()` prefers the current perspective so a panel in a hidden perspective does not make
the window switch away when the visible perspective has its own panel.

## Tests

`tests/view_reveal_test.cpp` covers: find by type, selecting a tab, restoring a minimized
folder, current-perspective preference and switching to a view in another perspective.
