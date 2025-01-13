cmake_minimum_required(VERSION 3.15)
project(opusfile VERSION "0.12" DESCRIPTION "The opusfile and opusurl libraries provide a high-level API for decoding and seeking within .opus files on disk or over http(s)" LANGUAGES C)

# Required modules
include(GNUInstallDirs)
include(CheckIncludeFiles)
include(CMakePackageConfigHelpers)

include_directories(../libogg/include ../opus/include)

set(OPUSFILE_HEADERS
    include/opusfile.h
)

set(OPUSFILE_SOURCES
    src/info.c
    src/internal.c
    src/opusfile.c
    src/stream.c
)

set(PROJECT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(PROJECT_VERSION_MINOR ${CMAKE_MATCH_2})
set(PROJECT_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# Helper function to configure pkg-config files
function(configure_pkg_config_file pkg_config_file_in)
    set(prefix ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix ${CMAKE_INSTALL_FULL_BINDIR})
    set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
    set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR})
    set(VERSION ${PROJECT_VERSION})
    string(REPLACE ".in" "" pkg_config_file ${pkg_config_file_in})
    configure_file(${pkg_config_file_in} ${pkg_config_file} @ONLY)
endfunction()

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

# Configure config_type.h
check_include_files(inttypes.h INCLUDE_INTTYPES_H)
check_include_files(stdint.h INCLUDE_STDINT_H)
check_include_files(sys/types.h INCLUDE_SYS_TYPES_H)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(SIZE16 int16_t)
set(USIZE16 uint16_t)
set(SIZE32 int32_t)
set(USIZE32 uint32_t)
set(SIZE64 int64_t)
set(USIZE64 uint64_t)

include(CheckTypeSize)

add_library(opusfile ${OPUSFILE_HEADERS} ${OPUSFILE_SOURCES})
target_include_directories(opusfile PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(
    opusfile PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded"
    SOVERSION ${LIB_SOVERSION}
    VERSION ${LIB_VERSION}
    PUBLIC_HEADER "${OPUSFILE_HEADERS}"
)

configure_pkg_config_file(opusfile.pc.in)

if(INSTALL_PKG_CONFIG_MODULE)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/opusfile.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()

set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
include(CPack)
