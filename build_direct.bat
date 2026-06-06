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
    echo ERRORE: sqlite3.c non trovato.
    exit /b 1
)
if not exist "sqlite3.h" (
    echo ERRORE: sqlite3.h non trovato.
    exit /b 1
)
echo SQLite3 OK

echo.
echo Verificando libmicrohttpd...
if not exist "libmicrohttpd-dll.lib" (
    echo ERRORE: libmicrohttpd-dll.lib non trovato nella directory corrente.
    exit /b 1
)
if not exist "libmicrohttpd-dll.dll" (
    echo ERRORE: libmicrohttpd-dll.dll non trovato nella directory corrente.
    exit /b 1
)
echo libmicrohttpd OK

:: Creo la cartella bin se non esiste
if not exist "bin" mkdir bin

echo.
echo Copiando la DLL in bin\...
copy /Y "libmicrohttpd-dll.dll" "bin\libmicrohttpd-dll.dll" > nul

echo.
echo Compilazione in corso (server.c + sqlite3.c + libmicrohttpd)...
echo.

:: Nota: ho aggiornato il nome della libreria da linkare in libmicrohttpd-dll.lib
cl.exe /O2 /I. /D_CRT_SECURE_NO_WARNINGS ^
    /Fo"bin\\" ^
    /Fe"bin\course_server.exe" ^
    server.c sqlite3.c ^
    /link ws2_32.lib libmicrohttpd-dll.lib

if %ERRORLEVEL% equ 0 (
    echo.
    echo ===================================
    echo BUILD COMPLETATO CON SUCCESSO!
    echo ===================================
    echo Eseguibile: bin\course_server.exe
    echo.
    echo Per avviare il server digita:
    echo   .\bin\course_server.exe
    echo.
    exit /b 0
) else (
    echo.
    echo BUILD FALLITO.
    exit /b 1
)

endlocal