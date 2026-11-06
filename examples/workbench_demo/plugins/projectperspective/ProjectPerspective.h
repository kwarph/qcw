// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwPerspective.h"

class ProjectPerspective : public QcwPerspective
{
public:
    explicit ProjectPerspective(QcwWindow* window = nullptr);

    void createInitialLayout() override;
};
