:: Build script for AppVeyor (https://ci.appveyor.com/)
:: Environment:
::     TOOLCHAIN      -  Toolchain Version  [9.0/10.0/11.0/12.0/14.0/cygwin/mingw]
::     CONFIGURATION  -  determines EPICS build  [dynamic/static, -debug]
::     PLATFORM       -  "x86" -> use 32bit architecture
::
:: Prepares an Appveyor build by excuting the following steps
::     - Set up configure\CONFIG_SITE for static vs. dynamic build
::     - Install Cygwin / Mingw  (TOOLCHAIN setting) in the in the appropriate flavor
::     - Download and install Make-4.1 from EPICS download page

Setlocal EnableDelayedExpansion

set MY_OS=64BIT
if "%PLATFORM%"=="x86" set MY_OS=32BIT

echo [INFO] Platform: %MY_OS%

:: with MSVC either static or debug can be handled as part
:: of EPICS_HOST_ARCH but not both. So we set the appropriate
:: options in CONFIG_SITE. For mingw and cygwin they are missing
:: some static and debug targets so set things here too
echo.%CONFIGURATION% | findstr /C:"static">nul && (
    echo SHARED_LIBRARIES=NO>> configure\CONFIG_SITE
    echo STATIC_BUILD=YES>> configure\CONFIG_SITE
    echo [INFO] EPICS set up for static build
) || (
    echo [INFO] EPICS set up for dynamic build
)

echo.%CONFIGURATION% | findstr /C:"debug">nul && (
    echo HOST_OPT=NO>> configure\CONFIG_SITE
    echo [INFO] EPICS set up for debug build
) || (
    echo [INFO] EPICS set up for optimized build
)

if "%TOOLCHAIN%"=="cygwin" (
    if "%MY_OS%"=="64BIT" (
        echo [INFO] Installing Cygwin 64bit and dependencies
        @powershell -Command "(new-object net.webclient).DownloadFile('http://www.cygwin.com/setup-x86_64.exe', 'C:\cygwin64\setup-x86_64.exe')"
        C:\cygwin64\setup-x86_64.exe -q -P "libreadline-devel,libncursesw-devel"
    ) else (
        echo [INFO] Installing Cygwin 32bit and dependencies
        @powershell -Command "(new-object net.webclient).DownloadFile('http://www.cygwin.com/setup-x86.exe', 'C:\cygwin\setup-x86.exe')"
        C:\cygwin\setup-x86.exe -q -P "libreadline-devel,libncursesw-devel"
    )
)

echo [INFO] Installing Make 4.2.1 from ANL web site
curl -fsS --retry 3 -o C:\tools\make-4.2.1.zip https://epics.anl.gov/download/tools/make-4.2.1-win64.zip
cd \tools
"C:\Program Files\7-Zip\7z" e make-4.2.1.zip

if "%TOOLCHAIN%"=="2019" (
    set "PERLVER=5.30.0.1"
    echo [INFO] Installing Strawberry Perl %PERLVER%
    curl -fsS --retry 3 -o C:\tools\perl-%PERLVER%.zip http://strawberryperl.com/download/%PERLVER%/strawberry-perl-%PERLVER%-64bit.zip
    cd \tools
    "C:\Program Files\7-Zip\7z" x perl-%PERLVER%.zip -oC:\strawberry
    cd \strawberry
    :: we set PATH in appveyor-build.bat
    call relocation.pl.bat
)
