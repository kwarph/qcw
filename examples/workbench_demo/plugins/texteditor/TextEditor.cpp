// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "TextEditor.h"

#include "DemoFonts.h"
#include "DemoServices.h"
#include "DemoStandardContextMenu.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwWindow.h"
#include "QcwOverlayScrollBarUtils.h"
#include "QcwTopicService.h"
#include "QcwSettings.h"
#include "QcwTabFolder.h"
#include "QcwWindowActionIds.h"

#include <QAction>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QVBoxLayout>
#include <QTimer>

namespace
{
constexpr QLatin1String ShowLineNumberKey { "showLineNumber" };
constexpr QLatin1String FontKey { "font" };

// Space between each edge of the gutter and the digits; the gutter is sized with it on both sides.
constexpr int LineNumberPadding { 6 };

void setError(QString* error, const QString& message)
{
    if (error)
    {
        *error = message;
    }
}
} // namespace

class CodeEditor;

/// The gutter drawn to the left of the text; it only forwards its painting to the editor.
class LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor* editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    CodeEditor* mEditor;
};

class CodeEditor : public QPlainTextEdit
{
public:
    explicit CodeEditor(QWidget* parent = nullptr);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent* event);

    bool lineNumbersVisible() const;
    void setLineNumbersVisible(bool visible);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool event(QEvent* event) override;

private:
    void updateLineNumberAreaWidth();
    void updateLineNumberArea(const QRect& rect, int dy);
    void highlightCurrentLine();

    LineNumberArea* mLineNumberArea;
    bool mLineNumbersVisible { true };
};

LineNumberArea::LineNumberArea(CodeEditor* editor) :
    QWidget { editor }, mEditor { editor }
{
}

QSize LineNumberArea::sizeHint() const
{
    return { mEditor->lineNumberAreaWidth(), 0 };
}

void LineNumberArea::paintEvent(QPaintEvent* event)
{
    mEditor->lineNumberAreaPaintEvent(event);
}

CodeEditor::CodeEditor(QWidget* parent) :
    QPlainTextEdit { parent }, mLineNumberArea { new LineNumberArea { this } }
{
    setFrameShape(QFrame::NoFrame);

    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth();
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() const
{
    if (!mLineNumbersVisible)
    {
        return 0;
    }

    int digits = 1;
    int blocks = qMax(1, blockCount());
    while (blocks >= 10)
    {
        blocks /= 10;
        ++digits;
    }
    digits = qMax(2, digits);

    return 2 * LineNumberPadding + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
    if (!mLineNumbersVisible)
    {
        return;
    }

    QPainter painter { mLineNumberArea };
    painter.fillRect(event->rect(), palette().color(QPalette::AlternateBase));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    const int width = mLineNumberArea->width();
    const int height = fontMetrics().height();

    painter.setPen(palette().color(QPalette::Text));
    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            painter.drawText(LineNumberPadding, top, width - 2 * LineNumberPadding, height, Qt::AlignRight,
                             QString::number(blockNumber + 1));
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

bool CodeEditor::lineNumbersVisible() const
{
    return mLineNumbersVisible;
}

void CodeEditor::setLineNumbersVisible(bool visible)
{
    if (mLineNumbersVisible == visible)
    {
        return;
    }

    mLineNumbersVisible = visible;
    mLineNumberArea->setVisible(visible);
    updateLineNumberAreaWidth();
    update();
}

void CodeEditor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    const QRect area = contentsRect();
    mLineNumberArea->setGeometry(area.left(), area.top(), lineNumberAreaWidth(), area.height());
}

bool CodeEditor::event(QEvent* event)
{
    // The gutter width depends on the font, so a new font has to re-measure it.
    if (event->type() == QEvent::FontChange)
    {
        updateLineNumberAreaWidth();
        update();
    }

    return QPlainTextEdit::event(event);
}

void CodeEditor::updateLineNumberAreaWidth()
{
    const int width = mLineNumbersVisible ? lineNumberAreaWidth() : 0;
    setViewportMargins(width, 0, 0, 0);

    // The viewport margin changing does not resize the editor, so the gutter has to be resized
    // here as well; otherwise it keeps the width of the line count it was last laid out with and
    // clips the numbers once the document gains a digit.
    const QRect area = contentsRect();
    mLineNumberArea->setGeometry(area.left(), area.top(), width, area.height());
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy)
{
    if (!mLineNumbersVisible)
    {
        return;
    }

    // Scroll the gutter itself, not the editor: scrolling the editor moves the gutter along with
    // its children and leaves the strip it uncovers unpainted, so only the first screen keeps numbers.
    if (dy != 0)
    {
        mLineNumberArea->scroll(0, dy);
    }
    else
    {
        mLineNumberArea->update(0, rect.y(), mLineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect()))
    {
        updateLineNumberAreaWidth();
    }
}

void CodeEditor::highlightCurrentLine()
{
    QTextEdit::ExtraSelection selection;
    QColor lineColor = palette().color(QPalette::Highlight);
    lineColor.setAlpha(30);
    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    setExtraSelections({ selection });
}

TextEditor::TextEditor(QWidget* parent) :
    QcwView { parent },
    mEditor { new CodeEditor { this } },
    mStatusWidget { new QWidget { this } },
    mPositionLabel { new QLabel { mStatusWidget } },
    mEndingLabel { new QLabel { mStatusWidget } },
    mContextMenu { mEditor->createStandardContextMenu() }
{
    attachOverlayScrollBar(mEditor, this);

    auto* layout = new QVBoxLayout { this };
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mEditor);

    setFocusProxy(mEditor);

    auto* statusLayout = new QHBoxLayout { mStatusWidget };
    statusLayout->setContentsMargins(6, 0, 6, 0);
    statusLayout->setSpacing(12);
    statusLayout->addWidget(mPositionLabel);
    statusLayout->addWidget(mEndingLabel);

    // The window shows this widget while the editor is its current view. Hidden from the
    // start, it cannot sit at the view's top-left corner over the first line while the
    // editor is visible but some other view (here: the shared folder) is current.
    mStatusWidget->hide();

    // The standard context-menu actions carry no icons, so the window's Edit tool bar would draw
    // them as text only. Take the icon theme's counterpart where the theme has one.
    static const QHash<QString, QIcon::ThemeIcon> actionIcons {
        { QStringLiteral("edit-undo"), QIcon::ThemeIcon::EditUndo },
        { QStringLiteral("edit-redo"), QIcon::ThemeIcon::EditRedo },
        { QStringLiteral("edit-cut"), QIcon::ThemeIcon::EditCut },
        { QStringLiteral("edit-copy"), QIcon::ThemeIcon::EditCopy },
        { QStringLiteral("edit-paste"), QIcon::ThemeIcon::EditPaste },
        { QStringLiteral("edit-delete"), QIcon::ThemeIcon::EditDelete },
        { QStringLiteral("select-all"), QIcon::ThemeIcon::EditSelectAll },
    };

    for (auto* action : mContextMenu->actions())
    {
        const auto icon = actionIcons.constFind(action->objectName());
        if (icon != actionIcons.constEnd())
        {
            action->setIcon(QIcon::fromTheme(icon.value()));
        }
    }

    mEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mEditor, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos)
    {
        updateEditActions(); // the clipboard may have changed without a signal
        mContextMenu->exec(mEditor->mapToGlobal(pos));
    });

    connect(mEditor, &QPlainTextEdit::cursorPositionChanged, this, [this]()
    {
        updateCursorStatus();
        updateEditActions();
        publishSelection();
    });
    connect(mEditor, &QPlainTextEdit::textChanged, this, [this]()
    {
        updateEditActions();
        // The revision changed, so a reader that caches a parse by revision has to let go of it.
        publishSelection();
    });
    connect(mEditor, &QPlainTextEdit::modificationChanged, this, &TextEditor::updateDirtyState);
    connect(mEditor, &QPlainTextEdit::undoAvailable, this, [this]()
    {
        updateEditActions();
    });
    connect(mEditor, &QPlainTextEdit::redoAvailable, this, [this]()
    {
        updateEditActions();
    });

    setupHandlers();
    applyAppearanceSettings();
    updateCursorStatus();
    updateEditActions();

    // The service is registered while the plugin loads, before any editor exists; registering
    // from here keeps the creator free of service wiring.
    if (auto* service = demo::editorService())
    {
        service->trackEditor(this);
    }
}

TextEditor::~TextEditor()
{
    if (auto* window = owningWindow())
    {
        window->topicService()->publish(QString(demo::TopicEditorClosed), { { QString(demo::PathKey), mFilePath } });
    }
}

bool TextEditor::loadFile(const QString& filePath)
{
    QFile file { filePath };
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("Open File"), tr("Cannot open \"%1\":\n%2").arg(filePath, file.errorString()));
        return false;
    }

    QString text = QString::fromUtf8(file.readAll());
    const int crlfCount = text.count(QStringLiteral("\r\n"));
    const int lfCount = text.count(QLatin1Char('\n')) - crlfCount;
    const int crCount = text.count(QLatin1Char('\r')) - crlfCount;

    if (crlfCount > 0 && crlfCount >= lfCount && crlfCount >= crCount)
    {
        mLineEnding = LineEnding::CRLF;
    }
    else if (crCount > 0 && crCount > lfCount)
    {
        mLineEnding = LineEnding::CR;
    }
    else
    {
        mLineEnding = LineEnding::LF;
    }

    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    mEditor->setPlainText(text);
    mEditor->document()->setModified(false);
    setFilePath(filePath);
    updateCursorStatus();
    updateDirtyState();

    return true;
}

bool TextEditor::save(QString* error)
{
    if (mFilePath.isEmpty())
    {
        return saveAs(error);
    }

    return saveTo(mFilePath, error);
}

bool TextEditor::saveTo(const QString& filePath, QString* error)
{
    if (filePath.isEmpty())
    {
        setError(error, tr("No file name was given."));
        return false;
    }

    QString text = mEditor->toPlainText();
    switch (mLineEnding)
    {
    case LineEnding::CRLF:
        text.replace(QStringLiteral("\n"), QStringLiteral("\r\n"));
        break;
    case LineEnding::CR:
        text.replace(QStringLiteral("\n"), QStringLiteral("\r"));
        break;
    case LineEnding::LF:
        break;
    }

    QSaveFile file { filePath };
    if (!file.open(QIODevice::WriteOnly))
    {
        setError(error, tr("Cannot write \"%1\":\n%2").arg(filePath, file.errorString()));
        return false;
    }

    const QByteArray bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        setError(error, tr("Cannot write \"%1\":\n%2").arg(filePath, file.errorString()));
        return false;
    }

    setFilePath(filePath);
    mEditor->document()->setModified(false);
    updateDirtyState();
    publishEvent(QString(demo::TopicEditorSaved));

    return true;
}

QString TextEditor::filePath() const
{
    return mFilePath;
}

void TextEditor::setFilePath(const QString& filePath)
{
    const QString absolute = filePath.isEmpty() ? QString() : QFileInfo { filePath }.absoluteFilePath();
    if (absolute == mFilePath)
    {
        return;
    }

    const QString oldPath = mFilePath;
    mFilePath = absolute;
    updateTabInfo();

    publishSelection(); // The selection topic carries the new path.

    publishEvent(QString(demo::TopicEditorPathChanged), { { QString(demo::OldPathKey), oldPath } });
}

bool TextEditor::isModified() const
{
    return mEditor->document()->isModified();
}

void TextEditor::applyAppearanceSettings()
{
    QSettings settings = QcwSettings::get();
    settings.beginGroup(QString(demo::TextEditorSettingsGroup));

    const bool showLineNumbers = settings.value(QString(ShowLineNumberKey), true).toBool();
    const bool hasFont = settings.contains(QString(FontKey));
    const QFont storedFont = hasFont ? settings.value(QString(FontKey)).value<QFont>() : QFont();
    settings.endGroup();

    // QFont() names a family ("Sans Serif" in Qt 6), so the key's presence, not the family,
    // says whether the user picked a font.
    const QFont font = hasFont ? storedFont : demo::defaultMonospaceFont();

    mEditor->setFont(font);
    mEditor->setLineNumbersVisible(showLineNumbers);
    updateCursorStatus();
}

void TextEditor::closeTab()
{
    if (auto* folder = currentFolder())
    {
        folder->removeTab(currentTabIndex());
        return;
    }

    deleteLater();
}

QString TextEditor::text() const
{
    return mEditor->toPlainText();
}

int TextEditor::documentRevision() const
{
    return mEditor->document()->revision();
}

void TextEditor::revealPosition(int position)
{
    if (position >= 0)
    {
        const int lastPosition = qMax(0, mEditor->document()->characterCount() - 1);
        QTextCursor cursor = mEditor->textCursor();
        cursor.setPosition(qBound(0, position, lastPosition));
        mEditor->setTextCursor(cursor);
        mEditor->centerCursor();
    }

    mEditor->setFocus();
}

void TextEditor::publishSelection()
{
    // Only the current view owns the window's subject; background tabs stay silent.
    if (auto* window = owningWindow(); window && window->currentView() == this)
    {
        const QTextCursor cursor = mEditor->textCursor();
        QVariantMap payload;
        payload[QString(demo::PathKey)] = mFilePath;
        payload[QString(demo::PositionKey)] = cursor.position();
        payload[QString(demo::AnchorKey)] = cursor.anchor();
        payload[QString(demo::RevisionKey)] = documentRevision();
        payload[QString(demo::SelectedTextKey)] = cursor.selectedText();

        window->topicService()->publish(QString(demo::TopicEditorSelection), payload);
    }
}

void TextEditor::publishEvent(const QString& topic, const QVariantMap& data)
{
    QVariantMap payload = data;
    payload.insert(QString(demo::PathKey), mFilePath);
    if (auto* window = owningWindow())
    {
        window->topicService()->publish(topic, payload);
    }
}

QWidget* TextEditor::statusBarWidget() const
{
    return mStatusWidget;
}

void TextEditor::saveExtraState(QVariantMap& state) const
{
    if (!mFilePath.isEmpty())
    {
        state.insert(QStringLiteral("filePath"), mFilePath);
    }
}

void TextEditor::restoreExtraState(const QVariantMap& state)
{
    const QString savedPath = state.value(QStringLiteral("filePath")).toString();
    if (savedPath.isEmpty() || savedPath == mFilePath)
    {
        return;
    }

    if (QFileInfo::exists(savedPath))
    {
        loadFile(savedPath);
    }
    else
    {
        setFilePath(savedPath);
    }
}

bool TextEditor::event(QEvent* event)
{
    if (event->type() == QEvent::ParentChange || event->type() == QEvent::Show)
    {
        connectToCurrentFolder();
    }

    if (event->type() == QEvent::Show)
    {
        if (auto* window = owningWindow())
        {
            if (!mOpenedPublished)
            {
                mOpenedPublished = true;
                window->topicService()->publish(QString(demo::TopicEditorOpened), { { QString(demo::PathKey), mFilePath } });
            }

            // The view is shown before the window finishes making it current; publish on the
            // next event-loop turn so publishSelection() sees this editor as currentView().
            QTimer::singleShot(0, this, &TextEditor::publishSelection);
        }
    }
    else if (event->type() == QEvent::Hide)
    {
        // A tab or perspective switch hides the editor; drop the subject so consumers do not
        // keep showing a selection that is off screen.
        if (auto* window = owningWindow())
        {
            window->topicService()->clear(QString(demo::TopicEditorSelection));
        }
    }

    return QcwView::event(event);
}

QcwTabFolder* TextEditor::currentFolder()
{
    for (QWidget* widget = parentWidget(); widget; widget = widget->parentWidget())
    {
        auto* folder = qobject_cast<QcwTabFolder*>(widget);
        if (folder && folder->indexOf(this) >= 0)
        {
            return folder;
        }
    }

    return nullptr;
}

int TextEditor::currentTabIndex()
{
    if (auto* folder = currentFolder())
    {
        return folder->indexOf(this);
    }

    return -1;
}

void TextEditor::updateCursorStatus()
{
    const QTextCursor cursor = mEditor->textCursor();
    mPositionLabel->setText(tr("Ln %1, Col %2").arg(cursor.blockNumber() + 1).arg(cursor.positionInBlock() + 1));

    switch (mLineEnding)
    {
    case LineEnding::CRLF:
        mEndingLabel->setText(tr("CRLF"));
        break;
    case LineEnding::CR:
        mEndingLabel->setText(tr("CR"));
        break;
    case LineEnding::LF:
        mEndingLabel->setText(tr("LF"));
        break;
    }
}

void TextEditor::retranslateUi()
{
    updateCursorStatus();
    updateTabInfo();

    // The command handlers trigger and read these very actions, so they are re-texted in place
    // instead of replaced.
    demo::retranslateStandardContextMenu(mContextMenu, mEditor);
}

void TextEditor::updateEditActions()
{
    // The Edit menu's commands are handled by these actions, so they have to show the editor's
    // state rather than the state they had when the menu was built.
    const QTextCursor cursor = mEditor->textCursor();
    const bool hasSelection = cursor.hasSelection();
    const bool editable = !mEditor->isReadOnly();

    for (auto* action : mContextMenu->actions())
    {
        const auto name = action->objectName();
        if (name == QStringLiteral("edit-undo"))
        {
            action->setEnabled(editable && mEditor->document()->isUndoAvailable());
        }
        else if (name == QStringLiteral("edit-redo"))
        {
            action->setEnabled(editable && mEditor->document()->isRedoAvailable());
        }
        else if (name == QStringLiteral("edit-cut") || name == QStringLiteral("edit-delete"))
        {
            action->setEnabled(editable && hasSelection);
        }
        else if (name == QStringLiteral("edit-copy"))
        {
            action->setEnabled(hasSelection);
        }
        else if (name == QStringLiteral("edit-paste"))
        {
            action->setEnabled(editable && mEditor->canPaste());
        }
        else if (name == QStringLiteral("select-all"))
        {
            action->setEnabled(!mEditor->document()->isEmpty());
        }
    }

    for (auto* handler : mHandlers)
    {
        handler->notifyStateChanged();
    }
}

void TextEditor::updateTabInfo()
{
    auto* folder = currentFolder();
    if (!folder)
    {
        return;
    }

    const int index = folder->indexOf(this);
    if (index < 0)
    {
        return;
    }

    const QString title = mFilePath.isEmpty() ? tr("Untitled") : QFileInfo { mFilePath }.fileName();
    folder->setTabText(index, title);
    folder->setTabToolTip(index, mFilePath.isEmpty() ? title : mFilePath);
}

void TextEditor::updateDirtyState()
{
    if (auto* folder = currentFolder())
    {
        folder->setTabDirty(folder->indexOf(this), mEditor->document()->isModified());
    }
}

void TextEditor::connectToCurrentFolder()
{
    updateTabInfo();
    updateDirtyState();
}

void TextEditor::canClose(const std::function<void(bool)>& callback)
{
    callback(confirmClose());
}

void TextEditor::publishEvent(QLatin1String topic, const QVariantMap& data)
{
    QVariantMap payload = data;
    payload.insert(QString(demo::PathKey), mFilePath);
    if (auto* window = owningWindow())
    {
        window->topicService()->publish(topic, payload);
    }
}

bool TextEditor::saveAs(QString* error)
{
    const QString suggested =
        mFilePath.isEmpty() ? QStringLiteral("%1/Untitled.txt").arg(QDir::homePath()) : mFilePath;

    const QString target = QFileDialog::getSaveFileName(this, tr("Save File As"), suggested);
    if (target.isEmpty())
    {
        setError(error, tr("Save cancelled."));
        return false;
    }

    return saveTo(target, error);
}

bool TextEditor::confirmClose()
{
    if (!mEditor->document()->isModified())
    {
        return true;
    }

    const auto answer =
        QMessageBox::question(this, tr("Close File"),
                              tr("The file \"%1\" has been modified.\nDo you want to save your changes?")
                                  .arg(mFilePath.isEmpty() ? tr("Untitled") : QFileInfo { mFilePath }.fileName()),
                              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);

    if (answer == QMessageBox::Cancel)
    {
        return false;
    }

    if (answer == QMessageBox::Discard)
    {
        return true;
    }

    QString error;
    if (save(&error))
    {
        return true;
    }

    if (!error.isEmpty() && error != tr("Save cancelled."))
    {
        QMessageBox::warning(this, tr("Save Failed"), error);
    }

    return false;
}
void TextEditor::setupHandlers()
{
    const auto addHandler = [this](const QString& commandId, const QString& actionName)
    {
        auto* handler = new QcwLambdaCommandHandler(this);
        handler->onExecute([this, actionName](const QVariantMap&)
        {
            for (auto* action : mContextMenu->actions())
            {
                if (action->objectName() == actionName)
                {
                    action->trigger();
                    break;
                }
            }
        });
        handler->onEnabled([this, actionName]()
        {
            for (auto* action : mContextMenu->actions())
            {
                if (action->objectName() == actionName)
                {
                    return action->isEnabled();
                }
            }
            return false;
        });
        mHandlers.insert(commandId, handler);
    };

    addHandler(QcwWindowActionIds::ActionUndo, QStringLiteral("edit-undo"));
    addHandler(QcwWindowActionIds::ActionRedo, QStringLiteral("edit-redo"));
    addHandler(QcwWindowActionIds::ActionCut, QStringLiteral("edit-cut"));
    addHandler(QcwWindowActionIds::ActionCopy, QStringLiteral("edit-copy"));
    addHandler(QcwWindowActionIds::ActionPaste, QStringLiteral("edit-paste"));
    addHandler(QcwWindowActionIds::ActionDelete, QStringLiteral("edit-delete"));
    addHandler(QcwWindowActionIds::ActionSelectAll, QStringLiteral("select-all"));
}

QHash<QString, QcwCommandHandler*> TextEditor::commandHandlers() const
{
    return mHandlers;
}
