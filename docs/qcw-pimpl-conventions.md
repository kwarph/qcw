# Private implementation (pimpl) and member naming

- Status: Conventions in force — the library follows them today
- Scope: every `QCW_EXPORT` class, every class in a plugin, and every private data member
- Purpose: keep private members out of the interfaces plugins depend on, and make the naming of
  what stays visible uniform

---

## 1. The principle

An interface that serves the outside world should expose as few private members as possible —
that means both member functions and data members. Everything else in this document follows from
that one rule.

## 2. When a class gets a pimpl

There is no quantitative threshold; the principle above decides.

Signals in favour:

- The class is exported with `QCW_EXPORT` and plugins inherit from it or embed it. Private
  members are part of the object layout, so adding or removing one changes the size of every
  plugin's subclass and every stack-allocated instance. A pimpl keeps internal changes from
  forcing all plugins to be rebuilt.
- There are many private members, or declaring them would require includes the public header
  must not carry (`QHash`, `QList<QMetaObject::Connection>`, member objects of other classes).
  The pimpl moves those includes into the `.cpp`.
- The destructor has a real teardown to perform (disconnecting, deleting children).

Signals against:

- Two or three data members: keep them as ordinary `private:` members of the host class
  (§5.2).
- A pure data carrier: use a `struct` (§5.1).
- A helper used only inside one `.cpp`: put it in an anonymous namespace there; it needs neither
  a pimpl nor an export macro.

## 3. Structure

- The public header only forward-declares `class MyWidgetPrivate;`. The definition of
  `MyWidgetPrivate` lives in `MyWidget.cpp`, so the header knows nothing about its contents.
- The `impl` pointer sits in a `private:` section, never a `public:` one.
- `friend class MyWidgetPrivate;` appears only when the private really needs the host's
  protected or private members; otherwise it is left out. In this library that is the case for
  exactly three classes: `QcwWindow`, `QcwWorkbench` and `QcwView`.
- The host class declares **and defines** its destructor (`delete impl;`). Relying on the
  compiler-generated destructor leaks the private, and declaring the destructor in the header
  without defining it fails to compile, because `MyWidgetPrivate` is an incomplete type there.
- `impl` is built in the constructor's member-init list and is never null afterwards, so call
  sites do not null-check it.
- `MyWidgetPrivate` is not a `QObject` and carries no `Q_OBJECT`. Signals, slots and the
  metaobject stay on the host class; the private holds data members and plain member functions
  and reaches the host through `owner`.

## 4. Naming

Assume the host class is `MyWidget`:

```cpp
// MyWidget.h

class MyWidgetPrivate;

class MyWidget : public QWidget
{
public:
    explicit MyWidget(QWidget* parent = nullptr);
    ~MyWidget() override;

    ...

private:
    // friend class MyWidgetPrivate;   // only when the private needs the host's protected members
    MyWidgetPrivate* impl;             // the pimpl pointer is always named: impl
};

// MyWidget.cpp

class MyWidgetPrivate
{
public:
    explicit MyWidgetPrivate(MyWidget* interface);   // the host argument is named: interface

    ...

public:
    MyWidget* owner;             // the back-pointer to the host is named: owner
    QString imagesPath;          // every data member, public or private, is lowerCamelCase
    ...
};

MyWidget::MyWidget(QWidget* parent) :
    QWidget { parent }, impl { new MyWidgetPrivate { this } }
{
    ...
}

MyWidget::~MyWidget()
{
    delete impl;
    ...
}

MyWidgetPrivate::MyWidgetPrivate(MyWidget* interface) :
    owner { interface }
{
    ...
}
```

## 5. Naming without a pimpl

### 5.1 Data structures
A type with no or almost no member functions whose job is to carry data names every member
lowerCamelCase:

```cpp
struct SessionData
{
    QString docRoot;
    QString cookie;
};
```

### 5.2 Classes with a few private data members
Private data members are `m` followed by UpperCamelCase, with no underscore:

```cpp
class MyService : public QObject
{
public:
    explicit MyService(QObject* parent = nullptr);
    ~MyService() override;

    QString dataPath() const
    {
        return mDataPath;
    }

    ...

private:
    QString mDataPath;
    QString mTopic;
};
```

## 6. The exception: Qt's destruction order

`delete impl;` does not end the destruction. `~QWidget` then tears the child widgets down, and
that dispatches events to other objects, which may in turn call an accessor on the instance being
destroyed — at a moment when `impl` is already freed. So:

- If instances of the class can be reached during a child's teardown (as `QcwWindow` can, when a
  `QcwTabFolder` releases its panes), the destructor sets `impl = nullptr;` right after
  `delete impl;`, and the accessors that such a call can reach answer with an empty value instead
  of dereferencing the freed private.
- This does not contradict the rule that the process-wide singletons are never null-checked. That
  rule is about `Q_GLOBAL_STATIC` globals, which are valid for the whole process; here the window
  is the short span of one destructor.

## 7. Summary

| Situation | Member style |
|---|---|
| Host of a pimpl | `impl` in a `private:` section, pointing at `XxxPrivate` defined in the `.cpp` |
| Inside `XxxPrivate` | lowerCamelCase data members; the back-pointer to the host is `owner` |
| Private member of a non-pimpl class | `m` + UpperCamelCase, no underscore |
| Pure data struct | lowerCamelCase |
