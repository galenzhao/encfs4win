@ECHO OFF
SETLOCAL EnableDelayedExpansion
REM build-dokany.bat
REM *****************************************************************************
REM Author:   Charles Munson <jetwhiz@jetwhiz.com>
REM 
REM ****************************************************************************
REM Copyright (c) 2016, Charles Munson
REM 
REM This program is free software: you can redistribute it and/or modify it
REM under the terms of the GNU Lesser General Public License as published by the
REM Free Software Foundation, either version 3 of the License, or (at your
REM option) any later version.
REM 
REM This program is distributed in the hope that it will be useful, but WITHOUT
REM ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
REM FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
REM for more details.
REM 
REM You should have received a copy of the GNU Lesser General Public License
REM along with this program.  If not, see <http://www.gnu.org/licenses/>.


REM Prefer a system Dokan 2.x install (compatible with the installed driver).
set SOURCE_URI=https://github.com/dokan-dev/dokany.git
set VERSION=v2.3.1.1000
set VERSION_STR=v2.3.1.1000
set SRC_DIR_NAME=dokan


REM ========= DO NOT EDIT BELOW THIS LINE =====================


REM Prefer already-configured DOKAN_ROOT that looks like Dokan 2.x
if defined DOKAN_ROOT (
  if exist "%DOKAN_ROOT%\x86\lib\dokan2.lib" (
    if exist "%DOKAN_ROOT%\x86\lib\dokanfuse2.lib" (
      if exist "%DOKAN_ROOT%\include\fuse.h" (goto :already_installed)
    )
  )
  if exist "%DOKAN_ROOT%\Win32\Release\dokan2.lib" (
    if exist "%DOKAN_ROOT%\Win32\Release\dokanfuse2.lib" (
      if exist "%DOKAN_ROOT%\dokan_fuse\include\fuse.h" (goto :already_installed)
    )
  )
)

REM Prefer the official Dokan 2.x installer environment variables
if defined DokanLibrary2 (
  if exist "%DokanLibrary2%\x86\lib\dokan2.lib" (
    if exist "%DokanLibrary2%\x86\lib\dokanfuse2.lib" (
      if exist "%DokanLibrary2%\include\fuse.h" (
        for %%I in ("%DokanLibrary2%") do (
          endlocal & set "DOKAN_ROOT=%%~fI"
        )
        echo.
        echo ==================================================
        echo     Using installed Dokan 2.x at DOKAN_ROOT
        echo ==================================================
        echo DOKAN_ROOT=%DOKAN_ROOT%
        echo.
        exit /b 0
      )
    )
  )
)


REM Failed to find Dokan -- ask user if they want us to build it for them
echo.
if "%INTERACTIVE%"=="1" (
    SET /P CONFIRM_BUILD="Dokan 2.x (DOKAN_ROOT / DokanLibrary2) was not detected.  Build v2.3.1 from source? (Y/n): "
    if /I NOT "!CONFIRM_BUILD!"=="y" exit /b 1
)


REM move into deps folder 
if NOT exist "deps" mkdir deps
pushd deps


REM Clone git repository and switch to VERSION release 
echo.
echo ==================================================
echo             CLONING DOKANY REPOSITORY (%VERSION%)            
echo ==================================================
git clone %SOURCE_URI% %SRC_DIR_NAME% > %SRC_DIR_NAME%-clone.log
pushd %SRC_DIR_NAME%
git clean -ffdx
git reset --hard %VERSION%
git checkout %VERSION%

REM build libraries (user-mode only; driver comes from the Dokan installer)
echo.
echo ==================================================
echo              BUILDING DOKANY LIBRARIES             
echo ==================================================
if not defined ENCFS_PLATFORM_TOOLSET set ENCFS_PLATFORM_TOOLSET=v143
if not defined ENCFS_WINSDK_VERSION set ENCFS_WINSDK_VERSION=10.0
msbuild dokan.sln /p:WindowsTargetPlatformVersion=%ENCFS_WINSDK_VERSION% /p:PlatformToolset=%ENCFS_PLATFORM_TOOLSET% /p:Configuration=Release /p:Platform=Win32 /t:dokan;dokan_fuse

REM verify necessary libraries were successfully built 
if NOT exist ".\Win32\Release\dokan2.lib" goto :build_failure
if NOT exist ".\Win32\Release\dokan2.dll" goto :build_failure
if NOT exist ".\Win32\Release\dokanfuse2.lib" goto :build_failure
if NOT exist ".\Win32\Release\dokanfuse2.dll" goto :build_failure

REM set DOKAN_ROOT environment variable for the caller
for %%I in ("%CD%") do (
    endlocal & set "DOKAN_ROOT=%%~fI"
)
setx DOKAN_ROOT "%DOKAN_ROOT%" >nul

goto :build_success



:build_success

echo.
echo ==================================================
echo      Dokan library successfully installed! 
echo ==================================================
echo.

goto :build_end



:build_failure

echo.
echo ==================================================
echo     Failed to build necessary Dokan library! 
echo ==================================================
echo.
exit /b 1

goto :build_end



:already_installed

echo.
echo ==================================================
echo            Dokan already installed
echo ==================================================
echo.
exit /b 0

goto :build_end



:build_end
popd
popd
exit /b 0
