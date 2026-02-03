cmake_minimum_required(VERSION 3.10)
project(zenfetch C)

set(CMAKE_C_STANDARD 99)

# Common source files
set(CBONSAI_SOURCES cbonsai.c)
set(ZENFETCH_SOURCES zenfetch.c)

if(WIN32)
    # Windows build with PDCurses
    # PDCurses can be installed via vcpkg: vcpkg install pdcurses
    find_package(unofficial-pdcurses CONFIG)

    if(unofficial-pdcurses_FOUND)
        # Using vcpkg PDCurses
        add_library(cbonsai_lib STATIC ${CBONSAI_SOURCES})
        target_compile_definitions(cbonsai_lib PRIVATE CBONSAI_LIBRARY)
        target_link_libraries(cbonsai_lib PRIVATE unofficial::pdcurses::pdcurses)

        add_executable(cbonsai ${CBONSAI_SOURCES})
        target_link_libraries(cbonsai PRIVATE unofficial::pdcurses::pdcurses)

        add_executable(zenfetch ${ZENFETCH_SOURCES})
        target_link_libraries(zenfetch PRIVATE cbonsai_lib unofficial::pdcurses::pdcurses iphlpapi ws2_32)
    else()
        # Manual PDCurses path
        message(STATUS "PDCurses not found via vcpkg, looking for manual installation")

        # You can set PDCURSES_DIR to point to your PDCurses installation
        if(DEFINED PDCURSES_DIR)
            set(PDCURSES_INCLUDE_DIR "${PDCURSES_DIR}")
            set(PDCURSES_LIBRARY "${PDCURSES_DIR}/wincon/pdcurses.lib")
        else()
            message(FATAL_ERROR "PDCurses not found. Install via vcpkg (vcpkg install pdcurses) or set PDCURSES_DIR")
        endif()

        add_library(cbonsai_lib STATIC ${CBONSAI_SOURCES})
        target_compile_definitions(cbonsai_lib PRIVATE CBONSAI_LIBRARY)
        target_include_directories(cbonsai_lib PRIVATE ${PDCURSES_INCLUDE_DIR})
        target_link_libraries(cbonsai_lib PRIVATE ${PDCURSES_LIBRARY})

        add_executable(cbonsai ${CBONSAI_SOURCES})
        target_include_directories(cbonsai PRIVATE ${PDCURSES_INCLUDE_DIR})
        target_link_libraries(cbonsai PRIVATE ${PDCURSES_LIBRARY})

        add_executable(zenfetch ${ZENFETCH_SOURCES})
        target_include_directories(zenfetch PRIVATE ${PDCURSES_INCLUDE_DIR})
        target_link_libraries(zenfetch PRIVATE cbonsai_lib ${PDCURSES_LIBRARY} iphlpapi ws2_32)
    endif()
else()
    # Linux/Unix build with ncurses
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NCURSES REQUIRED ncursesw panelw)

    add_library(cbonsai_lib STATIC ${CBONSAI_SOURCES})
    target_compile_definitions(cbonsai_lib PRIVATE CBONSAI_LIBRARY)
    target_include_directories(cbonsai_lib PRIVATE ${NCURSES_INCLUDE_DIRS})
    target_link_libraries(cbonsai_lib PRIVATE ${NCURSES_LIBRARIES})
    target_compile_options(cbonsai_lib PRIVATE ${NCURSES_CFLAGS_OTHER})

    add_executable(cbonsai ${CBONSAI_SOURCES})
    target_include_directories(cbonsai PRIVATE ${NCURSES_INCLUDE_DIRS})
    target_link_libraries(cbonsai PRIVATE ${NCURSES_LIBRARIES})
    target_compile_options(cbonsai PRIVATE ${NCURSES_CFLAGS_OTHER})

    add_executable(zenfetch ${ZENFETCH_SOURCES})
    target_include_directories(zenfetch PRIVATE ${NCURSES_INCLUDE_DIRS})
    target_link_libraries(zenfetch PRIVATE cbonsai_lib ${NCURSES_LIBRARIES})
    target_compile_options(zenfetch PRIVATE ${NCURSES_CFLAGS_OTHER})
endif()

# Compiler warnings and definitions
if(MSVC)
    # Suppress MSVC security warnings for standard C functions
    target_compile_definitions(cbonsai_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(cbonsai PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(zenfetch PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(cbonsai_lib PRIVATE /W3)
    target_compile_options(cbonsai PRIVATE /W3)
    target_compile_options(zenfetch PRIVATE /W3)
else()
    target_compile_options(cbonsai_lib PRIVATE -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -pedantic)
    target_compile_options(cbonsai PRIVATE -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -pedantic)
    target_compile_options(zenfetch PRIVATE -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -pedantic)
endif()

# Installation
install(TARGETS cbonsai zenfetch RUNTIME DESTINATION bin)
