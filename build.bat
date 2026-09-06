@ECHO OFF
REM build.bat
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


REM Allow building of encfs4win 2.0
set ENCFS_MAJOR_VERSION=1

REM Allow non-interactive building 
set INTERACTIVE=1

REM process command line parameters 
:param_loop
IF NOT "%1"=="" (
    IF /I "%1"=="--beta" (
        set ENCFS_MAJOR_VERSION=2
        SHIFT
    )
    IF /I "%1"=="--yes" (
        SET INTERACTIVE=0
        SHIFT
    )
    SHIFT
    GOTO :param_loop
)


REM Remember the PWD for encfs4win project
set PROJECT_DIR=%CD%


REM Make sure perl is installed 
perl < nul
if NOT %ERRORLEVEL% == 0 goto :no_perl


REM MSYS is required to build libgcrypt, and we don't know where it is yet 
if "%ENCFS_MAJOR_VERSION%"=="2" (
    set MSYS_BIN_DIR=C:\MinGW\msys\1.0\bin
    if NOT exist "%MSYS_BIN_DIR%\sh.exe" (
        SET /P MSYS_BIN_DIR=Please input the path to your MSYS bin directory: 
    )
    if NOT exist "%MSYS_BIN_DIR%\sh.exe" goto :no_msys
)


REM Make sure MSBUILD is available (and set up environment for modern MSVC)
set DEPS_DIR=%CD%\deps
set "VSWHERE=%PROGRAMFILES(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if NOT exist "%VSWHERE%" goto :no_msbuild

REM Prefer complete VS IDE installs, then Build Tools; always pick an install that has the C++ toolset
set "VSINSTALLDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products Microsoft.VisualStudio.Product.Community,Microsoft.VisualStudio.Product.Professional,Microsoft.VisualStudio.Product.Enterprise -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%i"
if not defined VSINSTALLDIR (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALLDIR=%%i"
)
if not defined VSINSTALLDIR goto :no_msbuild
if NOT exist "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars32.bat" goto :no_msbuild
if NOT exist "%VSINSTALLDIR%\MSBuild\Current\Bin\MSBuild.exe" goto :no_msbuild

set "PATH=%VSINSTALLDIR%\MSBuild\Current\Bin;%PATH%"

REM Match PlatformToolset to the selected Visual Studio generation
if not defined ENCFS_PLATFORM_TOOLSET (
    echo.%VSINSTALLDIR%| findstr /I "\\18\\" >nul
    if not errorlevel 1 set ENCFS_PLATFORM_TOOLSET=v145
)
if not defined ENCFS_PLATFORM_TOOLSET (
    echo.%VSINSTALLDIR%| findstr /I "\\2022\\" >nul
    if not errorlevel 1 set ENCFS_PLATFORM_TOOLSET=v143
)
if not defined ENCFS_PLATFORM_TOOLSET set ENCFS_PLATFORM_TOOLSET=v143
if not defined ENCFS_WINSDK_VERSION set ENCFS_WINSDK_VERSION=10.0

echo.
echo Using Visual Studio at: %VSINSTALLDIR%
echo PlatformToolset=%ENCFS_PLATFORM_TOOLSET%  WindowsTargetPlatformVersion=%ENCFS_WINSDK_VERSION%
echo.

REM Initialize MSVC build environment (cl / nmake / INCLUDE / LIB).
REM VsDevCmd is preferred, but can fail under non-interactive/redirected hosts,
REM so we always ensure a usable MSVC + Windows SDK environment afterwards.
set "VSCMD_SKIP_SENDTELEMETRY=1"
call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars32.bat" >"%TEMP%\encfs-vcvars.log" 2>&1

REM Locate the newest installed MSVC toolset under this VS install
set "MSVC_ROOT="
for /f "usebackq delims=" %%i in (`dir /b /ad /o-n "%VSINSTALLDIR%\VC\Tools\MSVC"`) do (
    if not defined MSVC_ROOT if exist "%VSINSTALLDIR%\VC\Tools\MSVC\%%i\bin\Hostx86\x86\cl.exe" set "MSVC_ROOT=%VSINSTALLDIR%\VC\Tools\MSVC\%%i"
)
if not defined MSVC_ROOT goto :no_msbuild

REM Locate a Windows 10/11 SDK with headers+libs
set "WINSDK_ROOT=%ProgramFiles(x86)%\Windows Kits\10"
set "WINSDK_VER="
for /f "usebackq delims=" %%i in (`dir /b /ad /o-n "%WINSDK_ROOT%\Include"`) do (
    if not defined WINSDK_VER if exist "%WINSDK_ROOT%\Include\%%i\um\windows.h" if exist "%WINSDK_ROOT%\Lib\%%i\um\x86\kernel32.lib" set "WINSDK_VER=%%i"
)
if not defined WINSDK_VER goto :no_msbuild

set "PATH=%MSVC_ROOT%\bin\Hostx86\x86;%VSINSTALLDIR%\MSBuild\Current\Bin;%PATH%"
set "INCLUDE=%MSVC_ROOT%\include;%MSVC_ROOT%\atlmfc\include;%WINSDK_ROOT%\Include\%WINSDK_VER%\ucrt;%WINSDK_ROOT%\Include\%WINSDK_VER%\shared;%WINSDK_ROOT%\Include\%WINSDK_VER%\um;%WINSDK_ROOT%\Include\%WINSDK_VER%\winrt;%WINSDK_ROOT%\Include\%WINSDK_VER%\cppwinrt"
set "LIB=%MSVC_ROOT%\lib\x86;%MSVC_ROOT%\atlmfc\lib\x86;%WINSDK_ROOT%\Lib\%WINSDK_VER%\ucrt\x86;%WINSDK_ROOT%\Lib\%WINSDK_VER%\um\x86"
set "LIBPATH=%MSVC_ROOT%\lib\x86;%MSVC_ROOT%\atlmfc\lib\x86"

echo MSVC_ROOT=%MSVC_ROOT%
echo WINSDK_VER=%WINSDK_VER%
echo.

where nmake >nul 2>&1
if ERRORLEVEL 1 goto :no_msbuild
where cl >nul 2>&1
if ERRORLEVEL 1 goto :no_msbuild
where msbuild >nul 2>&1
if ERRORLEVEL 1 goto :no_msbuild
if not exist "%MSVC_ROOT%\include\excpt.h" goto :no_msbuild

REM Reuse previously built dependencies when present
if not defined OPENSSL_ROOT (
    if exist "%PROJECT_DIR%\deps\openssl\install-dir\include\openssl\ssl.h" set "OPENSSL_ROOT=%PROJECT_DIR%\deps\openssl\install-dir"
)
if not defined DOKAN_ROOT (
    if defined DokanLibrary2 if exist "%DokanLibrary2%\x86\lib\dokan2.lib" if exist "%DokanLibrary2%\include\fuse.h" set "DOKAN_ROOT=%DokanLibrary2%"
)
if not defined DOKAN_ROOT (
    if exist "%PROJECT_DIR%\deps\dokan\Win32\Release\dokan2.lib" if exist "%PROJECT_DIR%\deps\dokan\dokan_fuse\include\fuse.h" set "DOKAN_ROOT=%PROJECT_DIR%\deps\dokan"
)
if not defined DOKAN_ROOT (
    if exist "%PROJECT_DIR%\deps\dokan\Win32\Release\dokan1.lib" if exist "%PROJECT_DIR%\deps\dokan\dokan_fuse\include\fuse.h" set "DOKAN_ROOT=%PROJECT_DIR%\deps\dokan"
)



REM openssl
call build-openssl.bat
if NOT %ERRORLEVEL% == 0 goto :no_openssl
if not defined OPENSSL_ROOT set "OPENSSL_ROOT=%PROJECT_DIR%\deps\openssl\install-dir"
if NOT exist "%OPENSSL_ROOT%\include\openssl\ssl.h" set "OPENSSL_ROOT=%PROJECT_DIR%\deps\openssl\install-dir"


REM libgpg-error
if "%ENCFS_MAJOR_VERSION%"=="2" (
    call build-libgpgerror.bat
    if NOT %ERRORLEVEL% == 0 goto :no_libgpgerror
)


REM libgcrypt
if "%ENCFS_MAJOR_VERSION%"=="2" (
    call build-libgcrypt.bat
    if NOT %ERRORLEVEL% == 0 goto :no_libgcrypt
)


REM tinyxml2
call build-tinyxml2.bat
if NOT %ERRORLEVEL% == 0 goto :no_tinyxml2


REM easyloggingpp
call build-easyloggingpp.bat
if NOT %ERRORLEVEL% == 0 goto :no_easyloggingpp


REM dokany
call build-dokany.bat
if NOT %ERRORLEVEL% == 0 goto :no_dokany
if not defined DOKAN_ROOT (
    if defined DokanLibrary2 set "DOKAN_ROOT=%DokanLibrary2%"
)
if not defined DOKAN_ROOT set "DOKAN_ROOT=%PROJECT_DIR%\deps\dokan"
REM Strip trailing backslash — otherwise msbuild /p:"path\" eats the closing quote
if "%DOKAN_ROOT:~-1%"=="\" set "DOKAN_ROOT=%DOKAN_ROOT:~0,-1%"
if not defined DOKAN_LIB_DIR (
    if exist "%DOKAN_ROOT%\x86\lib\dokan2.lib" (
        set "DOKAN_LIB_DIR=%DOKAN_ROOT%\x86\lib"
    ) else if exist "%DOKAN_ROOT%\Win32\Release\dokan2.lib" (
        set "DOKAN_LIB_DIR=%DOKAN_ROOT%\Win32\Release"
    ) else if defined DokanLibrary2_LibraryPath_x86 (
        set "DOKAN_LIB_DIR=%DokanLibrary2_LibraryPath_x86%"
    )
)
if "%DOKAN_LIB_DIR:~-1%"=="\" set "DOKAN_LIB_DIR=%DOKAN_LIB_DIR:~0,-1%"

echo.
echo OPENSSL_ROOT=%OPENSSL_ROOT%
echo DOKAN_ROOT=%DOKAN_ROOT%
echo DOKAN_LIB_DIR=%DOKAN_LIB_DIR%
echo.

REM (Clean,)? Build encfs 
echo.
echo ==================================================
echo                   BUILDING ENCFS             
echo ==================================================
msbuild encfs/encfs.sln /p:Configuration=Release /p:Platform=x86 /p:PlatformToolset=%ENCFS_PLATFORM_TOOLSET% /p:WindowsTargetPlatformVersion=%ENCFS_WINSDK_VERSION% /p:DOKAN_ROOT="%DOKAN_ROOT%" /p:DOKAN_LIB_DIR="%DOKAN_LIB_DIR%" /t:Clean,Build

REM verify necessary executables were successfully installed  
if NOT exist ".\encfs\Release\encfs.exe" goto :build_failure
if NOT exist ".\encfs\Release\encfsctl.exe" goto :build_failure

REM Copy runtime DLLs next to the executables (Win32 / x86)
echo.
echo ==================================================
echo              COPYING RUNTIME DLLS
echo ==================================================
set "RELEASE_DIR=%PROJECT_DIR%\encfs\Release"
set "DOKAN_DLL_DIR="
if exist "%DOKAN_ROOT%\x86\dokanfuse2.dll" set "DOKAN_DLL_DIR=%DOKAN_ROOT%\x86"
if not defined DOKAN_DLL_DIR if exist "%DOKAN_ROOT%\Win32\Release\dokanfuse2.dll" set "DOKAN_DLL_DIR=%DOKAN_ROOT%\Win32\Release"
if not defined DOKAN_DLL_DIR if exist "%DOKAN_ROOT%\dokanfuse2.dll" set "DOKAN_DLL_DIR=%DOKAN_ROOT%"

if not defined DOKAN_DLL_DIR (
    echo Failed to locate dokanfuse2.dll for runtime copy.
    goto :build_failure
)

copy /Y "%DOKAN_DLL_DIR%\dokanfuse2.dll" "%RELEASE_DIR%\" >nul
if exist "%DOKAN_DLL_DIR%\dokan2.dll" copy /Y "%DOKAN_DLL_DIR%\dokan2.dll" "%RELEASE_DIR%\" >nul
if exist "%DOKAN_ROOT%\dokan2.dll" if not exist "%RELEASE_DIR%\dokan2.dll" copy /Y "%DOKAN_ROOT%\dokan2.dll" "%RELEASE_DIR%\" >nul
if exist "%OPENSSL_ROOT%\bin\libeay32.dll" copy /Y "%OPENSSL_ROOT%\bin\libeay32.dll" "%RELEASE_DIR%\" >nul
if exist "%OPENSSL_ROOT%\bin\ssleay32.dll" copy /Y "%OPENSSL_ROOT%\bin\ssleay32.dll" "%RELEASE_DIR%\" >nul

if NOT exist "%RELEASE_DIR%\dokanfuse2.dll" goto :build_failure
if NOT exist "%RELEASE_DIR%\libeay32.dll" goto :build_failure
if NOT exist "%RELEASE_DIR%\ssleay32.dll" goto :build_failure

echo Copied runtime DLLs to %RELEASE_DIR%
dir /b "%RELEASE_DIR%\*.dll"

goto :build_success



:build_success

echo.
echo ==================================================
echo           Encfs successfully built! 
echo ==================================================
echo.

goto :end



:build_failure

echo.
echo ==================================================
echo        Failed to build necessary Encfs! 
echo ==================================================
echo.
exit /b 1

goto :end



:no_msbuild

echo.
echo ==================================================
echo   A recent Visual Studio with MSVC C++ tools (VS 2022+) is required!
echo ==================================================
echo.
exit /b 1

goto :end



:no_msys

echo.
echo ==================================================
echo     MSYS is required to build this project!
echo ==================================================
echo.
exit /b 1

goto :end



:no_perl

echo.
echo ==================================================
echo     Perl is required to build this project!
echo ==================================================
echo.
exit /b 1

goto :end



:no_openssl

echo.
echo ==================================================
echo    OpenSSL could not be built, and is required!
echo ==================================================
echo.
exit /b 1

goto :end



:no_libgpgerror

echo.
echo ==================================================
echo  libgpg-error could not be built, and is required!
echo ==================================================
echo.
exit /b 1

goto :end



:no_libgcrypt

echo.
echo ==================================================
echo   libgcrypt could not be built, and is required!
echo ==================================================
echo.
exit /b 1

goto :end



:no_tinyxml2

echo.
echo ==================================================
echo   tinyxml2 could not be built, and is required!
echo ==================================================
echo.
exit /b 1

goto :end



:no_easyloggingpp

echo.
echo ==================================================
echo easylogging++ could not be built, and is required!
echo ==================================================
echo.
exit /b 1

goto :end



:no_dokany

echo.
echo ==================================================
echo    Dokany could not be built, and is required!
echo ==================================================
echo.
exit /b 1

goto :end



:end
exit /b 0
