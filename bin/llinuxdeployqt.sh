#!/bin/bash
set -e

export LD_LIBRARY_PATH="${QT_PATH_AND_VER}/gcc_64/lib:$LD_LIBRARY_PATH"
export PATH="${QT_PATH_AND_VER}/gcc_64/bin:$PATH"

rm -rf AppDir
mkdir -p AppDir/usr/bin
cp RiffTabs AppDir/usr/bin/
cp ./RiffTabs.png AppDir/
cp -r ./plugins AppDir/

# 1. Copy your startup script to the root of AppDir as 'AppRun'
cp RiffTabs.sh AppDir/AppRun

# 2. Make AppRun executable so the system can run it
chmod +x AppDir/AppRun

cat > AppDir/RiffTabs.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=RiffTabs
Exec=RiffTabs
Icon=RiffTabs
Categories=AudioVideo;Audio;
EOF

export LD_LIBRARY_PATH="${QT_PATH_AND_VER}/gcc_64/lib:$LD_LIBRARY_PATH"
export PATH="${QT_PATH_AND_VER}/gcc_64/bin:$PATH"

linuxdeployqt AppDir/usr/bin/RiffTabs \
    -bundle-non-qt-libs \
    -no-translations \
    -qmake=${QT_PATH_AND_VER}/gcc_64/bin/qmake \
    -verbose=2

appimagetool AppDir RiffTabs.AppImage

echo "Done: RiffTabs.AppImage"

