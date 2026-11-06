// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "QcwDefaultTabFolderRenderer.h"

class QJsonObject;

/// The library's default tab renderer plus a JSON-configurable folder stylesheet,
/// the demo's Default window style.
class DefaultTabFolderRenderer : public QcwDefaultTabFolderRenderer
{
public:
    explicit DefaultTabFolderRenderer(QcwTabFolder* folder);

    QcwTabFolderRenderer* clone(QcwTabFolder* folder) const override;

    /// Reads the Light or Dark entry of @p jobj, matching the application palette.
    void setThemeData(const QJsonObject& jobj);

protected:
    QString mFolderCss;
};
