# A bundled qcwdocking keeps its catalogs in its own checkout; an installed one asks the
# caller to set the directory explicitly.
set(QCW_QCWDOCKING_TRANSLATIONS_DIR "" CACHE PATH
    "Directory holding the qcwdocking translation catalogs to stage beside demos")
if(NOT QCW_USE_SYSTEM_QCWDOCKING AND NOT QCW_QCWDOCKING_TRANSLATIONS_DIR)
    set(QCW_QCWDOCKING_TRANSLATIONS_DIR "${PROJECT_SOURCE_DIR}/third_party/qcw-docking/translations")
endif()

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

    if(EXISTS "${QCW_QCWDOCKING_TRANSLATIONS_DIR}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${QCW_QCWDOCKING_TRANSLATIONS_DIR}" "$<TARGET_FILE_DIR:${target}>/translations"
            COMMENT "Deploying qcwdocking translations to $<TARGET_FILE_DIR:${target}>"
        )
    endif()
endfunction()

add_subdirectory(workbench_demo)
