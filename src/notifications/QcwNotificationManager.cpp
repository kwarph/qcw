// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwNotificationManager.h"
#include "QcwNotificationBox.h"
#include "QcwSettings.h"

#include <QDebug>
#include <QGuiApplication>
#include <QGlobalStatic>
#include <QScreen>
#include <QWindow>

#include <utility>

#if defined(QCW_HAS_XCB) && QT_CONFIG(xcb)
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>

#include <cstdlib>
#endif

QT_BEGIN_NAMESPACE

namespace
{

constexpr QLatin1StringView kEnabledKey("notification/enabled");
constexpr QLatin1StringView kCornerKey("notification/corner");
constexpr QLatin1StringView kMaxVisibleBoxesKey("notification/maxVisibleBoxes");
constexpr QLatin1StringView kTimeoutKey("notification/timeout");

constexpr int StackMargin = 16;  // gap between the stack and the screen edge
constexpr int StackSpacing = 10; // gap between two cards

#if defined(QCW_HAS_XCB) && QT_CONFIG(xcb)

QRect netWorkArea()
{
    auto* x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11 || !x11->connection())
    {
        return {};
    }

    xcb_connection_t* connection = x11->connection();
    xcb_screen_t* xcbScreen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    if (!xcbScreen)
    {
        return {};
    }

    static const xcb_atom_t workAreaAtom = [connection]()
    {
        static constexpr char name[] = "_NET_WORKAREA";
        xcb_intern_atom_reply_t* reply =
            xcb_intern_atom_reply(connection, xcb_intern_atom(connection, false, sizeof(name) - 1, name), nullptr);
        if (!reply)
        {
            return xcb_atom_t { XCB_ATOM_NONE };
        }

        const xcb_atom_t atom = reply->atom;
        std::free(reply);
        return atom;
    }();

    if (workAreaAtom == XCB_ATOM_NONE)
    {
        return {};
    }

    xcb_get_property_reply_t* reply = xcb_get_property_reply(
        connection, xcb_get_property(connection, false, xcbScreen->root, workAreaAtom, XCB_ATOM_CARDINAL, 0, 4),
        nullptr);
    if (!reply)
    {
        return {};
    }

    QRect workArea;
    if (reply->type == XCB_ATOM_CARDINAL && reply->format == 32 && reply->value_len >= 4)
    {
        const auto* values = static_cast<const uint32_t*>(xcb_get_property_value(reply));
        workArea = QRect(int(values[0]), int(values[1]), int(values[2]), int(values[3]));
    }
    std::free(reply);
    return workArea;
}

// QXcbScreen::availableGeometry() returns the whole screen on multi-monitor X11 and
// ignores _NET_WORKAREA, which would place the stack under a panel.
QRect availableArea(QScreen* screen)
{
    QRect area = screen->availableGeometry();
    const QRect workArea = netWorkArea();
    if (!workArea.isValid())
    {
        return area;
    }

    // _NET_WORKAREA is in native pixels; QScreen works in device-independent pixels.
    // Mixed per-screen scales do not map linearly, so only correct uniform layouts.
    const qreal factor = screen->devicePixelRatio();
    for (auto* sibling : screen->virtualSiblings())
    {
        if (!qFuzzyCompare(sibling->devicePixelRatio(), factor))
        {
            return area;
        }
    }

    const QRect logicalWorkArea(QPoint(qRound(workArea.x() / factor), qRound(workArea.y() / factor)),
                                QSize(qRound(workArea.width() / factor), qRound(workArea.height() / factor)));
    return area.intersected(logicalWorkArea);
}

#else

QRect availableArea(QScreen* screen)
{
    return screen->availableGeometry();
}

#endif

bool isValidCorner(Qt::Corner corner)
{
    switch (corner)
    {
    case Qt::TopLeftCorner:
    case Qt::TopRightCorner:
    case Qt::BottomLeftCorner:
    case Qt::BottomRightCorner:
        return true;
    }

    return false;
}

} // namespace

class QcwNotificationManagerPrivate
{
public:
    explicit QcwNotificationManagerPrivate(QcwNotificationManager* interface);

    void ensureApplicationHooks();
    void watchScreen(QScreen* screen);
    void positionBoxes();
    QScreen* targetScreen() const;
    void removeBox(QcwNotificationBox* box);

public:
    QcwNotificationManager* owner;
    QcwNotificationBoxCreator boxCreator { &QcwNotificationBox::createDefaultNotificationBox };
    QList<QcwNotificationBox*> boxes; // oldest first; the last one is the newest.
    QSet<QScreen*> watchedScreens;
    QPointer<QScreen> lastScreen;
    Qt::Corner corner { QcwNotificationManager::DefaultPosition };
    int maxVisibleBoxes { QcwNotificationManager::DefaultVisibleBoxes };
    int defaultTimeout { QcwNotificationManager::DefaultTimeout };
    bool notificationEnabled { true };
    bool hooksInstalled { false };
};

// -----------------------------------------------------------------------------

Q_GLOBAL_STATIC(QcwNotificationManager, notificationManagerInstance);

QcwNotificationManager::QcwNotificationManager() :
    impl { new QcwNotificationManagerPrivate { this } }
{
    auto settings = QcwSettings::get();

    impl->notificationEnabled = settings.value(kEnabledKey, true).toBool();

    const auto storedCorner =
        static_cast<Qt::Corner>(settings.value(kCornerKey, static_cast<int>(DefaultPosition)).toInt());
    if (isValidCorner(storedCorner))
    {
        impl->corner = storedCorner;
    }
    else
    {
        // Repair the stored value so the next start does not have to correct it again.
        settings.setValue(kCornerKey, static_cast<int>(impl->corner));
    }

    // An unusable limit would leave the stack without room for a card, so fall back to the
    // default and write it back instead of repairing it on every start.
    impl->maxVisibleBoxes = settings.value(kMaxVisibleBoxesKey, DefaultVisibleBoxes).toInt();
    if (impl->maxVisibleBoxes <= 0)
    {
        impl->maxVisibleBoxes = DefaultVisibleBoxes;
        settings.setValue(kMaxVisibleBoxesKey, impl->maxVisibleBoxes);
    }

    impl->defaultTimeout = settings.value(kTimeoutKey, DefaultTimeout).toInt();
    if (impl->defaultTimeout < 0)
    {
        impl->defaultTimeout = DefaultTimeout;
        settings.setValue(kTimeoutKey, impl->defaultTimeout);
    }

    impl->ensureApplicationHooks();
}

QcwNotificationManager::~QcwNotificationManager()
{
    delete impl;
}

QcwNotificationManager* QcwNotificationManager::instance()
{
    return notificationManagerInstance;
}

void QcwNotificationManager::setNotificationBoxCreator(QcwNotificationBoxCreator creator)
{
    impl->boxCreator = creator ? creator : &QcwNotificationBox::createDefaultNotificationBox;
}

void QcwNotificationManager::post(const QcwNotification& notification)
{
    if (!impl->notificationEnabled)
    {
        qDebug() << "QcwNotificationManager::post(): notifications are disabled, ignoring";
        return;
    }

    impl->ensureApplicationHooks();

    if (!notification.id.isEmpty())
    {
        withdraw(notification.id);
    }

    while (!impl->boxes.isEmpty() && impl->boxes.size() >= impl->maxVisibleBoxes)
    {
        impl->boxes.first()->close();
    }

    QcwNotification resolved = notification;
    if (resolved.timeoutMs < 0)
    {
        resolved.timeoutMs = impl->defaultTimeout;
    }

    // The creator returns a fully built card: calling setupUi() here as well would duplicate
    // the layout of a creator that already did so.
    auto box = impl->boxCreator(resolved);
    if (!box)
    {
        qWarning() << "QcwNotificationManager::post(): the notification box creator returned no box";
        return;
    }

    connect(box, &QcwNotificationBox::linkActivated, this, [this, box](const QString& href)
    {
        emit linkActivated(box->id(), href);
    });
    connect(box, &QcwNotificationBox::dismissed, this, [this, box]()
    {
        impl->removeBox(box);
    });
    // A card that is deleted without being closed must still leave the stack.
    connect(box, &QObject::destroyed, this, [this, box]()
    {
        impl->removeBox(box);
    });

    impl->boxes.append(box);
    // Position first so the card appears where it belongs instead of jumping after show().
    impl->positionBoxes();
    box->show();
}

void QcwNotificationManager::withdraw(const QString& id)
{
    if (id.isEmpty())
    {
        return;
    }

    QcwNotificationBox* box = nullptr;
    for (auto* candidate : std::as_const(impl->boxes))
    {
        if (candidate->id() == id)
        {
            box = candidate;
            break;
        }
    }

    if (box)
    {
        box->close();
    }
}

void QcwNotificationManager::clear()
{
    const auto boxes = impl->boxes;
    for (auto* box : boxes)
    {
        box->close();
    }
}

void QcwNotificationManager::setNotificationEnabled(bool enabled)
{
    if (impl->notificationEnabled == enabled)
    {
        return;
    }

    impl->notificationEnabled = enabled;

    auto settings = QcwSettings::get();
    settings.setValue(kEnabledKey, enabled);
}

bool QcwNotificationManager::isNotificationEnabled() const
{
    return impl->notificationEnabled;
}

void QcwNotificationManager::setPosition(Qt::Corner corner)
{
    if (!isValidCorner(corner))
    {
        qWarning() << "QcwNotificationManager::setPosition(): invalid corner" << static_cast<int>(corner);
        return;
    }

    if (impl->corner == corner)
    {
        return;
    }

    impl->corner = corner;
    impl->positionBoxes();

    auto settings = QcwSettings::get();
    settings.setValue(kCornerKey, static_cast<int>(impl->corner));
}

Qt::Corner QcwNotificationManager::position() const
{
    return impl->corner;
}

void QcwNotificationManager::setMaxVisibleBoxes(int count)
{
    count = qMax(1, count);
    if (impl->maxVisibleBoxes == count)
    {
        return;
    }

    impl->maxVisibleBoxes = count;

    while (impl->boxes.size() > impl->maxVisibleBoxes)
    {
        impl->boxes.first()->close();
    }

    auto settings = QcwSettings::get();
    settings.setValue(kMaxVisibleBoxesKey, impl->maxVisibleBoxes);
}

int QcwNotificationManager::maxVisibleBoxes() const
{
    return impl->maxVisibleBoxes;
}

void QcwNotificationManager::setDefaultTimeout(int ms)
{
    ms = qMax(0, ms);
    if (impl->defaultTimeout == ms)
    {
        return;
    }

    impl->defaultTimeout = ms;

    auto settings = QcwSettings::get();
    settings.setValue(kTimeoutKey, impl->defaultTimeout);
}

int QcwNotificationManager::defaultTimeout() const
{
    return impl->defaultTimeout;
}

// -----------------------------------------------------------------------------

QcwNotificationManagerPrivate::QcwNotificationManagerPrivate(QcwNotificationManager* interface) :
    owner { interface }
{
}

void QcwNotificationManagerPrivate::ensureApplicationHooks()
{
    if (hooksInstalled || !qGuiApp)
    {
        return;
    }
    hooksInstalled = true;

    const auto screens = qGuiApp->screens();
    for (auto* screen : screens)
    {
        watchScreen(screen);
    }

    QObject::connect(qGuiApp, &QGuiApplication::screenAdded, owner, [this](QScreen* screen)
    {
        watchScreen(screen);
    });

    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, owner, [this](QWindow* window)
    {
        if (window && window->screen())
        {
            lastScreen = window->screen();
        }
    });
}

void QcwNotificationManagerPrivate::watchScreen(QScreen* screen)
{
    if (!screen || watchedScreens.contains(screen))
    {
        return;
    }

    watchedScreens.insert(screen);

    QObject::connect(screen, &QScreen::availableGeometryChanged, owner, [this](const QRect&)
    {
        positionBoxes();
    });

    QObject::connect(screen, &QObject::destroyed, owner, [this, screen]()
    {
        watchedScreens.remove(screen);
    });
}

QScreen* QcwNotificationManagerPrivate::targetScreen() const
{
    if (auto* window = QGuiApplication::focusWindow())
    {
        if (auto* screen = window->screen())
        {
            return screen;
        }
    }

    if (lastScreen)
    {
        return lastScreen;
    }

    return QGuiApplication::primaryScreen();
}

void QcwNotificationManagerPrivate::positionBoxes()
{
    if (boxes.isEmpty())
    {
        return;
    }

    QScreen* screen = targetScreen();
    if (!screen)
    {
        return;
    }
    lastScreen = screen;

    const QRect area = availableArea(screen);
    const bool top = (corner == Qt::TopLeftCorner || corner == Qt::TopRightCorner);
    const bool left = (corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner);

    // Newest first: it sits closest to the chosen corner and pushes older cards away.
    int y = top ? area.top() + StackMargin : area.bottom() - StackMargin + 1;
    for (auto it = boxes.crbegin(); it != boxes.crend(); ++it)
    {
        auto box = *it;
        box->adjustSize();

        const QSize size = box->size();
        const int x = left ? area.left() + StackMargin : area.right() - size.width() + 1 - StackMargin;
        if (top)
        {
            box->move(x, y);
            y += size.height() + StackSpacing;
        }
        else
        {
            y -= size.height();
            box->move(x, y);
            y -= StackSpacing;
        }
    }
}

void QcwNotificationManagerPrivate::removeBox(QcwNotificationBox* box)
{
    if (boxes.removeOne(box))
    {
        positionBoxes();
    }
}

QT_END_NAMESPACE
