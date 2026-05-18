@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo Build script per progetto-vessio Windows
echo ==================================================

set CL_PATH=D:\Visual Studio\2022\Professional\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64

if exist "%CL_PATH%\cl.exe" (
    echo Found cl.exe: %CL_PATH%
    set "PATH=%CL_PATH%;%PATH%"
) else (
    echo Errore: cl.exe non trovato in %CL_PATH%
    exit /b 1
)

where cl >nul 2>&1
if errorlevel 1 (
    echo Errore: cl.exe non accessibile dal PATH
    exit /b 1
)
echo cl.exe OK

where cmake >nul 2>&1
if errorlevel 1 (
    echo Errore: CMake non trovato nel PATH
    echo Installa CMake da https://cmake.org/download/
    exit /b 1
)
echo cmake OK

echo.
echo Inizio build...

if exist "build" (
    echo Pulizia cartella build precedente...
    rmdir /s /q build
)

if not exist "build" mkdir build
cd build

cmake -G "Visual Studio 17 2022" -A x64 ..

if errorlevel 1 (
    echo CMake configure fallita
    cd ..
    exit /b 1
)

cmake --build . --config Release

if errorlevel 1 (
    echo Build fallita
    cd ..
    exit /b 1
)

echo.
echo BUILD COMPLETATO!
echo.

if exist "bin\Release\course_server.exe" (
    echo Eseguibile: build\bin\Release\course_server.exe
) else if exist "Release\course_server.exe" (
    echo Eseguibile: build\Release\course_server.exe
) else (
    for /r . %%F in (course_server.exe) do echo Trovato: %%F
)

cd ..
exit /b 0
