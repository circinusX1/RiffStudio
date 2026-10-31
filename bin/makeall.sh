#!/bin/bash

[[ -z ${QT_PATH_AND_VER} ]] && echo "Please export QT_PATH_AND_VER=Path/To/Qt/x.y.z" && exit 1


# sudo mv linuxdeployqt-continuous-x86_64.AppImage /usr/local/bin/linux  @ https://github.com/probonopd/linuxdeployqt/releases
# sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool @ https://github.com/AppImage/appimagetool/releases
# sudo chmod +x /usr/local/bin/appimagetool
# sudo chmod +x /usr/local/bin/linuxdeployqt


./qqmake.sh ../RiffTabs.pro
make
./llinuxdeployqt.sh

