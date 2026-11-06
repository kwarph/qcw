// QcwView::subscribeWindowTopic() is the framework-side answer to the pattern that used to crash
// a view at shutdown: a view that caches its QcwWindow* and (un)subscribes from its topic service
// in its own ParentChange handler reaches the window's private after ~QcwWindow has deleted it.
// The base class holds the handlers instead and does the subscribing on the view's behalf, so the
// window pointer never leaves the library. This test drives each transition — attach, publish,
// re-subscribe, move between windows, detach and the window's death — and checks what the handler
// is handed.

#include "QcwTabFolder.h"
#include "QcwTopicService.h"
#include "QcwView.h"
#include "QcwWindow.h"

#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <cstdio>

namespace
{

constexpr QLatin1String SelectionTopic { "win/test/selection" };
constexpr QLatin1String OtherTopic { "win/test/other" };
constexpr QLatin1String LateTopic { "win/test/late" };

int gFailures = 0;

void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

QVariantMap selectionOf(const QString& path)
{
    return { { QStringLiteral("path"), path } };
}

QString pathOf(const QVariantMap& payload)
{
    return payload.value(QStringLiteral("path")).toString();
}

class TestWindow : public QcwWindow
{
public:
    QcwTabFolder* createSharedFolder() override
    {
        return new QcwTabFolder { this };
    }
};

/// Records what the framework hands it, one counter per followed topic.
class RecordingView : public QcwView
{
public:
    explicit RecordingView(QWidget* parent = nullptr) : QcwView { parent }
    {
    }

    int selectionCalls { 0 };
    int otherCalls { 0 };
    QVariantMap lastSelection;
    QVariantMap lastOther;

    void reset()
    {
        selectionCalls = 0;
        otherCalls = 0;
        lastSelection = {};
        lastOther = {};
    }

    void followSelection()
    {
        subscribeWindowTopic(QString(SelectionTopic), [this](const QVariantMap& payload)
        {
            ++selectionCalls;
            lastSelection = payload;
        });
    }

    void follow()
    {
        followSelection();

        subscribeWindowTopic(QString(OtherTopic), [this](const QVariantMap& payload)
        {
            ++otherCalls;
            lastOther = payload;
        });
    }
};

/// Subscribes another topic from inside a detach delivery — the one moment a handler runs while
/// the framework is between two windows.
class RearmingView : public QcwView
{
public:
    explicit RearmingView(QWidget* parent = nullptr) : QcwView { parent }
    {
    }

    int selectionCalls { 0 };
    int lateCalls { 0 };

    void follow()
    {
        subscribeWindowTopic(QString(SelectionTopic), [this](const QVariantMap& payload)
        {
            ++selectionCalls;

            if (payload.isEmpty() && !mRearmed)
            {
                mRearmed = true;
                subscribeWindowTopic(QString(LateTopic), [this](const QVariantMap&)
                {
                    ++lateCalls;
                });
            }
        });
    }

private:
    bool mRearmed { false };
};

/// Reparents itself while a transition is being delivered, so the pass that is running is on its
/// way to a window the view no longer belongs to. The nested move has to win.
class FleeingView : public QcwView
{
public:
    explicit FleeingView(QWidget* parent = nullptr) : QcwView { parent }
    {
    }

    int selectionCalls { 0 };
    QVariantMap lastSelection;
    QWidget* fleeTo { nullptr };

    void follow()
    {
        subscribeWindowTopic(QString(SelectionTopic), [this](const QVariantMap& payload)
        {
            ++selectionCalls;
            lastSelection = payload;

            if (payload.isEmpty() && fleeTo && !mFled)
            {
                mFled = true;
                setParent(fleeTo);
            }
        });
    }

private:
    bool mFled { false };
};

TestWindow* makeWindow()
{
    auto* window = new TestWindow;
    window->setupUi();
    window->resize(800, 500);
    window->show();
    return window;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto* first = makeWindow();
    auto* second = makeWindow();

    {
        QPointer<RecordingView> view = new RecordingView;
        view->follow();

        // Subscribing while the view belongs to no window only registers the handler: the attach
        // is what delivers, and a topic that was never published still has to be reported empty.
        check(view->selectionCalls == 0 && view->otherCalls == 0,
              "a detached subscription delivers nothing");
        check(view->currentWindowTopic(QString(SelectionTopic)).isEmpty(),
              "reading a topic of no window is empty");

        first->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("first.txt")));
        check(view->selectionCalls == 0, "a publication before the attach reaches nobody");

        first->sharedFolder()->addTab(view, QStringLiteral("Recording"));
        QCoreApplication::processEvents();

        check(view->selectionCalls == 1, "attaching delivers the current payload exactly once");
        check(pathOf(view->lastSelection) == QStringLiteral("first.txt"),
              "and it is the window's cached payload");
        check(view->otherCalls == 1 && view->lastOther.isEmpty(),
              "a topic with no cache is reported as empty on the attach");
        check(pathOf(view->currentWindowTopic(QString(SelectionTopic))) == QStringLiteral("first.txt"),
              "currentWindowTopic reads the owning window");

        first->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("second.txt")));
        check(view->selectionCalls == 2 && pathOf(view->lastSelection) == QStringLiteral("second.txt"),
              "a publication reaches the handler");

        // Re-subscribing a topic replaces its subscription instead of adding a second one, and
        // reports the current state while doing so.
        view->follow();

        check(view->selectionCalls == 3 && pathOf(view->lastSelection) == QStringLiteral("second.txt"),
              "re-subscribing replays the cached payload once");
        check(view->otherCalls == 2 && view->lastOther.isEmpty(),
              "re-subscribing an uncached topic reports it empty");

        first->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("third.txt")));
        check(view->selectionCalls == 4, "one publication after a re-subscribe is one callback, not two");

        view->unsubscribeWindowTopic(QString(SelectionTopic));
        first->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("fourth.txt")));
        check(view->selectionCalls == 4, "unsubscribeWindowTopic() stops delivery");

        view->followSelection();
        check(view->selectionCalls == 5 && pathOf(view->lastSelection) == QStringLiteral("fourth.txt"),
              "subscribing again reads the state the publications left");

        // Moving the pane to another window: it leaves the first with the empty state it now has,
        // and the second supplies its own current payload for every followed topic.
        second->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("other window.txt")));

        int indexInAdopter = -1;
        first->sharedFolder()->takeTab(second->sharedFolder(), 0, indexInAdopter);
        QCoreApplication::processEvents();

        check(indexInAdopter >= 0, "the pane moved to the second window");
        check(view->selectionCalls == 7, "leaving and arriving each deliver once");
        check(pathOf(view->lastSelection) == QStringLiteral("other window.txt"),
              "and the arrival carries the new window's payload");
        check(view->otherCalls == 4 && view->lastOther.isEmpty(),
              "every followed topic is reported on the move");

        view->reset();
        second->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("fifth.txt")));
        check(view->selectionCalls == 1 && pathOf(view->lastSelection) == QStringLiteral("fifth.txt"),
              "the second window now feeds the handler");

        view->reset();
        first->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("stale.txt")));
        check(view->selectionCalls == 0, "the window the view left no longer reaches it");
        check(view->currentWindowTopic(QString(SelectionTopic))
                  == second->topicService()->current(QString(SelectionTopic)),
              "currentWindowTopic followed the move");

        // Taking the tab out of the folder leaves the pane with no window at all, which is the
        // detach the contract promises; the pane itself is deleted, as the tab pane manager owns
        // the last reference.
        view->reset();
        second->sharedFolder()->removeTab(0);
        QCoreApplication::processEvents();

        check(view->selectionCalls == 1 && view->lastSelection.isEmpty(),
              "losing the window reports the empty state once");
        check(view->otherCalls == 1 && view->lastOther.isEmpty(),
              "for every topic the view still follows");
        check(view->currentWindowTopic(QString(SelectionTopic)).isEmpty(),
              "and the topic reads empty afterwards");

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        check(view.isNull(), "the released pane was deleted");

        second->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("after the view")));
        check(true, "publishing after the subscriber died is harmless");
    }

    // The window's own death: ~QcwWindow deletes the private, and only then does QWidget tear the
    // folders down, whose destructors release the panes with events of their own. The view must
    // hear nothing more and must not reach back into the freed private — what AddressSanitizer
    // reports here when it does.
    {
        auto* dying = makeWindow();
        QPointer<RecordingView> view = new RecordingView;
        view->follow();
        dying->sharedFolder()->addTab(view, QStringLiteral("Recording"));
        QCoreApplication::processEvents();
        check(view->selectionCalls == 1 && view->otherCalls == 1,
              "the view followed its window before the teardown");

        view->reset();
        delete dying;

        check(view->selectionCalls == 0 && view->otherCalls == 0,
              "the teardown delivers nothing to a view of the dying window");
        check(!view.isNull(), "the view itself is still alive until the manager deletes it");

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        check(view.isNull(), "and the released pane went with the window");
    }

    // A handler that subscribes while it is being told about a detach must land on the window the
    // view is moving to, never on the one it is leaving: the pair of fields is reassigned before
    // anything is delivered.
    {
        QPointer<RearmingView> view = new RearmingView;
        view->follow();

        first->topicService()->publish(QString(LateTopic), selectionOf(QStringLiteral("not for the view")));
        view->setParent(first->sharedFolder());
        QCoreApplication::processEvents();

        check(view->selectionCalls == 1 && view->lateCalls == 0,
              "the view attached to the first window and followed one topic");

        view->setParent(second->sharedFolder());
        QCoreApplication::processEvents();

        // The detach reports Selection once with the empty map, and the handler it calls there
        // subscribes Late — on the window being moved to, which reports that one too before the
        // attach loop replays Selection and reports Late again. Two reports of the same empty
        // state is what the contract allows: a handler applies state, it does not count events.
        check(view->selectionCalls == 3 && view->lateCalls == 2,
              "a re-subscribe from the detach delivery lands on the window being moved to");

        view->lateCalls = 0;
        first->topicService()->publish(QString(LateTopic), selectionOf(QStringLiteral("stale again")));
        check(view->lateCalls == 0, "the abandoned service keeps no subscription of the view");

        second->topicService()->publish(QString(LateTopic), selectionOf(QStringLiteral("late.txt")));
        check(view->lateCalls == 1, "and the new one delivers once");

        delete view;
    }

    // The same thing with a handler that moves the view: the transition that was running finishes
    // and the framework then does the one the move asked for, so the subscriptions end up on the
    // window the view really is in.
    {
        auto* third = makeWindow();

        QPointer<FleeingView> view = new FleeingView;
        view->follow();
        view->setParent(first->sharedFolder());
        QCoreApplication::processEvents();

        check(view->selectionCalls == 1, "the view attached to the first window");

        // The detach delivery of the next move is what flees, so the target is the third window
        // while the running transition is still on its way to the second.
        view->fleeTo = third->sharedFolder();
        view->setParent(second->sharedFolder());
        QCoreApplication::processEvents();

        check(view->selectionCalls > 1, "the move and the flight were both reported");

        view->selectionCalls = 0;
        third->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("fled.txt")));
        check(view->selectionCalls == 1 && pathOf(view->lastSelection) == QStringLiteral("fled.txt"),
              "the window the view fled to is the one that reaches it");

        second->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("left behind")));
        first->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("gone")));
        check(view->selectionCalls == 1, "neither of the windows it passed through keeps a subscription");
        check(view->currentWindowTopic(QString(SelectionTopic))
                  == third->topicService()->current(QString(SelectionTopic)),
              "and the topic reads the window it ended up in");

        delete view;
        delete third;
    }

    // A floating folder is top-level, so its views reach their window through the folder's
    // WindowId and the list of top-level windows — which is read again during that window's
    // teardown, when the id accessor is the one thing that must not touch the freed private.
    {
        auto* window = makeWindow();
        // A floating folder names its window by id, so the id has to be set for the fallback in
        // QcwView::owningWindow() to have anything to match.
        window->setId(QStringLiteral("view-window-topic-floating"));

        auto* floating = new QcwTabFolder;
        floating->setFloating(true);
        floating->setProperty("WindowId", window->id());

        QPointer<RecordingView> view = new RecordingView;
        view->follow();
        floating->addTab(view, QStringLiteral("Recording"));
        floating->show();
        QCoreApplication::processEvents();

        check(view->selectionCalls == 1, "a view in a floating folder follows its window");

        view->reset();
        window->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("floating.txt")));
        check(view->selectionCalls == 1 && pathOf(view->lastSelection) == QStringLiteral("floating.txt"),
              "and the publication reaches it through the folder id");

        view->reset();
        delete window;

        check(view->selectionCalls == 0 && view->otherCalls == 0,
              "the folder outlives its window without reaching back into it");

        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }

    // A surviving window still serves views created after another window died.
    {
        second->topicService()->publish(QString(SelectionTopic), selectionOf(QStringLiteral("sixth.txt")));

        auto* view = new RecordingView;
        view->follow();
        second->sharedFolder()->addTab(view, QStringLiteral("Recording"));

        check(view->selectionCalls == 1 && pathOf(view->lastSelection) == QStringLiteral("sixth.txt"),
              "a view created after a window died still follows the survivor");
    }

    delete first;
    delete second;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    std::fprintf(stderr, "failures=%d\n", gFailures);

    if (qEnvironmentVariableIsSet("QCW_TEST_AUTOCLOSE"))
    {
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    }

    app.exec();

    return gFailures == 0 ? 0 : 1;
}
