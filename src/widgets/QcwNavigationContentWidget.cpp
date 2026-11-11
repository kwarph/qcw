// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwNavigationContentWidget.h"
#include "QcwOverlayScrollBarUtils.h"
#include "QcwSplitter.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTreeView>

class QcwNavigationContentWidgetPrivate
{
public:
    explicit QcwNavigationContentWidgetPrivate(QcwNavigationContentWidget* interface);

    void setupUi();

public:
    QcwNavigationContentWidget* owner;
    QLineEdit* lineEdit { nullptr };
    QLabel* pathLabel { nullptr };
    QSplitter* splitter { nullptr };
    QTreeView* treeView { nullptr };
    QVBoxLayout* rightLayout { nullptr };
    QWidget* contentWidget { nullptr };
};

// -----------------------------------------------------------------------------

QcwNavigationContentWidget::QcwNavigationContentWidget(QWidget* parent) :
    QWidget { parent }, impl { new QcwNavigationContentWidgetPrivate { this } }
{
    impl->setupUi();
}

QcwNavigationContentWidget::~QcwNavigationContentWidget()
{
    delete impl;
}

QSplitter* QcwNavigationContentWidget::splitter() const
{
    return impl->splitter;
}

QTreeView* QcwNavigationContentWidget::treeView() const
{
    return impl->treeView;
}

QLineEdit* QcwNavigationContentWidget::filterEdit() const
{
    return impl->lineEdit;
}

QLabel* QcwNavigationContentWidget::navigationLabel() const
{
    return impl->pathLabel;
}

void QcwNavigationContentWidget::setContentWidget(QWidget* contentWidget)
{
    if (contentWidget && !impl->contentWidget)
    {
        impl->contentWidget = contentWidget;
        impl->contentWidget->setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        impl->rightLayout->addWidget(impl->contentWidget, 1);
    }
}

QWidget* QcwNavigationContentWidget::contentWidget() const
{
    return impl->contentWidget;
}

// -----------------------------------------------------------------------------

QcwNavigationContentWidgetPrivate::QcwNavigationContentWidgetPrivate(QcwNavigationContentWidget* interface) :
    owner { interface }
{
}

void QcwNavigationContentWidgetPrivate::setupUi()
{
    auto mainLayout = new QVBoxLayout(owner);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    splitter = new QcwSplitter(owner);
    splitter->setObjectName("splitter");
    splitter->setOrientation(Qt::Orientation::Horizontal);

    auto widget = new QWidget(splitter);
    auto vLayout = new QVBoxLayout(widget);
    vLayout->setSpacing(0);
    vLayout->setContentsMargins(0, 0, 0, 0);
    auto hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(6, 6, 6, 6);

    lineEdit = new QLineEdit(widget);
    lineEdit->setObjectName("lineEdit");
    lineEdit->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
    lineEdit->setClearButtonEnabled(true);
    // A subclass normally replaces this with its own translated label; the base keeps a
    // usable placeholder for frames that do not have one.
    lineEdit->setPlaceholderText(QcwNavigationContentWidget::tr("Filter"));

    hLayout->addWidget(lineEdit);

    vLayout->addLayout(hLayout);

    treeView = new QTreeView(widget);
    treeView->setObjectName("treeView");
    treeView->setSizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
    treeView->setFrameShape(QFrame::Shape::NoFrame);
    treeView->setLineWidth(0);
    treeView->setHeaderHidden(true);
    treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView->setUniformRowHeights(true);

    attachOverlayScrollBar(treeView, widget);

    vLayout->addWidget(treeView);

    splitter->addWidget(widget);

    auto widget1 = new QWidget(splitter);
    rightLayout = new QVBoxLayout(widget1);
    rightLayout->setSpacing(0);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto hLayout2 = new QHBoxLayout();
    hLayout2->setContentsMargins(6, 6, 6, 3);

    pathLabel = new QLabel(widget1);
    pathLabel->setObjectName("pathLabel");
    QSizePolicy sizePolicy2(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Preferred);
    sizePolicy2.setHorizontalStretch(0);
    sizePolicy2.setVerticalStretch(0);
    sizePolicy2.setHeightForWidth(pathLabel->sizePolicy().hasHeightForWidth());
    pathLabel->setSizePolicy(sizePolicy2);
    pathLabel->setFixedHeight(lineEdit->height());

    hLayout2->addWidget(pathLabel);

    rightLayout->addLayout(hLayout2);

    auto line = new QFrame(widget1);
    line->setObjectName("line");
    line->setFrameShape(QFrame::Shape::HLine);
    line->setFrameShadow(QFrame::Shadow::Sunken);

    rightLayout->addWidget(line);

    splitter->addWidget(widget1);

    mainLayout->addWidget(splitter);
}
