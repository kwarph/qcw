# examples/workbench_demo/CMakeLists.txt

project(workbench_demo VERSION 1.0.0 LANGUAGES CXX)

include_directories(common)

add_subdirectory(common)
add_subdirectory(plugins)
add_subdirectory(app)
