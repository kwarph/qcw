// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPluginTemplate.h"
#include "QcwViewCreator.h"

#include <QSet>

class LogView;

/**
 * @brief Creates LogView views, which show the process log as QcwLogger writes it.
 *
 * The creator also registers the views' appearance page; once registered, the page belongs to
 * QcwPreferencesManager and a font chosen there reaches the log views that are already open.
 */
class LogViewCreator : public QcwViewCreator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qcw.QcwViewCreator" FILE "logview.json")
    Q_INTERFACES(QcwViewCreator)
    QCW_PLUGIN_INTERFACE

public:
    QString defaultWindowTitle() const override;
    QIcon defaultViewIcon() const override;

protected:
    void doRegisterContributions() override;

private:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>& args) override;

    QSet<LogView*> mViews;
};
