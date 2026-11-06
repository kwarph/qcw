// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwThemeManager.h"
#include "QcwSettings.h"
#include "QcwThemeUtils.h"
#include <QApplication>
#include <QColor>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGlobalStatic>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaEnum>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>

QT_BEGIN_NAMESPACE

namespace
{

constexpr QLatin1StringView kCurrentThemeKey("appearance/themes/currentTheme");
constexpr QLatin1StringView kThemeInfosKey("appearance/themes/themeInfos");
constexpr QLatin1StringView kAppCssFileName("main.css");

bool isThemeDirectory(const QDir& dir)
{
    return QFileInfo(dir.filePath(QcwThemeManager::PaletteFileName)).isFile();
}

QJsonObject readJsonObject(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "QcwThemeManager: cannot read" << filePath;
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "QcwThemeManager: cannot parse" << filePath << ':' << error.errorString();
        return {};
    }

    if (!document.isObject())
    {
        qWarning() << "QcwThemeManager: not a JSON object:" << filePath;
        return {};
    }

    return document.object();
}

} // namespace

Q_GLOBAL_STATIC(QcwThemeManager, themeManagerInstance);

QcwThemeManager* QcwThemeManager::instance()
{
    return themeManagerInstance();
}

void QcwThemeManager::initialize()
{
    if (mInitialized)
    {
        return;
    }

    if (!qApp)
    {
        qWarning() << "QcwThemeManager::initialize(): a QApplication must exist";
        return;
    }

    auto settings = QcwSettings::get();

    QMap<QString, QString> savedThemes;
    const QByteArray serialized = settings.value(kThemeInfosKey).toByteArray();
    if (!serialized.isEmpty())
    {
        QDataStream stream(serialized);
        stream >> savedThemes;
        if (stream.status() != QDataStream::Ok)
        {
            qWarning() << "QcwThemeManager::initialize(): ignoring a corrupt theme registry";
            savedThemes.clear();
        }
    }

    for (auto it = savedThemes.cbegin(); it != savedThemes.cend(); ++it)
    {
        const QDir dir(it.value());
        if (isThemeDirectory(dir))
        {
            mThemes.insert(it.key(), dir.absolutePath());
        }
    }

    // Themes shipped next to the executable win over stale paths from the settings file.
    // Installed data (share/qcw/themes) is scanned first so that the executable-local copy
    // takes precedence when a theme exists in both places.
    QStringList themeRoots;
    const QString installedThemes = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, QStringLiteral("qcw/themes/ui"), QStandardPaths::LocateDirectory);
    if (!installedThemes.isEmpty())
    {
        themeRoots.append(installedThemes);
    }
    themeRoots.append(QDir(QApplication::applicationDirPath()).filePath(QStringLiteral("themes/ui")));

    for (const QString& root : themeRoots)
    {
        const QDir themesDir(root);
        const auto themeNames = themesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString& themeName : themeNames)
        {
            const QDir themeDir(themesDir.absoluteFilePath(themeName));
            if (isThemeDirectory(themeDir))
            {
                mThemes.insert(themeName, themeDir.absolutePath());
            }
        }
    }

    mCurrTheme = settings.value(kCurrentThemeKey).toString();
    if (!mThemes.contains(mCurrTheme))
    {
        mCurrTheme = fallbackTheme();
    }

    saveThemes();
    saveCurrentTheme();
    mInitialized = true;

    // Guard the palette against foreign pushes from the desktop platform theme.
    qApp->installEventFilter(this);

    if (!mCurrTheme.isEmpty())
    {
        applyTheme();
        emit themeChanged();
    }
}

bool QcwThemeManager::registerTheme(const QString& themeName, const QString& themeDir)
{
    if (themeName.isEmpty() || themeDir.isEmpty())
    {
        qWarning() << "QcwThemeManager::registerTheme(): empty theme name or directory";
        return false;
    }

    const QDir dir(themeDir);
    if (!isThemeDirectory(dir))
    {
        qWarning() << "QcwThemeManager::registerTheme(): no" << QcwThemeManager::PaletteFileName << "in" << themeDir;
        return false;
    }

    if (mThemes.contains(themeName))
    {
        return false;
    }

    if (isThemeDirectoryUsed(themeDir))
    {
        qWarning() << "QcwThemeManager::registerTheme(): directory" << themeDir << "is already used by a theme";
        return false;
    }

    mThemes.insert(themeName, dir.absolutePath());
    saveThemes();
    emit themeAdded(themeName);
    return true;
}

bool QcwThemeManager::removeTheme(const QString& themeName)
{
    if (!mThemes.contains(themeName))
    {
        return false;
    }

    mThemes.remove(themeName);
    saveThemes();

    if (mCurrTheme != themeName)
    {
        return true;
    }

    mCurrTheme = fallbackTheme();
    saveCurrentTheme();

    if (mCurrTheme.isEmpty())
    {
        if (qApp)
        {
            qApp->setPalette(qApp->style()->standardPalette());
        }
        applyAppCss();
    }
    else
    {
        applyTheme();
    }

    emit themeChanged();
    return true;
}

bool QcwThemeManager::changeTheme(const QString& themeName)
{
    if (!mThemes.contains(themeName))
    {
        return false;
    }

    if (mCurrTheme == themeName)
    {
        return true;
    }

    mCurrTheme = themeName;
    saveCurrentTheme();
    applyTheme();
    emit themeChanged();
    return true;
}

QString QcwThemeManager::currentTheme() const
{
    return mCurrTheme;
}

QString QcwThemeManager::currentThemeDirectory() const
{
    return themeDirectory(mCurrTheme);
}

QString QcwThemeManager::themeDirectory(const QString& themeName) const
{
    return mThemes.value(themeName);
}

bool QcwThemeManager::hasTheme(const QString& themeName) const
{
    return mThemes.contains(themeName);
}

bool QcwThemeManager::isThemeDirectoryUsed(const QString& themeDirectory) const
{
    if (themeDirectory.isEmpty())
    {
        return false;
    }

    const QString directory = QDir(themeDirectory).absolutePath();
    for (const QString& registeredDirectory : mThemes)
    {
        if (registeredDirectory == directory)
        {
            return true;
        }
    }
    return false;
}

QJsonObject QcwThemeManager::loadWidgetThemeData(const QString& widgetType) const
{
    if (widgetType.isEmpty() || mCurrTheme.isEmpty())
    {
        return {};
    }

    QString fileName = widgetType;
    if (!fileName.endsWith(QStringLiteral(".theme"), Qt::CaseInsensitive))
    {
        fileName += QStringLiteral(".theme");
    }

    const QString filePath = QDir(currentThemeDirectory()).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError)
    {
        qWarning() << "QcwThemeManager::loadWidgetThemeData(): cannot parse" << filePath << ':' << error.errorString();
        return {};
    }

    if (!document.isObject())
    {
        qWarning() << "QcwThemeManager::loadWidgetThemeData(): not a JSON object:" << filePath;
        return {};
    }

    return document.object();
}

const QMap<QString, QString>& QcwThemeManager::allThemes() const
{
    return mThemes;
}

void QcwThemeManager::saveThemes()
{
    QByteArray serialized;
    QDataStream stream(&serialized, QIODevice::WriteOnly);
    stream << mThemes;

    auto settings = QcwSettings::get();
    settings.setValue(kThemeInfosKey, serialized);
}

void QcwThemeManager::saveCurrentTheme()
{
    auto settings = QcwSettings::get();
    settings.setValue(kCurrentThemeKey, mCurrTheme);
}

void QcwThemeManager::applyTheme()
{
    if (!qApp || mCurrTheme.isEmpty())
    {
        return;
    }

    // The application stylesheet belongs to the theme directory, not to its palette, so a theme
    // whose colors fail to parse still gets its chrome and never leaves the previous theme's rules in
    // place. Main.css is re-read on foreign palette pushes too, but the guard in applyAppCss() keeps
    // an unchanged stylesheet from triggering a repolish.
    applyAppCss();

    const QDir themeDir(currentThemeDirectory());
    const QString palettePath = themeDir.filePath(QString::fromLatin1(PaletteFileName));
    const QJsonObject data = readJsonObject(palettePath);
    const QJsonObject colors = data.value(QStringLiteral("colors")).toObject();
    if (colors.isEmpty())
    {
        qWarning() << "QcwThemeManager::applyTheme(): no colors in" << palettePath;
        return;
    }

    // Suppress our own ApplicationPaletteChange notifications while we apply, so the
    // event filter below only reacts to foreign (desktop) palette pushes.
    mApplyingPalette = true;

    const QString styleName = data.value(QStringLiteral("style")).toString();
    if (!styleName.isEmpty() && qApp->style()->objectName().compare(styleName, Qt::CaseInsensitive) != 0)
    {
        if (auto* style = QStyleFactory::create(styleName))
        {
            qApp->setStyle(style);
        }
        else
        {
            qWarning() << "QcwThemeManager::applyTheme(): unknown style" << styleName << "in" << palettePath;
        }
    }

    const auto roles = QMetaEnum::fromType<QPalette::ColorRole>();
    QPalette palette = qApp->style()->standardPalette();

    const auto applyColors = [&](const QJsonObject& values, QPalette::ColorGroup group)
    {
        for (auto it = values.constBegin(); it != values.constEnd(); ++it)
        {
            bool ok = false;
            const int role = roles.keyToValue(it.key().toLatin1().constData(), &ok);
            if (!ok)
            {
                qWarning() << "QcwThemeManager::applyTheme(): unknown color role" << it.key() << "in" << palettePath;
                continue;
            }

            const QColor color = QcwThemeUtils::parseColor(it.value().toString());
            if (!color.isValid())
            {
                qWarning() << "QcwThemeManager::applyTheme(): invalid color" << it.value() << "for role" << it.key()
                           << "in" << palettePath;
                continue;
            }

            palette.setColor(group, static_cast<QPalette::ColorRole>(role), color);
        }
    };

    // The colors cover every group; disabledColors only dims the roles that need it.
    applyColors(colors, QPalette::Active);
    applyColors(colors, QPalette::Inactive);
    applyColors(colors, QPalette::Disabled);
    applyColors(data.value(QStringLiteral("disabledColors")).toObject(), QPalette::Disabled);

    qApp->setPalette(palette);
    mApplyingPalette = false;
}

void QcwThemeManager::applyAppCss()
{
    // main.css holds the rules that are not tied to a window style - toolbars, tool buttons and
    // scrollbars - so dialogs and other top-level windows wear the same chrome as the workbench.
    // Without a current theme there is no directory to look in; QDir would otherwise turn the empty
    // path into a relative "main.css" and pick up whatever the process working directory holds.
    const QString themeDirectory = currentThemeDirectory();

    QString css;
    bool found = false;
    if (!themeDirectory.isEmpty())
    {
        QFile file(QDir(themeDirectory).filePath(QString::fromLatin1(kAppCssFileName)));
        found = file.open(QIODevice::ReadOnly);
        if (found)
        {
            css = QString::fromUtf8(file.readAll());
        }
    }

    // The application stylesheet is a single slot that the host application may already use. Take
    // it over once a theme ships main.css; after that, a theme without the file clears the rules
    // an earlier theme applied instead of leaving them behind.
    if (!found && !mAppCssApplied)
    {
        return;
    }

    // Rules that arrive through the application stylesheet must not stop the widgets from
    // following the application font and palette either.
    QCoreApplication::setAttribute(Qt::AA_UseStyleSheetPropagationInWidgetStyles);

    if (qApp->styleSheet() != css)
    {
        qApp->setStyleSheet(css);
    }

    mAppCssApplied = true;
}

bool QcwThemeManager::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && !mApplyingPalette &&
        !mCurrTheme.isEmpty())
    {
        // The desktop platform theme just overwrote our palette; put ours back and hide the
        // foreign change so widgets never repaint with the system colors.
        applyTheme();
        return true;
    }

    return QObject::eventFilter(watched, event);
}

QString QcwThemeManager::fallbackTheme() const
{
    if (mThemes.contains(DefaultThemeName))
    {
        return DefaultThemeName;
    }

    return mThemes.isEmpty() ? QString() : mThemes.constBegin().key();
}

QT_END_NAMESPACE
