#!/bin/bash
# Download official pre-built CEF minimal distribution from Spotify CDN
# No H.264 or PipeWire — not needed for Outlook (email only, no video calls)
set -euo pipefail

CEF_VERSION="133.4.2+g0852ba6+chromium-133.0.6943.127"
CEF_FILENAME="cef_binary_${CEF_VERSION}_linux64_minimal"
CEF_URL="https://cef-builds.spotifycdn.com/${CEF_FILENAME}.tar.bz2"
CEF_DIR="${1:-/tmp/cef}"

if [ -d "$CEF_DIR/include" ]; then
    echo "[cef] Already downloaded at $CEF_DIR"
    exit 0
fi

echo "[cef] Downloading CEF ${CEF_VERSION} (minimal)..."
mkdir -p "$CEF_DIR"
curl -fSL "$CEF_URL" -o /tmp/cef_minimal.tar.bz2

echo "[cef] Extracting..."
tar xjf /tmp/cef_minimal.tar.bz2 -C /tmp/
cp -a /tmp/${CEF_FILENAME}/* "$CEF_DIR/"
rm -rf /tmp/cef_minimal.tar.bz2 "/tmp/${CEF_FILENAME}"

# Build libcef_dll_wrapper
echo "[cef] Building libcef_dll_wrapper..."
mkdir -p "$CEF_DIR/build"
cd "$CEF_DIR/build"
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_SANDBOX=OFF
make -j$(nproc) libcef_dll_wrapper

# Copy resources into Release/ (CEF resolves relative to libcef.so)
cp "$CEF_DIR/Resources/icudtl.dat" "$CEF_DIR/Release/"
cp "$CEF_DIR/Resources/chrome_100_percent.pak" "$CEF_DIR/Release/"
cp "$CEF_DIR/Resources/chrome_200_percent.pak" "$CEF_DIR/Release/"
cp "$CEF_DIR/Resources/resources.pak" "$CEF_DIR/Release/"
cp -r "$CEF_DIR/Resources/locales" "$CEF_DIR/Release/"

echo "[cef] CEF ready at $CEF_DIR (minimal — no H.264, no PipeWire)"
