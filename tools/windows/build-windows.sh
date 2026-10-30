#!/usr/bin/env bash
# Builds Ipe with the modern Qt UI inside MSYS2 (UCRT64).
# Called by build-windows.ps1 -- you normally don't run this directly.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$(cd "$HERE/../.." && pwd)"
OUT="$SRC/dist/ipe-modern"

step() { echo; echo "=== $* ==="; }

step "Installing packages"
pacman -S --needed --noconfirm \
    git make zip \
    mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-svg \
    mingw-w64-ucrt-x86_64-cairo mingw-w64-ucrt-x86_64-freetype \
    mingw-w64-ucrt-x86_64-lua mingw-w64-ucrt-x86_64-libspiro \
    mingw-w64-ucrt-x86_64-gsl mingw-w64-ucrt-x86_64-zlib \
    mingw-w64-ucrt-x86_64-imagemagick mingw-w64-ucrt-x86_64-ntldd

export COMSPEC="${COMSPEC:-C:\\Windows\\system32\\cmd.exe}"

step "Toolchain"
gcc --version | head -1
pkg-config --modversion Qt6Widgets cairo freetype2 gsl
LUAPKG=$(pkg-config --list-all | grep -o -E "^lua[0-9.]*" | sort | tail -1)
MOCDIR=$(qtpaths6 --query QT_HOST_LIBEXECS 2>/dev/null || true)
[ -x "$MOCDIR/moc.exe" ] || MOCDIR=$(dirname "$(find /ucrt64 -name moc.exe | head -1)")
echo "Lua package: $LUAPKG"
echo "moc: $MOCDIR/moc.exe"

step "Using the source in this repository"
cd "$SRC"
git log --oneline -1 2>/dev/null || true

step "Windows icon"
mkdir -p build
( cd artwork/ipe.iconset && magick icon_16x16.png icon_32x32.png icon_64x64.png \
      icon_128x128.png icon_256x256.png ../../build/ipe.ico )

step "Compiling"
cd src
make -j"$(nproc)" IPEUI=QT IPEMSYS2=1 LUA_PACKAGE="$LUAPKG" MOC="$MOCDIR/moc.exe" \
     ipe ipetoipe iperender ipescript ipeextract ipe6upgrade ipecurl
cd ..

step "Assembling the application folder"
rm -rf "$OUT"
mkdir -p "$OUT"/{bin,lua,icons,ipelets,styles,scripts}
cp build/bin/*.exe build/bin/*.dll "$OUT/bin/"
cp src/ipe/lua/*.lua "$OUT/lua/"
cp artwork/icons.ipe artwork/ipe_logo.ipe artwork/ipe.iconset/icon_128x128.png "$OUT/icons/"
cp src/ipelets/lua/*.lua "$OUT/ipelets/"
cp styles/* "$OUT/styles/"
cp scripts/* "$OUT/scripts/" 2>/dev/null || true
cp LICENSE.txt "$OUT/"
WDQ=$(command -v windeployqt6 || command -v windeployqt || true)
if [ -n "$WDQ" ]; then
    "$WDQ" --no-translations --no-system-d3d-compiler --no-opengl-sw \
           --dir "$OUT/bin" "$OUT/bin/ipe.exe" || echo "windeployqt reported a problem (continuing)"
else
    echo "windeployqt not found; copying Qt plugins by hand"
    QTPLUG=$(qtpaths6 --query QT_INSTALL_PLUGINS)
    for d in platforms styles imageformats iconengines; do
        [ -d "$QTPLUG/$d" ] && cp -r "$QTPLUG/$d" "$OUT/bin/"
    done
fi
# Copy every DLL from MSYS2 that the programs and plugins need
for pass in 1 2 3; do
    find "$OUT/bin" \( -name '*.exe' -o -name '*.dll' \) | while read -r f; do
        ntldd -R "$f" 2>/dev/null | grep -i -o -E '[A-Za-z]:\\[^ ]*ucrt64\\bin\\[^ ]*\.dll' || true
    done | sort -u | while read -r w; do
        u=$(cygpath -u "$w"); b=$(basename "$u")
        [ -f "$OUT/bin/$b" ] || cp "$u" "$OUT/bin/"
    done
done
cat > "$OUT/Ipe.bat" <<'EOF'
@start "" "%~dp0bin\ipe.exe" %*
EOF

step "Zip"
cd "$SRC/dist"
rm -f ipe-modern-windows-x64.zip
zip -qr ipe-modern-windows-x64.zip ipe-modern
du -sh ipe-modern ipe-modern-windows-x64.zip

echo
echo "BUILD SUCCEEDED"
echo "Run: $(cygpath -w "$OUT/bin/ipe.exe")"
