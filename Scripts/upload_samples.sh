#!/usr/bin/env bash
# Zips each sample-pack folder and uploads it as an asset on a "samples"
# GitHub Release, so the GitHub Actions workflow
# (.github/workflows/build-installers.yml) can download them and bundle
# them into BOTH the Windows and macOS installers automatically.
#
# Run this ONCE after first pushing this repo to GitHub, and again any
# time you add, remove, or change a sample pack under
# ~/Music/27wav rioleyva & noahmejia banks/ -- then push to main (or
# re-run the "Build installers" workflow from the Actions tab) so the
# next installer build picks up the change.
#
# Requires the GitHub CLI ("gh"):
#   brew install gh
#   gh auth login        # once, if you haven't already
#
# Must be run on your own Mac, from inside this repo (not through the
# remote-devices bridge -- that VM has no working gh/network for this).
#
# Usage:
#   ./Scripts/upload_samples.sh
#
# Override the source folder (defaults to the same folder the plugin
# itself reads from, and the same one Scripts/build_installer.sh reads
# for local mac builds):
#   W27_SAMPLES_DIR="/path/to/banks" ./Scripts/upload_samples.sh

set -euo pipefail

SAMPLES_FOLDER_NAME="27wav rioleyva & noahmejia banks"
SAMPLES_SOURCE_DIR="${W27_SAMPLES_DIR:-$HOME/Music/${SAMPLES_FOLDER_NAME}}"

if [ ! -d "$SAMPLES_SOURCE_DIR" ]; then
    echo "error: samples folder not found: $SAMPLES_SOURCE_DIR" >&2
    exit 1
fi

if ! command -v gh >/dev/null 2>&1; then
    echo "error: GitHub CLI ('gh') not found. Install it with: brew install gh" >&2
    exit 1
fi
if ! gh auth status >/dev/null 2>&1; then
    echo "error: not logged in to GitHub CLI. Run: gh auth login" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "Zipping sample packs from: $SAMPLES_SOURCE_DIR"
ZIP_FILES=()
for pack_dir in "$SAMPLES_SOURCE_DIR"/*/; do
    [ -d "$pack_dir" ] || continue
    pack_name="$(basename "$pack_dir")"
    zip_path="${WORK_DIR}/${pack_name}.zip"
    echo "  - ${pack_name}"
    ( cd "$SAMPLES_SOURCE_DIR" && zip -qr -X "$zip_path" "$pack_name" -x "*.DS_Store" )
    ZIP_FILES+=("$zip_path")
done

if [ ${#ZIP_FILES[@]} -eq 0 ]; then
    echo "error: no sample-pack folders found under $SAMPLES_SOURCE_DIR" >&2
    exit 1
fi

echo
echo "Total upload size: $(du -ch "${ZIP_FILES[@]}" | tail -1 | awk '{print $1}')"
echo

if gh release view samples >/dev/null 2>&1; then
    echo "Updating existing 'samples' release..."
    gh release upload samples "${ZIP_FILES[@]}" --clobber
else
    echo "Creating 'samples' release..."
    gh release create samples "${ZIP_FILES[@]}" \
        --title "Sample banks (build assets -- not for direct download)" \
        --notes "Sample-bank zips consumed by .github/workflows/build-installers.yml. Not meant for people to download by hand -- grab the actual plugin installer from the 'latest' release instead." \
        --latest=false
fi

echo
echo "Done. Push to main (or re-run the 'Build installers' workflow from the Actions tab) to bake these into both installers."
