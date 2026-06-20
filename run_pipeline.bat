@echo off
title GeoIQ & GeoHealth Pipeline Automated Runner
echo =======================================================================
echo           GeoIQ & GeoHealth Pipeline Automated Runner
echo =======================================================================
echo.
echo [Step 1] Running Environmental Hazard Assessment (Health Pipeline)...
echo Loading 14,571 samples to screen for groundwater toxicant hazards...
.\GeoIQ.exe --config config_health.ini
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Health pipeline failed to run!
    pause
    exit /b %ERRORLEVEL%
)
echo Health database successfully generated at: output\audit_health.db
echo.

echo [Step 2] Running Geological Exploration Assessment (Geology Pipeline)...
echo Restoring geology exploration predictions to output\predictions.geojson...
.\GeoIQ.exe --config config_geology.ini
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Geology pipeline failed to run!
    pause
    exit /b %ERRORLEVEL%
)
echo Geology predictions successfully generated at: output\predictions.geojson
echo.

echo [Step 3] Running HTML Dashboard Generator...
echo Generating interactive dashboards for Tier 1, Tier 2, and Tier 3...
if exist "HTMLWriter.exe" (
    .\HTMLWriter.exe
) else if exist "frontend\HTMLWriter.exe" (
    .\frontend\HTMLWriter.exe
) else (
    echo.
    echo ERROR: HTMLWriter.exe not found!
    pause
    exit /b 1
)
if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: HTML Dashboard Generator failed!
    pause
    exit /b %ERRORLEVEL%
)
echo.
echo =======================================================================
echo           SUCCESS: Pipeline Completed!
echo =======================================================================
echo.
echo Opening the Tier 1 Public Safety dashboard in your default browser...
start "" "frontend\output\tier1_public.html"
echo.
echo You can access the other tiers in:
echo   - Tier 2 Advanced Research: frontend\output\tier2_advanced.html
echo   - Tier 3 Full Auditor:     frontend\output\tier3_auditor.html
echo.
pause
