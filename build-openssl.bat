@ECHO OFF
SETLOCAL EnableDelayedExpansion
REM build-openssl.bat
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


REM versioning variables 
set VERSION=1.0.2u
set VERSION_STR=1_0_2u
set SOURCE_URI=https://github.com/openssl/openssl.git



REM ========= DO NOT EDIT BELOW THIS LINE =====================



REM set up some globally-constant settings
set SRC_DIR_NAME=openssl
set INSTALL_DIR=install-dir


REM don't bother if they already have an installation
if defined OPENSSL_ROOT (
  if exist "%OPENSSL_ROOT%\bin\libeay32.dll" (
    if exist "%OPENSSL_ROOT%\bin\ssleay32.dll" (
      if exist "%OPENSSL_ROOT%\include" (goto :already_installed)
    )
  )
)


REM Failed to find OpenSSL -- ask user if they want us to build it for them
echo.
if "%INTERACTIVE%"=="1" (
    SET /P CONFIRM_BUILD="OpenSSL (OPENSSL_ROOT) was not detected.  Should we install it now? (Y/n): "
    if /I NOT "!CONFIRM_BUILD!"=="y" exit /b 1
)


REM move into deps folder 
if NOT exist "deps" mkdir deps
pushd deps


REM Clone git repository and switch to VERSION release 
echo.
echo ==================================================
echo            CLONING OPENSSL REPOSITORY (%VERSION%)             
echo ==================================================
git clone %SOURCE_URI% %SRC_DIR_NAME% > %SRC_DIR_NAME%-clone.log
pushd %SRC_DIR_NAME%
git clean -ffdx
git reset --hard OpenSSL_%VERSION_STR%
git checkout OpenSSL_%VERSION_STR%

REM build libraries (no assembly) 
echo.
echo ==================================================
echo             BUILDING OPENSSL LIBRARIES             
echo ==================================================
perl Configure VC-WIN32 no-asm --prefix="%INSTALL_DIR%"
cmd /c ms\do_ms
nmake -f ms\ntdll.mak
nmake -f ms\ntdll.mak install

REM verify necessary libraries were successfully installed  
if NOT exist ".\%INSTALL_DIR%\bin\libeay32.dll" goto :build_failure
if NOT exist ".\%INSTALL_DIR%\bin\ssleay32.dll" goto :build_failure
if NOT exist ".\%INSTALL_DIR%\include" goto :build_failure

REM set OPENSSL_ROOT environment variable for the caller
for %%I in ("%CD%\%INSTALL_DIR%") do (
    endlocal & set "OPENSSL_ROOT=%%~fI"
)
setx OPENSSL_ROOT "%OPENSSL_ROOT%" >nul

goto :build_success



:build_success

echo.
echo ==================================================
echo     OpenSSL libraries successfully installed! 
echo ==================================================
echo.

goto :build_end



:build_failure

echo.
echo ==================================================
echo        Failed to build OpenSSL libraries
echo ==================================================
echo.
exit /b 1

goto :build_end



:already_installed

echo.
echo ==================================================
echo            OpenSSL already installed
echo ==================================================
echo.
exit /b 0

goto :build_end



:build_end
popd
popd
exit /b 0
