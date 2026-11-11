// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwNavigationContentDialog.h"
#include "QcwNavigationContentWidget.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE

QcwNavigationContentDialog::QcwNavigationContentDialog(QWidget* parent) :
    QDialog { parent }
{
    mMainLayout = new QVBoxLayout(this);
    mMainLayout->setObjectName("verticalLayout");
    mMainLayout->setContentsMargins(0, 0, 0, 0);
    mMainLayout->setSpacing(0);

    auto line = new QFrame(this);
    line->setObjectName("line");
    line->setFrameShape(QFrame::Shape::HLine);
    line->setFrameShadow(QFrame::Shadow::Sunken);
    mMainLayout->addWidget(line);

    mButtonBox = new QDialogButtonBox(this);
    mButtonBox->setObjectName("buttonBox");
    mButtonBox->setOrientation(Qt::Orientation::Horizontal);

    // Keep the action buttons on the left and the standard buttons on the right, with the
    // stretch in between; that is the layout the preferences dialog had before the split.
    mButtonLayout = new QHBoxLayout();
    mButtonLayout->setContentsMargins(9, 9, 9, 9);
    mButtonLayout->setSpacing(9);
    mButtonLayout->addStretch(1);
    mButtonLayout->addWidget(mButtonBox);

    mMainLayout->addLayout(mButtonLayout);
}

void QcwNavigationContentDialog::setContentWidget(QcwNavigationContentWidget* contentWidget)
{
    if (!contentWidget || mContentWidget)
    {
        return;
    }

    mContentWidget = contentWidget;
    mMainLayout->insertWidget(0, mContentWidget, 1);
}

void QcwNavigationContentDialog::addActionButton(QAbstractButton* button)
{
    if (!button || !mButtonLayout)
    {
        return;
    }

    // The button box sits after the stretch; insert before both so action buttons keep their
    // insertion order and stay left of the stretch.
    const int buttonBoxIndex = mButtonLayout->indexOf(mButtonBox);
    mButtonLayout->insertWidget(buttonBoxIndex > 0 ? buttonBoxIndex - 1 : 0, button);
}

QT_END_NAMESPACE
