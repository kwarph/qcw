// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwWindowStyler.h"
#include "QcwCommandRegistry.h"
#include "QcwPreferencesDialog.h"
#include "QcwWindowActionIds.h"
#include "QcwNotificationManager.h"
#include "QcwFastViewStyleSettings.h"
#include "QcwPerspective.h"
#include "QcwSettings.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwTabFolderRenderer.h"
#include "QcwTabFolderRendererCreators.h"
#include "QcwThemeManager.h"
#include "QcwThemeUtils.h"
#include "QcwWindow.h"
#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QGlobalStatic>
#include <QJsonObject>
#include <QLayout>
#include <QSet>
#include <QTimer>

QT_BEGIN_NAMESPACE

namespace
{

constexpr QLatin1StringView kCurrentStyleKey("appearance/windowStyle/currentStyle");
constexpr QLatin1StringView kWindowThemePrefix("QcwWindow.");
constexpr QLatin1StringView kThemeFileSuffix(".theme");
constexpr QLatin1StringView kStyleNotificationId("window-style-unavailable");

struct WindowStyle
{
    QString rendererType;
    QString windowCss;
    QMargins centralMargins;
    int fastViewMargin { 6 };
    bool fastViewHasBorder { false };
};

WindowStyle loadWindowStyle(const QString& style)
{
    WindowStyle result;
    if (style.isEmpty())
    {
        return result;
    }

    const QJsonObject jobj = themeManager->loadWidgetThemeData(QStringLiteral("%1%2").arg(kWindowThemePrefix, style));
    result.rendererType = jobj.value("folderRenderer").toString();
    result.windowCss = jobj.value("windowCss").toString();
    result.centralMargins = QcwThemeUtils::parseMargins(jobj.value("centralMargins").toString());

    if (jobj.contains("fastViewMargin"))
    {
        result.fastViewMargin = jobj.value("fastViewMargin").toInt();
    }

    if (jobj.contains("fastViewHasBorder"))
    {
        result.fastViewHasBorder = jobj.value("fastViewHasBorder").toBool();
    }

    fastViewStyleSettings->setFastViewMargin(result.fastViewMargin);
    fastViewStyleSettings->setFastViewHasBorder(result.fastViewHasBorder);

    const QString themeDirectory = QDir::fromNativeSeparators(themeManager->currentThemeDirectory());
    if (!themeDirectory.isEmpty())
    {
        result.windowCss.replace(QStringLiteral("%{themeDirectory}"), themeDirectory);
    }

    return result;
}

QcwTabFolderRendererCreator defaultRendererCreator()
{
    return [](QcwTabFolder* folder)
    {
        return QcwTabFolderRenderer::createDefaultRenderer(folder);
    };
}

} // namespace

// -----------------------------------------------------------------------------

class QcwWindowStylerPrivate
{
public:
    void applyStyle(QcwWindow* window, const WindowStyle& style) const;
    void applyStyle(QcwTabFolder* folder, const WindowStyle& style) const;

    /// Runs the application's Preferences command, which owns the single preferences dialog.
    void openPreferences() const;

    static QList<QcwWindow*> allWindows();
    static QSet<QcwTabFolder*> allFolders(QcwWindow* window);

public:
    QList<QString> styles;
    QString warnedStyle;
    QAction* openPreferencesAction { nullptr };
    QSet<QcwWindow*> followedWindows;
    bool initialized { false };
};

// -----------------------------------------------------------------------------

Q_GLOBAL_STATIC(QcwWindowStyler, windowStylerInstance);

QcwWindowStyler::QcwWindowStyler() :
    impl { new QcwWindowStylerPrivate }
{
    impl->openPreferencesAction = new QAction(this);
    connect(impl->openPreferencesAction, &QAction::triggered, this, [this]()
    {
        impl->openPreferences();
    });
}

QcwWindowStyler::~QcwWindowStyler()
{
    delete impl;
}

QcwWindowStyler* QcwWindowStyler::instance()
{
    return windowStylerInstance;
}

void QcwWindowStyler::initialize()
{
    if (impl->initialized)
    {
        return;
    }

    impl->initialized = true;

    // A window stylesheet puts the window's widgets under QStyleSheetStyle, which by default
    // stops them from following QApplication::setFont(). The appearance preferences change the
    // application font at run time, so keep the regular propagation.
    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);

    collectStyles();
    applyStyle();

    // Queued, not direct: the theme-aware renderers reload their data from the same signal.
    // Running after those direct handlers means renderers built here see the new theme.
    QObject::connect(themeManager, &QcwThemeManager::themeChanged, this, [this]()
    {
        collectStyles();
        applyStyle();
    }, Qt::QueuedConnection);
}

void QcwWindowStyler::collectStyles()
{
    impl->styles.clear();
    impl->styles.append(QString(FramedStyle));

    const QDir themeDir(themeManager->currentThemeDirectory());
    const auto fileNames = themeDir.entryList({ QStringLiteral("QcwWindow.*.theme") }, QDir::Files, QDir::Name);
    for (const auto& fileName : fileNames)
    {
        if (!fileName.startsWith(kWindowThemePrefix) || !fileName.endsWith(kThemeFileSuffix))
        {
            continue;
        }

        const auto styleLength = fileName.size() - kWindowThemePrefix.size() - kThemeFileSuffix.size();
        const QString styleName = fileName.mid(kWindowThemePrefix.size(), styleLength);
        if (!styleName.isEmpty() && !impl->styles.contains(styleName))
        {
            impl->styles.append(styleName);
        }
    }

    emit stylesChanged();
}

void QcwWindowStyler::setCurrentStyle(const QString& style)
{
    const QString resolvedStyle = style.isEmpty() ? QString(FramedStyle) : style;

    QcwSettings::get().setValue(kCurrentStyleKey, resolvedStyle);
    applyStyle();
}

QString QcwWindowStyler::currentStyle() const
{
    const QString style = QcwSettings::get().value(kCurrentStyleKey, QString(FramedStyle)).toString();
    return style.isEmpty() ? QString(FramedStyle) : style;
}

QList<QString> QcwWindowStyler::allStyles() const
{
    if (impl->styles.isEmpty())
    {
        return { QString(FramedStyle) };
    }

    return impl->styles;
}

void QcwWindowStyler::applyStyle()
{
    const QString styleName = currentStyle();
    reportUnresolvedStyle(styleName);

    const WindowStyle style = loadWindowStyle(styleName);
    for (auto window : QcwWindowStylerPrivate::allWindows())
    {
        impl->applyStyle(window, style);
        followWindow(window);
    }
}

void QcwWindowStyler::applyStyle(QcwWindow* window) const
{
    if (!window)
    {
        return;
    }

    const QString styleName = currentStyle();
    reportUnresolvedStyle(styleName);

    impl->applyStyle(window, loadWindowStyle(styleName));
    followWindow(window);
}

void QcwWindowStyler::followWindow(QcwWindow* window) const
{
    if (!window || impl->followedWindows.contains(window))
    {
        return;
    }

    impl->followedWindows.insert(window);

    connect(window, &QcwWindow::folderAdded, this, [this](QcwTabFolder* folder)
    {
        impl->applyStyle(folder, loadWindowStyle(currentStyle()));
    });

    connect(window, &QObject::destroyed, this, [this, window]()
    {
        impl->followedWindows.remove(window);
    });
}

void QcwWindowStyler::reportUnresolvedStyle(const QString& style) const
{
    // Every collected name resolves and FramedStyle needs no file of its own, so anything else
    // reaching here is a stored value the current theme stopped answering to. The window wears
    // the built-in look while the preferences combo shows its first entry, so say it once - in
    // the log and on screen - and let the user choose a replacement. Nothing is collected before
    // collectStyles(), when every name would look unresolved, so the report waits for that.
    if (impl->styles.isEmpty() || style == QString(FramedStyle) || impl->styles.contains(style) ||
        impl->warnedStyle == style)
    {
        return;
    }

    impl->warnedStyle = style;

    const QString offered = allStyles().join(QStringLiteral(", "));
    qWarning() << "QcwWindowStyler: the saved window style" << style << "has no theme file in"
               << themeManager->currentThemeDirectory() << "- the window uses the built-in style."
               << "Available styles:" << offered;

    QcwNotification notification;
    notification.id = QString(kStyleNotificationId);
    notification.level = QcwNotification::Level::Warning;
    notification.title = tr("Window style unavailable");
    notification.text = tr("The saved window style <b>%1</b> does not exist in the current theme, "
                           "so the window uses the built-in look. Choose one of %2 in the "
                           "appearance preferences.")
                            .arg(style.toHtmlEscaped(), offered.toHtmlEscaped());
    // The card asks for a decision, so it stays until the user closes it or opens Preferences.
    notification.timeoutMs = 0;

    // The button shows only while the application publishes a Preferences command for it to run;
    // that command owns the one dialog, so triggering it is all the card has to do.
    if (commandRegistry->contains(QcwWindowActionIds::ActionPreferences))
    {
        impl->openPreferencesAction->setText(tr("Open Preferences..."));
        notification.actions = { impl->openPreferencesAction };
    }

    // Deferred so the window is on screen before the card appears: the manager anchors its stack
    // to the screen of the current focus window, and the first style is applied while startup has
    // shown no window yet.
    QTimer::singleShot(0, this, [this, notification]()
    {
        notificationManager->post(notification);
    });
}

// -----------------------------------------------------------------------------

void QcwWindowStylerPrivate::openPreferences() const
{
    // The application's Preferences command owns the single dialog; running it through the
    // active window keeps the workbench in charge of the one instance.
    for (auto* widget : QApplication::topLevelWidgets())
    {
        if (auto* window = qobject_cast<QcwWindow*>(widget))
        {
            if (window->isActiveWindow())
            {
                window->executeCommand(QcwWindowActionIds::ActionPreferences);
                return;
            }
        }
    }

    // No active window yet; open the dialog directly.
    auto* dialog = new QcwPreferencesDialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(900, 640);
    dialog->show();
}

void QcwWindowStylerPrivate::applyStyle(QcwWindow* window, const WindowStyle& style) const
{
    if (!window)
    {
        return;
    }

    const auto folders = allFolders(window);
    for (auto folder : folders)
    {
        applyStyle(folder, style);
    }

    if (window->styleSheet() != style.windowCss)
    {
        window->setStyleSheet(style.windowCss);
    }

    // Set on every apply, so a style without the key restores the layout's zero margins.
    if (auto central = window->centralWidget())
    {
        if (auto layout = central->layout())
        {
            layout->setContentsMargins(style.centralMargins);
        }
    }
}

void QcwWindowStylerPrivate::applyStyle(QcwTabFolder* folder, const WindowStyle& style) const
{
    if (!folder)
    {
        return;
    }

    auto creator = folderRendererCreators->getCreator(style.rendererType);
    if (!creator)
    {
        creator = defaultRendererCreator();
    }

    // A styled renderer installs its own folder stylesheet; clear whatever an earlier style
    // left behind before the replacement decides whether there is one.
    folder->setStyleSheet(QString());
    folder->setRenderer(creator(folder));
}

QList<QcwWindow*> QcwWindowStylerPrivate::allWindows()
{
    QList<QcwWindow*> windows;

    const auto topLevelWidgets = QApplication::topLevelWidgets();
    for (auto widget : topLevelWidgets)
    {
        if (auto window = qobject_cast<QcwWindow*>(widget))
        {
            windows.append(window);
        }
    }

    return windows;
}

QSet<QcwTabFolder*> QcwWindowStylerPrivate::allFolders(QcwWindow* window)
{
    QSet<QcwTabFolder*> folders;

    for (auto perspective : window->allPerspectives())
    {
        if (!perspective)
        {
            continue;
        }

        if (auto container = perspective->folderContainer())
        {
            folders.unite(container->folders());
        }

        folders.unite(perspective->floatingFolders());
    }

    if (auto sharedFolder = window->sharedFolder())
    {
        folders.insert(sharedFolder);
    }

    return folders;
}

QT_END_NAMESPACE
