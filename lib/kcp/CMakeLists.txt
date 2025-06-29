CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

project(kcp LANGUAGES C)

include(CTest)
include(GNUInstallDirs)

cmake_policy(SET CMP0054 NEW)

if(BUILD_SHARED_LIBS AND WIN32)
    set(exports_def_file "${CMAKE_CURRENT_BINARY_DIR}/exports.def")
    set(exports_def_contents
"EXPORTS 
    ikcp_create
    ikcp_release
    ikcp_setoutput
    ikcp_recv
    ikcp_send
    ikcp_update
    ikcp_check
    ikcp_input
    ikcp_flush
    ikcp_peeksize
    ikcp_setmtu
    ikcp_wndsize
    ikcp_waitsnd
    ikcp_nodelay
    ikcp_log
    ikcp_allocator
    ikcp_getconv
")

    file(WRITE "${exports_def_file}" "${exports_def_contents}")
    add_library(kcp ikcp.c "${exports_def_file}")
else()
    add_library(kcp ikcp.c)
endif()

install(FILES ikcp.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS kcp
    EXPORT kcp-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)
