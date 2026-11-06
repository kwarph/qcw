// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

/**
 * @file QcwPluginTemplate.h
 * @brief Macros that write the @ref QcwPlugin metadata boilerplate of a plugin class.
 *
 * @details Every plugin repeats the same seven one-line accessors, and the values come from
 * constants rather than code, so the plugin's own header only declares what it really adds:
 *
 * @code
 * class SampleViewCreator : public QcwViewCreator
 * {
 *     Q_OBJECT
 *     Q_PLUGIN_METADATA(IID "org.qcw.QcwViewCreator" FILE "sampleview.json")
 *     Q_INTERFACES(QcwViewCreator)
 *     QCW_PLUGIN_INTERFACE
 *
 * public:
 *     QString defaultWindowTitle() const override;
 *     QIcon defaultViewIcon() const override;
 * private:
 *     QcwView* doCreate(QWidget* parent, const QList<QVariant>& args) override;
 * };
 *
 * // in the .cpp, alongside the implementations above
 * QCW_PLUGIN_IMPLEMENT(SampleViewCreator, "SampleView")
 * @endcode
 *
 * @note QCW_PLUGIN_INTERFACE starts a @c public: section, which is what makes the members it
 * declares public in a @c class.
 * @note QCW_PLUGIN_IMPLEMENT needs these five macros defined by the plugin — the convention is
 * a small PluginInfo.h next to the source:
 * @c QCW_PLUGIN_VERSION, @c QCW_PLUGIN_PROVIDER, @c QCW_PLUGIN_UPDATE_TIME,
 * @c QCW_PLUGIN_DESCRIPTION and @c QCW_PLUGIN_DETAILS. All are string literals; the last two
 * go through tr(), so write them untranslated and wrapped in
 * <tt>QT_TRANSLATE_NOOP("ClassName", "...")</tt> — lupdate cannot see through the
 * macro argument, and the context has to match the creator's class name.
 * @note The second argument of QCW_PLUGIN_IMPLEMENT is the QcwPlugin::type() string, which has
 * to match what the saved state refers to.
 */

// clang-format off

#undef QCW_PLUGIN_INTERFACE
#define QCW_PLUGIN_INTERFACE                        \
public:                                             \
    QString type() const override;                  \
    QString version() const override;               \
    QString provider() const override;              \
    QString updateTime() const override;            \
    QString description() const override;           \
    QString details() const override;

#undef QCW_PLUGIN_IMPLEMENT
#define QCW_PLUGIN_IMPLEMENT(CreatorType, TypeId)   \
                                                    \
    QString CreatorType::type() const               \
    {                                               \
        return TypeId;                              \
    }                                               \
                                                    \
    QString CreatorType::version() const            \
    {                                               \
        return QCW_PLUGIN_VERSION;                  \
    }                                               \
                                                    \
    QString CreatorType::provider() const           \
    {                                               \
        return QCW_PLUGIN_PROVIDER;                 \
    }                                               \
                                                    \
    QString CreatorType::updateTime() const         \
    {                                               \
        return QCW_PLUGIN_UPDATE_TIME;              \
    }                                               \
                                                    \
    QString CreatorType::description() const        \
    {                                               \
        return tr(QCW_PLUGIN_DESCRIPTION);          \
    }                                               \
                                                    \
    QString CreatorType::details() const            \
    {                                               \
        return tr(QCW_PLUGIN_DETAILS);              \
    }

// clang-format on
