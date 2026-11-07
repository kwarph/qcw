cmake_minimum_required(VERSION 3.23)
# Single source of truth for the library version; release tags are v${PROJECT_VERSION}.
project(qcw
    VERSION 2.0.0
    LANGUAGES CXX
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(QCW_DEBUG)
endif()

option(QCW_TREAT_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
if(QCW_TREAT_WARNINGS_AS_ERRORS)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:-Werror=uninitialized>
        $<$<COMPILE_LANGUAGE:CXX>:-Werror=return-type>
    )
endif()

option(QCW_BUILD_TESTS "Build the test suite" OFF)
option(QCW_BUILD_DOCS "Build the API documentation target" OFF)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} 6.9 REQUIRED COMPONENTS
    Widgets
    Svg
)

include(GNUInstallDirs)

option(QCW_USE_SYSTEM_QCWDOCKING "Use an installed qcwdocking instead of the bundled submodule" OFF)
if(QCW_USE_SYSTEM_QCWDOCKING)
    find_package(qcwdocking 1.0 REQUIRED)
else()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcw-docking/CMakeLists.txt")
        message(FATAL_ERROR
            "third_party/qcw-docking is missing. Run 'git submodule update --init --recursive', "
            "or configure with -DQCW_USE_SYSTEM_QCWDOCKING=ON to use an installed qcwdocking.")
    endif()
    add_subdirectory(third_party/qcw-docking)
endif()

add_subdirectory(src)
add_subdirectory(examples)

if(QCW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(QCW_BUILD_DOCS)
    add_subdirectory(docs)
endif()

install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/qcw)
install(DIRECTORY themes translations DESTINATION ${CMAKE_INSTALL_DATADIR}/qcw)
