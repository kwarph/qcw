// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwWorkbench.h"

class Workbench : public QcwWorkbench
{
protected:
    QcwWindow* createWindow() override;
    bool preStart() override;
};
