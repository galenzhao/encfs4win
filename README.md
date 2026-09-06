# encfs4win - Windows port of EncFS 

[![Build status](https://ci.appveyor.com/api/projects/status/5tpsgd73ndv89v3e/branch/master?svg=true)](https://ci.appveyor.com/project/jetwhiz/encfs4win/branch/master)

## About

EncFS4win is a revival of the original encfs4win project, but has been brought up-to-date with recent changes done to the EncFS project.  The original encfs4win was based on EncFS v1.7.4, which was shown to have some [security issues](https://defuse.ca/audits/encfs.htm). [Updates to EncFS](https://github.com/vgough/encfs) have been done recently to fix many of these issues, and the goal of this project is to port these modernizations to Windows. 

EncFS encrypts individual files, by translating all requests for the virtual EncFS filesystem into the equivalent encrypted operations on the raw filesystem.

A GUI for encfs4win is available through encfsw, providing a tray icon that makes mounting and unmounting encrypted partitions even easier! 

EncFS provides an encrypted filesystem in user-space, using the [Dokan library](https://github.com/dokan-dev/dokany) for the filesystem interface. EncFS is open source software, licensed under the LGPL.

For more info, see:

 - The excellent [encfs manpage](encfs/encfs.pod)
 - The technical overview in [DESIGN.md](DESIGN.md)

**NOTE:** *Be sure to use a drive (like "X:") as plain_dir to avoid case sensitive problems which results in file/folder not found problems!*

## Installing

Installation is as simple as a few clicks!  Simply download the latest encfs4win installer from [encfs4win/releases](https://github.com/jetwhiz/encfs4win/releases) (e.g., encfs4win-installer.exe) and run it.  

The installer contains everything needed to run encfs on Windows, including the encfs executables, OpenSSL libraries, and all necessary MS Visual C++ libraries. It will also automatically install the Dokan library (if it is not already installed). 

Encfs4win is also now available through the [Chocolatey](https://github.com/chocolatey/choco) package manager.  For installation instructions via choco, please refer to https://chocolatey.org/packages/encfs4win.

## Building

Encfs4win has a few dependencies: 

* [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) - For building the project (including the MSVC v143 toolset and a Windows 10/11 SDK). Visual Studio 2015 (v140) is no longer required.
* [Dokan library](https://github.com/dokan-dev/dokany) - Handles FUSE portion of software. Prefer a system install of **Dokan 2.3.x** (`DokanLibrary2`). The build links `dokan2` / `dokanfuse2`. 
* [TinyXML2 library](https://github.com/leethomason/tinyxml2) - Reads and writes configuration files for encfs.
* [Easylogging++](https://github.com/easylogging/easyloggingpp) - Provides logging functionality. 
* [OpenSSL library](https://github.com/openssl/openssl) - Always use the latest version.  Note that you must have Perl installed in order to build OpenSSL!

### Automated version

Encfs4win now comes with a fully-automated build tool called "build.bat", located in the root directory. This tool will automatically download, build and install Dokan, TinyXML2, Easylogging++ and OpenSSL, before finally building encfs.  Look for "encfs.exe" and "encfsctl.exe" in the encfs\Release folder after building is finished. 

The automated tool will also check to see if any of these prerequisites are already installed (by looking for the DOKAN_ROOT and OPENSSL_ROOT environment variables).  If found, it will use the installed version and skip over that prerequisite.  

*Note that this tool also has the capability to build libgcrypt (and libgpg-error) if you provide the "--beta" argument (MSYS required).  Encfs is still in the process of providing support for the libgcrypt library, however.*

### Manual version

You can also choose to handle some or all of the prerequisites yourself.  After the above dependencies have been installed and built, simply open the encfs4win solution (encfs/encfs.sln) and build the solution.  Note that you must have the environment variables DOKAN_ROOT and OPENSSL_ROOT pointing to your Dokan and OpenSSL installations, respectively (otherwise you will need to modify the solution to point to your installations).  

This will result in encfs.exe and encfsctl.exe binaries being built and placed in the encfs\Release directory. 

## Environment

Encfs4win works on: 

* Windows 10
* Windows 8 and 8.1
* Windows Server 2012
* Windows 7

## Status

I will try to keep this updated with the [EncFS project](https://github.com/vgough/encfs) as changes come in upstream.  

EncFS4win has been successfully built against the original release of Dokan (v0.7.x) as well as the latest Dokan (v1.x.x).  

## Credits

Special thanks to [vgough/encfs](https://github.com/vgough/encfs) and [freddy77/encfs4win](https://github.com/freddy77/encfs4win) for establishing the groundwork that made this project possible! 
