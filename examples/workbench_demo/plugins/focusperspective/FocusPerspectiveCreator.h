// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPerspectiveCreator.h"
#include "QcwPluginTemplate.h"

class FocusPerspectiveCreator : public QcwPerspectiveCreator
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qcw.QcwPerspectiveCreator" FILE "focusperspective.json")
    Q_INTERFACES(QcwPerspectiveCreator)
    QCW_PLUGIN_INTERFACE

public:
    QIcon defaultPerspectiveIcon() const override;
    QString defaultName() const override;

private:
    QcwPerspective* doCreate(QcwWindow* window) override;
};
