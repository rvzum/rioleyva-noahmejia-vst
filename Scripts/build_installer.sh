#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# RIO LEYVA x NOAH MEJIA VST -- macOS installer builder.
#
# Builds a Release, universal (Intel + Apple Silicon) VST3 in a SEPARATE
# build directory (build-release/ -- your normal dev build/ directory is
# left untouched), then packages it into a .pkg installer that places the
# plugin at /Library/Audio/Plug-Ins/VST3/ for every user on the Mac it's
# run on.
#
# By default it ALSO bundles the sample banks (~/Music/27wav rioleyva &
# noahmejia banks/ on THIS Mac, i.e. wherever this script is actually run --
# see SAMPLES_SOURCE_DIR below) into the same installer, as a second,
# user-uncheckable-but-on-by-default component that installs into the
# INSTALLING user's own ~/Music/27wav rioleyva & noahmejia banks/ -- exactly
# where SampleLibraryManager::getRuntimeLibraryRoot() looks for it at
# runtime -- so a fresh install on someone else's Mac has sounds to play
# immediately, no separate file transfer needed. This is what makes the
# installer large (the sample banks currently run ~2 GB); pass --no-samples
# (or SKIP_SAMPLES=1) for a fast plugin-only build while iterating on the
# UI, same as before this feature existed.
#
# IMPORTANT: this .pkg is NOT signed or notarized (no Apple Developer ID
# configured). People you send it to will see a Gatekeeper warning the
# first time they try to open it -- this is expected, not a bug. See
# Packaging/README.md for exactly what to tell them.
#
# Run this from Terminal.app on your Mac (NOT through any remote/bridge
# shell) -- pkgbuild/productbuild only exist on real macOS:
#   chmod +x Scripts/build_installer.sh   # first time only
#   ./Scripts/build_installer.sh                 # plugin + sample banks
#   ./Scripts/build_installer.sh --no-samples     # plugin only, fast
#   W27_SAMPLES_DIR=/some/other/folder ./Scripts/build_installer.sh
#
# Output: Packaging/build/RIO LEYVA x NOAH MEJIA VST Installer.pkg
# ==============================================================================

INCLUDE_SAMPLES=1
for arg in "$@"; do
    case "$arg" in
        --no-samples) INCLUDE_SAMPLES=0 ;;
        *) echo "Unknown argument: $arg" >&2; exit 1 ;;
    esac
done
if [ "${SKIP_SAMPLES:-0}" = "1" ]; then
    INCLUDE_SAMPLES=0
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

PRODUCT_NAME="RIO LEYVA x NOAH MEJIA VST"
BUNDLE_ID="com.w27wav.rioleyva"
COMPONENT_PKG_ID="${BUNDLE_ID}.pkg"
SAMPLES_PKG_ID="${BUNDLE_ID}.samples.pkg"
INSTALL_LOCATION="/Library/Audio/Plug-Ins/VST3"

# SOURCE: read from your own curated master copy by default (same folder
# SampleLibraryManager::getRuntimeLibraryRoot() reads from on THIS Mac).
SAMPLES_FOLDER_NAME="27wav rioleyva & noahmejia banks"
SAMPLES_SOURCE_DIR="${W27_SAMPLES_DIR:-$HOME/Music/${SAMPLES_FOLDER_NAME}}"

# DESTINATION on the machine being installed to: a fixed, machine-wide
# location, NOT ~/Music -- must match
# SampleLibraryManager::getBundledLibraryRoot() exactly. Deliberately not
# the same path as SAMPLES_SOURCE_DIR above -- see that method's doc
# comment for why a per-user path doesn't work well from an installer.
SAMPLES_INSTALL_LOCATION="/Library/Application Support/27wav/Sample Banks"

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

# --- 0. Decide whether the sample banks are actually going in ---------------
if [ "${INCLUDE_SAMPLES}" = "1" ]; then
    if [ ! -d "${SAMPLES_SOURCE_DIR}" ] || [ -z "$(find "${SAMPLES_SOURCE_DIR}" -mindepth 1 -maxdepth 1 2>/dev/null)" ]; then
        echo "==> No sample banks found at: ${SAMPLES_SOURCE_DIR}"
        echo "    Building a plugin-only installer instead (pass --no-samples to silence this)."
        INCLUDE_SAMPLES=0
    else
        echo "==> Bundling sample banks from: ${SAMPLES_SOURCE_DIR}"
    fi
else
    echo "==> --no-samples / SKIP_SAMPLES set -- building a plugin-only installer."
fi

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

# --- 2. Stage the VST3 payload ------------------------------------------------
PACKAGING_DIR="Packaging"
STAGING_DIR="${PACKAGING_DIR}/build/staging"
PKG_OUTPUT_DIR="${PACKAGING_DIR}/build"
COMPONENT_PKG_NAME="27wavVST-Component.pkg"
COMPONENT_PKG="${PKG_OUTPUT_DIR}/${COMPONENT_PKG_NAME}"
SAMPLES_PKG_NAME="27wavVST-Samples-Component.pkg"
SAMPLES_PKG="${PKG_OUTPUT_DIR}/${SAMPLES_PKG_NAME}"
FINAL_PKG_BASENAME="${PRODUCT_NAME} Installer"
if [ "${INCLUDE_SAMPLES}" = "0" ]; then
    FINAL_PKG_BASENAME="${PRODUCT_NAME} Installer (no samples)"
fi
FINAL_PKG="${PKG_OUTPUT_DIR}/${FINAL_PKG_BASENAME}.pkg"
DISTRIBUTION_XML_RENDERED="${PKG_OUTPUT_DIR}/Distribution.xml"

echo "==> Staging plugin payload..."
rm -rf "${STAGING_DIR}"
mkdir -p "${STAGING_DIR}"
cp -R "${ARTEFACT_VST3}" "${STAGING_DIR}/"

mkdir -p "${PKG_OUTPUT_DIR}"

# --- 3. Component package (pkgbuild) -----------------------------------------
echo "==> Building plugin component package (pkgbuild)..."
pkgbuild \
    --root "${STAGING_DIR}" \
    --identifier "${COMPONENT_PKG_ID}" \
    --version "${VERSION}" \
    --install-location "${INSTALL_LOCATION}" \
    "${COMPONENT_PKG}"

SAMPLES_SIZE_HUMAN=""
if [ "${INCLUDE_SAMPLES}" = "1" ]; then
    echo "==> Staging sample-bank payload (this copies a few GB -- may take a minute)..."
    STAGING_SAMPLES_DIR="${PACKAGING_DIR}/build/staging-samples"
    rm -rf "${STAGING_SAMPLES_DIR}"
    mkdir -p "${STAGING_SAMPLES_DIR}"
    # Trailing slash on the source + "." copies CONTENTS, not the folder
    # itself -- pkgbuild's --root works the same way relative to
    # --install-location, exactly like the plugin component above (its
    # staging dir contains "ProductName.vst3" directly, not a wrapper
    # folder).
    cp -R "${SAMPLES_SOURCE_DIR}/." "${STAGING_SAMPLES_DIR}/"
    find "${STAGING_SAMPLES_DIR}" -name ".DS_Store" -delete

    SAMPLES_SIZE_HUMAN="$(du -sh "${STAGING_SAMPLES_DIR}" | awk '{print $1}')"
    echo "==> Building sample-banks component package (pkgbuild, ~${SAMPLES_SIZE_HUMAN})..."
    pkgbuild \
        --root "${STAGING_SAMPLES_DIR}" \
        --identifier "${SAMPLES_PKG_ID}" \
        --version "${VERSION}" \
        --install-location "${SAMPLES_INSTALL_LOCATION}" \
        "${SAMPLES_PKG}"
fi

# --- 4. Final distribution installer (productbuild) --------------------------
if [ "${INCLUDE_SAMPLES}" = "1" ]; then
    DISTRIBUTION_TEMPLATE="${PACKAGING_DIR}/Distribution.xml"
else
    DISTRIBUTION_TEMPLATE="${PACKAGING_DIR}/Distribution-plugin-only.xml"
fi

echo "==> Rendering ${DISTRIBUTION_TEMPLATE} (version ${VERSION})..."
sed \
    -e "s/__VERSION__/${VERSION}/g" \
    -e "s/__COMPONENT_PKG_ID__/${COMPONENT_PKG_ID}/g" \
    -e "s/__COMPONENT_PKG_NAME__/${COMPONENT_PKG_NAME}/g" \
    -e "s/__SAMPLES_PKG_ID__/${SAMPLES_PKG_ID}/g" \
    -e "s/__SAMPLES_PKG_NAME__/${SAMPLES_PKG_NAME}/g" \
    -e "s/__SAMPLES_SIZE__/${SAMPLES_SIZE_HUMAN}/g" \
    "${DISTRIBUTION_TEMPLATE}" > "${DISTRIBUTION_XML_RENDERED}"

echo "==> Building final installer (productbuild)..."
productbuild \
    --distribution "${DISTRIBUTION_XML_RENDERED}" \
    --resources "${PACKAGING_DIR}/Resources" \
    --package-path "${PKG_OUTPUT_DIR}" \
    "${FINAL_PKG}"

rm -f "${DISTRIBUTION_XML_RENDERED}" "${COMPONENT_PKG}" "${SAMPLES_PKG}"
rm -rf "${STAGING_DIR}" "${PACKAGING_DIR}/build/staging-samples"

echo ""
echo "=================================================================="
echo " Done: ${FINAL_PKG}"
echo " $(du -h "${FINAL_PKG}" | awk '{print $1}') total"
if [ "${INCLUDE_SAMPLES}" = "1" ]; then
    echo " Includes the sample banks (~${SAMPLES_SIZE_HUMAN}) as an optional,"
    echo " on-by-default component -- installs machine-wide to:"
    echo "   ${SAMPLES_INSTALL_LOCATION}"
    echo " (the plugin also still reads each user's own ~/Music/${SAMPLES_FOLDER_NAME}/,"
    echo " unchanged -- see SampleLibraryManager.h)."
else
    echo " Plugin only -- no sample banks (--no-samples / SKIP_SAMPLES)."
fi
echo ""
echo " This installer is UNSIGNED (no Apple Developer ID). Anyone you"
echo " send it to will need to bypass a one-time Gatekeeper warning --"
echo " see Packaging/README.md for exactly what to tell them."
echo "=================================================================="
