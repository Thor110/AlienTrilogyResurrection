@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ---------------------------------------------------------------------------
rem  make_release.bat - gathers a release of ALTEngine into
rem      ALTEngine\out\build\ALTEngine-v<version>
rem  from a finished Release build in
rem      ALTEngine\out\build\windows-release
rem  The version comes from ALTEngine\VERSION; change it there.
rem
rem  Build first (VS2022: select the "Windows x64 Release" preset and build,
rem  or "cmake --preset windows-release" then "cmake --build out\build\windows-release").
rem
rem  Taken from the build:   ALTEngine.exe, the DLLs vcpkg deployed beside it,
rem                          licenses\
rem  Taken from the source:  data\ (less ScanLights.flag), and LANGUAGE,
rem                          OVERRIDE and CUSTOM into GameData\CD as the
rem                          build lays them out
rem  Left behind:            vcpkg_installed, object files, CMake files, the
rem                          tests and tools, and anything the build folder
rem                          has picked up from running it - GameData's disc
rem                          install, settings, saves, logs.
rem
rem  Usage:  tools\make_release.bat [/zip]
rem          /zip   also packs the folder into ALTEngine-v<version>.zip
rem ---------------------------------------------------------------------------

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "BUILD=%ROOT%\out\build\windows-release"

if not exist "%ROOT%\VERSION" (
    echo ERROR: %ROOT%\VERSION not found.
    exit /b 1
)
set "VERSION="
for /f "usebackq tokens=* delims= " %%V in ("%ROOT%\VERSION") do if not defined VERSION set "VERSION=%%V"
if "%VERSION%"=="" (
    echo ERROR: %ROOT%\VERSION is empty.
    exit /b 1
)

set "OUT=%ROOT%\out\build\ALTEngine-v%VERSION%"

if not exist "%BUILD%\ALTEngine.exe" (
    echo ERROR: %BUILD%\ALTEngine.exe not found - build the windows-release preset first.
    exit /b 1
)

echo Making ALTEngine v%VERSION%
echo   from  %BUILD%
echo   into  %OUT%

if exist "%OUT%" (
    rmdir /s /q "%OUT%"
    if exist "%OUT%" (
        echo ERROR: could not clear %OUT% - is something in it open?
        exit /b 1
    )
)
mkdir "%OUT%" || exit /b 1

rem The executable and the DLLs beside it (SDL3, FFmpeg's).
copy /y "%BUILD%\ALTEngine.exe" "%OUT%\" >nul || goto :failed
set /a DLLS=0
for %%F in ("%BUILD%\*.dll") do (
    copy /y "%%F" "%OUT%\" >nul || goto :failed
    set /a DLLS+=1
)
if !DLLS! EQU 0 echo WARNING: no DLLs beside ALTEngine.exe - the release will not start without SDL3 and FFmpeg.

rem Licences for what it ships with.
if exist "%BUILD%\licenses" (
    robocopy "%BUILD%\licenses" "%OUT%\licenses" /e /njh /njs /nfl /ndl /np >nul
) else (
    robocopy "%ROOT%\third_party_licenses" "%OUT%\licenses" /e /njh /njs /nfl /ndl /np >nul
)
if errorlevel 8 goto :failed
if exist "%ROOT%\..\LICENSE" copy /y "%ROOT%\..\LICENSE" "%OUT%\LICENSE.txt" >nul

rem data\ - ScanLights.flag would start the one-off light scan on first launch.
robocopy "%ROOT%\data" "%OUT%\data" /e /xf ScanLights.flag /njh /njs /nfl /ndl /np >nul
if errorlevel 8 goto :failed

rem The language packs, the override guide and the port's own artwork, where
rem the engine looks for them.
robocopy "%ROOT%\LANGUAGE" "%OUT%\GameData\CD\LANGUAGE" /e /njh /njs /nfl /ndl /np >nul
if errorlevel 8 goto :failed
robocopy "%ROOT%\OVERRIDE" "%OUT%\GameData\CD\OVERRIDE" /e /njh /njs /nfl /ndl /np >nul
if errorlevel 8 goto :failed
robocopy "%ROOT%\CUSTOM" "%OUT%\GameData\CD\OVERRIDE\CUSTOM" /e /njh /njs /nfl /ndl /np >nul
if errorlevel 8 goto :failed

rem Size of what was gathered.
set /a BYTES=0
for /f "tokens=3" %%S in ('dir /s /-c "%OUT%" ^| findstr /c:"File(s)"') do set "BYTES=%%S"
echo Done: %OUT% (!DLLS! DLLs, !BYTES! bytes)

if /i "%~1"=="/zip" (
    set "ZIP=%ROOT%\out\build\ALTEngine-v%VERSION%.zip"
    if exist "!ZIP!" del /q "!ZIP!"
    powershell -NoProfile -Command "Compress-Archive -Path '%OUT%' -DestinationPath '!ZIP!'" || goto :failed
    echo Zipped: !ZIP!
)
exit /b 0

:failed
echo ERROR: copying into %OUT% failed.
exit /b 1
