// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "DemoGlobal.h"

namespace demo
{

/// The absolute path of @p path, resolved through symbolic links when it exists.
QCW_DEMO_EXPORT QString normalizedPath(const QString& path);

/// Whether @p path is @p prefix itself or lies below it.
QCW_DEMO_EXPORT bool pathIsUnder(const QString& path, const QString& prefix);

/// The project folders the last session had open.
QCW_DEMO_EXPORT QStringList rememberedProjectRoots();

QCW_DEMO_EXPORT void rememberProjectRoots(const QStringList& roots);

/// The project the last session had current, falling back to the first remembered project.
QCW_DEMO_EXPORT QString rememberedCurrentProject();

QCW_DEMO_EXPORT void rememberCurrentProject(const QString& root);

} // namespace demo
