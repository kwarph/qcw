# QCW Topic Service and Event Unification Design

- Status: Implemented — see §9 for the current implementation notes
- Scope: `workbench` module, inter-plugin communication, state and event dispatching
- Goal: Replace `QcwSelectionService` and `QcwEventBus` with a unified, stateful, and scoped `QcwTopicService`

---

## 1. Background and Goals

QCW previously used three separate mechanisms for inter-plugin communication:
1. **`QcwServiceRegistry`** for request-response actions (verbs).
2. **`QcwEventBus`** for fire-and-forget events (facts).
3. **`QcwSelectionService`** for tracking the current user focus (selection).

While functional, this triad had limitations:
- `QcwEventBus` did not cache state; late subscribers missed the current context.
- `QcwSelectionService` was too narrow, forcing a single selection model (`QcwSelection`) and restricting a view's ability to publish multiple types of context data (e.g., cursor position, current file, active data model).
- Developers were forced to choose between three APIs for similar pub/sub patterns.

### Design Goals
1. **Unify** state sharing and event broadcasting into a single `QcwTopicService`.
2. **Support complex payloads** via `QVariantMap`, capable of carrying primitives, lists, and interface pointers (`QObject*`).
3. **Provide state caching** with immediate replay for late subscribers.
4. **Enforce scoped namespaces** to distinguish between application-level and window-level topics.
5. **Ensure type-safe contracts** across plugins without requiring binary symbol linking or shared plugin headers.

### Non-Goals
- Replacing `QcwServiceRegistry`. Services remain the correct pattern for "command-like" interactions where a caller expects a return value.
- Implementing a full ORM or data-binding model. Topics carry context and state, not heavy database models.

---

## 2. Core Concepts

| Concept | Description |
|---|---|
| **Topic ID** | A namespaced string identifier (e.g., `"win/selection"`, `"app/themeChanged"`). |
| **Payload** | A `QVariantMap` carrying the topic's data. |
| **Stateful Topic** | A topic that caches its last published payload. New subscribers immediately receive the cached value. |
| **Stateless Topic** | A pure event. It is not cached; subscribers only receive future publications. |
| **Scope** | The level at which a topic is published and consumed: `Window` (per `QcwWindow`) or `Application` (per `QcwWorkbench`). |

---

## 3. Architecture and Scope

### 3.1 Scoped Services
`QcwTopicService` is instantiated at two levels:
- `QcwWorkbench::topicService()` (Application scope)
- `QcwWindow::topicService()` (Window scope)

Window-level services are isolated. Publishing `"win/selection"` in Window A does not affect Window B. 

### 3.2 Namespace Naming Convention
To prevent confusion when developers use the Registry or debug the system, the following prefixes are **strongly recommended** (and can be warned upon during registration):
- **`app/`** prefix: Published exclusively on the Application scope. (e.g., `"app/themeChanged"`)
- **`win/`** prefix: Published exclusively on the Window scope. (e.g., `"win/selection"`, `"win/editor/saved"`)

Overlapping topic names across scopes are technically allowed (they are isolated), but the prefix convention makes the ecosystem predictable.

---

## 4. Contract Design (The Interface Approach)

To avoid "guessing" topic IDs and payload keys, plugins must expose their communication contracts through **interfaces**. A provider plugin defines an interface (in a standalone header library) and registers it via `QcwServiceRegistry`.

### 4.1 Interface Example
```cpp
// Provided by the TextEditor plugin (in a shared contract library)
class IEditorContextSource
{
public:
    virtual ~IEditorContextSource() = default;

    // 1. Expose Topic IDs
    virtual QString textSelectionTopic() const = 0;
    virtual QString documentSavedTopic() const = 0;

    // 2. Define the Payload structure
    struct TextSelectionPayload
    {
        QString path;
        int startLine;
        int startColumn;
        QString selectedText;
    };

    // 3. Provide strong-typed serialization helpers
    static QVariantMap toMap(const TextSelectionPayload& payload);
    static TextSelectionPayload fromMap(const QVariantMap& map);
};

Q_DECLARE_INTERFACE(IEditorContextSource, "org.qcw.demo.IEditorContextSource")
```

### 4.2 Consumer Usage
```cpp
auto* source = serviceRegistry->service<IEditorContextSource>();
if (!source) return;

// In a view: the window scope is followed by QcwView, so no window pointer is cached.
subscribeWindowTopic(source->textSelectionTopic(), [](const QVariantMap& map) {
    auto payload = IEditorContextSource::fromMap(map);
    qDebug() << "Selected text in" << payload.path << "at line" << payload.startLine;
});

// Anywhere else, with a service at hand:
service->subscribe(source->textSelectionTopic(), this, [](const QVariantMap& map) { /* ... */ });
```
This ensures compile-time safety for payload keys and runtime discovery of topic IDs, without linking to the publisher's plugin binary.

---

## 5. API Design

### 5.1 `QcwTopicService`
```cpp
class QCW_EXPORT QcwTopicService : public QObject
{
    Q_OBJECT
public:
    // Accessors
    static QcwTopicService* forWindow(QcwWindow* window);
    static QcwTopicService* forApplication();

    // Publishing
    void publish(const QString& topicId, const QVariantMap& payload = {});
    void clear(const QString& topicId);

    // Querying
    QVariantMap current(const QString& topicId) const;

    // Subscribing
    QMetaObject::Connection subscribe(const QString& topicId, QObject* receiver,
                                      std::function<void(const QVariantMap&)> handler);
    void unsubscribe(const QString& topicId, QObject* receiver);
};
```
- The caller chooses the scope by choosing the service; `publish()` does not route on the id and
  does not validate the prefix.
- `subscribe()` immediately invokes the handler with the cached payload (if it exists).
- Handlers are automatically disconnected when the `receiver` QObject is destroyed.

> **Implementation notes.** The service currently caches every topic; `QcwTopicInfo::isStateful`
> is catalog metadata for `QcwTopicRegistry` and is not consulted yet. Delivery is synchronous in
> the calling thread and the service is not thread-safe, so publications from worker threads must
> be queued with `QMetaObject::invokeMethod()`. `clear()` drops the cached payload without
> notifying subscribers, and `QcwWindow` does not clear topics when the current view changes: a
> view that publishes its subject clears its own topic when it hides.

### 5.2 `QcwTopicRegistry`
```cpp
struct QCW_EXPORT QcwTopicInfo
{
    QString id;
    QString description;
    QString publisher;
    bool isStateful;
    QList<QPair<QString, QString>> payloadKeys;
};

class QCW_EXPORT QcwTopicRegistry
{
public:
    static QcwTopicRegistry* instance();

    bool registerTopic(const QcwTopicInfo& info);
    QList<QcwTopicInfo> allTopics() const;
    QList<QcwTopicInfo> topicsByPublisher(const QString& publisher) const;
};
```
- Validates prefix rules (`app/` or `win/`), warning on other ids without refusing them.
- Allows IDE tooling and debug panels to display available topics and their expected payloads.
- Is optional: `QcwTopicService` neither requires registration nor consults the registry.

### 5.3 `QcwView` window topics
```cpp
class QCW_EXPORT QcwView : public QWidget, public QcwPersistable
{
public:
    void subscribeWindowTopic(const QString& topicId,
                              std::function<void(const QVariantMap&)> handler);
    void unsubscribeWindowTopic(const QString& topicId);
    QVariantMap currentWindowTopic(const QString& topicId) const;
};
```
- The view keeps the handler table; the base class does the subscribing on `QEvent::ParentChange`
  and on show, so a view never caches a `QcwWindow*` or a `QcwTopicService*`.
- Contract: on (re-)attach the handler runs once with the topic's current payload (an empty map
  when it was cleared or never published), on a detach from a living window once with the empty
  map, and nothing further once the window has announced its destruction.
- Publishing is unaffected: a view reads `owningWindow()` where it needs it and calls
  `topicService()->publish()` or `clear()` there.

---

## 6. Migration Guide

### 6.1 Replacing `QcwSelectionService`
- **Remove:** `QcwSelectionService`, `QcwSelection`, and `QcwView::selection()`.
- **Implement:**
  - `QcwWindow` automatically monitors `currentViewChanged`.
  - `TextEditor` and `ProjectView` publish their selections directly via `topicService()->publish("win/selection", ...)`.
  - `OutlineView` follows `"win/selection"` through `QcwView::subscribeWindowTopic(...)`; the
    view holds no window pointer and the base class keeps the subscription on the window the view
    belongs to.

### 6.2 Replacing `QcwEventBus`
- **Remove:** `QcwEventBus`.
- **Implement:**
  - Replace `eventBus->publish("...")` with `topicService()->publish("win/...")`.
  - Mark event topics as `isStateful = false` in the Registry so they are not cached for late subscribers.

### 6.3 Example: TextEditor and OutlineView Migration
**Before (SelectionService):**
```cpp
// OutlineView
connect(service, &QcwSelectionService::selectionChanged, this, [this, service](const QcwSelection& selection) { ... });
```
**After (TopicService):**
```cpp
// TextEditor (Publisher)
void TextEditor::onCursorMoved() {
    if (!owningWindow()) return;
    owningWindow()->topicService()->publish("win/selection", {
        {"path", mFilePath},
        {"startLine", cursor.blockNumber()},
        {"startColumn", cursor.positionInBlock()}
    });
}

// OutlineView (Subscriber) — the window is followed by the base class, so the view holds no
// window pointer and no topic service of its own.
OutlineView::OutlineView(QWidget* parent) : QcwView(parent) {
    subscribeWindowTopic("win/selection", [this](const QVariantMap& payload) {
        QString path = payload["path"].toString();
        // update outline
    });

    // The publish side stays manual: owningWindow() is read where it is needed, never cached.
}
```

---

## 7. Implementation Phases

1. **PR 1: Foundation**
   - Implement `QcwTopicService` and `QcwTopicRegistry`.
   - Add unit tests for caching, scoping, and prefix validation.

2. **PR 2: Migration**
   - Remove `QcwSelection`, `QcwSelectionService`, `QcwEventBus`, and `QcwView::selection()`.
   - Update all built-in views (TextEditor, ProjectView, OutlineView, LogView, Notes) to use `QcwTopicService`.
   - Update the demo Workbench.

3. **PR 3: Cleanup and Documentation**
   - Remove obsolete CMake targets.
   - Update `README.md` and developer guides to reflect the new Topic API.

---

## 8. Design Decision Summary

| # | Decision |
|---|---|
| 1 | `QcwSelectionService` and `QcwEventBus` are deprecated and replaced by `QcwTopicService`. |
| 2 | `QcwView::selection()` is removed. Views publish directly to the Window's Topic Service. |
| 3 | Topic IDs use `app/` and `win/` prefixes to enforce scope separation. |
| 4 | Stateful topics cache their last payload; `subscribe()` triggers an immediate replay for late subscribers. |
| 5 | Cross-plugin contracts (Topic IDs and Payload shapes) are exposed via shared interface headers and registered in `QcwServiceRegistry`. |
| 6 | `QcwTopicRegistry` tracks available topics, enabling dynamic discovery and IDE debug tooling. |
| 7 | A view publishes to `"win/..."` using `owningWindow()->topicService()`. |
| 8 | Namespace overlaps across different scopes are technically allowed but discouraged via the prefix convention. |

---

## 9. Implementation Status

`QcwTopicService` and `QcwTopicRegistry` live in `src/workbench/`. `QcwWindow::topicService()`
and `QcwWorkbench::topicService()` own the per-window and application-scoped instances;
`QcwTopicService::forWindow()` and `QcwTopicService::forApplication()` are the static accessors.

The migration is complete in the library and the demo:

- `QcwSelection`, `QcwSelectionService`, `QcwEventBus` and `QcwView::selection()` are removed.
- `TextEditor` publishes `win/selection/editor` and the editor announcement topics; `ProjectView`
  publishes `win/selection/project` and follows the announcements through
  `QcwView::subscribeWindowTopic()`; `OutlineView` follows the editor selection topic the same
  way. Each publisher publishes only while it is the window's current
  view, and clears its subject topic when it hides.
- `tests/topic_service_test.cpp` covers registry insertion, caching and replay, explicit
  unsubscribe, and receiver destruction. The removed `selection_service_test` and
  `event_bus_test` targets are gone from `tests/CMakeLists.txt` as well.
- A view that consumes window topics calls `QcwView::subscribeWindowTopic()`, which drops and
  re-makes the subscriptions as the view moves between windows and stops delivering when its
  window is destroyed; `unsubscribe(topicId, receiver)` remains the way to stop one earlier. The
  value returned by `subscribe()` is only the receiver-destroyed cleanup handle; `disconnect()` on
  it does not stop the handler.

Known follow-ups:

- `QcwTopicInfo::isStateful` is not wired into `QcwTopicService`, so every topic is cached and an
  event-style topic replays its last payload to a late subscriber.
- The service has no thread-affinity guard and no queued delivery.
