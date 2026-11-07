# Docking-core audit workflow

This document records how the recent audit of `QcwTabFolder` and
`QcwTabFolderContainer` was planned, carried out and verified, so that the next audit of the
same kind can be repeated without rediscovering the process. It covers the family
decomposition, the tools used at each step, the build and run environments, the verification
bar, and the pitfalls found along the way.

Last updated: 2026-10-30.

## 1. Purpose and scope

The docking core is a pair of large stateful widgets whose behavior depends on input events,
feature flags, folder states, the splitter tree and serialized session documents. Most of
their code paths are exercised by normal use, but the rare combinations - a press on the
close button of an inactive folder, a folder minimized while it is maximized, a session
document with a damaged MRU list - were never reached by the test suite and hid real bugs.

The audit therefore targeted **behavior that had never been exercised before**, not a line-by-
line reread. Each round picks one *family* of related behavior, drives it through tests that
use only the public API, fixes what the tests expose, and proves the fix with a full test run,
sanitizers and coverage.

Out of scope: performance, API redesign, and rendering that cannot be asserted offscreen.
Behavior changes that alter a documented contract are allowed only when the contract itself is
wrong, and then the header documentation is updated in the same change.

## 2. Environment

| Item | Value |
|---|---|
| OS | Linux (x86_64) |
| Compiler | `/usr/bin/clang++`, C++17 |
| Qt | Qt 6.9+ (Widgets, Svg), CMake package at `/usr/lib/cmake/Qt6` |
| Build system | CMake 3.23+, Ninja, CTest |
| Test harness | plain `main()` + `check()`, no QtTest; `QT_QPA_PLATFORM=offscreen` |
| Test auto-close | `QCW_TEST_AUTOCLOSE=1` (set by CTest for every test) |
| Tests opt-in | `-DQCW_BUILD_TESTS=ON` (off by default) |
| Warnings | `QCW_TREAT_WARNINGS_AS_ERRORS=ON` by default |

Two dedicated build trees are used; the working tree stays clean of build output:

```sh
# coverage build
cmake -S . -B build-cov -G Ninja \
      -DQCW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping"

# sanitizer build
cmake -S . -B build-asan -G Ninja \
      -DQCW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -O1"
```

The drag-and-drop test cannot run under the offscreen plugin (`QOffscreenDrag::drag()` never
runs the nested loop that makes `QDropEvent::source()` meaningful). Its CTest entry overrides
the platform to `minimal`; every other test uses `offscreen`.

## 3. The plan: one family at a time

The audit is decomposed into families of behavior that share state and code paths. A family is
small enough to be understood in one sitting and large enough that its tests cross-check one
another.

| Family | Focus | Tests |
|---|---|---|
| Input and focus | `mousePress/Move/Release/DoubleClick`, the qApp event filter, `tabBarClicked`, hover, close button, drag threshold | `tab_folder_mouse_test` |
| Feature matrix | `MultiTabs`, `UseMRU`, `ShowFullTitle`, `ShowIcon`, `TabsMovable` and their combinations | `tab_folder_features_test` |
| Folder states and layout | minimize / maximize / restore, `originalSizes`, sibling visibility, deleting a folder while maximized | `folder_maximize_test` |
| Persistence and repair | `saveState`/`restoreState`, malformed documents, pane-id migration, MRU repair | `folder_restore_state_test`, `perspective_restore_test` |
| Drag, drop and detach | `dragEnter/dragMove/drop`, detachable tabs, floating folders | `docking_drag_drop_test` |
| Integration | perspectives, the shared folder, the dummy perspective, close protocol | `perspective_*_test`, `shared_folder_ownership_test` |

The families are audited roughly in this order: input first (everything else is reached
through it), then the feature matrix, then states and layout, then persistence, then
integration. Later families can expose bugs in earlier ones; when that happens the earlier test
file gains the regression case rather than a new file being invented.

## 4. The per-family loop

Each step names the tools used in this audit.

### Step 0 - Scope the family and create a task

Use `TaskCreate` for the family and `TaskUpdate` to mark it in progress. The task description
lists the sub-items (tests to add, paths to cover) so progress survives a context compaction.

### Step 1 - Read the contract, not just the code

Read the header documentation first - it is the contract the tests are allowed to assert:

- `QcwTabFolder.h` / `QcwTabFolderContainer.h` doc comments (`tabBarClicked`, `saveState`,
  `restoreState`, feature flags).
- `QcwDockingTypes.h` (`FolderState`, `FolderFeatures`, `TabPaneIdPropertyName`).
- `AGENTS.md` for project-wide rules (tests opt-in, singleton pattern, Qt docs location).
- `src/core/QcwSerializeUtils.h` for the session payload format.

Tools: `Read` for whole files, `Grep` for a symbol across the tree, `Glob` for file names.
When the scope is unknown, delegate a read-only search to the `Explore` subagent instead of
scanning file by file.

For Qt behavior, read the Qt 6.12 sources rather than guessing - your local checkout path is
recorded in `AGENTS.local.md`. For example
`QWidget::isAncestorOf()` is implemented in `kernel/qwidget.cpp`; the documented answer for
"is a widget an ancestor of itself" is in that file, and it is `true`.

### Step 2 - Trace the branches the family can take

Locate every conditional that the family can influence and note the state it depends on. Useful
searches: the feature flags (`MultiTabs`, `UseMRU`, `ShowFullTitle`), the folder state
(`FolderState`, `setState`), the layout records (`originalSizes`, `minimizedFolders`,
`maximizedFolder`), and the event handlers.

Compare the list of branches with what the existing tests actually reach. The coverage report
(step 6 of the previous rounds) is a good map of what has never executed; a branch that is
neither covered nor reachable from the public API is usually dead code, not untested behavior.

### Step 3 - Write a failing test that uses only the public API

Tests live in `tests/`, use the plain `main() + check()` harness, and are registered in
`tests/CMakeLists.txt` (both as a target and in the `foreach(target IN ITEMS ...)` list that
links `qcommonworkbench` and registers the CTest entry).

Rules that kept the tests honest:

- Drive the widget through public API and real events (`QTest`-free: post `QMouseEvent`s or
  call the public `minimize()`/`maximize()`), never through private members. Existing public
  observables: `isTabVisible()`, `tabBarFreeSpace()`, `tabData()`, `state()`, `features()`,
  `rootSplitter()`.
- Settle the geometry with a few `QApplication::processEvents()` calls before asserting
  sizes or visibility.
- Hold deletion candidates in a `QPointer` and pump `QEvent::DeferredDelete` before checking
  they are gone.
- For persistence, round-trip through `QXmlStreamWriter`/`QXmlStreamReader` and rebuild the
  widget from the document - do not call `saveState`/`restoreState` with a hand-made object
  graph.
- Allocate pane widgets on the heap and hand ownership to the folder exactly once. The folder
  reparents and deletes them; stack allocation or reuse across two restore cases is a
  use-after-free that ASan will report later, far from the test that caused it.

### Step 4 - Run it and classify the failure

Build one target and run it with the offscreen platform:

```sh
cd build-cov
ninja folder_maximize_test
QT_QPA_PLATFORM=offscreen QCW_TEST_AUTOCLOSE=1 ./bin/folder_maximize_test
```

A failing check is one of two things:

- **A real library bug.** The test follows the documented contract; the implementation
  violates it. Keep the test failing until the library is fixed.
- **A test-design mistake.** The test assumed a behavior the contract does not promise (for
  example, that a focus-promotion `tabBarClicked` is emitted exactly once). Fix the test to
  assert what is documented - silence while the press is handled, the real index on release -
  not an implementation detail.

Deciding between the two is step 1's contract reading, not intuition. If the contract itself
is wrong or silent, decide explicitly, document the decision in the header, and then test the
new contract.

### Step 5 - Fix the root cause, minimally

Change the smallest piece of code that restores the contract. Add a short comment only where
the reason is not obvious from the code - a subtle invariant, a Qt behavior that surprises, a
workaround. Update the header documentation when a contract becomes explicit (for example, the
`tabBarClicked` note about promotion). Do not refactor surrounding code in the same change.

### Step 6 - Verify in four passes

1. **Target test** - the new test passes, and each new check failed before the fix.
2. **Full suite in the coverage build** - `ctest --output-on-failure`, currently 30 tests.
3. **Full suite under ASan/UBSan** - rebuild `build-asan` and run `ctest`; any leak,
   use-after-free or signed overflow in the new paths fails the pass.
4. **Coverage** - confirm the repaired lines and branches are now executed (section 6).

### Step 7 - Record the result

Mark the task complete and summarize: the bug, its root cause, the fix, the tests, and the
coverage delta. Commit each family separately; the repository's commit style is a numbered
paragraph per finding that explains *why* the code was wrong, not just what changed.

## 5. Build and run commands

```sh
# configure (once)
cmake -S . -B build-cov -G Ninja -DQCW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
cmake -S . -B build-asan -G Ninja -DQCW_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -O1"

# build everything (library plus every test) and run the suite
cd build-cov && ninja && ctest --output-on-failure

# build and run one test directly (CTest supplies the environment automatically)
ninja folder_restore_state_test
QT_QPA_PLATFORM=offscreen QCW_TEST_AUTOCLOSE=1 ./bin/folder_restore_state_test

# sanitizer pass
cd build-asan && ninja && ctest --output-on-failure
```

## 6. Measuring coverage

The coverage build instruments the shared library; the test executables link it. Profiles are
written per process, so give each process its own file and clear stale data first:

```sh
cd build-cov
rm -f tests/cov-*.profraw
LLVM_PROFILE_FILE="$PWD/tests/cov-%p.profraw" ctest
llvm-profdata merge -sparse tests/cov-*.profraw -o qcw.profdata

# summary for the two docking sources
llvm-cov report ./src/libqcommonworkbench.so -instr-profile=qcw.profdata \
  | grep -E 'QcwTabFolder(Container)?\.cpp|Filename'

# per-line counts to prove specific repair paths executed
llvm-cov show ./src/libqcommonworkbench.so -instr-profile=qcw.profdata \
  -sources "$PWD/../qcw-docking/src/QcwTabFolder.cpp" -show-line-counts
```

Things that cost time and are worth remembering:

- **Report against the library, not the test binary.** The docking code lives in
  `libqcommonworkbench.so`; pointing `llvm-cov` at a test executable reports only the test's
  own translation unit and inline headers, with the library rows missing.
- **Absolute profile path.** CTest runs the tests with the working directory of the
  `add_test()` call (`build-cov/tests`), so a relative `LLVM_PROFILE_FILE` lands there, not in
  the build root, and a missing directory yields no profiles and no error.
- **Per-process names.** The default file `default.profraw` is overwritten by every test
  process; `%p` keeps one file per process.
- **Clean before every run.** Profiles left over from older binaries produce
  `N functions have mismatched data` warnings and skew the counts.
- **One `-sources` path per call.** A comma-joined list is treated as a single, nonexistent
  file name.
- **Ignore generated moc files in the report.** They are counted separately and say nothing
  about the handwritten logic.

Coverage finds *where* the untested branches are; the behavioral checks are what proves them.
Treat a repaired line with a nonzero count as evidence that the old test now walks the path,
not as proof that the new contract is right - the asserts carry that part.

## 7. Verification checklist

A family is done when all of these hold:

- [ ] Every new check failed before the fix (or before the library fix it targets).
- [ ] The fix is minimal, at the root cause, with a comment for any non-obvious reason.
- [ ] The target test passes.
- [ ] The full CTest suite passes in `build-cov` (currently 30/30).
- [ ] The full CTest suite passes in `build-asan` (ASan + UBSan clean).
- [ ] Coverage shows the repaired lines and branches executed.
- [ ] Header documentation still matches the behavior; contracts that became explicit are
      documented.
- [ ] No unrelated code was refactored or reformatted in the same change.

## 8. Lessons from this audit

1. **`QWidget::isAncestorOf()` is true for the widget itself.** An event filter that treats
   "the event widget is an ancestor-descendant of the folder" as "a deep child was pressed"
   therefore promotes the folder's own tab-bar and close-button presses, emitting
   `tabBarClicked(currentIndex)` on press and again - or not at all - on release. The guard has
   to exclude the folder (`widget != this`), not only unrelated widgets.
2. **Press/release contracts need both halves.** During focus promotion a folder may legally
   report the same index twice. Tests should assert "silent while the press is being handled,
   real index after release", not an exact emission count.
3. **Every hiding loop must exempt the current tab.** The MRU branch of
   `determineVisibleTabs()` did; the index-order branch did not, so at extreme widths a folder
   with `UseMRU` off could hide the tab whose page it was showing. A page visible without a
   visible tab is only reachable through the tab list - a real contract violation.
4. **Cross-records must be ended before a new state begins.** Minimizing a folder that is
   currently maximized has to end the maximization first (the same way deleting it does):
   otherwise every site ends up hidden and `maximizedFolder` still names a folder that is no
   longer maximized, corrupting the next restore. State transitions own the cleanup of the
   records they invalidate.
5. **Restore code is repair code.** Serialized sessions arrive damaged. Test the wrong root
   element, unknown child elements, panes that no longer exist, pane ids that moved, and MRU
   lists with duplicates or out-of-range entries - not only the happy round trip.
6. **Ownership in tests is a trap.** A restored tab adopts its pane widget; the folder deletes
   it. Stack-allocating panes, or reusing the same panes in a second restore case, turns into
   a use-after-free/double-free that ASan reports far from the test that caused it. Allocate
   fresh panes per case.
7. **`QObject` subclasses are not copyable.** When a test needs two folders, hold them in
   `std::unique_ptr` (or as separate stack objects); `QcwTabFolder a = *makeFolder(...)`
   does not compile.
8. **Settle before asserting geometry.** A few `QApplication::processEvents()` calls after
   `setSizes()`, `show()`, `maximize()` or `minimize()` are enough at fixed host sizes; exact
   `QSplitter::sizes()` round trips then compare cleanly.
9. **Offscreen cannot drag.** `QDrag::exec()` under the offscreen QPA returns without a
   nested loop, and `QDropEvent::source()` loses the originating folder. The drag/drop test
   overrides the platform to `minimal` in `tests/CMakeLists.txt`; every other test stays
   `offscreen`.
10. **Coverage is a locator, not an oracle.** Use it to find branches that never ran, then
    assert the behavior through the public API. A covered line can still be wrong; an
    uncovered line that no public API can reach is usually dead code.

## 9. Worked example: the 2026-10 docking audit

Earlier rounds of the same audit contributed the drag/drop fixes (reset the stale drop
reference and target area on `dragEnter`; accept a drop only for a detachable tab). The two
most recent families are documented below.

### Family: input and event filter (`tab_folder_mouse_test`)

- **Bug.** Pressing the tab bar or the close button of an inactive folder emitted
  `tabBarClicked(currentIndex)` from the event filter's deep-click promotion, because
  `isAncestorOf(this)` answered true for the folder itself. The release then reported the real
  index (or none), so consumers saw a spurious activation.
- **Fix.** Exclude the folder from the deep-click branch in the event filter
  (`qcw-docking/src/QcwTabFolder.cpp`, bundled as `third_party/qcw-docking`).
- **Tests.** Seven scenarios: selection on release, the close button, double click, hover and a
  short (cancelled) press, a cancelled drag detaching the tab into a floating folder, a
  non-detachable tab refusing that drag, and deep-click promotion.
- **Evidence.** Target test green; full suite green; ASan/UBSan green; `QcwTabFolder.cpp`
  regions 64.06% -> 68.55%, branches 50.39% -> 54.81%. The mouse and event-filter branches are
  all executed; the only untouched branches in this file are the cosmetic hover-state cleanup
  in `dragLeaveEvent()` and in the Enter/DragEnter arm of the event filter.

### Family: feature matrix and folder states (`tab_folder_features_test`,
`folder_maximize_test`, `folder_restore_state_test`)

- **Bug A.** With `UseMRU` off, narrowing the folder hid the current tab; the folder then
  displayed a page whose tab was reachable only through the tab list.
- **Bug B.** Minimizing a folder that was currently maximized left every site hidden and a
  stale `maximizedFolder` record behind; restoring afterwards produced the wrong visibility
  order and sizes.
- **Fixes.** Skip `currentIndex` in the non-MRU hiding loop (`determineVisibleTabs()`); end
  the maximization and restore its hidden siblings before minimizing
  (`minimizeRequested` handler).
- **Tests.**
  - Feature matrix: single-tab mode, MRU chooses which tab to hide, current tab survives
    extreme narrowing, `ShowFullTitle` width behavior.
  - States: maximize/restore sizes, restoring siblings one by one, minimizing the maximized
    folder, maximizing the only folder, deleting the maximized folder, save/restore of a
    maximized layout, save/restore of a minimized layout (including the re-emitted
    `folderMinimized` and the proportions coming back).
  - Repair: foreign root element refused, unknown elements skipped while known attributes
    apply, pane-id migration, tab dropped when its pane is gone, `recentIndexes` repair and
    the `currentIndex` fallback.
- **Evidence.** 30/30 CTest in `build-cov`; 30/30 under ASan/UBSan; `QcwTabFolder.cpp`
  regions 74.71% / branches 61.30% (lines 72.75%); `QcwTabFolderContainer.cpp` regions
  88.37% / branches 74.02% (lines 85.63%); the repaired lines were checked individually with
  `llvm-cov show -show-line-counts`.

## 10. Template for the next family

```
Family: <name>
Contract read: <headers + AGENTS.md sections>
Branches to exercise: <list of conditions/records>
Tests added/updated: <file>: <scenario names>
Bug(s) found: <one line per bug: symptom, root cause>
Fix: <file:function, why this is the root cause>
Evidence: target pass; ctest N/N; ASan N/N; coverage lines <ranges> executed
Docs updated: <header comments, if any>
```

Copy this block into the family's task description. The eventual commit message follows the
repository style instead: numbered paragraphs that explain each root cause.
