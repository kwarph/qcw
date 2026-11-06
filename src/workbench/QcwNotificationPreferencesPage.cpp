// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwNotificationPreferencesPage.h"
#include "QcwNotificationManager.h"
#include <QAbstractButton>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <array>

QT_BEGIN_NAMESPACE

namespace
{

struct CornerOption
{
    Qt::Corner corner;
    const char* text;
};

constexpr std::array<CornerOption, 4> kCornerOptions { {
    { Qt::BottomRightCorner, QT_TRANSLATE_NOOP("QcwNotificationPreferencesPage", "Bottom right") },
    { Qt::BottomLeftCorner, QT_TRANSLATE_NOOP("QcwNotificationPreferencesPage", "Bottom left") },
    { Qt::TopRightCorner, QT_TRANSLATE_NOOP("QcwNotificationPreferencesPage", "Top right") },
    { Qt::TopLeftCorner, QT_TRANSLATE_NOOP("QcwNotificationPreferencesPage", "Top left") },
} };

/// Identifies the action-and-link test notification, so only its links open a message box.
constexpr QLatin1StringView kTestLinkNotificationId("notification-preferences-test-link");

/// Wayland compositors place top-level windows themselves, so a client cannot pick a corner.
bool isWaylandSession()
{
    return QGuiApplication::platformName().startsWith(QLatin1StringView("wayland"), Qt::CaseInsensitive);
}

QString levelName(QcwNotification::Level level)
{
    switch (level)
    {
    case QcwNotification::Level::Success:
        return QCoreApplication::translate("QcwNotificationPreferencesPage", "Success");
    case QcwNotification::Level::Warning:
        return QCoreApplication::translate("QcwNotificationPreferencesPage", "Warning");
    case QcwNotification::Level::Critical:
        return QCoreApplication::translate("QcwNotificationPreferencesPage", "Critical");
    case QcwNotification::Level::Information:
        break;
    }

    return QCoreApplication::translate("QcwNotificationPreferencesPage", "Information");
}

} // namespace

class QcwNotificationPreferencesPagePrivate
{
public:
    explicit QcwNotificationPreferencesPagePrivate(QcwNotificationPreferencesPage* interface);

    void setupUi();
    void retranslateUi();
    void loadFromManager();
    void updateControls(bool enabled);
    void postNotification(QcwNotification::Level level);
    void postNotificationWithActions();
    void postSixNotifications();

public:
    QcwNotificationPreferencesPage* owner;
    QCheckBox* enableCheckBox { nullptr };
    QGroupBox* settingsGroupBox { nullptr };
    QLabel* waylandNoticeLabel { nullptr };
    QLabel* maxBoxesLabel { nullptr };
    QSpinBox* spinBoxBoxes { nullptr };
    QLabel* cornerLabel { nullptr };
    QComboBox* comboBoxCorners { nullptr };
    QLabel* timeoutLabel { nullptr };
    QSpinBox* spinBoxTimeout { nullptr };
    QGroupBox* testGroupBox { nullptr };
    QLabel* label { nullptr };
    QPushButton* informationButton { nullptr };
    QPushButton* successButton { nullptr };
    QPushButton* warningButton { nullptr };
    QPushButton* criticalButton { nullptr };
    QPushButton* actionsButton { nullptr };
    QPushButton* sixNotificationsButton { nullptr };
    QAction* retryAction { nullptr };
    QAction* detailsAction { nullptr };
    QAction* unavailableAction { nullptr };
};

// -----------------------------------------------------------------------------

QcwNotificationPreferencesPage::QcwNotificationPreferencesPage(QWidget* parent) :
    QcwPreferencesPage { parent }, impl { new QcwNotificationPreferencesPagePrivate { this } }
{
    impl->setupUi();
}

QcwNotificationPreferencesPage::~QcwNotificationPreferencesPage()
{
    delete impl;
}

QString QcwNotificationPreferencesPage::settingsGroup() const
{
    return QLatin1String("notification");
}

QString QcwNotificationPreferencesPage::name() const
{
    return tr("Notifications");
}

bool QcwNotificationPreferencesPage::matchesFilter(const QString& filterText) const
{
    const auto contains = [&filterText](const QString& text)
    {
        return text.contains(filterText, Qt::CaseInsensitive);
    };

    for (const auto* label : findChildren<QLabel*>())
    {
        if (contains(label->text()))
        {
            return true;
        }
    }

    for (const auto* button : findChildren<QAbstractButton*>())
    {
        if (contains(button->text()) || contains(button->toolTip()))
        {
            return true;
        }
    }

    for (const auto* groupBox : findChildren<QGroupBox*>())
    {
        if (contains(groupBox->title()))
        {
            return true;
        }
    }

    for (const auto* comboBox : findChildren<QComboBox*>())
    {
        for (int i = 0; i < comboBox->count(); ++i)
        {
            if (contains(comboBox->itemText(i)))
            {
                return true;
            }
        }
    }

    return false;
}

void QcwNotificationPreferencesPage::restoreDefaults()
{
    notificationManager->setNotificationEnabled(true);
    notificationManager->setMaxVisibleBoxes(QcwNotificationManager::DefaultVisibleBoxes);
    notificationManager->setPosition(QcwNotificationManager::DefaultPosition);
    notificationManager->setDefaultTimeout(QcwNotificationManager::DefaultTimeout);

    loadSettings();
}

void QcwNotificationPreferencesPage::loadSettings()
{
    impl->loadFromManager();
}

void QcwNotificationPreferencesPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QcwPreferencesPage::changeEvent(event);
}

// -----------------------------------------------------------------------------

QcwNotificationPreferencesPagePrivate::QcwNotificationPreferencesPagePrivate(
    QcwNotificationPreferencesPage* interface) :
    owner { interface }
{
}

void QcwNotificationPreferencesPagePrivate::setupUi()
{
    auto mainLayout = new QVBoxLayout(owner);
    mainLayout->setSpacing(12);
    mainLayout->setObjectName("verticalLayout_2");
    enableCheckBox = new QCheckBox(owner);
    enableCheckBox->setObjectName("enableCheckBox");
    enableCheckBox->setChecked(true);

    mainLayout->addWidget(enableCheckBox);

    settingsGroupBox = new QGroupBox(owner);
    settingsGroupBox->setObjectName("settingsGroupBox");
    auto formLayout = new QFormLayout(settingsGroupBox);
    formLayout->setObjectName("formLayout");
    maxBoxesLabel = new QLabel(settingsGroupBox);
    maxBoxesLabel->setObjectName("maxBoxesLabel");

    formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, maxBoxesLabel);

    spinBoxBoxes = new QSpinBox(settingsGroupBox);
    spinBoxBoxes->setObjectName("spinBoxBoxes");
    QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Fixed);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(spinBoxBoxes->sizePolicy().hasHeightForWidth());
    spinBoxBoxes->setSizePolicy(sizePolicy);
    spinBoxBoxes->setMinimum(1);
    spinBoxBoxes->setMaximum(5);
    spinBoxBoxes->setValue(5);

    formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, spinBoxBoxes);

    cornerLabel = new QLabel(settingsGroupBox);
    cornerLabel->setObjectName("cornerLabel");

    formLayout->setWidget(1, QFormLayout::ItemRole::LabelRole, cornerLabel);

    comboBoxCorners = new QComboBox(settingsGroupBox);
    comboBoxCorners->setObjectName("comboBoxCorners");
    sizePolicy.setHeightForWidth(comboBoxCorners->sizePolicy().hasHeightForWidth());
    comboBoxCorners->setSizePolicy(sizePolicy);
    for (const CornerOption& option : kCornerOptions)
    {
        comboBoxCorners->addItem(QString(), static_cast<int>(option.corner));
    }

    formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, comboBoxCorners);

    timeoutLabel = new QLabel(settingsGroupBox);
    timeoutLabel->setObjectName("timeoutLabel");

    formLayout->setWidget(2, QFormLayout::ItemRole::LabelRole, timeoutLabel);

    spinBoxTimeout = new QSpinBox(settingsGroupBox);
    spinBoxTimeout->setObjectName("spinBoxTimeout");
    sizePolicy.setHeightForWidth(spinBoxTimeout->sizePolicy().hasHeightForWidth());
    spinBoxTimeout->setSizePolicy(sizePolicy);
    spinBoxTimeout->setMaximum(120);
    spinBoxTimeout->setValue(10);

    formLayout->setWidget(2, QFormLayout::ItemRole::FieldRole, spinBoxTimeout);

    mainLayout->addWidget(settingsGroupBox);

    auto waylandNotice = new QWidget(owner);
    waylandNotice->setObjectName("waylandNotice");
    auto waylandNoticeLayout = new QHBoxLayout(waylandNotice);
    waylandNoticeLayout->setContentsMargins(0, 0, 0, 0);
    waylandNoticeLayout->setSpacing(6);

    auto waylandNoticeIcon = new QLabel(waylandNotice);
    waylandNoticeIcon->setObjectName("waylandNoticeIcon");
    waylandNoticeIcon->setPixmap(owner->style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(QSize(16, 16)));
    waylandNoticeIcon->setFixedSize(16, 16);
    waylandNoticeLayout->addWidget(waylandNoticeIcon, 0, Qt::AlignTop);

    waylandNoticeLabel = new QLabel(waylandNotice);
    waylandNoticeLabel->setObjectName("waylandNoticeLabel");
    waylandNoticeLabel->setWordWrap(true);
    waylandNoticeLayout->addWidget(waylandNoticeLabel, 1);

    waylandNotice->setVisible(isWaylandSession());

    mainLayout->addWidget(waylandNotice);

    testGroupBox = new QGroupBox(owner);
    testGroupBox->setObjectName("testGroupBox");
    auto verticalLayout = new QVBoxLayout(testGroupBox);
    verticalLayout->setSpacing(9);
    verticalLayout->setObjectName("verticalLayout");
    label = new QLabel(testGroupBox);
    label->setObjectName("label");
    label->setWordWrap(true);

    verticalLayout->addWidget(label);

    auto gridLayout = new QGridLayout();
    gridLayout->setSpacing(9);
    gridLayout->setObjectName("gridLayout");
    informationButton = new QPushButton(testGroupBox);
    informationButton->setObjectName("informationButton");

    gridLayout->addWidget(informationButton, 0, 0, 1, 1);

    successButton = new QPushButton(testGroupBox);
    successButton->setObjectName("successButton");

    gridLayout->addWidget(successButton, 0, 1, 1, 1);

    warningButton = new QPushButton(testGroupBox);
    warningButton->setObjectName("warningButton");

    gridLayout->addWidget(warningButton, 0, 2, 1, 1);

    criticalButton = new QPushButton(testGroupBox);
    criticalButton->setObjectName("criticalButton");

    gridLayout->addWidget(criticalButton, 0, 3, 1, 1);

    actionsButton = new QPushButton(testGroupBox);
    actionsButton->setObjectName("actionsButton");

    gridLayout->addWidget(actionsButton, 1, 0, 1, 2);

    sixNotificationsButton = new QPushButton(testGroupBox);
    sixNotificationsButton->setObjectName("sixNotificationsButton");

    gridLayout->addWidget(sixNotificationsButton, 1, 2, 1, 2);

    verticalLayout->addLayout(gridLayout);

    mainLayout->addWidget(testGroupBox);

    auto vSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

    mainLayout->addItem(vSpacer);

    settingsGroupBox->setEnabled(true);
    testGroupBox->setEnabled(true);

    retryAction = new QAction(owner);
    detailsAction = new QAction(owner);
    unavailableAction = new QAction(owner);
    unavailableAction->setEnabled(false);

    QObject::connect(enableCheckBox, &QCheckBox::checkStateChanged, owner, [this](Qt::CheckState state)
    {
        const bool isEnabled = state == Qt::Checked;
        updateControls(isEnabled);

        notificationManager->setNotificationEnabled(isEnabled);
    });

    QObject::connect(spinBoxBoxes, &QSpinBox::valueChanged, owner, [](int count)
    {
        notificationManager->setMaxVisibleBoxes(count);
    });

    QObject::connect(comboBoxCorners, &QComboBox::currentIndexChanged, owner, [this](int index)
    {
        if (index < 0)
        {
            return;
        }

        notificationManager->setPosition(static_cast<Qt::Corner>(comboBoxCorners->itemData(index).toInt()));
    });

    QObject::connect(spinBoxTimeout, &QSpinBox::valueChanged, owner, [](int seconds)
    {
        notificationManager->setDefaultTimeout(seconds * 1000);
    });

    QObject::connect(informationButton, &QPushButton::clicked, owner, [this]()
    {
        postNotification(QcwNotification::Level::Information);
    });

    QObject::connect(successButton, &QPushButton::clicked, owner, [this]()
    {
        postNotification(QcwNotification::Level::Success);
    });

    QObject::connect(warningButton, &QPushButton::clicked, owner, [this]()
    {
        postNotification(QcwNotification::Level::Warning);
    });

    QObject::connect(criticalButton, &QPushButton::clicked, owner, [this]()
    {
        postNotification(QcwNotification::Level::Critical);
    });

    QObject::connect(actionsButton, &QPushButton::clicked, owner, [this]()
    {
        postNotificationWithActions();
    });

    QObject::connect(sixNotificationsButton, &QPushButton::clicked, owner, [this]()
    {
        postSixNotifications();
    });

    QObject::connect(retryAction, &QAction::triggered, owner, [this]()
    {
        QMessageBox::information(
            owner, QCoreApplication::translate("QcwNotificationPreferencesPage", "Retry"),
            QCoreApplication::translate("QcwNotificationPreferencesPage", "The retry action was triggered."));
    });

    QObject::connect(detailsAction, &QAction::triggered, owner, [this]()
    {
        QMessageBox::information(
            owner, QCoreApplication::translate("QcwNotificationPreferencesPage", "Details"),
            QCoreApplication::translate("QcwNotificationPreferencesPage", "The details action was triggered."));
    });

    QObject::connect(notificationManager, &QcwNotificationManager::linkActivated, owner,
                     [this](const QString& notificationId, const QString& href)
    {
        // Other components post their own notifications; only the test card's link is ours.
        if (notificationId != kTestLinkNotificationId)
        {
            return;
        }

        QMessageBox::information(
            owner, QCoreApplication::translate("QcwNotificationPreferencesPage", "Link Activated"),
            QCoreApplication::translate("QcwNotificationPreferencesPage", "Notification: %1\nLink: %2")
                .arg(notificationId, href));
    });

    retranslateUi();
}

void QcwNotificationPreferencesPagePrivate::retranslateUi()
{
    owner->setWindowTitle(QCoreApplication::translate("QcwNotificationPreferencesPage", "Notifications"));
    enableCheckBox->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "Enable notifications"));
    settingsGroupBox->setTitle(QCoreApplication::translate("QcwNotificationPreferencesPage", "Notification Settings"));
    maxBoxesLabel->setText(
        QCoreApplication::translate("QcwNotificationPreferencesPage", "Maximum number of visible notification boxes:"));
    cornerLabel->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "Notification boxes show in:"));
    if (isWaylandSession())
    {
        waylandNoticeLabel->setText(
            QCoreApplication::translate("QcwNotificationPreferencesPage",
                                        "Note: on Wayland the compositor decides where application windows appear, "
                                        "so notification boxes may not show in the corner selected above."));
    }
    else
    {
        // Keep the hidden notice out of matchesFilter().
        waylandNoticeLabel->clear();
    }
    timeoutLabel->setText(
        QCoreApplication::translate("QcwNotificationPreferencesPage", "Time to wait a notification box closed:"));

    {
        const QSignalBlocker blocker(comboBoxCorners);
        for (int i = 0; i < comboBoxCorners->count() && i < int(kCornerOptions.size()); ++i)
        {
            comboBoxCorners->setItemText(
                i, QCoreApplication::translate("QcwNotificationPreferencesPage", kCornerOptions.at(i).text));
        }
    }

#if QT_CONFIG(tooltip)
    spinBoxTimeout->setToolTip(QCoreApplication::translate(
        "QcwNotificationPreferencesPage",
        "Time in seconds. 0 indicates that a notification box is not automatically closed."));
#endif // QT_CONFIG(tooltip)
    spinBoxTimeout->setSuffix(QCoreApplication::translate("QcwNotificationPreferencesPage", " sec"));
    testGroupBox->setTitle(QCoreApplication::translate("QcwNotificationPreferencesPage", "Notification Test"));
    label->setText(QCoreApplication::translate(
        "QcwNotificationPreferencesPage", "Click the following buttons to view the effect of the settings above."));
    informationButton->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "&Information"));
    successButton->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "&Success"));
    warningButton->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "&Warning"));
    criticalButton->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "&Critical"));
    actionsButton->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "&Actions and Links"));
    sixNotificationsButton->setText(
        QCoreApplication::translate("QcwNotificationPreferencesPage", "Post 6 Notifications"));
    retryAction->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "Retry"));
    detailsAction->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "Details"));
    unavailableAction->setText(QCoreApplication::translate("QcwNotificationPreferencesPage", "Unavailable"));
}

void QcwNotificationPreferencesPagePrivate::loadFromManager()
{
    const bool enabled = notificationManager->isNotificationEnabled();

    {
        const QSignalBlocker blocker(enableCheckBox);
        enableCheckBox->setChecked(enabled);
    }
    updateControls(enabled);

    {
        const QSignalBlocker blocker(spinBoxBoxes);
        spinBoxBoxes->setValue(notificationManager->maxVisibleBoxes());
    }

    {
        const QSignalBlocker blocker(comboBoxCorners);
        int index = comboBoxCorners->findData(static_cast<int>(notificationManager->position()));
        if (index < 0)
        {
            index = comboBoxCorners->findData(static_cast<int>(QcwNotificationManager::DefaultPosition));
        }
        comboBoxCorners->setCurrentIndex(index);
    }

    {
        const QSignalBlocker blocker(spinBoxTimeout);
        // The manager stores milliseconds; the control edits whole seconds.
        spinBoxTimeout->setValue((notificationManager->defaultTimeout() + 500) / 1000);
    }
}

void QcwNotificationPreferencesPagePrivate::updateControls(bool enabled)
{
    settingsGroupBox->setEnabled(enabled);
    testGroupBox->setEnabled(enabled);
}

void QcwNotificationPreferencesPagePrivate::postNotification(QcwNotification::Level level)
{
    QcwNotification notification;
    notification.level = level;
    notification.title =
        QCoreApplication::translate("QcwNotificationPreferencesPage", "%1 notification").arg(levelName(level));
    notification.text = QCoreApplication::translate("QcwNotificationPreferencesPage",
                                                    "Something happened and this card tells you about it.");

    notificationManager->post(notification);
}

void QcwNotificationPreferencesPagePrivate::postNotificationWithActions()
{
    QcwNotification notification;
    notification.id = kTestLinkNotificationId;
    notification.level = QcwNotification::Level::Warning;
    notification.title = QCoreApplication::translate("QcwNotificationPreferencesPage", "Connection lost");
    notification.text = QCoreApplication::translate(
        "QcwNotificationPreferencesPage", "The server stopped responding. <a href=\"Dolphin\">Show details</a>");
    notification.actions = { retryAction, detailsAction, unavailableAction };
    // Leave the card open long enough to try the actions and the link.
    notification.timeoutMs = 0;

    notificationManager->post(notification);
}

void QcwNotificationPreferencesPagePrivate::postSixNotifications()
{
    const int limit = notificationManager->maxVisibleBoxes();
    for (int i = 1; i <= 6; ++i)
    {
        QcwNotification notification;
        notification.id = QStringLiteral("notification-preferences-%1").arg(i);
        notification.title =
            QCoreApplication::translate("QcwNotificationPreferencesPage", "Notification %1 of 6").arg(i);
        notification.text = QCoreApplication::translate("QcwNotificationPreferencesPage",
                                                        "With a limit of %1 cards, the oldest one closes.")
                                .arg(limit);
        notification.timeoutMs = 0;

        notificationManager->post(notification);
    }
}

QT_END_NAMESPACE
