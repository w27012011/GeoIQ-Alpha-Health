cmake_minimum_required(VERSION 3.14)
project(GeoIQFrontendWriter VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SQLite3 amalgamation — copied from GeoIQ/src/utils/
set(SQLITE3_C "${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/sqlite3.c")
set(SQLITE3_H "${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/sqlite3.h")

if(NOT EXISTS ${SQLITE3_C})
    message(FATAL_ERROR
        "sqlite3.c not found at ${SQLITE3_C}\n"
        "Expected to be at GeoIQ/src/utils/sqlite3.c — same location GeoIQ.exe uses.")
endif()

add_executable(HTMLWriter
    main.cpp
    ${SQLITE3_C}   # SQLite3 amalgamation (C file, compiled as C)
)

# Include paths
target_include_directories(HTMLWriter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}        # for main.cpp → #include "src/..."
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils   # for sqlite3.h
)

# Create vendor/ symlink directory alias so src/HealthReader.h can find sqlite3.h
# via #include "../vendor/sqlite3.h"
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/sqlite3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/sqlite3.h
    COPYONLY
)

# Compile sqlite3.c as C (not C++) to avoid C++ issues
set_source_files_properties(${SQLITE3_C} PROPERTIES LANGUAGE C)

# Compiler flags
if(MSVC)
    target_compile_options(HTMLWriter PRIVATE /W3 /O2 /EHsc /utf-8)
else()
    target_compile_options(HTMLWriter PRIVATE -Wall -Wextra -O2 -Wno-unused-parameter)
endif()

# On Windows with MinGW, link ws2_32 for network functions sqlite3 may reference
if(WIN32 AND NOT MSVC)
    target_link_libraries(HTMLWriter PRIVATE ws2_32)
endif()

message(STATUS "GeoIQ HTML Frontend Generator — Build configured")
message(STATUS "  SQLite3: ${SQLITE3_C}")
message(STATUS "  Output:  HTMLWriter executable")
