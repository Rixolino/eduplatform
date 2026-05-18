@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo Build script per progetto-vessio (Windows)
echo ==================================================

rem Controlla che CMake sia disponibile
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Errore: CMake non trovato nel PATH.
    echo Installa CMake: https://cmake.org/download/
    exit /b 1
)

rem Rileva generator preferito (NMake ha meno problemi di Ninja su Windows)
set GENERATOR="NMake Makefiles"
where nmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo NMake non trovato, provando Visual Studio generator...
    set GENERATOR="Visual Studio 17 2022"
)

echo Using CMake generator: %GENERATOR%

rem Verifica presenza di vcpkg e usa toolchain se disponibile
set VCPKG_TOOLCHAIN_FILE=
if exist "%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set VCPKG_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake
) else if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    set VCPKG_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
)

if defined VCPKG_TOOLCHAIN_FILE (
    echo Found vcpkg toolchain at %VCPKG_TOOLCHAIN_FILE%
    echo Configuring CMake with vcpkg toolchain
    cmake -S . -B build -G %GENERATOR% -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN_FILE%"
) else (
    cmake -S . -B build -G %GENERATOR%
)

if %ERRORLEVEL% neq 0 (
    echo CMake configuration fallita.
    echo Tentando compilazione diretta con cl.exe...
    call :direct_compile
    exit /b !ERRORLEVEL!
)

echo Avvio build...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo CMake build fallita.
    echo Tentando compilazione diretta con cl.exe...
    call :direct_compile
    exit /b !ERRORLEVEL!
)

if exist "build\bin\course_server.exe" (
    echo Build completata!
    echo Eseguibile: build\bin\course_server.exe
    exit /b 0
) else if exist "build\Release\course_server.exe" (
    echo Build completata!
    echo Eseguibile: build\Release\course_server.exe
    exit /b 0
) else (
    echo Avviso: eseguibile non trovato in build. Controllare l'output.
    exit /b 1
)

:direct_compile
echo.
echo Compilazione diretta con cl.exe (Visual Studio)...
where cl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Errore: cl.exe non trovato.
    echo Installa Visual Studio Build Tools da https://visualstudio.microsoft.com/downloads/
    exit /b 1
)

if not exist "bin" mkdir bin

echo Compilando server.c...
cl.exe /std:c11 /W4 /O2 /I. server.c /link sqlite3.lib /out:bin\course_server.exe 2>&1

if %ERRORLEVEL% eq 0 (
    echo.
    echo Compilazione diretta completata!
    echo Eseguibile: bin\course_server.exe
    exit /b 0
) else (
    echo Compilazione fallita. Assicurati di avere:
    echo - Visual Studio Build Tools installato
    echo - sqlite3 disponibile nel PATH o nella cartella corrente
    exit /b 1
)

endlocal
exit /b 0
