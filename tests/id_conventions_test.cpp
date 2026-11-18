// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Checks the ids QCW publishes against docs/qcw-id-naming-conventions.md: the built-in
// commands and contributions after registration, the menu and tool bar targets, the payload
// key, and the interface IIDs. The runtime registries only validate the character set; the
// structure is enforced here.

#include "QcwBuiltinCommands.h"
#include "QcwCommandRegistry.h"
#include "QcwContributionRegistry.h"
#include "QcwPerspectiveCreator.h"
#include "QcwViewCreator.h"
#include "QcwWindowActionIds.h"

#include <QApplication>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <cstdio>

namespace
{

int failures = 0;

void check(bool condition, const QString& message)
{
    QTextStream stream { condition ? stdout : stderr };
    stream << (condition ? "ok: " : "FAIL: ") << message << '\n';
    if (!condition)
    {
        ++failures;
    }
}

bool matches(const QRegularExpression& expression, QLatin1String value)
{
    return expression.match(QString { value }).hasMatch();
}

bool matches(const QRegularExpression& expression, const QString& value)
{
    return expression.match(value).hasMatch();
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app { argc, argv };

    // A command id is [owner/]<MenuPath>/<action>; the owner and the action are lowerCamelCase,
    // the menu path segments are UpperCamelCase.
    const QRegularExpression command { QStringLiteral("^([a-z][a-z0-9]*/)?([A-Z][A-Za-z0-9]*/)+[a-z][A-Za-z0-9]*$") };
    const QRegularExpression menu { QStringLiteral("^[A-Z][A-Za-z0-9]*(/[A-Z][A-Za-z0-9]*)*$") };
    const QRegularExpression toolBar { QStringLiteral("^ToolBar/[A-Z][A-Za-z0-9]*$") };
    const QRegularExpression payloadKey { QStringLiteral("^[a-z][A-Za-z0-9]*$") };
    const QRegularExpression iid { QStringLiteral("^([a-z][a-z0-9]*\\.){2,}[A-Z][A-Za-z0-9]*(/[0-9]+\\.[0-9]+)?$") };

    QcwBuiltinCommands::unregisterCommands();
    QcwBuiltinCommands::registerCommands();

    for (const auto& info : commandRegistry->commands())
    {
        check(matches(command, info.id), QStringLiteral("command id follows the convention: %1").arg(info.id));
        check(matches(menu, info.category),
              QStringLiteral("command category is a menu target: %1 (%2)").arg(info.id, info.category));
    }

    for (const auto& contribution : contributionRegistry->allContributions())
    {
        check(matches(command, contribution.id),
              QStringLiteral("contribution id follows the convention: %1").arg(contribution.id));
        check(matches(menu, contribution.target) || matches(toolBar, contribution.target),
              QStringLiteral("contribution target is a menu or tool bar target: %1 (%2)")
                  .arg(contribution.id, contribution.target));
        check(matches(command, contribution.commandId),
              QStringLiteral("contribution names a command id: %1 (%2)").arg(contribution.id, contribution.commandId));
    }

    const QList<QLatin1String> targets {
        QcwWindowActionIds::MenuFile,
        QcwWindowActionIds::MenuEdit,
        QcwWindowActionIds::MenuWindow,
        QcwWindowActionIds::MenuHelp,
        QcwWindowActionIds::MenuWindowAppearance,
        QcwWindowActionIds::MenuWindowPerspective,
        QcwWindowActionIds::MenuNewView,
    };
    for (const auto target : targets)
    {
        check(matches(menu, target), QStringLiteral("menu target follows the convention: %1").arg(target));
    }

    check(matches(toolBar, QcwWindowActionIds::ToolBarMain),
          QStringLiteral("tool bar target follows the convention: %1").arg(QcwWindowActionIds::ToolBarMain));
    check(matches(payloadKey, QcwWindowActionIds::ParamWindowId),
          QStringLiteral("payload key is lowerCamelCase: %1").arg(QcwWindowActionIds::ParamWindowId));

    check(matches(iid, QLatin1String { QcwViewCreator_iid }),
          QStringLiteral("view creator IID is a reverse-DNS interface id: %1").arg(QcwViewCreator_iid));
    check(matches(iid, QLatin1String { QcwPerspectiveCreator_iid }),
          QStringLiteral("perspective creator IID is a reverse-DNS interface id: %1").arg(QcwPerspectiveCreator_iid));

    QcwBuiltinCommands::unregisterCommands();

    if (failures == 0)
    {
        QTextStream { stdout } << "passed\n";
    }
    return failures == 0 ? 0 : 1;
}
