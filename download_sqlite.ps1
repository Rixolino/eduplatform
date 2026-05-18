# Script per scaricare SQLite3 amalgamation
param(
    [string]$OutputDir = "."
)

$url = "https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip"
$zipFile = Join-Path $OutputDir "sqlite_temp.zip"
$extractDir = Join-Path $OutputDir "sqlite_temp"

Write-Host "Download SQLite3 da: $url" -ForegroundColor Green

try {
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor [System.Net.SecurityProtocolType]::Tls12
    
    Write-Host "Scaricando..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $url -OutFile $zipFile -TimeoutSec 120
    
    if (-not (Test-Path $zipFile)) {
        Write-Host "Errore: File non scaricato" -ForegroundColor Red
        exit 1
    }
    
    Write-Host "Estrazione..." -ForegroundColor Yellow
    Expand-Archive -Path $zipFile -DestinationPath $extractDir -Force
    
    Write-Host "Copia file..." -ForegroundColor Yellow
    $srcC = Get-ChildItem "$extractDir\*\sqlite3.c" -Recurse | Select-Object -First 1
    $srcH = Get-ChildItem "$extractDir\*\sqlite3.h" -Recurse | Select-Object -First 1
    
    if ($null -eq $srcC -or $null -eq $srcH) {
        Write-Host "Errore: sqlite3.c o sqlite3.h non trovati" -ForegroundColor Red
        exit 1
    }
    
    Copy-Item $srcC.FullName -Destination (Join-Path $OutputDir "sqlite3.c")
    Copy-Item $srcH.FullName -Destination (Join-Path $OutputDir "sqlite3.h")
    
    Write-Host "Pulizia..." -ForegroundColor Yellow
    Remove-Item $extractDir -Recurse -Force
    Remove-Item $zipFile -Force
    
    Write-Host "SQLite3 scaricato OK" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "Errore: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
