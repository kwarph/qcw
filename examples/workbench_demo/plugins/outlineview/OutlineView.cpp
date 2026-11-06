// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "OutlineView.h"

#include "DemoServices.h"
#include "QcwFixedRowHeightItemDelegate.h"
#include "QcwOverlayScrollBarUtils.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QLabel>
#include <QRegularExpression>
#include <QSet>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace
{

struct Symbol
{
    QString name;
    QString kind;
};

const QSet<QString>& controlKeywords()
{
    static const QSet<QString> keywords {
        QStringLiteral("if"),     QStringLiteral("for"),   QStringLiteral("while"),
        QStringLiteral("switch"), QStringLiteral("catch"), QStringLiteral("return"),
        QStringLiteral("sizeof"), QStringLiteral("do"),    QStringLiteral("else"),
    };
    return keywords;
}

/// The first symbol @p line declares, or an empty name when it declares none.
Symbol symbolForLine(const QString& line)
{
    static const QRegularExpression typePattern { QStringLiteral(
        R"(^\s*(?:class|struct|enum(?:\s+class)?|namespace|union)\s+([A-Za-z_]\w*))") };
    static const QRegularExpression headingPattern { QStringLiteral(R"(^#{1,6}\s+(.+?)\s*$)") };
    static const QRegularExpression functionPattern { QStringLiteral(
        R"(^\s*(?:[\w:<>,*&]+\s+)+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const\s*)?\{)") };
    static const QRegularExpression qualifiedPattern { QStringLiteral(
        R"(^\s*([A-Za-z_]\w*(?:::\w+)*)\s*\([^;{}]*\)\s*(?:const\s*)?\{)") };

    auto match = typePattern.match(line);
    if (match.hasMatch())
    {
        return { match.captured(1), QStringLiteral("type") };
    }

    match = headingPattern.match(line);
    if (match.hasMatch())
    {
        return { match.captured(1), QStringLiteral("heading") };
    }

    match = functionPattern.match(line);
    if (match.hasMatch() && !controlKeywords().contains(match.captured(1)))
    {
        return { match.captured(1), QStringLiteral("function") };
    }

    match = qualifiedPattern.match(line);
    if (match.hasMatch() && !controlKeywords().contains(match.captured(1)))
    {
        return { match.captured(1), QStringLiteral("function") };
    }

    return {};
}

} // namespace

OutlineView::OutlineView(QWidget* parent) :
    QcwView { parent }, mTitle { new QLabel { this } }, mTree { new QTreeWidget { this } }
{
    mTitle->setContentsMargins(6, 4, 6, 4);

    mTree->setHeaderLabels({ tr("Symbol"), tr("Kind") });
    mTree->setRootIsDecorated(false);
    mTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mTree->setAllColumnsShowFocus(true);
    mTree->setFrameShape(QFrame::NoFrame);
    mTree->setItemDelegate(new QcwFixedRowHeightItemDelegate { mTree });

    attachOverlayScrollBar(mTree, this);

    auto* layout = new QVBoxLayout { this };
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(mTitle);
    layout->addWidget(mTree, 1);

    setFocusProxy(mTree);

    connect(mTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int)
    {
        revealSymbol(item);
    });
    connect(mTree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int)
    {
        revealSymbol(item);
    });

    clearOutline();

    // The framework follows the window: on every (re-)attach the handler runs with the topic's
    // current payload, so an outline opened after the editor still shows the current symbols and
    // a cleared or never-published topic leaves the outline empty.
    subscribeWindowTopic(QString(demo::TopicEditorSelection),
                         [this](const QVariantMap& payload)
    {
        applySelection(payload);
    });
}

OutlineView::~OutlineView() = default;

void OutlineView::retranslateUi()
{
    mTree->setHeaderLabels({ tr("Symbol"), tr("Kind") });

    // The placeholder row and the tooltips are strings this view chose for the document it
    // parsed, so they are derived again the way rebuild() derived them.
    for (int row = 0; row < mTree->topLevelItemCount(); ++row)
    {
        auto* item = mTree->topLevelItem(row);
        const int offset = item->data(0, Qt::UserRole).toInt();
        if (offset < 0)
        {
            item->setText(0, tr("No symbols"));
        }
        else
        {
            item->setToolTip(0, tr("Character %1").arg(offset));
        }
    }

    if (mPath.isEmpty())
    {
        mTitle->setText(tr("No active editor"));
    }
}

void OutlineView::applySelection(const QVariantMap& payload)
{
    const QString path = payload.value(QString(demo::PathKey)).toString();
    auto* service = demo::editorService();

    if (path.isEmpty() || !service)
    {
        clearOutline();
        return;
    }

    const int revision = payload.value(QString(demo::RevisionKey)).toInt();
    if (path != mPath || revision != mRevision)
    {
        rebuild(path, service->textForFile(path), revision);
    }

    updateHighlight(payload.value(QString(demo::PositionKey)).toInt());
}

void OutlineView::clearOutline()
{
    mPath.clear();
    mRevision = -1;
    mTitle->setText(tr("No active editor"));
    mTitle->setToolTip({});
    mTree->clear();
}

void OutlineView::rebuild(const QString& filePath, const QString& text, int revision)
{
    mPath = filePath;
    mRevision = revision;

    mTitle->setText(QFileInfo { filePath }.fileName());
    mTitle->setToolTip(filePath);
    mTree->clear();

    const QStringList lines = text.split(QLatin1Char('\n'));
    int offset = 0;
    for (const QString& line : lines)
    {
        const Symbol symbol = symbolForLine(line);
        if (!symbol.name.isEmpty())
        {
            auto* item = new QTreeWidgetItem { mTree, { symbol.name, symbol.kind } };
            item->setData(0, Qt::UserRole, offset);
            item->setToolTip(0, tr("Character %1").arg(offset));
        }

        offset += line.size() + 1;
    }

    if (mTree->topLevelItemCount() == 0)
    {
        auto* item = new QTreeWidgetItem { mTree, { tr("No symbols"), {} } };
        item->setFlags(Qt::NoItemFlags);
        item->setData(0, Qt::UserRole, -1);
    }
}

void OutlineView::updateHighlight(int position)
{
    QTreeWidgetItem* best = nullptr;
    int bestPosition = -1;

    for (int i = 0; i < mTree->topLevelItemCount(); ++i)
    {
        auto* item = mTree->topLevelItem(i);
        const int start = item->data(0, Qt::UserRole).toInt();
        if (start < 0 || start > position || start < bestPosition)
        {
            continue;
        }

        best = item;
        bestPosition = start;
    }

    if (best == mTree->currentItem())
    {
        return;
    }

    mTree->setCurrentItem(best);
    if (best)
    {
        mTree->scrollToItem(best, QAbstractItemView::EnsureVisible);
    }
    else
    {
        // The cursor sits before the first symbol; nothing in the list describes it.
        mTree->clearSelection();
    }
}

void OutlineView::revealSymbol(QTreeWidgetItem* item)
{
    if (!item || mPath.isEmpty())
    {
        return;
    }

    const int position = item->data(0, Qt::UserRole).toInt();
    if (position < 0)
    {
        return;
    }

    if (auto* service = demo::editorService())
    {
        service->revealAt(mPath, position);
    }
}
