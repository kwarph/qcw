set(TARGET qcommonworkbench)

configure_file(core/QcwVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/QcwVersion.h @ONLY)

add_library(${TARGET} SHARED)

set_target_properties(${TARGET} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON # -fPIC
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME qcw
)

target_sources(${TARGET}
    PRIVATE
        core/QcwLogger.cpp
        core/QcwPathUtils.cpp
        core/QcwPersistable.cpp
        core/QcwPluginLoader.cpp
        core/QcwPluginManager.cpp
        core/QcwSettings.cpp
        core/QcwVersion.cpp

        resources/icons.qrc

        preferences/QcwPreferencesDialog.cpp
        preferences/QcwPreferencesManager.cpp
        preferences/QcwPreferencesPage.cpp
        preferences/QcwPreferencesWidget.cpp

        notifications/QcwNotificationBox.cpp
        notifications/QcwNotificationManager.cpp

        theming/QcwThemeDialog.cpp
        theming/QcwThemeIcons.cpp
        theming/QcwThemeManager.cpp
        theming/QcwThemeUtils.cpp

        widgets/QcwClickableLabel.cpp
        widgets/QcwFixedRowHeightItemDelegate.cpp
        widgets/QcwNavigationContentDialog.cpp
        widgets/QcwNavigationContentWidget.cpp
        widgets/QcwOverlayScrollArea.cpp
        widgets/QcwOverlayScrollBar.cpp
        widgets/QcwOverlayScrollBarAttacher.cpp
        widgets/QcwOverlayScrollBarUtils.cpp
        widgets/QcwSplitter.cpp
        widgets/QcwTreeItem.cpp
        widgets/QcwTreeModel.cpp
        widgets/QcwTreeProxyModel.cpp

        workbench/QcwActionSetRegistry.cpp
        workbench/QcwAppearancePreferencesPage.cpp
        workbench/QcwBuiltinCommands.cpp
        workbench/QcwCommand.cpp
        workbench/QcwCommandHandler.cpp
        workbench/QcwCommandPalette.cpp
        workbench/QcwCommandRegistry.cpp
        workbench/QcwContributionRegistry.cpp
        workbench/QcwContributionSorter.cpp
        workbench/QcwIdUtils.cpp
        workbench/QcwLambdaCommandHandler.cpp
        workbench/QcwNotificationPreferencesPage.cpp
        workbench/QcwPerspective.cpp
        workbench/QcwPerspectiveCreator.cpp
        workbench/QcwSavedPerspectives.cpp
        workbench/QcwServiceRegistry.cpp
        workbench/QcwStyledTabFolderRenderer.cpp
        workbench/QcwTopicRegistry.cpp
        workbench/QcwTopicService.cpp
        workbench/QcwTranslations.cpp
        workbench/QcwView.cpp
        workbench/QcwViewCreator.cpp
        workbench/QcwWindow.cpp
        workbench/QcwWindowStyler.cpp
        workbench/QcwWorkbench.cpp

        ${CMAKE_CURRENT_BINARY_DIR}/QcwVersion.h
)

target_sources(${TARGET}
    PUBLIC
    FILE_SET headers_core TYPE HEADERS
        BASE_DIRS core
        FILES
            core/QcwEnumUtils.h
            core/QcwGlobal.h
            core/QcwLogger.h
            core/QcwPathUtils.h
            core/QcwPersistable.h
            core/QcwPlugin.h
            core/QcwPluginLoader.h
            core/QcwPluginManager.h
            core/QcwPluginTemplate.h
            core/QcwSerializeUtils.h
            core/QcwSettings.h
            core/QcwSettingsProvider.h

    FILE_SET headers_preferences TYPE HEADERS
        BASE_DIRS preferences
        FILES
            preferences/QcwPreferencesDialog.h
            preferences/QcwPreferencesManager.h
            preferences/QcwPreferencesPage.h
            preferences/QcwPreferencesWidget.h

    FILE_SET headers_notifications TYPE HEADERS
        BASE_DIRS notifications
        FILES
            notifications/QcwNotification.h
            notifications/QcwNotificationBox.h
            notifications/QcwNotificationManager.h

    FILE_SET headers_theming TYPE HEADERS
        BASE_DIRS theming
        FILES
            theming/QcwThemeDialog.h
            theming/QcwThemeIcons.h
            theming/QcwThemeManager.h
            theming/QcwThemeUtils.h
            theming/QcwWidgetTheme.h

    FILE_SET headers_widgets TYPE HEADERS
        BASE_DIRS widgets
        FILES
            widgets/QcwClickableLabel.h
            widgets/QcwFixedRowHeightItemDelegate.h
            widgets/QcwNavigationContentDialog.h
            widgets/QcwNavigationContentWidget.h
            widgets/QcwOverlayScrollArea.h
            widgets/QcwOverlayScrollBar.h
            widgets/QcwOverlayScrollBarAttacher.h
            widgets/QcwOverlayScrollBarUtils.h
            widgets/QcwSplitter.h
            widgets/QcwTreeItem.h
            widgets/QcwTreeModel.h
            widgets/QcwTreeProxyModel.h

    FILE_SET headers_workbench TYPE HEADERS
        BASE_DIRS workbench
        FILES
            workbench/QcwActionSet.h
            workbench/QcwActionSetRegistry.h
            workbench/QcwAppearancePreferencesPage.h
            workbench/QcwBuiltinCommands.h
            workbench/QcwCommand.h
            workbench/QcwCommandHandler.h
            workbench/QcwCommandPalette.h
            workbench/QcwCommandRegistry.h
            workbench/QcwContribution.h
            workbench/QcwContributionRegistry.h
            workbench/QcwContributionSorter.h
            workbench/QcwIdUtils.h
            workbench/QcwLambdaCommandHandler.h
            workbench/QcwNotificationPreferencesPage.h
            workbench/QcwPerspective.h
            workbench/QcwPerspectiveCreator.h
            workbench/QcwSavedPerspectives.h
            workbench/QcwServiceRegistry.h
            workbench/QcwStyledTabFolderRenderer.h
            workbench/QcwTopicRegistry.h
            workbench/QcwTopicService.h
            workbench/QcwTranslations.h
            workbench/QcwView.h
            workbench/QcwViewCreator.h
            workbench/QcwWindow.h
            workbench/QcwWindowActionIds.h
            workbench/QcwWindowStyler.h
            workbench/QcwWorkbench.h
)

target_link_libraries(${TARGET}
    PUBLIC Qt::Widgets QCWDOCK::qcwdocking
    PRIVATE Qt::Svg
)

# QScreen::availableGeometry() ignores _NET_WORKAREA on multi-monitor X11, so the
# notification manager reads the workspace itself when XCB is available.
if(UNIX AND NOT APPLE)
    find_package(X11 QUIET)
    if(TARGET X11::xcb)
        target_link_libraries(${TARGET} PRIVATE X11::xcb)
        target_compile_definitions(${TARGET} PRIVATE QCW_HAS_XCB)
    endif()
endif()

target_include_directories(${TARGET} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/qcw>
)

target_compile_definitions(${TARGET} PRIVATE QCW_LIBRARY QT_DEPRECATED_WARNINGS)

install(TARGETS ${TARGET}
    EXPORT qcwTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILE_SET headers_core DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
    FILE_SET headers_preferences DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
    FILE_SET headers_notifications DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
    FILE_SET headers_theming DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
    FILE_SET headers_widgets DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
    FILE_SET headers_workbench DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/QcwVersion.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/qcw
)

include(CMakePackageConfigHelpers)

set(QCW_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/qcw")
set(QCW_INSTALL_DATA_DIR "${CMAKE_INSTALL_DATADIR}/qcw")

install(EXPORT qcwTargets
    FILE qcwTargets.cmake
    NAMESPACE QCW::
    DESTINATION "${QCW_CMAKE_INSTALL_DIR}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/qcwConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/qcwConfig.cmake"
    INSTALL_DESTINATION "${QCW_CMAKE_INSTALL_DIR}"
    PATH_VARS QCW_INSTALL_DATA_DIR
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/qcwConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/qcwConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/qcwConfigVersion.cmake"
    DESTINATION "${QCW_CMAKE_INSTALL_DIR}"
)
