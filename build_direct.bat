@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo Build DIRETTO con cl.exe (Visual Studio Compiler)
echo ==================================================

set CL_PATH=D:\Visual Studio\2022\Professional\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64
set VCVARS=D:\Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat

if exist "%CL_PATH%\cl.exe" (
    echo Found cl.exe: %CL_PATH%
) else (
    echo Errore: cl.exe non trovato in %CL_PATH%
    exit /b 1
)

echo.
echo Inizializzando ambiente Visual Studio...
if exist "%VCVARS%" (
    call "%VCVARS%" x64
    if errorlevel 1 (
        echo Errore durante l'inizializzazione di vcvarsall.bat
        exit /b 1
    )
) else (
    echo Errore: vcvarsall.bat non trovato in %VCVARS%
    exit /b 1
)

echo.
echo Verificando SQLite3...

if not exist "sqlite3.c" (
    echo.
    echo Scaricando SQLite3 amalgamation...
    powershell -ExecutionPolicy Bypass -File "download_sqlite.ps1" -OutputDir "."
    if errorlevel 1 (
        echo.
        echo ERRORE: Impossibile scaricare SQLite3
        echo.
        echo SOLUZIONE MANUALE:
        echo   1. Vai a: https://www.sqlite.org/download.html
        echo   2. Scarica: sqlite-amalgamation-3460000.zip
        echo   3. Estrai sqlite3.c e sqlite3.h in questa directory
        echo   4. Riesegui questo script
        echo.
        exit /b 1
    )
)

if not exist "sqlite3.h" (
    echo Errore: sqlite3.h non trovato dopo download
    exit /b 1
)

echo SQLite3 OK

echo.
echo Compilazione di server_simple.c (con Winsock, nessuna dipendenza esterna)...
echo.

cl.exe /O2 /I. /D_CRT_SECURE_NO_WARNINGS ^
    /Fo"bin\\" ^
    /Fe"bin\course_server.exe" ^
    server_simple.c sqlite3.c ^
    /link ws2_32.lib

if %ERRORLEVEL% equ 0 (
    echo.
    echo ===================================
    echo BUILD COMPLETATO!
    echo ===================================
    echo Eseguibile: bin\course_server.exe
    echo.
    echo Per avviare il server:
    echo   .\bin\course_server.exe
    echo.
    exit /b 0
) else (
    echo.
    echo BUILD FALLITO.
    exit /b 1
)

endlocal
