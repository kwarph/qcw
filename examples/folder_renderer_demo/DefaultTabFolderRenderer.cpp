// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "DefaultTabFolderRenderer.h"
#include "QcwTabFolder.h"
#include <QJsonObject>

DefaultTabFolderRenderer::DefaultTabFolderRenderer(QcwTabFolder* folder) :
    QcwDefaultTabFolderRenderer { folder }
{
}

QcwTabFolderRenderer* DefaultTabFolderRenderer::clone(QcwTabFolder* folder) const
{
    auto r = new DefaultTabFolderRenderer(folder);
    r->mFolderCss = mFolderCss;

    // setThemeData() applies the stylesheet through mFolder, so a clone into a
    // fresh folder has to repeat that here.
    r->mFolder->setStyleSheet(mFolderCss);

    return r;
}

void DefaultTabFolderRenderer::setThemeData(const QJsonObject& jobj)
{
    const QString style = isLightTheme() ? QStringLiteral("Light") : QStringLiteral("Dark");
    const auto themeObj = jobj[style];

    mFolderCss = themeObj["folderCss"].toString();
    mFolder->setStyleSheet(mFolderCss);
}
