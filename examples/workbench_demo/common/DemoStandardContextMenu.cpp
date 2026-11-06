
#include "DemoStandardContextMenu.h"

#include <QMenu>
#include <QPlainTextEdit>

class QMenu;
class QPlainTextEdit;

namespace demo
{

void retranslateStandardContextMenu(QMenu* menuInUse, QPlainTextEdit* editor)
{
    if (!menuInUse || !editor)
    {
        return;
    }

    auto* fresh = editor->createStandardContextMenu();
    const auto sources = fresh ? fresh->actions() : QList<QAction*> {};
    for (auto* action : menuInUse->actions())
    {
        const auto name = action->objectName();
        if (name.isEmpty()) // a separator carries no command to match
        {
            continue;
        }

        for (auto* source : sources)
        {
            if (source->objectName() == name)
            {
                action->setText(source->text());
                break;
            }
        }
    }

    delete fresh;
}
} // namespace demo
