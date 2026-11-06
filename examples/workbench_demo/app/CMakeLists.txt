# examples/workbench_demo/app/CMakeLists.txt

qt_add_executable(${PROJECT_NAME}
    main.cpp
    MainWindow.h MainWindow.cpp
    Workbench.h Workbench.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    qcommonworkbench
    democommon
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

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
)

# A build-tree run reads themes and translations from beside the executable, so stage both
# repository directories into bin with every build.
qcw_demo_deploy_runtime_data(${PROJECT_NAME})
