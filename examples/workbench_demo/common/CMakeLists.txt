set(TARGET democommon)

add_library(${TARGET} SHARED
    DemoFonts.h
    DemoFontPreferenceRow.cpp DemoFontPreferenceRow.h
    DemoGlobal.h
    DemoNotes.h
    DemoStandardContextMenu.cpp DemoStandardContextMenu.h
    DemoService.cpp DemoServices.h
    DemoUtils.h DemoUtils.cpp
    DemoNotes.cpp
)

set_target_properties(${TARGET} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON # -fPIC
)

target_link_libraries(${TARGET} PUBLIC
    qcommonworkbench
    Qt::Widgets
)

target_compile_definitions(${TARGET} PRIVATE QCW_DEMO_LIBRARY QT_DEPRECATED_WARNINGS)

install(TARGETS ${TARGET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
