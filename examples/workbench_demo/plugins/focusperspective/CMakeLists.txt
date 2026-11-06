# examples/workbench_demo/plugins/focusperspective/CMakeLists.txt

qcw_demo_add_plugin(focusperspective
    PluginInfo.h
    focusperspective.json
    FocusPerspective.h FocusPerspective.cpp
    FocusPerspectiveCreator.h FocusPerspectiveCreator.cpp
)
