# Stages the repository's themes and translations directories next to a demo executable:
# QcwThemeManager and the language selector search <application directory>/themes and
# <application directory>/translations, so a build-tree run finds what an installed run
# finds in the qcw data directories.
function(qcw_demo_deploy_runtime_data target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/themes" "$<TARGET_FILE_DIR:${target}>/themes"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/translations" "$<TARGET_FILE_DIR:${target}>/translations"
        COMMENT "Deploying themes and translations to $<TARGET_FILE_DIR:${target}>"
    )
endfunction()

add_subdirectory(folder_renderer_demo)
add_subdirectory(folder_container_demo)
add_subdirectory(workbench_demo)
