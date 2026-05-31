cmake_minimum_required(VERSION 3.10)
project(CorsePiattaforma VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Trovare le librerie richieste
find_package(PkgConfig REQUIRED)
pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

# Includere le directory
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SQLITE3_INCLUDE_DIRS}
    ${MICROHTTPD_INCLUDE_DIRS}
)

link_directories(
    ${SQLITE3_LIBRARY_DIRS}
    ${MICROHTTPD_LIBRARY_DIRS}
)

# Aggiungere l'eseguibile del server
add_executable(course_server server_simple.c)

# Linkare le librerie
target_link_libraries(course_server
    ${SQLITE3_LIBRARIES}
    ${MICROHTTPD_LIBRARIES}
    m
)

target_link_directories(course_server PRIVATE
    ${SQLITE3_LIBRARY_DIRS}
    ${MICROHTTPD_LIBRARY_DIRS}
)

# Opzioni di compilazione
if(MSVC)
    target_compile_options(course_server PRIVATE /W4 /WX)
else()
    target_compile_options(course_server PRIVATE -Wall -Wextra)
endif()

# Aggiungere i file sources per il database
set(SOURCES
    server_simple.c
)

# Opzioni di output
set_target_properties(course_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Messaggi di build
message(STATUS "Building Piattaforma Corsi Online Backend")
message(STATUS "SQLite3: ${SQLITE3_LIBRARIES}")
message(STATUS "libmicrohttpd: ${MICROHTTPD_LIBRARIES}")
