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
cp "$src/README.md" "$src/LICENSE" "$src/NOTICE" "$src/PORTING-TO-WINDOWS.md" "$src/scripts/HOW-TO-CHECK.txt" "$stage/"

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

# ENH Master 2D: the same plugin with the flat, no-OpenGL editor (for older graphics), its own zip
bundle2d="$build/EnhMaster2D_artefacts/Release/VST3/ENH Master 2D.vst3"
if [ -d "$bundle2d" ]; then
  name2="ENH-Master-2D-${version}-linux-x64"
  stage2="$dist/$name2"
  rm -rf "$stage2"; mkdir -p "$stage2"
  cp -r "$bundle2d" "$stage2/"
  find "$stage2" -name '*.so' -exec strip --strip-unneeded {} +
  cp "$src/README.md" "$src/LICENSE" "$src/NOTICE" "$src/scripts/HOW-TO-CHECK.txt" "$stage2/"
  cat > "$stage2/INSTALL.txt" <<'EOF2'
ENH Master 2D - installation (Linux, VST3)
==========================================

The same plugin as ENH Master - the same sound, presets and settings - with a flat 2D rack instead of
the 3D one: every unit's faceplate, one under another (the wheel or a drag slides the rack up and down).
No OpenGL: for older or weaker graphics. It installs alongside ENH Master (its own plugin).

    mkdir -p ~/.vst3
    cp -r "ENH Master 2D.vst3" ~/.vst3/

Then rescan plugins in your host.
EOF2
  ( cd "$stage2" && find . -type f ! -name CHECKSUMS.txt -print0 | sort -z | xargs -0 sha256sum > CHECKSUMS.txt )
  ( cd "$dist" && zip -qr "$name2.zip" "$name2" )
  rm -rf "$stage2"
  echo "$dist/$name2.zip"
fi
