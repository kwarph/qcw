# examples/workbench_demo/plugins/texteditor/CMakeLists.txt

qcw_demo_add_plugin(texteditor
    PluginInfo.h
    texteditor.json
    TextEditor.h TextEditor.cpp
    TextEditorCreator.h TextEditorCreator.cpp
    EditorService.h EditorService.cpp
    TextEditorAppearancePreferencesPage.h TextEditorAppearancePreferencesPage.cpp
)
