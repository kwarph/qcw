# examples/workbench_demo/plugins/projectperspective/CMakeLists.txt

qcw_demo_add_plugin(projectperspective
    PluginInfo.h
    projectperspective.json
    ProjectPerspective.h ProjectPerspective.cpp
    ProjectPerspectiveCreator.h ProjectPerspectiveCreator.cpp
)
