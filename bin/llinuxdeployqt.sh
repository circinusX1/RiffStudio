#!/bin/bash
set -e

export LD_LIBRARY_PATH="/home/marius/Qt/6.7.3/gcc_64/lib:$LD_LIBRARY_PATH"
export PATH="/home/marius/Qt/6.7.3/gcc_64/bin:$PATH"

rm -rf AppDir
mkdir -p AppDir/usr/bin
cp WebRiffStudio AppDir/usr/bin/
cp ./WebRiffStudio.png AppDir/
cp -r ./plugins AppDir/

# 1. Copy your startup script to the root of AppDir as 'AppRun'
cp WebRiffStudio.sh AppDir/AppRun

# 2. Make AppRun executable so the system can run it
chmod +x AppDir/AppRun

cat > AppDir/WebRiffStudio.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=WebRiffStudio
Exec=WebRiffStudio
Icon=WebRiffStudio
Categories=AudioVideo;Audio;
EOF

export LD_LIBRARY_PATH="/home/marius/Qt/6.7.3/gcc_64/lib:$LD_LIBRARY_PATH"
export PATH="/home/marius/Qt/6.7.3/gcc_64/bin:$PATH"

linuxdeployqt AppDir/usr/bin/WebRiffStudio \
    -bundle-non-qt-libs \
    -no-translations \
    -qmake=/home/marius/Qt/6.7.3/gcc_64/bin/qmake \
    -verbose=2

appimagetool AppDir WebRiffStudio.AppImage

echo "Done: WebRiffStudio.AppImage"

