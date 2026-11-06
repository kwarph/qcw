# examples/workbench_demo/plugins/logview/CMakeLists.txt

qcw_demo_add_plugin(logview
    PluginInfo.h
    logview.json
    LogView.h LogView.cpp
    LogViewCreator.h LogViewCreator.cpp
    LogViewAppearancePreferencesPage.h LogViewAppearancePreferencesPage.cpp
)
