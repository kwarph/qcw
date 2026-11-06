// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "EditorService.h"

#include "DemoUtils.h"
#include "QcwPerspective.h"
#include "QcwTabFolder.h"
#include "QcwTabFolderContainer.h"
#include "QcwWindow.h"
#include "TextEditor.h"

#include <QList>

#include <utility>

namespace
{
QcwTabFolder* folderOf(QWidget* pane)
{
    for (QWidget* widget = pane; widget; widget = widget->parentWidget())
    {
        if (auto* folder = qobject_cast<QcwTabFolder*>(widget))
        {
            return folder;
        }
    }

    return nullptr;
}
} // namespace

EditorService::EditorService(QObject* parent) :
    QObject { parent }
{
}

void EditorService::trackEditor(QWidget* editor)
{
    auto* textEditor = qobject_cast<TextEditor*>(editor);
    if (!textEditor || mEditors.contains(textEditor))
    {
        return;
    }

    mEditors.insert(textEditor);
    connect(textEditor, &QObject::destroyed, this, [this, textEditor]()
    {
        mEditors.remove(textEditor);
    });
}

void EditorService::applyAppearanceSettings()
{
    for (auto* editor : std::as_const(mEditors))
    {
        editor->applyAppearanceSettings();
    }
}

QWidget* EditorService::editorForFile(const QString& filePath) const
{
    const QString wanted = demo::normalizedPath(filePath);
    if (wanted.isEmpty())
    {
        return nullptr;
    }

    for (auto* editor : mEditors)
    {
        if (!editor->filePath().isEmpty() && demo::normalizedPath(editor->filePath()) == wanted)
        {
            return editor;
        }
    }

    return nullptr;
}

QString EditorService::editorFilePath(QWidget* editor) const
{
    if (auto* textEditor = qobject_cast<TextEditor*>(editor))
    {
        return textEditor->filePath();
    }

    return {};
}

bool EditorService::saveEditor(QWidget* editor, QString* error)
{
    auto* textEditor = qobject_cast<TextEditor*>(editor);
    if (!textEditor)
    {
        if (error)
        {
            *error = tr("The selected file is not open in an editor.");
        }
        return false;
    }

    return textEditor->save(error);
}

bool EditorService::saveEditorTo(QWidget* editor, const QString& targetPath, QString* error)
{
    auto* textEditor = qobject_cast<TextEditor*>(editor);
    if (!textEditor)
    {
        if (error)
        {
            *error = tr("The selected file is not open in an editor.");
        }
        return false;
    }

    return textEditor->saveTo(targetPath, error);
}

void EditorService::remapEditorPaths(const QString& oldPrefix, const QString& newPrefix)
{
    if (oldPrefix.isEmpty() || newPrefix.isEmpty())
    {
        return;
    }

    const QString oldPath = demo::normalizedPath(oldPrefix);
    const QString newPath = demo::normalizedPath(newPrefix);

    for (auto* editor : std::as_const(mEditors))
    {
        const QString path = demo::normalizedPath(editor->filePath());
        if (path == oldPath)
        {
            editor->setFilePath(newPath);
        }
        else if (path.startsWith(oldPath.endsWith(QLatin1Char('/')) ? oldPath : oldPath + QLatin1Char('/')))
        {
            editor->setFilePath(newPath + path.mid(oldPath.size()));
        }
    }
}

void EditorService::closeEditorsUnder(const QString& path)
{
    if (path.isEmpty())
    {
        return;
    }

    QList<TextEditor*> toClose;
    for (auto* editor : std::as_const(mEditors))
    {
        if (demo::pathIsUnder(editor->filePath(), path))
        {
            toClose.append(editor);
        }
    }

    for (auto* editor : std::as_const(toClose))
    {
        editor->closeTab();
    }
}

int EditorService::saveEditorsUnder(const QString& path, QStringList* errors)
{
    if (path.isEmpty())
    {
        return 0;
    }

    int saved = 0;
    for (auto* editor : std::as_const(mEditors))
    {
        if (!editor->isModified() || !demo::pathIsUnder(editor->filePath(), path))
        {
            continue;
        }

        QString error;
        if (editor->save(&error))
        {
            ++saved;
        }
        else if (errors)
        {
            errors->append(error);
        }
    }

    return saved;
}

QString EditorService::textForFile(const QString& filePath) const
{
    if (auto* editor = qobject_cast<TextEditor*>(editorForFile(filePath)))
    {
        return editor->text();
    }

    return {};
}

int EditorService::revisionForFile(const QString& filePath) const
{
    if (auto* editor = qobject_cast<TextEditor*>(editorForFile(filePath)))
    {
        return editor->documentRevision();
    }

    return -1;
}

bool EditorService::revealAt(const QString& filePath, int position)
{
    auto* editor = qobject_cast<TextEditor*>(editorForFile(filePath));
    if (!editor)
    {
        return false;
    }

    // The editor may sit in a hidden perspective or a background tab: navigation is also
    // "bring it to the front", or the user would click a symbol and see nothing move.
    if (auto* window = editor->owningWindow())
    {
        if (auto* folder = folderOf(editor))
        {
            for (auto* perspective : window->allPerspectives())
            {
                bool contains = perspective->floatingFolders().contains(folder);
                if (!contains)
                {
                    if (auto* container = perspective->folderContainer())
                    {
                        contains = container->folders().contains(folder);
                    }
                }

                if (contains)
                {
                    window->setCurrentPerspective(perspective->id());
                    perspective->activateFolder(folder);
                    break;
                }
            }

            const int index = folder->indexOf(editor);
            if (index >= 0)
            {
                folder->setCurrentIndex(index);
            }
        }
    }

    editor->revealPosition(position);
    return true;
}
