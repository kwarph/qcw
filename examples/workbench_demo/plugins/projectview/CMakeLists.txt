# examples/workbench_demo/plugins/projectview/CMakeLists.txt

qcw_demo_add_plugin(projectview
    PluginInfo.h
    projectview.json
    ProjectView.h ProjectView.cpp
    ProjectTreeModel.h ProjectTreeModel.cpp
    ProjectFilterProxyModel.h ProjectFilterProxyModel.cpp
    ProjectFilterDialog.h ProjectFilterDialog.cpp
    ProjectTreeView.h ProjectTreeView.cpp
    ProjectViewCreator.h ProjectViewCreator.cpp
)
