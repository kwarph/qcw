// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwThemeManager.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @class QcwWidgetTheme
 * @brief Loads a theme-aware widget's data file and reloads it when the theme changes.
 *
 * @tparam ThemeType Describes one widget's theme. It must provide:
 * @li a static member @c themeFile that converts to QString and names the file without the
 *     @c .theme suffix, for example @c "QcwTabFolder";
 * @li a static function @c doLoad(const QJsonObject&) that stores the values it needs in
 *     static members of @c ThemeType. It must also accept an empty object and fall back to
 *     the widget's defaults, because no theme may be current or the current theme may not
 *     ship a file for every widget.
 *
 * @details The first QcwWidgetTheme object created for a @c ThemeType connects @c load() to
 * QcwThemeManager::themeChanged() and loads the data of the current theme; further objects of
 * the same type do nothing. Create one instance where the widget is set up:
 * @code
 * struct QcwTabFolderTheme
 * {
 *     static constexpr const char* themeFile = "QcwTabFolder";
 *     static void doLoad(const QJsonObject& jobj);
 *     static inline QString tabBarCss;
 * };
 *
 * QcwTabFolder::QcwTabFolder(QWidget* parent) : QFrame(parent)
 * {
 *     static const QcwWidgetTheme<QcwTabFolderTheme> themeLoader;
 * }
 * @endcode
 *
 * Before doLoad() is called, every string value in the JSON data has its @c %{themeDirectory}
 * placeholders replaced with the current theme directory (with forward slashes), so CSS in a
 * @c .theme file can refer to theme images as @c "url(%{themeDirectory}/images/close.svg)". Other
 * placeholders, such as @c %{bgColor}, are left for doLoad() to handle.
 *
 * The instance only starts the machinery; it holds no data and can stay a function-local
 * static. There is no way to stop the automatic reloading; the connection lives as long as
 * the process, and plugins are never unloaded.
 *
 * @note All objects and load() must be used on the GUI thread.
 * @see QcwThemeManager, QcwThemeUtils
 */
template <typename ThemeType>
class QcwWidgetTheme
{
public:
    /// Connects the themeChanged() reload hook and loads the current theme's data once.
    QcwWidgetTheme()
    {
        ensureInitialized();
    }

    /**
     * @brief Reloads the widget theme data from the current theme.
     *
     * @details Loads the current theme's file, expands the @c %{themeDirectory} placeholders and
     * passes the result to @c ThemeType::doLoad(). doLoad() is called even when no theme is
     * current or the file is missing or invalid; the empty object lets it discard overrides
     * that are left over from the previous theme and restore the widget's defaults.
     */
    static void load()
    {
        QJsonObject data = themeManager->loadWidgetThemeData(ThemeType::themeFile);

        const QString themeDirectory = QDir::fromNativeSeparators(themeManager->currentThemeDirectory());
        if (!themeDirectory.isEmpty())
        {
            expandThemeDirectory(data, themeDirectory);
        }

        ThemeType::doLoad(data);
    }

private:
    static void ensureInitialized()
    {
        static const bool initialized = []
        {
            QObject::connect(themeManager, &QcwThemeManager::themeChanged, themeManager, []
            {
                QcwWidgetTheme<ThemeType>::load();
            });
            QcwWidgetTheme<ThemeType>::load();
            return true;
        }();

        Q_UNUSED(initialized)
    }

    static void expandThemeDirectory(QJsonObject& object, const QString& themeDirectory)
    {
        for (const QString& key : object.keys())
        {
            QJsonValue value = object.value(key);
            expandThemeDirectory(value, themeDirectory);
            object.insert(key, value);
        }
    }

    static void expandThemeDirectory(QJsonArray& array, const QString& themeDirectory)
    {
        for (qsizetype i = 0; i < array.size(); ++i)
        {
            QJsonValue value = array.at(i);
            expandThemeDirectory(value, themeDirectory);
            array.replace(i, value);
        }
    }

    static void expandThemeDirectory(QJsonValue& value, const QString& themeDirectory)
    {
        if (value.isString())
        {
            QString text = value.toString();
            text.replace(QStringLiteral("%{themeDirectory}"), themeDirectory);
            value = text;
        }
        else if (value.isObject())
        {
            QJsonObject object = value.toObject();
            expandThemeDirectory(object, themeDirectory);
            value = object;
        }
        else if (value.isArray())
        {
            QJsonArray array = value.toArray();
            expandThemeDirectory(array, themeDirectory);
            value = array;
        }
    }
};

QT_END_NAMESPACE
