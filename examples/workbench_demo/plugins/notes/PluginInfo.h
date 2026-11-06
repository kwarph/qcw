// SPDX-FileCopyrightText: 2026 Cheng Lei <kwarph@users.noreply.github.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// One pair of metadata files per plugin target: notes.json for the store (a plain QcwPlugin)
// and notesview.json for the view (a QcwViewCreator). Both targets share this header, so the
// include guard is what keeps a second inclusion from redefining the macros.

#define QCW_PLUGIN_VERSION "1.0.0"
#define QCW_PLUGIN_PROVIDER "Cheng Lei <kwarph@users.noreply.github.com>"
#define QCW_PLUGIN_UPDATE_TIME "2026-10-28 12:00:00 CST"
#define QCW_PLUGIN_DESCRIPTION QT_TRANSLATE_NOOP("NotesPlugin", "A notes store with no view of its own")
#define QCW_PLUGIN_DETAILS \
    QT_TRANSLATE_NOOP( \
        "NotesPlugin", \
        "The demo's non-UI plugin: it contributes a service, a command palette entry and "\
        "a preferences page, and nothing that appears in a tab by itself.")
