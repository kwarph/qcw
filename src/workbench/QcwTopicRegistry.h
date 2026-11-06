// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

QT_BEGIN_NAMESPACE

/**
 * @struct QcwTopicInfo
 * @brief Metadata describing a topic published through QcwTopicService.
 * @details Registration is optional; the registry is a catalog for discovery, documentation
 *          and debug tooling. QcwTopicService neither requires nor reads it.
 */
struct QCW_EXPORT QcwTopicInfo
{
    /// The topic id; use an "app/" or "win/" prefix.
    QString id;

    /// What the topic means and when it is published or cleared.
    QString description;

    /// The plugin or subsystem that publishes the topic.
    QString publisher;

    /// Whether the topic describes persistent state. Descriptive only at present:
    /// QcwTopicService caches every topic.
    bool isStateful { true };

    /// Expected payload keys and their type names, for documentation and tooling.
    QList<QPair<QString, QString>> payloadKeys;
};

/**
 * @class QcwTopicRegistry
 * @brief The process-wide catalog of topics published through QcwTopicService.
 *
 * @details Registration is optional. The catalog lets tooling, debug panels and developers
 * discover which topics exist, who publishes them, and which keys their payloads carry. Topics
 * published without registration still work; QcwTopicService does not consult the registry.
 * Registration checks only the id shape: an empty id is refused, and an id that starts with
 * neither "app/" nor "win/" is accepted with a warning.
 *
 * @par Ownership
 * The registry stores a copy of @p info and owns nothing. Registering again under the same id
 * replaces the copy, which is how a publisher updates its metadata; nothing in the framework
 * unloads a publisher, so there is no unregistration.
 *
 * The registry is not thread-safe; register and query from the GUI thread. This is the single
 * global instance — nothing connects to it, and lookups go through topicRegistry, so a second
 * registry would never be seen.
 *
 * @see QcwTopicService
 */
class QCW_EXPORT QcwTopicRegistry
{
public:
    /// The single global instance; use the topicRegistry macro rather than calling this directly.
    static QcwTopicRegistry* instance();

    /**
     * @brief Adds or replaces @p info.
     * @param info The topic metadata to register.
     * @return False when @p info.id is empty; true otherwise, including when the id does not
     *         follow the "app/" or "win/" naming convention (a warning is printed).
     * @details Registering an id that already exists replaces its metadata.
     */
    bool registerTopic(const QcwTopicInfo& info);

    /**
     * @brief Every registered topic, sorted by id.
     * @return A snapshot; registrations made afterwards do not change it.
     */
    QList<QcwTopicInfo> allTopics() const;

    /// The topics whose publisher equals @p publisher, sorted by id.
    QList<QcwTopicInfo> topicsByPublisher(const QString& publisher) const;

private:
    QMap<QString, QcwTopicInfo> mTopics;
};

/// Shorthand for QcwTopicRegistry::instance().
#ifndef topicRegistry
#define topicRegistry QcwTopicRegistry::instance()
#endif

QT_END_NAMESPACE
