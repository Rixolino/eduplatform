# Script per scaricare e installare libmicrohttpd
param(
    [string]$OutputDir = "."
)

Write-Host "Download libmicrohttpd precompiled..." -ForegroundColor Green

$url = "https://ftpmirror.gnu.org/libmicrohttpd/libmicrohttpd-latest-w64.zip"
$zipFile = Join-Path $OutputDir "libmicrohttpd_temp.zip"
$extractDir = Join-Path $OutputDir "libmicrohttpd_temp"

try {
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor [System.Net.SecurityProtocolType]::Tls12
    
    Write-Host "Scaricando..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $url -OutFile $zipFile -TimeoutSec 120
    
    if (-not (Test-Path $zipFile)) {
        Write-Host "File non scaricato. Provo URL alternativo..." -ForegroundColor Yellow
        # Fallback to a different mirror
        $url = "https://www.gnu.org/software/libmicrohttpd/libmicrohttpd-latest-w64.zip"
        Invoke-WebRequest -Uri $url -OutFile $zipFile -TimeoutSec 120
    }
    
    Write-Host "Estrazione..." -ForegroundColor Yellow
    Expand-Archive -Path $zipFile -DestinationPath $extractDir -Force
    
    # Copia file header
    Write-Host "Copia file header..." -ForegroundColor Yellow
    $srcHeader = Get-ChildItem "$extractDir\*\include\microhttpd.h" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($srcHeader) {
        Copy-Item $srcHeader.FullName -Destination (Join-Path $OutputDir "microhttpd.h")
        Write-Host "Header copiato: microhttpd.h" -ForegroundColor Green
    }
    
    # Copia libreria (se disponibile)
    Write-Host "Copia libreria..." -ForegroundColor Yellow
    $srcLib = Get-ChildItem "$extractDir\*\lib\libmicrohttpd.lib" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($srcLib) {
        Copy-Item $srcLib.FullName -Destination (Join-Path $OutputDir "libmicrohttpd.lib")
        Write-Host "Libreria copiata: libmicrohttpd.lib" -ForegroundColor Green
    }
    
    # Pulizia
    Write-Host "Pulizia..." -ForegroundColor Yellow
    Remove-Item $extractDir -Recurse -Force
    Remove-Item $zipFile -Force
    
    if (Test-Path (Join-Path $OutputDir "microhttpd.h")) {
        Write-Host "libmicrohttpd installato OK" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "Errore: header non trovato nel download" -ForegroundColor Red
        exit 1
    }
}
catch {
    Write-Host "Errore: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
