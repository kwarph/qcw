// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Exercises QcwContributionRegistry and QcwActionSetRegistry: registration, replacement and what
// a replacement is allowed to warn about, unregistration, invalid ids, and the
// contributionsChanged() signal. Registries are process globals, so every test clears what it
// touched.

#include "QcwActionSetRegistry.h"
#include "QcwContributionRegistry.h"

#include <QCoreApplication>
#include <QStringList>

#include <cstdio>
#include <functional>

namespace
{

int gFailures = 0;

void check(bool ok, const char* what, const QString& detail = {})
{
    std::fprintf(stderr, "%s %s%s%s\n", ok ? "ok  " : "FAIL", what, detail.isEmpty() ? "" : " - ",
                 detail.isEmpty() ? "" : qUtf8Printable(detail));
    if (!ok)
    {
        ++gFailures;
    }
}

void clearRegistries()
{
    for (const auto& contribution : contributionRegistry->allContributions())
    {
        contributionRegistry->unregisterContribution(contribution.id);
    }

    for (const auto& set : actionSetRegistry->allActionSets())
    {
        actionSetRegistry->unregisterActionSet(set.id);
    }
}

QcwContribution make(const QString& id,
                     const QString& target = QStringLiteral("Edit"),
                     const QString& commandId = QStringLiteral("Edit/Copy"),
                     const QString& actionSetId = {})
{
    QcwContribution contribution;
    contribution.id = id;
    contribution.target = target;
    contribution.commandId = commandId;
    contribution.actionSetId = actionSetId;
    return contribution;
}

/// Warnings the contribution registry raised while collectRegistryWarning() was the handler.
QStringList gWarnings;

void collectRegistryWarning(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    // Only this registry's own warnings, so unrelated noise cannot upset the counts below.
    if (type == QtWarningMsg && message.startsWith(QStringLiteral("QcwContributionRegistry:")))
    {
        gWarnings.append(message);
    }
}

/// Runs @p work with collectRegistryWarning() installed, and returns what it warned about.
QStringList warningsDuring(const std::function<void()>& work)
{
    gWarnings.clear();
    auto* previous = qInstallMessageHandler(collectRegistryWarning);
    work();
    qInstallMessageHandler(previous);
    return gWarnings;
}

void testReplaceWarning()
{
    clearRegistries();

    contributionRegistry->registerContribution(make("edit.copy"));

    // The same declaration again is the language-change path: nothing moved, so nothing is said.
    const auto same = warningsDuring([&]
    {
        contributionRegistry->registerContribution(make("edit.copy"));
    });
    check(same.isEmpty(), "re-registering an identical contribution warns nothing",
          same.join(QLatin1Char(' ')));

    // A duplicate id that says something else does replace the entry, and that is worth reporting.
    const auto moved = warningsDuring([&]
    {
        contributionRegistry->registerContribution(make("edit.copy", QStringLiteral("File")));
    });
    check(moved.size() == 1 && moved.first().contains(QStringLiteral("edit.copy")),
          "a duplicate id with different content warns once", moved.join(QLatin1Char(' ')));
    check(contributionRegistry->contributionsFor(QStringLiteral("File")).size() == 1,
          "the replacement still happens");

    clearRegistries();
}

void testRegisterAndLookup()
{
    clearRegistries();

    check(contributionRegistry->registerContribution(make("edit.copy")), "register a valid contribution");

    const auto contributions = contributionRegistry->contributionsFor(QStringLiteral("Edit"));
    check(contributions.size() == 1 && contributions[0].id == QStringLiteral("edit.copy"),
          "lookup returns the registered contribution", QString::number(contributions.size()));
}

void testReplaceEmitsOnce()
{
    clearRegistries();

    contributionRegistry->registerContribution(make("edit.copy"));

    int emitted = 0;
    QMetaObject::Connection conn =
        QObject::connect(contributionRegistry, &QcwContributionRegistry::contributionsChanged, [&emitted]()
    {
        ++emitted;
    });

    check(contributionRegistry->registerContribution(make("edit.copy", QStringLiteral("File"))),
          "replace a contribution");
    QObject::disconnect(conn);

    check(emitted == 1, "replace emits contributionsChanged once", QString::number(emitted));
    check(contributionRegistry->contributionsFor(QStringLiteral("File")).size() == 1,
          "the replacement moves the contribution to the new target");
    check(contributionRegistry->contributionsFor(QStringLiteral("Edit")).isEmpty(),
          "the old target no longer holds it");
}

void testUnregister()
{
    clearRegistries();

    contributionRegistry->registerContribution(make("edit.copy"));

    int emitted = 0;
    QMetaObject::Connection conn =
        QObject::connect(contributionRegistry, &QcwContributionRegistry::contributionsChanged, [&emitted]()
    {
        ++emitted;
    });

    check(contributionRegistry->unregisterContribution(QStringLiteral("edit.copy")),
          "unregister an existing contribution");
    QObject::disconnect(conn);

    check(emitted == 1, "unregister emits contributionsChanged once", QString::number(emitted));
    check(contributionRegistry->allContributions().isEmpty(), "the registry is empty after unregister");
    check(!contributionRegistry->unregisterContribution(QStringLiteral("edit.copy")),
          "unregistering a missing id returns false");
}

void testInvalidIdsRejected()
{
    clearRegistries();

    check(!contributionRegistry->registerContribution(make(QString())), "empty id rejected");
    check(!contributionRegistry->registerContribution(make(QStringLiteral("has space"))), "id with space rejected");
    check(!contributionRegistry->registerContribution(make(QStringLiteral("with:colon"))), "id with colon rejected");
    check(!contributionRegistry->registerContribution(make("edit.copy", QString())), "empty target rejected");
    check(!contributionRegistry->registerContribution(make("edit.copy", QStringLiteral("Edit"), QString())),
          "empty commandId rejected");

    check(contributionRegistry->allContributions().isEmpty(), "rejected registrations leave the registry empty");
}

void testUnregisterContributionsOf()
{
    clearRegistries();

    contributionRegistry->registerContribution(
        make("edit.copy", QStringLiteral("Edit"), QStringLiteral("Edit/Copy"), QStringLiteral("edit")));
    contributionRegistry->registerContribution(
        make("edit.paste", QStringLiteral("Edit"), QStringLiteral("Edit/Paste"), QStringLiteral("edit")));
    contributionRegistry->registerContribution(
        make("edit.cut", QStringLiteral("Edit"), QStringLiteral("Edit/Cut"), QStringLiteral("debug")));

    int emitted = 0;
    QMetaObject::Connection conn =
        QObject::connect(contributionRegistry, &QcwContributionRegistry::contributionsChanged, [&emitted]()
    {
        ++emitted;
    });

    const int removed = contributionRegistry->unregisterContributionsOf(QStringLiteral("edit"));
    QObject::disconnect(conn);

    check(removed == 2, "unregisterContributionsOf counts what it removed", QString::number(removed));
    check(emitted == 1, "it emits once", QString::number(emitted));
    check(contributionRegistry->allContributions().size() == 1 &&
              contributionRegistry->allContributions()[0].id == QStringLiteral("edit.cut"),
          "only the matching action set is gone");

    check(contributionRegistry->unregisterContributionsOf(QStringLiteral("edit")) == 0,
          "a second call removes nothing");
}

void testActionSetRegistry()
{
    clearRegistries();

    check(actionSetRegistry->registerActionSet({ QStringLiteral("edit"), QStringLiteral("Edit") }),
          "register an action set");
    check(actionSetRegistry->contains(QStringLiteral("edit")), "the action set is present");
    check(actionSetRegistry->actionSet(QStringLiteral("edit")) &&
              actionSetRegistry->actionSet(QStringLiteral("edit"))->label == QStringLiteral("Edit"),
          "the label round-trips");

    check(actionSetRegistry->registerActionSet({ QStringLiteral("edit"), QStringLiteral("Editor") }),
          "re-register an action set");
    check(actionSetRegistry->actionSet(QStringLiteral("edit"))->label == QStringLiteral("Editor"),
          "the label is replaced");

    check(!actionSetRegistry->registerActionSet({ QString(), QStringLiteral("Empty") }),
          "empty action set id rejected");
    check(!actionSetRegistry->registerActionSet({ QStringLiteral("has space"), QStringLiteral("Bad") }),
          "invalid action set id rejected");
    check(!actionSetRegistry->registerActionSet({ QStringLiteral("no-label"), QString() }), "empty label rejected");

    check(actionSetRegistry->unregisterActionSet(QStringLiteral("edit")), "unregister an action set");
    check(!actionSetRegistry->contains(QStringLiteral("edit")), "the action set is gone");
    check(!actionSetRegistry->unregisterActionSet(QStringLiteral("edit")),
          "unregistering a missing action set returns false");
}

void testAllActionSetsSorted()
{
    clearRegistries();

    actionSetRegistry->registerActionSet({ QStringLiteral("zzz"), QStringLiteral("Z") });
    actionSetRegistry->registerActionSet({ QStringLiteral("aaa"), QStringLiteral("A") });

    const auto sets = actionSetRegistry->allActionSets();
    check(sets.size() == 2 && sets[0].id == QStringLiteral("aaa") && sets[1].id == QStringLiteral("zzz"),
          "allActionSets is sorted by id");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    testRegisterAndLookup();
    testReplaceEmitsOnce();
    testReplaceWarning();
    testUnregister();
    testInvalidIdsRejected();
    testUnregisterContributionsOf();
    testActionSetRegistry();
    testAllActionSetsSorted();

    clearRegistries();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
