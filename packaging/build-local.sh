#!/bin/bash
# Run GitHub Actions workflows locally using nektos/act
# Generates .deb, .rpm, .pkg.tar.zst, and binary tar.gz packages under build/
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build/packages"

mkdir -p "$BUILD_DIR"

# Which job to run (default: all)
JOB="${1:-all}"

run_job() {
    local job_name="$1"
    echo "=== Building ${job_name} ==="
    cd "$PROJECT_DIR"
    act workflow_dispatch \
        -j "$job_name" \
        --artifact-server-path "$BUILD_DIR" \
        -W .github/workflows/build.yml \
        -P ubuntu-24.04=catthehacker/ubuntu:act-22.04 \
        --env APP_VERSION=1.0.0 \
        --env CEF_DIR=/tmp/cef
}

case "$JOB" in
    deb)
        run_job build-deb
        ;;
    rpm)
        run_job build-rpm
        ;;
    arch)
        run_job build-arch
        ;;
    binary)
        run_job build-binary
        ;;
    all)
        run_job build-deb
        run_job build-rpm
        run_job build-arch
        run_job build-binary
        ;;
    *)
        echo "Usage: $0 [deb|rpm|arch|binary|all]"
        echo "  deb    — Build .deb package (Ubuntu/Debian)"
        echo "  rpm    — Build .rpm package (Fedora/RHEL)"
        echo "  arch   — Build .pkg.tar.zst package (Arch Linux)"
        echo "  binary — Build binary tar.gz"
        echo "  all    — Build all packages (default)"
        exit 1
        ;;
esac

# Extract artifacts from act's zip files
echo ""
echo "=== Extracting artifacts ==="
find "$BUILD_DIR" -name "*.zip" -exec unzip -o -d "$BUILD_DIR" {} \;
# Clean up zip dirs
rm -rf "$BUILD_DIR"/[0-9]*

echo ""
echo "=== Packages ==="
find "$BUILD_DIR" -type f \( -name "*.deb" -o -name "*.rpm" -o -name "*.pkg.tar.zst" -o -name "*.tar.gz" \) -exec ls -lh {} \;
echo ""
echo "Output directory: $BUILD_DIR"
