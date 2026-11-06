// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "LogView.h"

#include "DemoFonts.h"
#include "DemoServices.h"
#include "DemoStandardContextMenu.h"
#include "QcwOverlayScrollBarUtils.h"
#include "QcwLambdaCommandHandler.h"
#include "QcwSettings.h"
#include "QcwThemeManager.h"
#include "QcwThemeUtils.h"
#include "QcwWidgetTheme.h"
#include "QcwWindowActionIds.h"

#include <QAction>
#include <QColor>
#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHash>
#include <QIcon>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QThread>
#include <QToolBar>
#include <QVBoxLayout>

namespace
{
constexpr QLatin1String FontKey { "font" };

struct LogViewTheme
{
    static constexpr const char* themeFile = "LogView";

    // Fallbacks used when no theme is current or it ships no LogView.theme. They are the light
    // theme's colors, so the pane stays legible on a light base.
    static inline const QColor DefaultDebugColor { 0x15, 0x65, 0xc0 };
    static inline const QColor DefaultInfoColor { 0x2e, 0x7d, 0x32 };
    static inline const QColor DefaultWarningColor { 0xb2, 0x6a, 0x00 };
    static inline const QColor DefaultCriticalColor { 0xc6, 0x28, 0x28 };
    static inline const QColor DefaultFatalColor { 0x8e, 0x00, 0x00 };

    static void reset()
    {
        debugColor = DefaultDebugColor;
        infoColor = DefaultInfoColor;
        warningColor = DefaultWarningColor;
        criticalColor = DefaultCriticalColor;
        fatalColor = DefaultFatalColor;
    }

    static void doLoad(const QJsonObject& jobj)
    {
        reset();

        const auto loadColor = [&jobj](const QString& key, QColor& target)
        {
            const QColor color = QcwThemeUtils::parseColor(jobj.value(key).toString());
            if (color.isValid())
            {
                target = color;
            }
        };

        loadColor(QStringLiteral("debugColor"), debugColor);
        loadColor(QStringLiteral("infoColor"), infoColor);
        loadColor(QStringLiteral("warningColor"), warningColor);
        loadColor(QStringLiteral("criticalColor"), criticalColor);
        loadColor(QStringLiteral("fatalColor"), fatalColor);
    }

    static QColor colorForLevel(const QString& level)
    {
        if (level == QLatin1String("DBUG"))
        {
            return debugColor;
        }
        if (level == QLatin1String("INFO"))
        {
            return infoColor;
        }
        if (level == QLatin1String("WARN"))
        {
            return warningColor;
        }
        if (level == QLatin1String("CRIT"))
        {
            return criticalColor;
        }
        if (level == QLatin1String("FATL"))
        {
            return fatalColor;
        }

        return {};
    }

    static inline QColor debugColor;
    static inline QColor infoColor;
    static inline QColor warningColor;
    static inline QColor criticalColor;
    static inline QColor fatalColor;
};

/**
 * Matches the level tag QcwLogger puts after the timestamp; the tag names the level a line is
 * painted in. Anchoring on the timestamp keeps brackets inside the message from being mistaken
 * for the tag.
 */
const QRegularExpression& levelTagPattern()
{
    static const QRegularExpression pattern { QStringLiteral(
        "^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2} (\\[[A-Z]{4}\\])") };
    return pattern;
}

void colorizeLine(const QTextBlock& block)
{
    const QRegularExpressionMatch match = levelTagPattern().match(block.text());
    if (!match.hasMatch())
    {
        return;
    }

    const QColor color = LogViewTheme::colorForLevel(match.captured(1).mid(1, 4));
    if (!color.isValid())
    {
        return;
    }

    // The tag picks the level, but the level's color covers the whole line, timestamp and
    // message included.
    QTextCursor cursor { block };
    cursor.select(QTextCursor::BlockUnderCursor);

    QTextCharFormat format;
    format.setForeground(color);
    cursor.mergeCharFormat(format);
}

} // namespace

LogView::LogView(QWidget* parent) :
    QcwView { parent },
    mEditor { new QPlainTextEdit { this } },
    mContextMenu { mEditor->createStandardContextMenu() },
    mToolBar { new QToolBar { this } },
    mClearAction { nullptr },
    mSaveAction { nullptr }
{
    mEditor->setReadOnly(true);
    mEditor->setFrameShape(QFrame::NoFrame);
    mEditor->setMaximumBlockCount(10000);

    attachOverlayScrollBar(mEditor, this);

    applyAppearanceSettings();

    // The standard context-menu actions carry no icons on some themes, so the window's Edit
    // tool bar would draw them as text only. Take the icon theme's counterpart.
    static const QHash<QString, QIcon::ThemeIcon> actionIcons {
        { QStringLiteral("edit-copy"), QIcon::ThemeIcon::EditCopy },
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
        updateEditActions(); // the selection may have changed since the last update
        mContextMenu->exec(mEditor->mapToGlobal(pos));
    });

    // The command handlers read and trigger these actions, so they have to follow the pane's
    // selection and content rather than keep the state they had when the menu was built.
    connect(mEditor, &QPlainTextEdit::cursorPositionChanged, this, &LogView::updateEditActions);
    connect(mEditor, &QPlainTextEdit::textChanged, this, &LogView::updateEditActions);

    auto* layout = new QVBoxLayout { this };
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mEditor);

    setFocusProxy(mEditor);

    mToolBar->setIconSize(QSize { 16, 16 });
    mToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    mClearAction =
        mToolBar->addAction(style()->standardIcon(QStyle::SP_TrashIcon), QString {}, this, &LogView::clearLog);

    mSaveAction =
        mToolBar->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), QString {}, this, &LogView::saveLog);

    translateUi();

    mLastSavePath =
        QStringLiteral("%1/qcw-demo.log").arg(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    // The loader installs the colors of the current theme and reloads them on themeChanged();
    // this connection then repaints the lines that are already in the pane. Both are made on
    // themeManager, so the loader runs first and the repaint sees the new colors.
    static QcwWidgetTheme<LogViewTheme> themeLoader;
    mThemeConnection = connect(themeManager, &QcwThemeManager::themeChanged, this, &LogView::applyThemeColors);

    loadExistingLines();
    QcwLogger::addSink(this);
    setupHandlers();
}

LogView::~LogView()
{
    QcwLogger::removeSink(this);
    disconnect(mThemeConnection);
}

void LogView::retranslateUi()
{
    translateUi();
}

void LogView::applyAppearanceSettings()
{
    QSettings settings = QcwSettings::get();
    settings.beginGroup(QString(demo::LogViewSettingsGroup));

    const bool hasFont = settings.contains(QString(FontKey));
    const QFont storedFont = hasFont ? settings.value(QString(FontKey)).value<QFont>() : QFont();
    settings.endGroup();

    // QFont() names a family ("Sans Serif" in Qt 6), so the key's presence, not the family,
    // says whether the user picked a font.
    mEditor->setFont(hasFont ? storedFont : demo::defaultMonospaceFont());
}

QToolBar* LogView::topRightToolBar() const
{
    return mToolBar;
}

void LogView::logLine(const QString& line)
{
    // QcwLogger may be raised from any thread; widget updates stay on the GUI thread.
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(this, [this, line]()
        {
            appendLine(line);
            scrollToEnd();
        }, Qt::QueuedConnection);
        return;
    }

    appendLine(line);
    scrollToEnd();
}

void LogView::translateUi()
{
    mClearAction->setText(tr("Clear Log"));
    mClearAction->setToolTip(tr("Clear the log view"));
    mSaveAction->setText(tr("Save Log..."));
    mSaveAction->setToolTip(tr("Save the log content to a file"));

    // The pane is read-only, so its two standard actions are all this view hands the window's Edit
    // menu — and they are the very objects the handlers trigger, so a language change re-texts them
    // in place instead of replacing them.
    demo::retranslateStandardContextMenu(mContextMenu, mEditor);
}

void LogView::appendLine(const QString& line)
{
    // A message may span several lines; start at the block the append begins in so that the
    // level tag on the first of them is colored too.
    QTextDocument* document = mEditor->document();
    const int firstBlock = qMax(0, document->blockCount() - 1);
    mEditor->appendPlainText(line);
    for (QTextBlock block = document->findBlockByNumber(firstBlock); block.isValid(); block = block.next())
    {
        colorizeLine(block);
    }
}

void LogView::applyThemeColors()
{
    QTextDocument* document = mEditor->document();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
    {
        colorizeLine(block);
    }
}

void LogView::updateEditActions()
{
    const bool hasSelection = mEditor->textCursor().hasSelection();

    for (auto* action : mContextMenu->actions())
    {
        const auto name = action->objectName();
        if (name == QLatin1String("edit-copy"))
        {
            action->setEnabled(hasSelection);
        }
        else if (name == QLatin1String("select-all"))
        {
            action->setEnabled(!mEditor->document()->isEmpty());
        }
    }

    for (auto* handler : mHandlers)
    {
        handler->notifyStateChanged();
    }
}

void LogView::scrollToEnd()
{
    QScrollBar* bar = mEditor->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void LogView::loadExistingLines()
{
    const QString filePath = QcwLogger::currentLogFile();
    if (filePath.isEmpty())
    {
        return;
    }

    QFile file { filePath };
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return;
    }

    while (!file.atEnd())
    {
        QString line = QString::fromUtf8(file.readLine());
        if (line.endsWith(QLatin1Char('\n')))
        {
            line.chop(1);
        }
        if (line.endsWith(QLatin1Char('\r')))
        {
            line.chop(1);
        }
        appendLine(line);
    }

    scrollToEnd();
}

void LogView::clearLog()
{
    mEditor->clear();
}

void LogView::saveLog()
{
    const QString filePath =
        QFileDialog::getSaveFileName(this, tr("Save Log"), mLastSavePath, tr("Log files (*.log);;All files (*)"));

    if (filePath.isEmpty())
    {
        return;
    }

    QSaveFile file { filePath };
    if (!file.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, tr("Save Log"), tr("Cannot write \"%1\":\n%2").arg(filePath, file.errorString()));
        return;
    }

    const QByteArray bytes = mEditor->toPlainText().toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit())
    {
        QMessageBox::warning(this, tr("Save Log"), tr("Cannot write \"%1\":\n%2").arg(filePath, file.errorString()));
        return;
    }

    mLastSavePath = filePath;
    qInfo().noquote() << tr("Saved the log view to %1").arg(filePath);
}
void LogView::setupHandlers()
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

    addHandler(QcwWindowActionIds::ActionCopy, QStringLiteral("edit-copy"));
    addHandler(QcwWindowActionIds::ActionSelectAll, QStringLiteral("select-all"));
}

QHash<QString, QcwCommandHandler*> LogView::commandHandlers() const
{
    return mHandlers;
}
