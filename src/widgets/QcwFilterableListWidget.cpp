// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwFilterableListWidget.h"
#include "QcwFixedRowHeightItemDelegate.h"

#include <QLineEdit>
#include <QListView>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOptionViewItem>

QT_BEGIN_NAMESPACE

class QcwFilterableListWidgetPrivate
{
public:
    explicit QcwFilterableListWidgetPrivate(QcwFilterableListWidget* interface) :
        owner { interface }
    {
    }

    /// The item behind @p viewIndex, an index of the proxy model that the view shows.
    QStandardItem* itemAt(const QModelIndex& viewIndex) const
    {
        if (!viewIndex.isValid())
        {
            return nullptr;
        }

        return model->item(proxy->mapToSource(viewIndex).row());
    }

    /**
     * @brief Reports @p item as the user's choice.
     * @details A double click makes the view emit both clicked() and activated() for one
     * gesture, so committing the same item twice in a row is squashed here; changing the
     * filter text re-arms it.
     */
    void commit(QStandardItem* item)
    {
        if (item == nullptr || item == lastCommittedItem)
        {
            return;
        }

        lastCommittedItem = item;
        emit owner->itemSelected(item);
    }

    /// Re-applies the texts the constructor captured, for the language now installed.
    void retranslateUi();

public:
    QcwFilterableListWidget* owner;
    QLineEdit* filterEdit { nullptr };
    QFrame* separator { nullptr };
    QListView* view { nullptr };
    QStandardItemModel* model { nullptr };
    QSortFilterProxyModel* proxy { nullptr };
    int maxTextWidth { 0 };
    int longestRow { -1 };
    QStandardItem* lastCommittedItem { nullptr };
};

// -----------------------------------------------------------------------------

/// Margin around, and gap between, the three widgets of the frame.
static constexpr int Spacing = 6;
/// Rows sizeHint() is willing to count before the list has to scroll.
static constexpr int MaxVisibleRows = 20;
/// Size suggested for a list that has no items to measure yet.
static constexpr int EmptySizeHint = 200;

QcwFilterableListWidget::QcwFilterableListWidget(bool filterBarOnTop, QWidget* parent) :
    QFrame { parent }, impl { new QcwFilterableListWidgetPrivate { this } }
{
    auto verticalLayout = new QVBoxLayout(this);
    verticalLayout->setContentsMargins(Spacing, Spacing, Spacing, Spacing);
    verticalLayout->setSpacing(Spacing);

    impl->filterEdit = new QLineEdit(this);
    impl->filterEdit->setObjectName("filterEdit");
    impl->filterEdit->setClearButtonEnabled(true);

    impl->separator = new QFrame(this);
    impl->separator->setObjectName("separator");
    impl->separator->setFrameShape(QFrame::HLine);
    impl->separator->setFrameShadow(QFrame::Sunken);

    impl->view = new QListView(this);
    impl->view->setObjectName("view");
    impl->view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    impl->view->setFrameShape(QFrame::NoFrame);
    impl->view->setFrameShadow(QFrame::Plain);
    impl->view->setIconSize(QSize(QcwFixedRowHeightItemDelegate::IconSize, QcwFixedRowHeightItemDelegate::IconSize));
    impl->view->setItemDelegate(new QcwFixedRowHeightItemDelegate { impl->view });

    if (filterBarOnTop)
    {
        verticalLayout->addWidget(impl->filterEdit);
        verticalLayout->addWidget(impl->separator);
        verticalLayout->addWidget(impl->view);
    }
    else
    {
        verticalLayout->addWidget(impl->view);
        verticalLayout->addWidget(impl->separator);
        verticalLayout->addWidget(impl->filterEdit);
    }

    impl->model = new QStandardItemModel(this);
    impl->proxy = new QSortFilterProxyModel(this);
    impl->proxy->setSourceModel(impl->model);
    impl->proxy->setDynamicSortFilter(true);
    impl->proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    impl->proxy->setFilterRole(Qt::DisplayRole);

    impl->view->setModel(impl->proxy);

    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);

    connect(impl->filterEdit, &QLineEdit::textChanged, this, [this](const QString& text)
    {
        impl->lastCommittedItem = nullptr;
        // Fixed string, not a pattern: the user is typing characters to search for, and an
        // invalid expression would hide the whole list.
        impl->proxy->setFilterFixedString(text);
        // index() is invalid when the filter matches nothing, which also clears a current
        // index that has just been filtered out.
        impl->view->setCurrentIndex(impl->view->model()->index(0, 0));
    });

    connect(impl->filterEdit, &QLineEdit::returnPressed, this, [this]()
    {
        // Resolve the item before resetting the filter: the proxy index of the current row
        // does not survive the rows that filtering adds back.
        auto item = impl->itemAt(impl->view->currentIndex());
        impl->filterEdit->clear();
        // Committing runs consumer code that may close and delete this widget, so it goes last.
        impl->commit(item);
    });

    connect(impl->view, &QListView::clicked, this, [this](const QModelIndex& index)
    {
        impl->commit(impl->itemAt(index));
    });

    connect(impl->view, &QListView::activated, this, [this](const QModelIndex& index)
    {
        impl->commit(impl->itemAt(index));
    });

    impl->retranslateUi();
}

QStandardItem* QcwFilterableListWidget::addItem(const QString& text, const QIcon& icon, const QFont& font)
{
    auto item = new QStandardItem(icon, text);
    item->setData(font, Qt::FontRole);

    // Remember the widest row for sizeHint(), measured with the font of this very item:
    // rows are not all drawn in the view font.
    const int width = QFontMetrics { font }.horizontalAdvance(text);
    if (width > impl->maxTextWidth)
    {
        impl->maxTextWidth = width;
        impl->longestRow = impl->model->rowCount();
    }

    impl->model->appendRow(item);

    return item;
}

QcwFilterableListWidget::~QcwFilterableListWidget()
{
    delete impl;
}

void QcwFilterableListWidget::setFilterBarVisible(bool visible)
{
    impl->filterEdit->setVisible(visible);
    impl->separator->setVisible(visible);
}

void QcwFilterableListWidget::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);

    if (!impl->filterEdit->isHidden())
    {
        impl->filterEdit->setFocus();
    }
}

void QcwFilterableListWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        impl->retranslateUi();
    }

    QFrame::changeEvent(event);
}

QSize QcwFilterableListWidget::sizeHint() const
{
    const int rows = impl->model->rowCount();
    if (impl->longestRow < 0)
    {
        return QSize(EmptySizeHint, EmptySizeHint);
    }

    QStyleOptionViewItem option;
    option.initFrom(impl->view);
    option.decorationSize = impl->view->iconSize();
    option.fontMetrics = QFontMetrics { impl->model->item(impl->longestRow)->font() };

    const QSize longestRowSize = impl->view->itemDelegate()->sizeHint(option, impl->model->index(impl->longestRow, 0));

    // A vertical scroll bar takes width away from the rows only when there are rows to scroll.
    const int visibleRows = qMin(rows, MaxVisibleRows);
    const int scrollBarExtent =
        visibleRows < rows ? style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this) : 0;

    const int width = longestRowSize.width() + scrollBarExtent + 2 * Spacing + 2 * frameWidth();

    // The frame margins, the rows, and the filter bar with the two gaps it brings.
    int height = visibleRows * QcwFixedRowHeightItemDelegate::RowHeight + 2 * Spacing + 2 * frameWidth();
    if (!impl->filterEdit->isHidden())
    {
        height += impl->filterEdit->sizeHint().height() + Spacing;
    }
    if (!impl->separator->isHidden())
    {
        height += impl->separator->sizeHint().height() + Spacing;
    }

    return QSize(width, height);
}

void QcwFilterableListWidgetPrivate::retranslateUi()
{
    filterEdit->setPlaceholderText(QCoreApplication::translate("QcwFilterableListWidget", "Filter"));
}

QT_END_NAMESPACE
