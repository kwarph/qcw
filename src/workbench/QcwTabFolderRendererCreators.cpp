// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "QcwTabFolderRendererCreators.h"
#include <QGlobalStatic>

QT_BEGIN_NAMESPACE

Q_GLOBAL_STATIC(QcwTabFolderRendererCreators, folderRendererCreatorsInstance);

QcwTabFolderRendererCreators* QcwTabFolderRendererCreators::instance()
{
    return folderRendererCreatorsInstance;
}

QT_END_NAMESPACE
