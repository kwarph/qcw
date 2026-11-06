# The plugin payload follows the layout QcwPluginLoader searches: next to the executable on
# Windows, inside the bundle on macOS, and in the installation's private library directory
# elsewhere.
if(APPLE)
    set(QCW_DEMO_PLUGIN_INSTALL_DIR
        "${CMAKE_INSTALL_BINDIR}/examples/${PROJECT_NAME}.app/Contents/PlugIns")
elseif(WIN32)
    set(QCW_DEMO_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}/examples/plugins")
else()
    set(QCW_DEMO_PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}/plugins")
endif()

set(QCW_DEMO_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")

function(qcw_demo_add_plugin target)
    qt_add_plugin(${target} ${ARGN})

    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE ON # -fPIC
    )

    target_link_libraries(${target} PRIVATE
        qcommonworkbench
        democommon
        Qt::Widgets
    )

    target_include_directories(${target} PRIVATE "${QCW_DEMO_COMMON_DIR}")

    # workbench_demo loads its plugins from bin/plugins/workbench_demo, a directory of its own
    # because the other examples deploy theirs to bin/plugins. Nothing fills this directory on
    # its own, so a run would otherwise pick up a library from a previous build whose
    # interfaces may no longer match.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/plugins/workbench_demo"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${target}>" "${CMAKE_BINARY_DIR}/bin/plugins/workbench_demo/"
        COMMENT "Deploying ${target} to bin/plugins/workbench_demo"
    )

    install(TARGETS ${target}
        LIBRARY DESTINATION "${QCW_DEMO_PLUGIN_INSTALL_DIR}"
        ARCHIVE DESTINATION "${QCW_DEMO_PLUGIN_INSTALL_DIR}"
        RUNTIME DESTINATION "${QCW_DEMO_PLUGIN_INSTALL_DIR}"
    )
endfunction()

add_subdirectory(texteditor)
add_subdirectory(logview)
add_subdirectory(projectview)
add_subdirectory(outlineview)
add_subdirectory(projectperspective)
add_subdirectory(focusperspective)
add_subdirectory(notes)
