param(
    [string]$OutputDir = "."
)

Write-Host "Download libmicrohttpd precompiled..." -ForegroundColor Green

# URL reale del rilascio precompilato per Windows (64-bit)
$url = "https://github.com/Karlson2k/libmicrohttpd/releases/download/v1.0.1/libmicrohttpd-1.0.1-w64-bin.zip"
# Se hai bisogno della versione 32-bit, usa:
# $url = "https://github.com/Karlson2k/libmicrohttpd/releases/download/v1.0.1/libmicrohttpd-1.0.1-w32-bin.zip"

$zipFile = Join-Path $OutputDir "libmicrohttpd_temp.zip"
$extractDir = Join-Path $OutputDir "libmicrohttpd_temp"

try {
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor [System.Net.SecurityProtocolType]::Tls12
    
    Write-Host "Scaricando da GitHub..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri $url -OutFile $zipFile -TimeoutSec 120
    
    Write-Host "Estrazione..." -ForegroundColor Yellow
    # Se la cartella temporanea esiste già, la pulisco per evitare conflitti
    if (Test-Path $extractDir) { Remove-Item $extractDir -Recurse -Force }
    Expand-Archive -Path $zipFile -DestinationPath $extractDir -Force
    
    # 1. Copia file header (microhttpd.h)
    Write-Host "Copia file header..." -ForegroundColor Yellow
    $srcHeader = Get-ChildItem -Path $extractDir -Filter "microhttpd.h" -Recurse | Select-Object -First 1
    if ($srcHeader) {
        Copy-Item $srcHeader.FullName -Destination (Join-Path $OutputDir "microhttpd.h") -Force
        Write-Host "Header copiato: microhttpd.h" -ForegroundColor Green
    }
    
    # 2. Copia file libreria (.lib)
    Write-Host "Copia libreria..." -ForegroundColor Yellow
    $srcLib = Get-ChildItem -Path $extractDir -Filter "libmicrohttpd.lib" -Recurse | Select-Object -First 1
    if ($srcLib) {
        Copy-Item $srcLib.FullName -Destination (Join-Path $OutputDir "libmicrohttpd.lib") -Force
        Write-Host "Libreria copiata: libmicrohttpd.lib" -ForegroundColor Green
    }
    
    # 3. Copia file DLL (Necessario per Windows a runtime!)
    Write-Host "Copia DLL di runtime..." -ForegroundColor Yellow
    $srcDll = Get-ChildItem -Path $extractDir -Filter "libmicrohttpd-*.dll" -Recurse | Select-Object -First 1
    if ($srcDll) {
        Copy-Item $srcDll.FullName -Destination (Join-Path $OutputDir $srcDll.Name) -Force
        Write-Host "DLL copiata: $($srcDll.Name)" -ForegroundColor Green
    }
    
    # Pulizia file temporanei
    Write-Host "Pulizia..." -ForegroundColor Yellow
    Remove-Item $extractDir -Recurse -Force
    Remove-Item $zipFile -Force
    
    # Verifica finale dell'installazione
    if (Test-Path (Join-Path $OutputDir "microhttpd.h")) {
        Write-Host "libmicrohttpd installato con successo!" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "Errore: file header non trovato dopo l'estrazione" -ForegroundColor Red
        exit 1
    }
}
catch {
    Write-Host "Errore durante l'esecuzione: $($_.Exception.Message)" -ForegroundColor Red
    # Tentativo di pulizia anche in caso di errore
    if (Test-Path $extractDir) { Remove-Item $extractDir -Recurse -Force }
    if (Test-Path $zipFile) { Remove-Item $zipFile -Force }
    exit 1
}