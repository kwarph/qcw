// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QtGlobal>

/**
 * @brief Export macro for the demo:: common helper library (democommon).
 *
 * @details Mirrors QcwGlobal.h's QCW_EXPORT for the QCW library itself, but for the demo's
 * own shared helper. QCW_EXPORT must not be reused here: it tracks QCW's build, and
 * defining QCW_LIBRARY while compiling the demo helpers makes the linker emit protected
 * references to QCW's symbols, which cannot be imported from another shared library.
 */
#ifndef QCW_DEMO_STATIC_LIBRARY
#ifdef QCW_DEMO_LIBRARY
#define QCW_DEMO_EXPORT Q_DECL_EXPORT
#else
#define QCW_DEMO_EXPORT Q_DECL_IMPORT
#endif
#else
#define QCW_DEMO_EXPORT
#endif
