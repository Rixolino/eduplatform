cmake_minimum_required(VERSION 3.10)
project(CorsePiattaforma VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── SQLite3 ────────────────────────────────────────────────────────────────────
# CMake ships a FindSQLite3 module (≥3.14). On Linux it finds the system package;
# on Windows use vcpkg: cmake -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
find_package(SQLite3 REQUIRED)

# ── Eseguibile ────────────────────────────────────────────────────────────────
add_executable(course_server server_simple.c)

# ── Include ────────────────────────────────────────────────────────────────────
target_include_directories(course_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(course_server PRIVATE SQLite::SQLite3)

if(WIN32)
    # Winsock2 richiesto per i raw socket su Windows
    target_link_libraries(course_server PRIVATE ws2_32)
else()
    # math library richiesta su POSIX (opzionale ma sicura)
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


# ── Secondo esempio ────────────────────────────────────────────────────────────
add_executable(edu_project main.cpp)
