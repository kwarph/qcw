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
    check(menu != nullptr, QStringLiteral("a view is constructed with its context menu"));
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

    // addContextMenuSection() appends a separated section; an empty title adds the actions
    // directly, a non-empty one puts them in a submenu.
    QAction sectionAction { &view };
    sectionAction.setText(QStringLiteral("Section Action"));
    view.addContextMenuSection(QStringLiteral("Section"), { &sectionAction });
    check(menu->actions().size() == 2 && menu->actions().at(0)->isSeparator(),
          QStringLiteral("addContextMenuSection() separates the new section"));
    QMenu* section = menu->actions().at(1)->menu();
    check(section != nullptr && section->actions() == QList<QAction*> { &sectionAction },
          QStringLiteral("the section is a submenu holding the actions"));

    // insertContextMenuSection() places a section around an action registered in namedActions().
    QAction anchorAction { &view };
    anchorAction.setText(QStringLiteral("Anchor Action"));
    view.namedActions().insert(QLatin1String("Test/anchor"), &anchorAction);
    view.addContextMenuSection(QString(), { &anchorAction });

    QAction beforeAction { &view };
    beforeAction.setText(QStringLiteral("Before Action"));
    view.insertContextMenuSection(true, QLatin1String("Test/anchor"), QString(), { &beforeAction });
    check(menu->actions().indexOf(&beforeAction) != -1 &&
              menu->actions().indexOf(&beforeAction) < menu->actions().indexOf(&anchorAction),
          QStringLiteral("insertContextMenuSection(before) places the actions before the anchor"));

    QAction afterAction { &view };
    afterAction.setText(QStringLiteral("After Action"));
    view.insertContextMenuSection(false, QLatin1String("Test/anchor"), QString(), { &afterAction });
    check(menu->actions().indexOf(&afterAction) > menu->actions().indexOf(&anchorAction),
          QStringLiteral("insertContextMenuSection(after) places the actions after the anchor"));

    TestView other;
    check(other.contextMenu() != menu, QStringLiteral("each view has its own context menu"));
    check(other.namedActions().isEmpty(), QStringLiteral("each view has its own named-action map"));

    if (failures == 0)
    {
        QTextStream { stdout } << "passed\n";
    }
    return failures == 0 ? 0 : 1;
}
