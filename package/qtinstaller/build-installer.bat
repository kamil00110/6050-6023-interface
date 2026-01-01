@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Building Traintastic QtIFW Installer
echo ========================================
echo.

:: Set paths
set PACKAGE_DIR=%~dp0
set ROOT_DIR=%PACKAGE_DIR%\..\..
set CONFIG_DIR=%PACKAGE_DIR%config
set PACKAGES_DIR=%PACKAGE_DIR%packages
set OUTPUT_DIR=%PACKAGE_DIR%output

::ifier binary creator tool path
set BINARYCREATOR="C:\Qt\Tools\QtInstallerFramework\4.8.1\bin\binarycreator.exe"

:: Check if binarycreator exists
if not exist %BINARYCREATOR% (
    echo ERROR: Qt Installer Framework not found at %BINARYCREATOR%
    echo Please install Qt Installer Framework or update the path
    exit /b 1
)

:: Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

:: Clean previous builds
echo Cleaning previous builds...
rmdir /s /q "%PACKAGES_DIR%\org.traintastic.server\data" 2>nul
rmdir /s /q "%PACKAGES_DIR%\org.traintastic.client\data" 2>nul
rmdir /s /q "%PACKAGES_DIR%\org.traintastic.shared\data" 2>nul

:: Create data directories
echo Creating package data directories...
mkdir "%PACKAGES_DIR%\org.traintastic.server\data"
mkdir "%PACKAGES_DIR%\org.traintastic.client\data"
mkdir "%PACKAGES_DIR%\org.traintastic.shared\data"

:: Copy server files
echo Copying server files...
mkdir "%PACKAGES_DIR%\org.traintastic.server\data\server"
copy "%ROOT_DIR%\server\build\traintastic-server.exe" "%PACKAGES_DIR%\org.traintastic.server\data\server\" >nul

:: Copy client files
echo Copying client files...
mkdir "%PACKAGES_DIR%\org.traintastic.client\data\client"
xcopy /E /I /Y "%ROOT_DIR%\client\build\Release\*" "%PACKAGES_DIR%\org.traintastic.client\data\client\" >nul

:: Copy shared files (translations, manual, LNCV)
echo Copying shared files...
mkdir "%PACKAGES_DIR%\org.traintastic.shared\data\translations"
mkdir "%PACKAGES_DIR%\org.traintastic.shared\data\manual"
mkdir "%PACKAGES_DIR%\org.traintastic.shared\data\lncv"

xcopy /E /I /Y "%ROOT_DIR%\shared\translations\*.lang" "%PACKAGES_DIR%\org.traintastic.shared\data\translations\" >nul
xcopy /E /I /Y "%ROOT_DIR%\manual\output\*" "%PACKAGES_DIR%\org.traintastic.shared\data\manual\" >nul
xcopy /E /I /Y "%ROOT_DIR%\shared\data\lncv\xml\*.xml" "%PACKAGES_DIR%\org.traintastic.shared\data\lncv\" >nul
copy "%ROOT_DIR%\shared\data\lncv\xml\lncvmodule.xsd" "%PACKAGES_DIR%\org.traintastic.shared\data\lncv\" >nul

:: Copy license files
echo Copying license files...
copy "%ROOT_DIR%\LICENSE" "%PACKAGES_DIR%\org.traintastic.server\meta\license.txt" >nul
copy "%ROOT_DIR%\LICENSE" "%PACKAGES_DIR%\org.traintastic.client\meta\license.txt" >nul
copy "%ROOT_DIR%\LICENSE" "%PACKAGES_DIR%\org.traintastic.shared\meta\license.txt" >nul

:: Read version from binary
echo Reading version information...
for /f "tokens=*" %%a in ('powershell -Command "(Get-Item '%ROOT_DIR%\server\build\traintastic-server.exe').VersionInfo.FileVersion"') do set VERSION=%%a
echo Version: %VERSION%

:: Update version in config.xml
echo Updating version in config files...
powershell -Command "(Get-Content '%CONFIG_DIR%\config.xml') -replace '<Version>.*</Version>', '<Version>%VERSION%</Version>' | Set-Content '%CONFIG_DIR%\config.xml'"
powershell -Command "(Get-Content '%PACKAGES_DIR%\org.traintastic.server\meta\package.xml') -replace '<Version>.*</Version>', '<Version>%VERSION%</Version>' | Set-Content '%PACKAGES_DIR%\org.traintastic.server\meta\package.xml'"
powershell -Command "(Get-Content '%PACKAGES_DIR%\org.traintastic.client\meta\package.xml') -replace '<Version>.*</Version>', '<Version>%VERSION%</Version>' | Set-Content '%PACKAGES_DIR%\org.traintastic.client\meta\package.xml'"
powershell -Command "(Get-Content '%PACKAGES_DIR%\org.traintastic.shared\meta\package.xml') -replace '<Version>.*</Version>', '<Version>%VERSION%</Version>' | Set-Content '%PACKAGES_DIR%\org.traintastic.shared\meta\package.xml'"

:: Build installer
echo.
echo Building installer...
set INSTALLER_NAME=traintastic-setup-v%VERSION%.exe
%BINARYCREATOR% --offline-only -c "%CONFIG_DIR%\config.xml" -p "%PACKAGES_DIR%" "%OUTPUT_DIR%\%INSTALLER_NAME%"

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Installer build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo Output: %OUTPUT_DIR%\%INSTALLER_NAME%
echo ========================================

endlocal
