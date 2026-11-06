find_package(Doxygen REQUIRED)

find_program(QCW_QHELPGENERATOR
    NAMES qhelpgenerator
    HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBEXECS}"
          "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}"
)

set(QCW_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(QCW_DOC_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")

if(QCW_QHELPGENERATOR)
    set(QCW_QHG_LOCATION "GENERATE_QHP           = YES\nQHG_LOCATION           = ${QCW_QHELPGENERATOR}")
    set(QCW_QCH_FILE "${QCW_DOC_OUTPUT_DIR}/qch/${PROJECT_NAME}-${PROJECT_VERSION}.qch")
else()
    message(WARNING "qhelpgenerator not found; the docs target writes HTML only, without the .qch")
    set(QCW_QHG_LOCATION "GENERATE_QHP           = NO")
    set(QCW_QCH_FILE "")
endif()

configure_file(Doxyfile.in "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)

add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    BYPRODUCTS "${QCW_DOC_OUTPUT_DIR}/html/index.html" "${QCW_QCH_FILE}"
    COMMENT "Generating the QCW API documentation"
    VERBATIM
)
