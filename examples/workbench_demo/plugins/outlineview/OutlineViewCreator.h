// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPluginTemplate.h"
#include "QcwViewCreator.h"

/**
 * @brief Creates OutlineView views, which follow the selection of the current editor.
 */
class OutlineViewCreator : public QcwViewCreator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qcw.QcwViewCreator" FILE "outlineview.json")
    Q_INTERFACES(QcwViewCreator)
    QCW_PLUGIN_INTERFACE

public:
    QString defaultWindowTitle() const override;
    QIcon defaultViewIcon() const override;

    bool singleInstancePerWindow() const override
    {
        return true;
    }

private:
    QcwView* doCreate(QWidget* parent, const QList<QVariant>& args) override;
};
