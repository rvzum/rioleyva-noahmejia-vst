#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# 27wav rioleyva VST -- macOS installer builder.
#
# Builds a Release, universal (Intel + Apple Silicon) VST3 in a SEPARATE
# build directory (build-release/ -- your normal dev build/ directory is
# left untouched), then packages it into a .pkg installer that places the
# plugin at /Library/Audio/Plug-Ins/VST3/ for every user on the Mac it's
# run on.
#
# IMPORTANT: this .pkg is NOT signed or notarized (no Apple Developer ID
# configured). People you send it to will see a Gatekeeper warning the
# first time they try to open it -- this is expected, not a bug. See
# Packaging/README.md for exactly what to tell them.
#
# Run this from Terminal.app on your Mac (NOT through any remote/bridge
# shell) -- pkgbuild/productbuild only exist on real macOS:
#   chmod +x Scripts/build_installer.sh   # first time only
#   ./Scripts/build_installer.sh
#
# Output: Packaging/build/27wav rioleyva VST Installer.pkg
# ==============================================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

PRODUCT_NAME="RIO LEYVA x NOAH MEJIA VST"
BUNDLE_ID="com.w27wav.rioleyva"
COMPONENT_PKG_ID="${BUNDLE_ID}.pkg"
INSTALL_LOCATION="/Library/Audio/Plug-Ins/VST3"

VERSION="$(grep -m1 'project(27wavVST VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')"
if [ -z "${VERSION}" ]; then
    echo "ERROR: could not read the project version from CMakeLists.txt" >&2
    exit 1
fi
echo "==> Packaging ${PRODUCT_NAME} version ${VERSION}"

for tool in pkgbuild productbuild; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "ERROR: '${tool}' not found. It ships with macOS itself (Installer framework)," >&2
        echo "       normally available once Xcode Command Line Tools are installed" >&2
        echo "       ('xcode-select --install'). Since this project already builds with" >&2
        echo "       CMake + AppleClang, the CLT are almost certainly already present --" >&2
        echo "       double-check you're running this in a normal Terminal.app on your" >&2
        echo "       Mac, not inside any remote/bridge/VM shell." >&2
        exit 1
    fi
done

# --- 1. Build a Release, universal VST3 in its own build directory ----------
# A separate directory (not the usual build/) because this project uses the
# Unix Makefiles generator, which is single-config: reusing build/ here
# would force it from your normal dev config to Release and back every time
# you switch between developing and packaging. This way both stay intact.
RELEASE_BUILD_DIR="build-release"

echo "==> Configuring Release build in ${RELEASE_BUILD_DIR}/ ..."
cmake -B "${RELEASE_BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "==> Building ${PRODUCT_NAME} (VST3 only) -- this can take a few minutes..."
cmake --build "${RELEASE_BUILD_DIR}" --config Release --target 27wavVST_VST3

# Locate the built .vst3 by searching rather than hardcoding the path: JUCE's
# CMake support nests it under a "<Config>/" subfolder when CMAKE_BUILD_TYPE
# is explicitly set (e.g. "27wavVST_artefacts/Release/VST3/...") but NOT when
# it's left empty, as this project's normal dev build/ directory does (plain
# "27wavVST_artefacts/VST3/..." there) -- a hardcoded path guess bit us here
# (this exact bug: the script looked in the wrong place and exited before
# ever reaching pkgbuild/productbuild). Searching avoids relying on that
# detail at all.
ARTEFACT_VST3="$(find "${RELEASE_BUILD_DIR}/27wavVST_artefacts" -type d -name "${PRODUCT_NAME}.vst3" -print -quit 2>/dev/null)"
if [ -z "${ARTEFACT_VST3}" ] || [ ! -d "${ARTEFACT_VST3}" ]; then
    echo "ERROR: could not find a built '${PRODUCT_NAME}.vst3' anywhere under:" >&2
    echo "         ${RELEASE_BUILD_DIR}/27wavVST_artefacts" >&2
    echo "       Check the cmake --build output above for errors." >&2
    exit 1
fi
echo "==> Found built plugin: ${ARTEFACT_VST3}"

# --- 2. Stage the payload -----------------------------------------------------
PACKAGING_DIR="Packaging"
STAGING_DIR="${PACKAGING_DIR}/build/staging"
PKG_OUTPUT_DIR="${PACKAGING_DIR}/build"
COMPONENT_PKG_NAME="27wavVST-Component.pkg"
COMPONENT_PKG="${PKG_OUTPUT_DIR}/${COMPONENT_PKG_NAME}"
FINAL_PKG="${PKG_OUTPUT_DIR}/${PRODUCT_NAME} Installer.pkg"
DISTRIBUTION_XML_RENDERED="${PKG_OUTPUT_DIR}/Distribution.xml"

echo "==> Staging payload..."
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}"
cp -R "${ARTEFACT_VST3}" "${STAGING_DIR}/"

mkdir -p "${PKG_OUTPUT_DIR}"

# --- 3. Component package (pkgbuild) -----------------------------------------
echo "==> Building component package (pkgbuild)..."
pkgbuild \
    --root "${STAGING_DIR}" \
    --identifier "${COMPONENT_PKG_ID}" \
    --version "${VERSION}" \
    --install-location "${INSTALL_LOCATION}" \
    "${COMPONENT_PKG}"

# --- 4. Final distribution installer (productbuild) --------------------------
echo "==> Rendering Distribution.xml (version ${VERSION})..."
sed "s/__VERSION__/${VERSION}/g; s/__COMPONENT_PKG_ID__/${COMPONENT_PKG_ID}/g; s/__COMPONENT_PKG_NAME__/${COMPONENT_PKG_NAME}/g" \
    "${PACKAGING_DIR}/Distribution.xml" > "${DISTRIBUTION_XML_RENDERED}"

echo "==> Building final installer (productbuild)..."
productbuild \
    --distribution "${DISTRIBUTION_XML_RENDERED}" \
    --resources "${PACKAGING_DIR}/Resources" \
    --package-path "${PKG_OUTPUT_DIR}" \
    "${FINAL_PKG}"

rm -f "${DISTRIBUTION_XML_RENDERED}" "${COMPONENT_PKG}"
rm -rf "${STAGING_DIR}"

echo ""
echo "=================================================================="
echo " Done: ${FINAL_PKG}"
echo ""
echo " This installer is UNSIGNED (no Apple Developer ID). Anyone you"
echo " send it to will need to bypass a one-time Gatekeeper warning --"
echo " see Packaging/README.md for exactly what to tell them."
echo "=================================================================="
