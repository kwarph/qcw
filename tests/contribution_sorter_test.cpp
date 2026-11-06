// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Exercises QcwContributionSorter on plain values: filtering by action set, before/after as
// contribution ids and command ids, @start pinning, cycle handling and the append rules. The
// sorter is a pure function, so this test never builds a window or reads the registries.

#include "QcwContributionSorter.h"

#include <QCoreApplication>

#include <cstdio>

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

QcwContribution make(const QString& id,
                     const QString& commandId,
                     const QString& before = {},
                     const QString& after = {},
                     const QString& actionSetId = {},
                     bool visible = true)
{
    QcwContribution contribution;
    contribution.id = id;
    contribution.target = QStringLiteral("Edit");
    contribution.commandId = commandId;
    contribution.before = before;
    contribution.after = after;
    contribution.actionSetId = actionSetId;
    contribution.visible = visible;
    return contribution;
}

QString idsOf(const QList<QcwContribution>& contributions)
{
    QStringList ids;
    for (const auto& contribution : contributions)
    {
        ids.append(contribution.id);
    }
    return ids.join(QLatin1Char(','));
}

void expectOrder(const QList<QcwContribution>& actual, const QString& expected, const char* what)
{
    check(idsOf(actual) == expected, what, QStringLiteral("actual=%1 expected=%2").arg(idsOf(actual), expected));
}

void testNoConstraintsKeepsRegistrationOrder()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste"),
        make("c3", "Edit/Cut"),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2,c3"),
                "no constraints: registration order");
}

void testBeforeByContributionId()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste", QStringLiteral("c1")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c2,c1"),
                "before by contribution id");
}

void testAfterByContributionId()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste", {}, QStringLiteral("c1")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2"),
                "after by contribution id");
}

void testBeforeByCommandId()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste", QStringLiteral("Edit/Copy")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c2,c1"),
                "before by command id");
}

void testAfterByCommandId()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste", {}, QStringLiteral("Edit/Copy")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2"),
                "after by command id");
}

void testSelfReferenceIgnored()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", QStringLiteral("c1")),
        make("c2", "Edit/Paste", {}, QStringLiteral("c2")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2"),
                "self-reference ignored");
}

void testMissingReferenceIgnored()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", QStringLiteral("does-not-exist")),
        make("c2", "Edit/Paste"),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2"),
                "unresolved reference keeps registration order");
}

void testCycleFallsBackToRegistrationOrder()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", QStringLiteral("c2")),
        make("c2", "Edit/Paste", QStringLiteral("c1")),
        make("c3", "Edit/Cut"),
    };

    // c1 and c2 form a cycle; both are appended in registration order after any constrained
    // entry that could be emitted (none here). c3 is unconstrained and appends last.
    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2,c3"),
                "cycle falls back to registration order");
}

void testStartAnchorPinsFirst()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste", QStringLiteral("@start")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c2,c1"),
                "@start pins first");
}

void testStartAnchorOverridesOtherConstraints()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy"),
        make("c2", "Edit/Paste", QStringLiteral("@start"), QStringLiteral("c1")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c2,c1"),
                "@start wins over after");
}

void testActionSetFiltering()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", {}, {}, QStringLiteral("edit")),
        make("c2", "Edit/Paste", {}, {}, QStringLiteral("debug")),
        make("c3", "Edit/Cut"),
    };

    expectOrder(QcwContributionSorter::sort(input, { QStringLiteral("edit") }, QStringLiteral("Edit")),
                QStringLiteral("c1,c3"), "only visible action sets");
}

void testActionSetEmptyMeansAllVisible()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", {}, {}, QStringLiteral("edit")),
        make("c2", "Edit/Paste", {}, {}, QStringLiteral("debug")),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c1,c2"),
                "empty visible set means all visible");
}

void testUnregisteredActionSetHidesContribution()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", {}, {}, QStringLiteral("never")),
        make("c2", "Edit/Paste"),
    };

    expectOrder(QcwContributionSorter::sort(input, { QStringLiteral("edit") }, QStringLiteral("Edit")),
                QStringLiteral("c2"), "unknown action set hides contribution");
}

void testInvisibleContributionDropped()
{
    const QList<QcwContribution> input {
        make("c1", "Edit/Copy", {}, {}, {}, false),
        make("c2", "Edit/Paste"),
    };

    expectOrder(QcwContributionSorter::sort(input, {}, QStringLiteral("Edit")), QStringLiteral("c2"),
                "invisible contribution dropped");
}

void testEmptyInput()
{
    check(QcwContributionSorter::sort({}, {}, QStringLiteral("Edit")).isEmpty(), "empty input yields empty output");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    testNoConstraintsKeepsRegistrationOrder();
    testBeforeByContributionId();
    testAfterByContributionId();
    testBeforeByCommandId();
    testAfterByCommandId();
    testSelfReferenceIgnored();
    testMissingReferenceIgnored();
    testCycleFallsBackToRegistrationOrder();
    testStartAnchorPinsFirst();
    testStartAnchorOverridesOtherConstraints();
    testActionSetFiltering();
    testActionSetEmptyMeansAllVisible();
    testUnregisteredActionSetHidesContribution();
    testInvisibleContributionDropped();
    testEmptyInput();

    std::fprintf(stderr, "%s (%d failure(s))\n", gFailures == 0 ? "PASS" : "FAIL", gFailures);
    return gFailures == 0 ? 0 : 1;
}
