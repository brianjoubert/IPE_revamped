# --------------------------------------------------------------------
# Build Ipe (modern Qt UI) on Windows.
# Installs MSYS2 if needed (into C:\msys64, no admin rights required),
# then compiles Ipe.  Everything is logged to build-log.txt.
# --------------------------------------------------------------------
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$log = Join-Path $here "build-log.txt"
$msys = "C:\msys64"
Start-Transcript -Path $log -Force | Out-Null

try {
    if (-not (Test-Path "$msys\usr\bin\bash.exe")) {
        Write-Host "=== Downloading MSYS2 (about 100 MB) ==="
        $sfx = Join-Path $env:TEMP "msys2-base.sfx.exe"
        $url = "https://github.com/msys2/msys2-installer/releases/download/nightly-x86_64/msys2-base-x86_64-latest.sfx.exe"
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -Uri $url -OutFile $sfx
        Write-Host "=== Unpacking MSYS2 to $msys ==="
        & $sfx -y -oC:\ | Out-Null
        if (-not (Test-Path "$msys\usr\bin\bash.exe")) { throw "MSYS2 could not be unpacked to $msys" }
        Write-Host "=== First start of MSYS2 ==="
        & "$msys\usr\bin\bash.exe" -lc "true"
    }

    Write-Host "=== Updating MSYS2 ==="
    & "$msys\usr\bin\bash.exe" -lc "pacman -Syuu --noconfirm" | Write-Host
    # a core update closes the shell; run once more to finish
    & "$msys\usr\bin\bash.exe" -lc "pacman -Syuu --noconfirm" | Write-Host

    Write-Host "=== Building Ipe ==="
    $env:MSYSTEM = "UCRT64"
    $env:CHERE_INVOKING = "1"
    $unixHere = (& "$msys\usr\bin\cygpath.exe" -u $here).Trim()
    & "$msys\usr\bin\bash.exe" -lc "cd '$unixHere' && bash ./build-windows.sh 2>&1" | Write-Host
    if ($LASTEXITCODE -ne 0) { throw "Build script failed (exit code $LASTEXITCODE)" }

    $exe = Join-Path $here "..\..\dist\ipe-modern\bin\ipe.exe"
    Write-Host ""
    Write-Host "Done! Starting Ipe: $exe"
    Start-Process $exe
}
catch {
    Write-Host ""
    Write-Host "BUILD FAILED: $_"
    Write-Host "Tell Claude it failed -- the log is in $log"
}
finally {
    Stop-Transcript | Out-Null
}
Read-Host "Press Enter to close"
