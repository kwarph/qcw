// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwNotificationBox.h"

#include <QAction>
#include <QCloseEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

QT_BEGIN_NAMESPACE

namespace
{

constexpr int CardWidth = 400;
constexpr int CardMargin = 9;
constexpr int CardSpacing = 6;
constexpr int IconSize = 20;

Qt::WindowFlags notificationWindowFlags()
{
    // Under X11, Qt::Tool becomes a utility window, which some taskbars (notably KDE's)
    // list as a separate entry for every card. Tooltip windows are skipped instead.
    if (QGuiApplication::platformName().startsWith(QLatin1StringView("xcb")))
    {
        return Qt::ToolTip | Qt::WindowStaysOnTopHint;
    }

    return Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
}

QString levelColor(QcwNotification::Level level)
{
    switch (level)
    {
    case QcwNotification::Level::Success:
        return QStringLiteral("#2e9e44");
    case QcwNotification::Level::Warning:
        return QStringLiteral("#e0a800");
    case QcwNotification::Level::Critical:
        return QStringLiteral("#d64545");
    case QcwNotification::Level::Information:
        break;
    }
    return QStringLiteral("#2f80ed");
}

QStyle::StandardPixmap levelStandardIcon(QcwNotification::Level level)
{
    switch (level)
    {
    case QcwNotification::Level::Success:
        return QStyle::SP_DialogApplyButton;
    case QcwNotification::Level::Warning:
        return QStyle::SP_MessageBoxWarning;
    case QcwNotification::Level::Critical:
        return QStyle::SP_MessageBoxCritical;
    case QcwNotification::Level::Information:
        break;
    }
    return QStyle::SP_MessageBoxInformation;
}

} // namespace

// -----------------------------------------------------------------------------

class QcwNotificationBoxDefault : public QcwNotificationBox
{
public:
    explicit QcwNotificationBoxDefault(const QcwNotification& notification);

    void setupUi() override;

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();

    QLabel* iconLabel { nullptr };
    QLabel* titleLabel { nullptr };
    QLabel* textLabel { nullptr };
    QToolButton* closeButton { nullptr };
};

// -----------------------------------------------------------------------------

class QcwNotificationBoxPrivate
{
public:
    QcwNotificationBoxPrivate(QcwNotificationBox* interface, const QcwNotification& data);

public:
    QcwNotificationBox* owner;
    QcwNotification notification;
    int timeoutMs { 0 };
    int remainingMs { 0 };
    bool hovered { false };
    QTimer* timer { nullptr };
};

QcwNotificationBoxPrivate::QcwNotificationBoxPrivate(QcwNotificationBox* interface, const QcwNotification& data) :
    owner { interface }, notification { data }, timeoutMs { qMax(0, data.timeoutMs) }
{
    if (timeoutMs > 0)
    {
        timer = new QTimer(owner);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, owner, [this]()
        {
            owner->close();
        });
    }
}

// -----------------------------------------------------------------------------

QcwNotificationBox::QcwNotificationBox(const QcwNotification& notification) :
    QFrame { nullptr }, impl { new QcwNotificationBoxPrivate { this, notification } }
{
    setObjectName(QStringLiteral("notificationBox"));
    setWindowFlags(notificationWindowFlags());
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedWidth(CardWidth);
}

QcwNotificationBox::~QcwNotificationBox()
{
    delete impl;
}

QcwNotificationBox* QcwNotificationBox::createDefaultNotificationBox(const QcwNotification& notification)
{
    auto* box = new QcwNotificationBoxDefault { notification };
    box->setupUi();
    return box;
}

QString QcwNotificationBox::id() const
{
    return impl->notification.id;
}

QSize QcwNotificationBox::sizeHint() const
{
    if (const auto* boxLayout = layout())
    {
        // The wrapped body needs its height at the fixed card width, not at the width QLabel
        // reports as its natural one.
        const int height = boxLayout->hasHeightForWidth() ? boxLayout->totalHeightForWidth(CardWidth) :
                                                            boxLayout->totalSizeHint().height();
        return { CardWidth, height };
    }
    return QFrame::sizeHint();
}

void QcwNotificationBox::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);

    if (impl->timer && !impl->timer->isActive() && !impl->hovered)
    {
        impl->timer->start(impl->timeoutMs);
    }
}

void QcwNotificationBox::enterEvent(QEnterEvent* event)
{
    impl->hovered = true;
    if (impl->timer && impl->timer->isActive())
    {
        impl->remainingMs = qMax(0, impl->timer->remainingTime());
        impl->timer->stop();
    }
    QFrame::enterEvent(event);
}

void QcwNotificationBox::leaveEvent(QEvent* event)
{
    impl->hovered = false;
    if (impl->timer && impl->remainingMs > 0)
    {
        impl->timer->start(impl->remainingMs);
        impl->remainingMs = 0;
    }
    QFrame::leaveEvent(event);
}

void QcwNotificationBox::closeEvent(QCloseEvent* event)
{
    if (impl->timer)
    {
        impl->timer->stop();
    }
    emit dismissed();
    QFrame::closeEvent(event);
}

const QcwNotification& QcwNotificationBox::notification() const
{
    return impl->notification;
}

// -----------------------------------------------------------------------------

QcwNotificationBoxDefault::QcwNotificationBoxDefault(const QcwNotification& notification) :
    QcwNotificationBox { notification }
{
}

void QcwNotificationBoxDefault::setupUi()
{
    auto borderColor = palette().color(QPalette::Disabled, QPalette::WindowText);
    // The frame and buttons follow the palette; only the accent color per level is fixed.
    // Style-sheet palette role names are lower case and case-sensitive.
    setStyleSheet(QStringLiteral("QFrame#notificationBox {"
                                 "    background-color: palette(base);"
                                 "    border: 1px solid %1;"
                                 "    border-left: 4px solid %2;"
                                 "}"
                                 "QToolButton#notificationCloseButton {"
                                 "    border: none;"
                                 "}"
                                 "QToolButton#notificationActionButton {"
                                 "    color: palette(link);"
                                 "    border: none;"
                                 "    border-radius: 4px;"
                                 "    padding: 3px 6px;"
                                 "}"
                                 "QToolButton#notificationActionButton:hover {"
                                 "    color: palette(highlight);"
                                 "    border: 1px solid palette(highlight);"
                                 "}"
                                 "QToolButton#notificationActionButton:pressed {"
                                 "    color: palette(highlight);"
                                 "    border: 1px solid palette(highlight);"
                                 "}"
                                 "QToolButton#notificationActionButton:disabled {"
                                 "    color: %1;"
                                 "}")
                      .arg(borderColor.name(), levelColor(notification().level)));

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(CardMargin, CardMargin, CardMargin, CardMargin);
    mainLayout->setSpacing(CardSpacing);

    auto headerLayout = new QHBoxLayout;
    headerLayout->setSpacing(CardSpacing);

    iconLabel = new QLabel(this);
    iconLabel->setObjectName(QStringLiteral("notificationIcon"));

    QIcon icon = style()->standardIcon(levelStandardIcon(notification().level));
    iconLabel->setPixmap(icon.pixmap(QSize(IconSize, IconSize), devicePixelRatioF()));
    iconLabel->setFixedSize(IconSize, IconSize);
    iconLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(iconLabel, 0, Qt::AlignTop);

    titleLabel = new QLabel(notification().title, this);
    QFont font = titleLabel->font();
    font.setBold(true);
    titleLabel->setFont(font);

    titleLabel->setObjectName(QStringLiteral("notificationTitle"));
    titleLabel->setTextFormat(Qt::PlainText);
    titleLabel->setWordWrap(true);
    titleLabel->setVisible(!notification().title.isEmpty());
    headerLayout->addWidget(titleLabel, 1);

    closeButton = new QToolButton(this);
    closeButton->setObjectName(QStringLiteral("notificationCloseButton"));
    closeButton->setAutoRaise(true);
    closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    closeButton->setIconSize(QSize(16, 16));
    closeButton->setFixedSize(QSize(20, 20));
    headerLayout->addWidget(closeButton, 0, Qt::AlignTop);
    QObject::connect(closeButton, &QToolButton::clicked, this, &QWidget::close);

    mainLayout->addLayout(headerLayout);

    if (!notification().text.isEmpty())
    {
        textLabel = new QLabel(notification().text, this);
        textLabel->setObjectName(QStringLiteral("notificationText"));
        textLabel->setTextFormat(Qt::RichText);
        textLabel->setWordWrap(true);
        textLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::TextSelectableByMouse);
        textLabel->setOpenExternalLinks(false);
        textLabel->setFocusPolicy(Qt::NoFocus);
        QObject::connect(textLabel, &QLabel::linkActivated, this, &QcwNotificationBox::linkActivated);
        mainLayout->addWidget(textLabel);
    }

    QList<QAction*> actions;
    for (auto* action : std::as_const(notification().actions))
    {
        if (action)
        {
            actions.append(action);
        }
    }

    if (!actions.isEmpty())
    {
        auto actionsLayout = new QHBoxLayout;
        actionsLayout->setSpacing(6);
        actionsLayout->addStretch(1);

        for (auto* action : std::as_const(actions))
        {
            auto button = new QToolButton(this);
            button->setObjectName(QStringLiteral("notificationActionButton"));
            button->setDefaultAction(action);
            button->setAutoRaise(true);
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            // The action may also be used elsewhere, so the card listens to it rather than
            // replacing its triggered() connection.
            QObject::connect(action, &QAction::triggered, this, [this, action]()
            {
                emit actionTriggered(action);
                if (notification().closeOnActionTriggered)
                {
                    close();
                }
            });
            actionsLayout->addWidget(button);
        }

        mainLayout->addLayout(actionsLayout);
    }

    retranslateUi();
}

void QcwNotificationBoxDefault::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QcwNotificationBox::changeEvent(event);
}

void QcwNotificationBoxDefault::retranslateUi()
{
    // The card is a top-level window, so Qt delivers the language change to it directly.
    closeButton->setToolTip(QcwNotificationBox::tr("Close"));
}

QT_END_NAMESPACE
