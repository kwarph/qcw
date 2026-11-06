# examples/workbench_demo/plugins/notes/CMakeLists.txt
#
# Two targets, and that split is the demonstration: the store is a plain QcwPlugin with no
# view, the view is an ordinary QcwViewCreator plugin. Neither links to the other; they meet
# through demo::INotesService in common/DemoNotes.h.

# The non-UI plugin. It builds no widget of its own; the Qt::Widgets dependency comes from
# the demo helper and from QcwPreferencesPage, which the page it contributes derives from.
qcw_demo_add_plugin(notesplugin
    PluginInfo.h
    notes.json
    NotesPlugin.h NotesPlugin.cpp
    NotesService.h NotesService.cpp
    NotesPreferencesPage.h NotesPreferencesPage.cpp
)

# The window in which the store is visible.
qcw_demo_add_plugin(notesview
    PluginInfo.h
    notesview.json
    NotesView.h NotesView.cpp
    NotesViewCreator.h NotesViewCreator.cpp
)
