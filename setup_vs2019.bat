@echo off
setlocal enabledelayedexpansion

echo ==================================================
echo Setup Visual Studio 2019 Build Tools PATH
echo ==================================================

rem Percorsi comuni di VS 2019 Build Tools
set VS2019_PATH=D:\Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64

if exist "%VS2019_PATH%\cl.exe" (
    echo Found cl.exe at: %VS2019_PATH%
    set PATH=%VS2019_PATH%;%PATH%
    echo Added to PATH
    exit /b 0
)

rem Prova cartella alternativa in D:\Visual Studio
for /d %%D in ("D:\Visual Studio\2019\BuildTools\VC\Tools\MSVC\*") do (
    if exist "%%D\bin\Hostx64\x64\cl.exe" (
        echo Found cl.exe at: %%D\bin\Hostx64\x64
        set PATH=%%D\bin\Hostx64\x64;%PATH%
        echo Added to PATH
        exit /b 0
    )
)

rem Prova percorsi secondari
for /d %%D in ("D:\Visual Studio\*\BuildTools\VC\Tools\MSVC\*") do (
    if exist "%%D\bin\Hostx64\x64\cl.exe" (
        echo Found cl.exe at: %%D\bin\Hostx64\x64
        set PATH=%%D\bin\Hostx64\x64;%PATH%
        echo Added to PATH
        exit /b 0
    )
)

echo Errore: Visual Studio 2019 Build Tools non trovato.
echo Installa da: https://visualstudio.microsoft.com/downloads/
echo Scegli "Build Tools for Visual Studio 2019" e seleziona "C++ build tools"
exit /b 1
