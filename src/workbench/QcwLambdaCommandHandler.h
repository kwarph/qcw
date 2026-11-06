// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwCommandHandler.h"

#include <functional>
#include <utility>

QT_BEGIN_NAMESPACE

/**
 * @class QcwLambdaCommandHandler
 * @brief A QcwCommandHandler whose behaviour is supplied by lambdas.
 *
 * @details The workbench and the window use it for the built-in commands, whose bodies are a
 * few lines that would not earn a class. Each lambda is optional: a handler that only needs
 * execute() leaves isChecked() and setChecked() unset, and isChecked() then answers false.
 *
 * There is no lambda for visibility, so a lambda handler is always visible and its command
 * keeps its menu entry as long as it has a handler at all. Subclass QcwCommandHandler when a
 * command must disappear with its context.
 */
class QCW_EXPORT QcwLambdaCommandHandler : public QcwCommandHandler
{
    Q_OBJECT
public:
    using ExecuteFn = std::function<void(const QVariantMap&)>;
    using CheckedFn = std::function<bool()>;
    using SetCheckedFn = std::function<void(bool)>;
    using EnabledFn = std::function<bool()>;

    explicit QcwLambdaCommandHandler(QObject* parent = nullptr);

    void execute(const Params& params) override;
    bool isChecked() const override;
    void setChecked(bool checked) override;
    bool isEnabled() const override;

    QcwLambdaCommandHandler& onExecute(ExecuteFn fn);
    QcwLambdaCommandHandler& onChecked(CheckedFn fn);
    QcwLambdaCommandHandler& onSetChecked(SetCheckedFn fn);
    QcwLambdaCommandHandler& onEnabled(EnabledFn fn);

private:
    ExecuteFn mExecute;
    CheckedFn mChecked;
    SetCheckedFn mSetChecked;
    EnabledFn mEnabled;
};

QT_END_NAMESPACE
