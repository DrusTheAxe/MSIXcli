@ECHO Off
SETLOCAL

IF %1x == x GoTo Help
IF %2x == x GoTo Help

SET CONFIG=%1
SET ARCH=%2

ECHO === MakeBuild Configuration=%CONFIG% Architecture=%ARCH% ===

SET OUTDIR=%~dp0..\..\bin\%CONFIG%\%ARCH%\msixadmin
SET BINDIR_MSIX=%OUTDIR%\..\msix

CD /D %~dp0..\..

IF "%VisualStudioVersion%" != "18.0" CALL C:\Util\vc26vars64.bat

ECHO msbuild -p:Configuration=%CONFIG%;Platform=%ARCH% -t:Rebuild -p:MSIXADMIN=0 MSIXcli.sln
msbuild -p:Configuration=%CONFIG%;Platform=%ARCH% -t:Rebuild -p:MSIXADMIN=0 MSIXcli.sln
IF ERRORLEVEL 1 GoTo TheEnd
IF NOT EXIST %BINDIR_MSIX% MD %BINDIR_MSIX%
ECHO COPY %OUTDIR%\msixadmin.exe %BINDIR_MSIX%\msix.exe
COPY %OUTDIR%\msixadmin.exe %BINDIR_MSIX%\msix.exe
ECHO COPY %OUTDIR%\msixadmin.pdb %BINDIR_MSIX%\msix.pdb
COPY %OUTDIR%\msixadmin.pdb %BINDIR_MSIX%\msix.pdb

msbuild -p:Configuration=%CONFIG%;Platform=%ARCH% -t:Rebuild -p:MSIXADMIN=1 MSIXcli.sln
IF ERRORLEVEL 1 GoTo TheEnd

GoTo TheEnd

:Help
ECHO Usage: MAKEBUILD configuration architecture
ECHO   configuration = Debug or Release
ECHO    architecture = x64 OR arm64

:TheEnd
ENDLOCAL
