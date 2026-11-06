# examples/folder_renderer_demo/CMakeLists.txt

project(folder_renderer_demo VERSION 1.0.0 LANGUAGES CXX)

qt_add_executable(${PROJECT_NAME}
    main.cpp
    MainWindow.h
    MainWindow.cpp
    MainWindow.ui
    PreferencesDialog.h
    PreferencesDialog.cpp
    PreferencesDialog.ui
    StyledTabFolderRenderer.h
    StyledTabFolderRenderer.cpp
    DefaultTabFolderRenderer.h
    DefaultTabFolderRenderer.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    qcommonworkbench
    Qt::Widgets
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"

    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"

    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)

qcw_demo_deploy_runtime_data(${PROJECT_NAME})
