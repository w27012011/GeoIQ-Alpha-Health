param(
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$BuildDir = "build"
$ExeName  = "HTMLWriter.exe"

Write-Host "GeoIQ HTML Frontend Generator - Build Script" -ForegroundColor Cyan

# Locate MinGW g++
$mingwPaths = @(
    "D:\mingw32\bin",
    "D:\mingw64\bin",
    "C:\msys64\mingw32\bin",
    "C:\msys64\mingw64\bin",
    "C:\mingw64\bin",
    "C:\mingw32\bin",
    "C:\mingw\bin"
)
$mingwBin = $null
foreach ($p in $mingwPaths) {
    if (Test-Path "$p\g++.exe") { $mingwBin = $p; break }
}
if ($null -eq $mingwBin) {
    Write-Host "ERROR: MinGW g++ not found." -ForegroundColor Red
    exit 1
}
Write-Host "MinGW: $mingwBin" -ForegroundColor Green

$gcc = "$mingwBin\gcc.exe"
$gpp = "$mingwBin\g++.exe"

# Verify sqlite3 source exists (reuse from GeoIQ - do NOT modify GeoIQ.exe)
$sqlite3C = "..\src\utils\sqlite3.c"
$sqlite3H = "..\src\utils\sqlite3.h"
if (-not (Test-Path $sqlite3C)) {
    Write-Host "ERROR: sqlite3.c not found at $sqlite3C" -ForegroundColor Red
    exit 1
}

# Copy sqlite3.h to vendor/ so src/HealthReader.h can include it
if (-not (Test-Path "vendor")) { New-Item -ItemType Directory -Path "vendor" | Out-Null }
Copy-Item $sqlite3H "vendor\sqlite3.h" -Force
Write-Host "sqlite3.h copied to vendor/" -ForegroundColor Green

# Create output directory
if (-not (Test-Path "output")) { New-Item -ItemType Directory -Path "output" | Out-Null }

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}
if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

# Step 1: Compile sqlite3.c as C
$sqliteObj = "$BuildDir\sqlite3.o"
if (-not (Test-Path $sqliteObj)) {
    Write-Host "[1/2] Compiling sqlite3.c..." -ForegroundColor Cyan
    $gcArgs = @(
        "-O2", "-c",
        "-DSQLITE_THREADSAFE=0",
        "-DSQLITE_OMIT_LOAD_EXTENSION",
        "-I..\src\utils",
        "-o", $sqliteObj,
        $sqlite3C
    )
    & $gcc @gcArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: sqlite3.c failed to compile" -ForegroundColor Red
        exit 1
    }
    Write-Host "  OK: sqlite3.o" -ForegroundColor Green
} else {
    Write-Host "[1/2] sqlite3.o already exists, skipping compilation." -ForegroundColor Green
}

# Step 2: Compile HTMLWriter
Write-Host "[2/2] Compiling HTMLWriter..." -ForegroundColor Cyan

$cppArgs = @(
    "-std=c++17", "-O2",
    "-Wall", "-Wno-unused-parameter",
    "-I.",
    "-I..\src\utils",
    "-Ivendor",
    "main.cpp",
    $sqliteObj,
    "-static-libgcc", "-static-libstdc++", "-static",
    "-o", "$BuildDir\$ExeName",
    "-lws2_32"
)

& $gpp @cppArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: HTMLWriter compilation failed" -ForegroundColor Red
    exit 1
}

Copy-Item "$BuildDir\$ExeName" ".\$ExeName" -Force
$sz = [math]::Round((Get-Item ".\$ExeName").Length / 1KB)
Write-Host "Build SUCCESS: HTMLWriter.exe ($sz KB)" -ForegroundColor Green

if ($Run) {
    Write-Host ""
    Write-Host "Running HTMLWriter.exe..." -ForegroundColor Cyan
    Push-Location ".."
    try {
        & ".\frontend\HTMLWriter.exe"
    } finally {
        Pop-Location
    }
    Write-Host "Done. Check frontend\output\" -ForegroundColor Green
}
