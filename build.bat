@echo off

:: name of compiling project
set project_name=sdf_decomposition_2d
:: PC specific!
set premake_path=C:\premake\premake5.exe

:Done

echo Enter command to setup project %project_name%
echo Enter 'help' to show list of commands
echo.

set /p user_input=

IF "%user_input%" == ""         GOTO PrintHelp
IF "%user_input%" == "help"     GOTO PrintHelp
IF "%user_input%" == "compile"  GOTO Compile
IF "%user_input%" == "c"        GOTO Compile
IF "%user_input%" == "run"      GOTO Run
IF "%user_input%" == "r"        GOTO Run
IF "%user_input%" == "exit"     EXIT

::
:: PREMAKE COMMANDS
::

echo.
%premake_path% "%user_input%"
echo.
GOTO Done

::
:: HELP
::

:PrintHelp

echo.
echo Enter 'setup_project.bat action' where action is one of the following:
echo.
echo   compile / c       Compile your program
echo   run     / r       Run compiled program
echo   clean             Remove all binaries and intermediate binaries and project files.
echo   exit              Exit from this program
echo.
echo Premake commands:
echo.
echo   codelite          Generate CodeLite project files
echo   gmake             Generate GNU makefiles for Linux
echo   vs2005            Generate Visual Studio 2005 project files
echo   vs2008            Generate Visual Studio 2008 project files
echo   vs2010            Generate Visual Studio 2010 project files
echo   vs2012            Generate Visual Studio 2012 project files
echo   vs2013            Generate Visual Studio 2013 project files
echo   vs2015            Generate Visual Studio 2015 project files
echo   vs2017            Generate Visual Studio 2017 project files
echo   vs2019            Generate Visual Studio 2019 project files
echo   vs2022            Generate Visual Studio 2022 project files
echo   xcode4            Generate Apple Xcode 4 project files
echo.

GOTO Done

::
:: COMPILE & RUN
::

:Compile
echo.

%premake_path% vs2022

if not defined DevEnvDir (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
)

set solutionFile="%project_name%.sln"
msbuild /t:Build /p:Configuration=Debug /p:Platform=x64 %solutionFile%

echo.

bin\Debug-windows-x86_64\%project_name%\%project_name%.exe

GOTO Done

::
:: RUN
::

:Run
echo.

bin\Debug-windows-x86_64\%project_name%\%project_name%.exe

GOTO Done