:: Universal build script for AppVeyor (https://ci.appveyor.com/)
:: Environment:
::     TOOLCHAIN      -  toolchain version   [10.0/11.0/12.0/14.0/2017/2019/mingw]
::     CONFIGURATION  -  determines EPICS build   [dynamic/static]
::     PLATFORM       -  architecture   [x86/x64]
::
:: All command line args are passed to make

Setlocal EnableDelayedExpansion

:: we do not currently have a combined static and debug EPICS_HOST_ARCH target
:: So a combined debug and static target will appear to be just static
:: but debug will have been specified in CONFIG_SITE by appveyor-prepare.bat
set "ST="
echo.%CONFIGURATION% | findstr /C:"debug">nul && (
    set "ST=-debug"
)
echo.%CONFIGURATION% | findstr /C:"static">nul && (
    set "ST=-static"
)

set MY_OS=64BIT
if "%PLATFORM%"=="x86" set MY_OS=32BIT

echo [INFO] Platform: %MY_OS%

:: Use parallel make, except for 3.14
set "MAKEARGS=-j2 -Otarget"
if "%APPVEYOR_REPO_BRANCH%"=="3.14" set MAKEARGS=

if "%TOOLCHAIN%"=="mingw" (
    set "MAKE=mingw32-make"
    if "%MY_OS%"=="64BIT" (
        set "EPICS_HOST_ARCH=windows-x64-mingw"
        set "INCLUDE=C:\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\include;%INCLUDE%"
        set "PATH=C:\mingw-w64\x86_64-8.1.0-posix-seh-rt_v6-rev0\mingw64\bin;%PATH%"
        echo [INFO] MinGW Toolchain 64bit
    ) else (
        set "EPICS_HOST_ARCH=win32-x86-mingw"
        set "INCLUDE=C:\mingw-w64\i686-6.3.0-posix-dwarf-rt_v5-rev1\mingw32\include;%INCLUDE%"
        set "PATH=C:\mingw-w64\i686-6.3.0-posix-dwarf-rt_v5-rev1\mingw32\bin;%PATH%"
        echo [INFO] MinGW Toolchain 32bit
    )
    echo [INFO] Compiler Version
    gcc -v
    goto Finish
)

if "%TOOLCHAIN%"=="2019" (
    echo [INFO] Setting strawberry perl path
    set "PATH=c:\strawberry\perl\site\bin;C:\strawberry\perl\bin;%PATH%"
)

set "VSINSTALL=C:\Program Files (x86)\Microsoft Visual Studio %TOOLCHAIN%"
if not exist "%VSINSTALL%\" set "VSINSTALL=C:\Program Files (x86)\Microsoft Visual Studio\%TOOLCHAIN%\Community"
if not exist "%VSINSTALL%\" goto MSMissing

set "MAKE=C:\tools\make"

echo [INFO] APPVEYOR_BUILD_WORKER_IMAGE=%APPVEYOR_BUILD_WORKER_IMAGE%

if "%MY_OS%"=="64BIT" (
    set EPICS_HOST_ARCH=windows-x64%ST%
    :: VS 2017/2019
    if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
        where cl
        if !ERRORLEVEL! NEQ 0 goto MSMissing
        goto MSFound
    )
    if exist "%VSINSTALL%\VC\vcvarsall.bat" (
        call "%VSINSTALL%\VC\vcvarsall.bat" amd64
        where cl
        if !ERRORLEVEL! NEQ 0 (
            call "%VSINSTALL%\VC\vcvarsall.bat" x86_amd64
            where cl
            if !ERRORLEVEL! NEQ 0 goto MSMissing
        )
        goto MSFound
    )
    if exist "%VSINSTALL%\VC\bin\amd64\vcvars64.bat" (
        call "%VSINSTALL%\VC\bin\amd64\vcvars64.bat"
        where cl
        if !ERRORLEVEL! NEQ 0 goto MSMissing
        goto MSFound
    )
) else (
    set EPICS_HOST_ARCH=win32-x86%ST%
    :: VS 2017/2019
    if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat" (
        call "%VSINSTALL%\VC\Auxiliary\Build\vcvars32.bat"
        where cl
        if !ERRORLEVEL! NEQ 0 goto MSMissing
        goto MSFound
    )
    if exist "%VSINSTALL%\VC\vcvarsall.bat" (
        call "%VSINSTALL%\VC\vcvarsall.bat" x86
        where cl
        if !ERRORLEVEL! NEQ 0 goto MSMissing
        goto MSFound
    )
    if exist "%VSINSTALL%\VC\bin\vcvars32.bat" (
        call "%VSINSTALL%\VC\bin\vcvars32.bat"
        where cl
        if !ERRORLEVEL! NEQ 0 goto MSMissing
        goto MSFound
    )
    if exist "%VSINSTALL%\Common7\Tools\vsvars32.bat" (
        call "%VSINSTALL%\Common7\Tools\vsvars32.bat"
        where cl
        if !ERRORLEVEL! NEQ 0 goto MSMissing
        goto MSFound
    )
)

:MSMissing
echo [INFO] Installation for MSVC Toolchain %TOOLCHAIN% / %MY_OS% seems to be missing
exit 1

:MSFound
echo [INFO] Microsoft Visual Studio Toolchain %TOOLCHAIN%
echo [INFO] Compiler Version
cl

:Finish
echo [INFO] EPICS_HOST_ARCH: %EPICS_HOST_ARCH%
echo [INFO] Make version
%MAKE% --version
echo [INFO] Perl version
perl --version

%MAKE% %MAKEARGS% %*
