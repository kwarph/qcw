// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwCommandPalette.h"

#include "QcwCommandRegistry.h"
#include "QcwTreeProxyModel.h"
#include "QcwWindow.h"
#include "QcwWindowActionIds.h"
#include "QcwOverlayScrollBarUtils.h"
#include "QcwFixedRowHeightItemDelegate.h"

#include <QAction>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSet>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringList>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

QT_BEGIN_NAMESPACE

namespace
{

enum CommandPaletteRole
{
    CommandIdRole = Qt::UserRole,
    CommandSearchRole
};

struct PaletteEntry
{
    QString id;
    QString label;
    QString description;
    QKeySequence shortcut;
    QAction* action { nullptr };
};

QList<QModelIndex> enabledCommandIndexes(const QAbstractItemModel* model)
{
    QList<QModelIndex> indexes;

    std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& parent)
    {
        for (int row = 0; row < model->rowCount(parent); ++row)
        {
            const auto index = model->index(row, 0, parent);
            const auto flags = index.flags();
            if (flags.testFlag(Qt::ItemIsEnabled) && flags.testFlag(Qt::ItemIsSelectable) &&
                index.data(CommandIdRole).isValid())
            {
                indexes.append(index);
            }

            walk(index);
        }
    };
    walk({});

    return indexes;
}

} // namespace

class QcwCommandPalettePrivate
{
public:
    explicit QcwCommandPalettePrivate(QcwCommandPalette* interface);

    void buildCommands();
    void retranslateUi();
    void triggerCurrent();
    void selectFirstEnabled();
    void moveCurrent(int step);

public:
    QcwCommandPalette* owner;

    QPointer<QcwWindow> window;
    QLineEdit* filter { nullptr };
    QTreeView* treeView { nullptr };
    QStandardItemModel* model { nullptr };
    QcwTreeProxyModel* proxyModel { nullptr };
};

// -----------------------------------------------------------------------------

QcwCommandPalette::QcwCommandPalette(QcwWindow* window) :
    QDialog { window }, impl { new QcwCommandPalettePrivate { this } }
{
    impl->window = window;
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout { this };
    layout->setContentsMargins(9, 9, 9, 9);
    layout->setSpacing(6);

    impl->filter = new QLineEdit { this };
    impl->filter->setObjectName(QStringLiteral("commandPaletteFilter"));
    impl->filter->setClearButtonEnabled(true);
    impl->filter->installEventFilter(this);
    layout->addWidget(impl->filter);

    impl->model = new QStandardItemModel { this };

    impl->proxyModel = new QcwTreeProxyModel { this };
    impl->proxyModel->setFilterRole(CommandSearchRole);
    impl->proxyModel->setFilterKeyColumn(0);
    impl->proxyModel->setSourceModel(impl->model);

    impl->treeView = new QTreeView { this };
    impl->treeView->setObjectName(QStringLiteral("commandPaletteTree"));
    impl->treeView->setModel(impl->proxyModel);
    impl->treeView->setItemDelegate(new QcwFixedRowHeightItemDelegate { this });
    impl->treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    impl->treeView->header()->setStretchLastSection(true);

    attachOverlayScrollBar(impl->treeView, this);

    layout->addWidget(impl->treeView, 1);

    impl->retranslateUi();

    connect(impl->filter, &QLineEdit::textChanged, this, [this](const QString& text)
    {
        impl->proxyModel->setFilterText(text);
        impl->treeView->expandAll();
        impl->selectFirstEnabled();
    });

    connect(impl->filter, &QLineEdit::returnPressed, this, [this]
    {
        impl->triggerCurrent();
    });

    connect(impl->treeView, &QTreeView::activated, this, [this]
    {
        impl->triggerCurrent();
    });

    impl->filter->setFocus();
}

QcwCommandPalette::~QcwCommandPalette()
{
    delete impl;
}

bool QcwCommandPalette::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == impl->filter && event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        switch (keyEvent->key())
        {
        case Qt::Key_Down:
            impl->moveCurrent(1);
            return true;
        case Qt::Key_Up:
            impl->moveCurrent(-1);
            return true;
        case Qt::Key_PageDown:
            impl->moveCurrent(8);
            return true;
        case Qt::Key_PageUp:
            impl->moveCurrent(-8);
            return true;
        case Qt::Key_Escape:
            reject();
            return true;
        default:
            break;
        }
    }

    return QDialog::eventFilter(watched, event);
}

void QcwCommandPalette::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QDialog::changeEvent(event);
}

void QcwCommandPalettePrivate::retranslateUi()
{
    owner->setWindowTitle(QcwCommandPalette::tr("Command Palette"));
    filter->setPlaceholderText(QcwCommandPalette::tr("Type a command..."));
    model->setHorizontalHeaderLabels({ QcwCommandPalette::tr("Command"), QcwCommandPalette::tr("Shortcut") });

    buildCommands();
    treeView->expandAll();
    selectFirstEnabled();
}

// -----------------------------------------------------------------------------

QcwCommandPalettePrivate::QcwCommandPalettePrivate(QcwCommandPalette* interface) :
    owner { interface }
{
}

void QcwCommandPalettePrivate::buildCommands()
{
    model->removeRows(0, model->rowCount());

    QMap<QString, QList<PaletteEntry>> groups;

    if (window)
    {
        for (const auto& info : commandRegistry->commands())
        {
            const auto category = info.category.isEmpty() ? QcwCommandPalette::tr("Application") : info.category;
            auto* action = window->resolveCommand(info.id);
            groups[category].append({ info.id, info.label, info.description, info.defaultShortcut, action });
        }
    }

    for (auto& entries : groups)
    {
        std::sort(entries.begin(), entries.end(), [](const PaletteEntry& left, const PaletteEntry& right)
        {
            return QString::compare(left.label, right.label, Qt::CaseInsensitive) < 0;
        });
    }

    for (auto group = groups.cbegin(); group != groups.cend(); ++group)
    {
        auto* categoryItem = new QStandardItem { group.key() };
        categoryItem->setEditable(false);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);
        categoryItem->setData(group.key(), CommandSearchRole);

        auto font = categoryItem->font();
        font.setBold(true);
        categoryItem->setFont(font);

        for (const auto& entry : group.value())
        {
            const auto label = entry.label.isEmpty() ? entry.id : entry.label;
            const auto shortcut =
                entry.action && !entry.action->shortcut().isEmpty() ? entry.action->shortcut() : entry.shortcut;
            const auto shortcutText = shortcut.toString(QKeySequence::NativeText);

            auto* nameItem = new QStandardItem { label };
            nameItem->setEditable(false);
            nameItem->setData(entry.id, CommandIdRole);
            nameItem->setData(QStringList { group.key(), label, entry.id, entry.description, shortcutText }
                                  .join(QLatin1Char(' '))
                                  .simplified(),
                              CommandSearchRole);

            if (entry.action)
            {
                nameItem->setIcon(entry.action->icon());
            }

            auto description = entry.description;
            if (description.isEmpty() && entry.action)
            {
                description = entry.action->toolTip();
            }
            if (!description.isEmpty())
            {
                nameItem->setToolTip(description);
            }

            const bool usable = entry.action && entry.action->isEnabled();
            if (!usable)
            {
                nameItem->setFlags(nameItem->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            }

            auto* shortcutItem = new QStandardItem { shortcutText };
            shortcutItem->setEditable(false);
            if (!usable)
            {
                shortcutItem->setFlags(shortcutItem->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
            }

            categoryItem->appendRow({ nameItem, shortcutItem });
        }

        model->appendRow(categoryItem);
    }
}

void QcwCommandPalettePrivate::triggerCurrent()
{
    const auto index = treeView->currentIndex();
    if (!index.isValid())
    {
        return;
    }

    const auto commandId = index.data(CommandIdRole).toString();
    if (commandId.isEmpty())
    {
        return;
    }

    QPointer<QcwWindow> theWindow = window;

    // Close first and trigger on the next turn: the command may delete this window (and the
    // palette with it), and a dialog that opened over a still-visible palette would look odd.
    owner->close();

    QTimer::singleShot(0, theWindow.data(), [theWindow, commandId]()
    {
        if (!theWindow)
        {
            return;
        }

        // The command QAction the window resolves carries the window's own id, which is what
        // Remove This Window needs as its target, so the palette has no command-specific path.
        auto* action = theWindow->resolveCommand(commandId);
        if (!action || !action->isEnabled())
        {
            return;
        }

        action->trigger();
    });
}

void QcwCommandPalettePrivate::selectFirstEnabled()
{
    const auto indexes = enabledCommandIndexes(proxyModel);
    treeView->setCurrentIndex(indexes.isEmpty() ? QModelIndex {} : indexes.first());
}

void QcwCommandPalettePrivate::moveCurrent(int step)
{
    const auto indexes = enabledCommandIndexes(proxyModel);
    if (indexes.isEmpty())
    {
        return;
    }

    const int current = indexes.indexOf(treeView->currentIndex());
    int next = step > 0 ? 0 : indexes.size() - 1;
    if (current >= 0)
    {
        next = std::clamp(current + step, 0, static_cast<int>(indexes.size()) - 1);
    }

    treeView->setCurrentIndex(indexes.at(next));
}

QT_END_NAMESPACE
