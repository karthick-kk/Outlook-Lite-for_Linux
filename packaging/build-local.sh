#!/bin/bash
# Build OFL packages locally using cargo tauri build
set -euo pipefail

cd "$(dirname "$0")/.."

FORMAT="${1:-deb}"

case "$FORMAT" in
    deb)
        echo "Building .deb package..."
        cargo tauri build --bundles deb
        echo ""
        echo "Package built at: src-tauri/target/release/bundle/deb/"
        ls -lh src-tauri/target/release/bundle/deb/*.deb 2>/dev/null || true
        ;;
    rpm)
        echo "Building .rpm package..."
        cargo tauri build --bundles rpm
        echo ""
        echo "Package built at: src-tauri/target/release/bundle/rpm/"
        ls -lh src-tauri/target/release/bundle/rpm/*.rpm 2>/dev/null || true
        ;;
    arch)
        echo "Building Arch package with makepkg..."
        cd packaging
        makepkg -sf --noconfirm
        echo ""
        ls -lh *.pkg.tar.zst 2>/dev/null || true
        ;;
    binary)
        echo "Building release binary..."
        cargo build --release --manifest-path src-tauri/Cargo.toml
        echo ""
        echo "Binary at: src-tauri/target/release/ofl"
        ls -lh src-tauri/target/release/ofl
        ;;
    all)
        "$0" deb
        "$0" rpm
        "$0" binary
        ;;
    *)
        echo "Usage: $0 [deb|rpm|arch|binary|all]"
        echo "  deb    — Build .deb package (default)"
        echo "  rpm    — Build .rpm package"
        echo "  arch   — Build .pkg.tar.zst via makepkg"
        echo "  binary — Build release binary only"
        echo "  all    — Build deb, rpm, and binary"
        exit 1
        ;;
esac
