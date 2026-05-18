@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo Setup dipendenze per progetto-vessio
echo ==================================================

set VCPKG_ROOT=C:\vcpkg

echo.
echo Verificando vcpkg...
if not exist "%VCPKG_ROOT%" (
    echo Clonando vcpkg...
    cd %USERPROFILE%
    git clone https://github.com/Microsoft/vcpkg.git vcpkg
    if errorlevel 1 (
        echo Errore: git non trovato. Installa Git da https://git-scm.com/
        exit /b 1
    )
    cd %VCPKG_ROOT%
    
    echo Bootstrapping vcpkg...
    call .\bootstrap-vcpkg.bat
    if errorlevel 1 (
        echo Errore durante il bootstrap di vcpkg
        exit /b 1
    )
)

cd %VCPKG_ROOT%

echo.
echo Installando SQLite3 (x64-windows)...
.\vcpkg install sqlite3:x64-windows
if errorlevel 1 (
    echo Errore durante install di sqlite3
    exit /b 1
)

echo.
echo Installando libmicrohttpd (x64-windows)...
.\vcpkg install libmicrohttpd:x64-windows
if errorlevel 1 (
    echo Errore durante install di libmicrohttpd
    exit /b 1
)

echo.
echo ===================================
echo Setup COMPLETATO!
echo ===================================
echo.
echo Ora esegui dal workspace:
echo   cd C:\Users\SIMONE\Documents\progetto-vessio
echo   .\build_final.bat
echo.
echo Oppure, se CMake fallisce ancora, usa:
echo   .\build_direct.bat
echo.

endlocal
