@echo off
setlocal
rem Adds the complete Ipe source code (with its history) to this repository.
rem Double-click this file once, then click "Push origin" in GitHub Desktop.
cd /d "%~dp0"

set "GIT="
rem 1. the git that comes with GitHub Desktop
for /d %%D in ("%LOCALAPPDATA%\GitHubDesktop\app-*") do if exist "%%D\resources\app\git\cmd\git.exe" set "GIT=%%D\resources\app\git\cmd\git.exe"
rem 2. Git for Windows, if installed
if not defined GIT for /f "delims=" %%G in ('where git 2^>nul') do if not defined GIT set "GIT=%%G"
rem 3. the git from MSYS2 (needs its own folder on the PATH)
if not defined GIT if exist "C:\msys64\usr\bin\git.exe" (
  set "GIT=C:\msys64\usr\bin\git.exe"
  set "PATH=C:\msys64\usr\bin;%PATH%"
)
if not defined GIT (
  echo Could not find git on this computer. Tell Claude.
  pause
  exit /b 1
)
echo Using git: %GIT%
set "G="%GIT%" -c safe.directory=* -c core.autocrlf=false -c "user.name=Fabien Lafont" -c "user.email=lafont.fabien@gmail.com""

%G% fetch ipe-revamped.bundle main || goto :fail
%G% merge --no-edit -X theirs -m "Update from Claude: fix the Windows build workflow" FETCH_HEAD || goto :fail

echo.
echo Done! Latest commits:
%G% log --oneline -5
del ipe-revamped.bundle
echo.
echo Next: in GitHub Desktop click "Push origin".
echo GitHub then builds the Windows version (about 15 minutes); it appears under "Releases".
pause
(goto) 2>nul & del "%~f0"

:fail
echo.
echo Something went wrong (see above). Tell Claude what the message says.
pause
exit /b 1
