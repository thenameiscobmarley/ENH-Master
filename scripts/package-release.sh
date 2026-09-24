#!/usr/bin/env bash
# Packages a built plugin into a release zip.
#   package-release.sh <version> [build dir] [source dir] [output dir]
# Used by .github/workflows/release.yml and by hand for local releases.
set -euo pipefail

version=${1:?usage: package-release.sh <version> [build] [source] [dist]}
build=${2:-build}
src=${3:-.}
dist=${4:-dist}

bundle="$build/EnhMaster_artefacts/Release/VST3/ENH Master.vst3"
[ -d "$bundle" ] || { echo "no VST3 bundle at $bundle - build first" >&2; exit 1; }

name="ENH-Master-${version}-linux-x64"
stage="$dist/$name"
rm -rf "$stage"
mkdir -p "$stage"

cp -r "$bundle" "$stage/"
# Ship a stripped binary: same plugin, a fraction of the download
find "$stage" -name '*.so' -exec strip --strip-unneeded {} +
cp "$src/README.md" "$src/LICENSE" "$src/NOTICE" "$src/PORTING-TO-WINDOWS.md" "$stage/"

cat > "$stage/INSTALL.txt" <<'EOF'
ENH Master - installation (Linux, VST3)
=======================================

1. Copy the bundle into your VST3 folder:

       mkdir -p ~/.vst3
       cp -r "ENH Master.vst3" ~/.vst3/

   (System-wide instead: /usr/lib/vst3 or /usr/local/lib/vst3.)

2. Rescan plugins in your host. In Carla: Add Plugin -> Refresh -> VST3.

Requirements: a 64-bit Linux system with X11 and OpenGL 3.2. The UI renders on
the GPU; on very old hardware, reduce the editor size.

Not working? Run the host from a terminal and look for "ENH Master" messages.
Windows/macOS are not supported yet - see PORTING-TO-WINDOWS.md, which includes
a prompt for having a coding AI do the port.

Documentation lives in the repository's Vault/ folder (an Obsidian vault):
https://github.com/thenameiscobmarley/ENH-Master
EOF

# Fingerprints of everything inside, to check a copy against (sha256sum -c CHECKSUMS.txt)
( cd "$stage" && find . -type f ! -name CHECKSUMS.txt -print0 | sort -z | xargs -0 sha256sum > CHECKSUMS.txt )

mkdir -p "$dist"
( cd "$dist" && zip -qr "$name.zip" "$name" )
rm -rf "$stage"
echo "$dist/$name.zip"
