// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwLambdaCommandHandler.h"

QT_BEGIN_NAMESPACE

QcwLambdaCommandHandler::QcwLambdaCommandHandler(QObject* parent) :
    QcwCommandHandler { parent }
{
}

void QcwLambdaCommandHandler::execute(const Params& params)
{
    if (mExecute)
    {
        mExecute(params);
    }
}

bool QcwLambdaCommandHandler::isChecked() const
{
    return mChecked ? mChecked() : false;
}

void QcwLambdaCommandHandler::setChecked(bool checked)
{
    if (mSetChecked)
    {
        mSetChecked(checked);
    }
}

bool QcwLambdaCommandHandler::isEnabled() const
{
    return mEnabled ? mEnabled() : QcwCommandHandler::isEnabled();
}

QcwLambdaCommandHandler& QcwLambdaCommandHandler::onExecute(ExecuteFn fn)
{
    mExecute = std::move(fn);
    return *this;
}

QcwLambdaCommandHandler& QcwLambdaCommandHandler::onChecked(CheckedFn fn)
{
    mChecked = std::move(fn);
    return *this;
}

QcwLambdaCommandHandler& QcwLambdaCommandHandler::onSetChecked(SetCheckedFn fn)
{
    mSetChecked = std::move(fn);
    return *this;
}

QcwLambdaCommandHandler& QcwLambdaCommandHandler::onEnabled(EnabledFn fn)
{
    mEnabled = std::move(fn);
    return *this;
}

QT_END_NAMESPACE
