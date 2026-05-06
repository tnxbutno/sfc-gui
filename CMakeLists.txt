cmake_minimum_required(VERSION 3.25)
project(sfc-gui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

# ---------------------------------------------------------------------------
# Qt 6
# ---------------------------------------------------------------------------

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# ---------------------------------------------------------------------------
# libsfc_lib — paths relative to this project's location
# ---------------------------------------------------------------------------

set(SFC_ROOT "${CMAKE_SOURCE_DIR}/../sfc" CACHE PATH "Path to the sfc source tree")
set(SFC_BUILD_DIR "${SFC_ROOT}/build" CACHE PATH "Path to the sfc build directory")

if(NOT EXISTS "${SFC_ROOT}/include/sfc/encoder.h")
    message(FATAL_ERROR "SFC headers not found at ${SFC_ROOT}/include. Set -DSFC_ROOT=<path>.")
endif()

# find_library resolves the correct name (libsfc_lib.a / sfc_lib.lib) and
# searches Release/Debug subdirs for multi-config generators (Visual Studio).
find_library(SFC_LIB_LOCATION
    NAMES sfc_lib
    PATHS
        "${SFC_BUILD_DIR}"
        "${SFC_BUILD_DIR}/Release"
        "${SFC_BUILD_DIR}/Debug"
        "${SFC_BUILD_DIR}/RelWithDebInfo"
    NO_DEFAULT_PATH
    REQUIRED
)

add_library(sfc_lib STATIC IMPORTED)
set_target_properties(sfc_lib PROPERTIES
    IMPORTED_LOCATION             "${SFC_LIB_LOCATION}"
    INTERFACE_INCLUDE_DIRECTORIES "${SFC_ROOT}/include"
)

# sfc_lib itself depends on blake3, zstd, brotli, lz4.
find_library(BLAKE3_LIB
    NAMES blake3
    PATHS
        "${SFC_BUILD_DIR}/_deps/blake3-build"
        "${SFC_BUILD_DIR}/_deps/blake3-build/Release"
        "${SFC_BUILD_DIR}/_deps/blake3-build/Debug"
    NO_DEFAULT_PATH
    REQUIRED
)

if(WIN32)
    find_package(zstd              CONFIG REQUIRED)
    find_package(unofficial-brotli CONFIG REQUIRED)
    find_package(lz4               CONFIG REQUIRED)

    set(SFC_COMP_LIBS
        zstd::libzstd_static
        unofficial::brotli::brotlienc
        unofficial::brotli::brotlidec
        LZ4::lz4_static
    )
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ZSTD   REQUIRED libzstd)
    pkg_check_modules(BROTLI REQUIRED libbrotlienc libbrotlidec)
    pkg_check_modules(LZ4    REQUIRED liblz4)

    set(SFC_COMP_LIBS
        ${ZSTD_LIBRARIES}
        ${BROTLI_LIBRARIES}
        ${LZ4_LIBRARIES}
    )
    set(SFC_COMP_LINK_DIRS
        ${ZSTD_LIBRARY_DIRS}
        ${BROTLI_LIBRARY_DIRS}
        ${LZ4_LIBRARY_DIRS}
    )
endif()

set_property(TARGET sfc_lib APPEND PROPERTY
    INTERFACE_LINK_LIBRARIES ${BLAKE3_LIB} ${SFC_COMP_LIBS}
)
if(NOT WIN32)
    set_property(TARGET sfc_lib APPEND PROPERTY
        INTERFACE_LINK_DIRECTORIES ${SFC_COMP_LINK_DIRS}
    )
endif()

# ---------------------------------------------------------------------------
# sfc-gui executable
# ---------------------------------------------------------------------------

set(GUI_SOURCES
    main.cpp
    mainwindow.cpp
    worker/sfcworker.cpp
    widgets/dropzone.cpp
    widgets/chunkgridwidget.cpp
    widgets/metadataeditor.cpp
    tabs/packtab.cpp
    tabs/unpacktab.cpp
    tabs/infotab.cpp
    tabs/verifytab.cpp
)

set(GUI_HEADERS
    mainwindow.h
    worker/sfcworker.h
    widgets/dropzone.h
    widgets/chunkgridwidget.h
    widgets/metadataeditor.h
    tabs/packtab.h
    tabs/unpacktab.h
    tabs/infotab.h
    tabs/verifytab.h
)

qt_add_executable(sfc-gui
    ${GUI_SOURCES}
    ${GUI_HEADERS}
)

target_include_directories(sfc-gui PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(sfc-gui PRIVATE
    Qt6::Widgets
    sfc_lib
)

set_target_properties(sfc-gui PROPERTIES
    MACOSX_BUNDLE                   TRUE
    MACOSX_BUNDLE_BUNDLE_NAME       "SFC"
    MACOSX_BUNDLE_BUNDLE_VERSION    ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

enable_testing()

add_executable(sfc_gui_tests
    tests/test_gui.cpp
    worker/sfcworker.cpp
    widgets/metadataeditor.cpp
    widgets/chunkgridwidget.cpp
    widgets/dropzone.cpp
)

target_include_directories(sfc_gui_tests PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(sfc_gui_tests PRIVATE
    Qt6::Widgets
    Qt6::Test
    sfc_lib
)

add_test(NAME sfc_gui_tests COMMAND sfc_gui_tests)
