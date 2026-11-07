# tests/CMakeLists.txt

qt_add_executable(view_top_right_tool_bar_test
    view_top_right_tool_bar_test.cpp
)

qt_add_executable(view_status_bar_widget_test
    view_status_bar_widget_test.cpp
)

qt_add_executable(plugin_loader_test
    plugin_loader_test.cpp
)

qt_add_executable(folder_minimize_restore_test
    folder_minimize_restore_test.cpp
)

qt_add_executable(theme_icons_test
    theme_icons_test.cpp
)

qt_add_executable(window_styler_test
    window_styler_test.cpp
)

qt_add_executable(perspective_restore_test
    perspective_restore_test.cpp
)

qt_add_executable(theme_utils_test
    theme_utils_test.cpp
)

qt_add_executable(logger_test
    logger_test.cpp
)

qt_add_executable(service_registry_test
    service_registry_test.cpp
)

qt_add_executable(window_command_menu_test
    window_command_menu_test.cpp
)

qt_add_executable(window_remove_command_test
    window_remove_command_test.cpp
)

qt_add_executable(command_palette_test
    command_palette_test.cpp
)

qt_add_executable(singleton_view_test
    singleton_view_test.cpp
)

qt_add_executable(perspective_reset_test
    perspective_reset_test.cpp
)

qt_add_executable(perspective_shared_objects_test
    perspective_shared_objects_test.cpp
)

qt_add_executable(perspective_close_test
    perspective_close_test.cpp
)

qt_add_executable(shared_folder_ownership_test
    shared_folder_ownership_test.cpp
)

qt_add_executable(splitter_test
    splitter_test.cpp
)

qt_add_executable(view_close_protocol_test
    view_close_protocol_test.cpp
)

qt_add_executable(quit_protocol_test
    quit_protocol_test.cpp
)

qt_add_executable(quit_without_windows_test
    quit_without_windows_test.cpp
)

qt_add_executable(language_change_test
    language_change_test.cpp
)

qt_add_executable(tree_proxy_model_test
    tree_proxy_model_test.cpp
)

qt_add_executable(tree_model_test
    tree_model_test.cpp
)

qt_add_executable(contribution_sorter_test
    contribution_sorter_test.cpp
)

qt_add_executable(contribution_registry_test
    contribution_registry_test.cpp
)

qt_add_executable(handler_resolution_test
    handler_resolution_test.cpp
)

qt_add_executable(command_action_test
    command_action_test.cpp
)

qt_add_executable(builtin_commands_test
    builtin_commands_test.cpp
)

qt_add_executable(id_catalog_dump_test
    id_catalog_dump_test.cpp
)

qt_add_executable(topic_service_test
    topic_service_test.cpp
)

qt_add_executable(window_topic_teardown_test
    window_topic_teardown_test.cpp
)

qt_add_executable(view_window_topic_test
    view_window_topic_test.cpp
)

qt_add_executable(translations_test
    translations_test.cpp
)

# The test registers the repository's catalog directory to load a real compiled catalog from it.
# It cannot use the copy the demo deployment stages beside the executables: building this target
# alone does not run that step.
target_compile_definitions(translations_test PRIVATE
    QCW_REPO_TRANSLATIONS_DIR="${CMAKE_SOURCE_DIR}/translations"
)

foreach(target IN ITEMS
    view_top_right_tool_bar_test
    view_status_bar_widget_test
    plugin_loader_test
    folder_minimize_restore_test
    theme_icons_test
    window_styler_test
    perspective_restore_test
    theme_utils_test
    logger_test
    service_registry_test
    window_command_menu_test
    window_remove_command_test
    command_palette_test
    singleton_view_test
    perspective_reset_test
    perspective_shared_objects_test
    perspective_close_test
    shared_folder_ownership_test
    splitter_test
    view_close_protocol_test
    quit_protocol_test
    quit_without_windows_test
    language_change_test
    tree_proxy_model_test
    tree_model_test
    contribution_sorter_test
    contribution_registry_test
    handler_resolution_test
    command_action_test
    builtin_commands_test
    id_catalog_dump_test
    topic_service_test
    window_topic_teardown_test
    view_window_topic_test
    translations_test
)
    target_link_libraries(${target} PRIVATE
        qcommonworkbench
        Qt::Widgets
    )

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QCW_TEST_AUTOCLOSE=1"
    )
endforeach()
