@ECHO Off
SETLOCAL

IF %1x == x GoTo Help
IF %2x == x GoTo Help

SET CONFIG=%1
SET ARCH=%2

ECHO === MakeBuild Configuration=%CONFIG% Architecture=%ARCH% ===

SET OUTDIR=%~dp0..\..\bin\%CONFIG%\%ARCH%\msix
SET BINDIR_MSIXADMIN=%OUTDIR%\..\msixadmin

CD /D %~dp0..\..

IF "%VisualStudioVersion%" != "18.0" CALL C:\Util\vc26vars64.bat

ECHO msbuild -p:Configuration=%CONFIG%;Platform=%ARCH% -t:Rebuild -p:MSIXADMIN=1 MSIXcli.sln
msbuild -p:Configuration=%CONFIG%;Platform=%ARCH% -t:Rebuild -p:MSIXADMIN=1 MSIXcli.sln
IF ERRORLEVEL 1 GoTo TheEnd
IF NOT EXIST %BINDIR_MSIXADMIN% MD %BINDIR_MSIXADMIN%
ECHO COPY %OUTDIR%\msix.exe %BINDIR_MSIXADMIN%\msixadmin.exe
COPY %OUTDIR%\msix.exe %BINDIR_MSIXADMIN%\msixadmin.exe
ECHO COPY %OUTDIR%\msix.pdb %BINDIR_MSIXADMIN%\msixadmin.pdb
COPY %OUTDIR%\msix.pdb %BINDIR_MSIXADMIN%\msixadmin.pdb

msbuild -p:Configuration=%CONFIG%;Platform=%ARCH% -t:Rebuild -p:MSIXADMIN=0 MSIXcli.sln
IF ERRORLEVEL 1 GoTo TheEnd

GoTo TheEnd

:Help
ECHO Usage: MAKEBUILD configuration architecture
ECHO   configuration = Debug or Release
ECHO    architecture = x64 OR arm64

:TheEnd
ENDLOCAL
