// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <QtGlobal>

/**
 * @brief Export macro for the QCW library.
 *
 * @details Handles symbol visibility for shared libraries.
 * - When building the library (QCW_LIBRARY defined), symbols are exported.
 * - When using the library, symbols are imported.
 * - For static builds (QCW_STATIC_LIBRARY defined), the macro is empty.
 *
 * @note This header deliberately carries nothing beyond the macro, so every translation unit
 * can include it without pulling in module-specific vocabulary. Docking and tab-folder types
 * live in QcwDockingTypes.h.
 */
#ifndef QCW_STATIC_LIBRARY
#ifdef QCW_LIBRARY
#define QCW_EXPORT Q_DECL_EXPORT
#else
#define QCW_EXPORT Q_DECL_IMPORT
#endif
#else
#define QCW_EXPORT
#endif
