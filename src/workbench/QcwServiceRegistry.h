// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwGlobal.h"
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE

/**
 * @class QcwServiceRegistry
 * @brief The process-wide registry of service objects, keyed by their interface id.
 *
 * @details Plugins are separate libraries and cannot link to each other, so a plugin that
 * offers a capability registers the object implementing it here, and a plugin that needs the
 * capability looks it up by the interface — never by the other plugin's class or type name.
 * The key is the id the interface passes to @c Q_DECLARE_INTERFACE, so the identity is part
 * of the contract rather than a string each side has to agree on separately.
 *
 * @par Writing and reading
 * @code
 * // provider, in doRegisterContributions()
 * auto* service = new EditorService;
 * if (!serviceRegistry->registerService<demo::IEditorService>(service))
 * {
 *     delete service;
 * }
 *
 * // consumer, anywhere
 * if (auto* editor = serviceRegistry->service<demo::IEditorService>())
 * {
 *     editor->revealAt(file, position);
 * }
 * @endcode
 * registerService() takes a QObject* and refuses one that does not implement the interface.
 * The service exists before any window is made, so consumers may look it up as soon as plugins
 * have loaded. A creator answers for the views it builds and nothing else, so the service is
 * not the creator object itself.
 *
 * @par Ownership
 * A successful registration transfers ownership to the registry: the service is detached
 * from any parent and lives until the process exits. The registry deletes nothing, not even
 * at shutdown, so a registered service must never be deleted — the registry holds a raw
 * pointer, and deleting it leaves that pointer dangling for every consumer. A refused service
 * stays with the caller, who must release it.
 *
 * @par One service per interface
 * The first registration for an interface wins; a later one is refused rather than replacing
 * the object, so pointers already handed out keep meaning the same service. There is no
 * deregistration: a registered service stays for the rest of the run.
 *
 * @par What is not a service
 * Services are verbs across a contract, not a way to pass values around. Topics — the current
 * subject, announcements and other shared context — travel through QcwTopicService, either at
 * window scope (QcwWindow::topicService()) or at application scope
 * (QcwWorkbench::topicService()); configuration belongs to QcwSettings.
 *
 * @note Not thread-safe; register and look up from the GUI thread, which is where plugins are
 * loaded. A service may itself be called from a worker thread if its documentation says so.
 * @note This is the single global instance; constructing a second registry is pointless
 * because nothing connects to it and lookups go through the macro.
 * @see QcwTopicService, QcwSettings
 */
class QCW_EXPORT QcwServiceRegistry
{
public:
    static QcwServiceRegistry* instance();

    /**
     * @brief Registers @p service under the id of @p Service and takes ownership of it.
     * @tparam Service Interface type declared with @c Q_DECLARE_INTERFACE.
     * @param service The object implementing @p Service; must be a QObject.
     * @return @c true when it was added, @c false when @p service is null, does not implement
     *         @p Service, or another service already answers to that interface. A refused
     *         service stays with the caller, who must delete it.
     */
    template <typename Service>
    bool registerService(QObject* service)
    {
        if (service == nullptr || qobject_cast<Service*>(service) == nullptr)
        {
            return false;
        }

        return registerObject(QString::fromUtf8(qobject_interface_iid<Service*>()), service);
    }

    /// The service registered for @p Service, or @c nullptr when there is none.
    /// The pointer stays valid for the rest of the run; do not delete it.
    template <typename Service>
    Service* service() const
    {
        return qobject_cast<Service*>(serviceObject(QString::fromUtf8(qobject_interface_iid<Service*>())));
    }

    /**
     * @brief Registers @p service under @p interfaceId and takes ownership of it.
     * @details The non-template form, for a host that keys services by an id obtained at run
     * time. Prefer registerService(), which derives the id from the interface itself.
     */
    bool registerObject(const QString& interfaceId, QObject* service);

    /// The object registered under @p interfaceId, or @c nullptr when there is none.
    /// The pointer stays valid for the rest of the run; do not delete it.
    QObject* serviceObject(const QString& interfaceId) const;

    /// True when a service is registered under @p interfaceId.
    bool contains(const QString& interfaceId) const;

    /// Every interface id that has a service.
    QStringList interfaceIds() const;

    /// How many services are registered.
    int size() const;

    /// True when no service is registered.
    bool empty() const;

private:
    QHash<QString, QObject*> mServices;
};

/// Shorthand for QcwServiceRegistry::instance().
#ifndef serviceRegistry
#define serviceRegistry QcwServiceRegistry::instance()
#endif

QT_END_NAMESPACE
