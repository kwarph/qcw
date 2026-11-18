// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

// Checks the context menu and named-action container every QcwView exposes: lazy creation,
// the view owning the menu, stable identity across calls, and a borrowed action map.

#include "QcwView.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QTextStream>

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

class TestView : public QcwView
{
public:
    using QcwView::contextMenu;
    using QcwView::namedActions;

    explicit TestView(QWidget* parent = nullptr) :
        QcwView { parent }
    {
    }
};

} // namespace

int main(int argc, char* argv[])
{
    QApplication app { argc, argv };

    TestView view;
    check(view.namedActions().isEmpty(), QStringLiteral("a new view starts with no named actions"));

    QMenu* menu = view.contextMenu();
    check(menu != nullptr, QStringLiteral("contextMenu() creates the menu"));
    check(menu == view.contextMenu(), QStringLiteral("contextMenu() returns the same menu on every call"));
    check(menu->parent() == &view, QStringLiteral("the view owns the context menu"));
    check(menu->actions().isEmpty(), QStringLiteral("QcwView adds nothing to the menu by default"));

    QAction action { &view };
    action.setText(QStringLiteral("Test Action"));
    view.namedActions().insert(QLatin1String("Test/action"), &action);
    check(view.namedActions().size() == 1, QStringLiteral("namedActions() accepts an action"));
    check(view.namedActions().value(QLatin1String("Test/action")) == &action,
          QStringLiteral("the named action is retrievable by its name"));

    view.namedActions().remove(QLatin1String("Test/action"));
    check(view.namedActions().isEmpty(), QStringLiteral("a named action can be removed"));

    TestView other;
    check(other.contextMenu() != menu, QStringLiteral("each view has its own context menu"));
    check(other.namedActions().isEmpty(), QStringLiteral("each view has its own named-action map"));

    if (failures == 0)
    {
        QTextStream { stdout } << "passed\n";
    }
    return failures == 0 ? 0 : 1;
}
