// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPerspective.h"

class FocusPerspective : public QcwPerspective
{
public:
    explicit FocusPerspective(QcwWindow* window = nullptr);

    void createInitialLayout() override;
};
