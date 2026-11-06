# examples/workbench_demo/plugins/outlineview/CMakeLists.txt

qcw_demo_add_plugin(outlineview
    PluginInfo.h
    outlineview.json
    OutlineView.h OutlineView.cpp
    OutlineViewCreator.h OutlineViewCreator.cpp
)
