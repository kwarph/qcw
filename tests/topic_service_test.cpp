// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTopicRegistry.h"
#include "QcwTopicService.h"

#include <QCoreApplication>
#include <QObject>
#include <cstdio>

namespace
{

int gFailures = 0;
void check(bool ok, const char* what)
{
    std::fprintf(stderr, "%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++gFailures;
    }
}

void testRegistry()
{
    QcwTopicInfo info;
    info.id = QStringLiteral("win/selection");
    info.description = QStringLiteral("Selection");
    info.publisher = QStringLiteral("Test");
    info.isStateful = true;

    check(topicRegistry->registerTopic(info), "register valid topic");
    check(topicRegistry->allTopics().size() == 1, "registry size is 1");
    check(topicRegistry->allTopics()[0].id == QStringLiteral("win/selection"), "topic id matches");
}

void testServiceStateAndSubscribe()
{
    QcwTopicService service;
    int callCount = 0;
    QVariantMap lastPayload;

    QObject receiver;

    // Subscribe before any publish
    service.subscribe(QStringLiteral("win/test"), &receiver, [&](const QVariantMap& map)
    {
        ++callCount;
        lastPayload = map;
    });

    check(callCount == 0, "no immediate callback without cached state");

    service.publish(QStringLiteral("win/test"), { { "value", 42 } });
    check(callCount == 1, "callback invoked on publish");
    check(lastPayload["value"].toInt() == 42, "payload matches");

    // Subscribe after publish -> should replay immediately
    QObject receiver2;
    int callCount2 = 0;
    service.subscribe(QStringLiteral("win/test"), &receiver2, [&](const QVariantMap&)
    {
        ++callCount2;
    });

    check(callCount2 == 1, "callback invoked immediately with cached state");
}

void testUnsubscribe()
{
    QcwTopicService service;

    service.publish(QStringLiteral("win/test"), { { "value", 1 } });

    int callCount1 = 0;
    auto* receiver1 = new QObject();
    service.subscribe(QStringLiteral("win/test"), receiver1, [&](const QVariantMap&)
    {
        ++callCount1;
    });
    check(callCount1 == 1, "receiver1 got immediate replay");

    service.unsubscribe(QStringLiteral("win/test"), receiver1);
    service.publish(QStringLiteral("win/test"), { { "value", 2 } });
    check(callCount1 == 1, "receiver1 not invoked after explicit unsubscribe");

    int callCount2 = 0;
    auto* receiver2 = new QObject();
    service.subscribe(QStringLiteral("win/test"), receiver2, [&](const QVariantMap&)
    {
        ++callCount2;
    });
    check(callCount2 == 1, "receiver2 got immediate replay");

    delete receiver2;
    service.publish(QStringLiteral("win/test"), { { "value", 3 } });
    check(callCount2 == 1, "receiver2 not invoked after destruction");

    delete receiver1;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    testRegistry();
    testServiceStateAndSubscribe();
    testUnsubscribe();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
