cmake_minimum_required(VERSION 3.10)
project(CorsePiattaforma VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── SQLite3 ────────────────────────────────────────────────────────────────────
# CMake ships a FindSQLite3 module (≥3.14). On Linux it finds the system package;
# on Windows use vcpkg: cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(SQLite3 REQUIRED)

# ── libmicrohttpd ──────────────────────────────────────────────────────────────
# On Linux/macOS: installed via package manager (apt/brew/nix).
# On Windows: install via vcpkg (vcpkg install libmicrohttpd:x64-windows).
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)
else()
    # Fallback for Windows without pkg-config (vcpkg provides find_package support)
    find_package(unofficial-libmicrohttpd CONFIG QUIET)
    if(unofficial-libmicrohttpd_FOUND)
        set(MICROHTTPD_LIBRARIES unofficial::libmicrohttpd::libmicrohttpd)
    else()
        message(FATAL_ERROR "libmicrohttpd not found. On Windows run: vcpkg install libmicrohttpd")
    endif()
endif()

# ── Eseguibile ────────────────────────────────────────────────────────────────
add_executable(course_server server.c)

# ── Include ────────────────────────────────────────────────────────────────────
target_include_directories(course_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLite3_INCLUDE_DIRS}
    ${MICROHTTPD_INCLUDE_DIRS}
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(course_server PRIVATE SQLite::SQLite3 ${MICROHTTPD_LIBRARIES})
target_link_directories(course_server PRIVATE ${MICROHTTPD_LIBRARY_DIRS})

if(WIN32)
    target_link_libraries(course_server PRIVATE ws2_32)
else()
    target_link_libraries(course_server PRIVATE m)
endif()

# ── Opzioni di compilazione ───────────────────────────────────────────────────
if(MSVC)
    target_compile_options(course_server PRIVATE /W3)
    # Disabilita avvisi su funzioni POSIX deprecate da MSVC (_CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(course_server PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    target_compile_options(course_server PRIVATE -Wall -Wextra)
endif()

# ── Output directory ──────────────────────────────────────────────────────────
set_target_properties(course_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── Info di build ─────────────────────────────────────────────────────────────
message(STATUS "Building Piattaforma Corsi Online Backend")
message(STATUS "SQLite3 include: ${SQLite3_INCLUDE_DIRS}")
message(STATUS "SQLite3 libs:    ${SQLite3_LIBRARIES}")
if(WIN32)
    message(STATUS "Platform: Windows (linking ws2_32)")
else()
    message(STATUS "Platform: POSIX")
endif()

