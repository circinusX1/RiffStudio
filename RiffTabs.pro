QT += core gui widgets multimedia network

# Qt 6 moved QOpenGLWidget into its own module
greaterThan(QT_MAJOR_VERSION, 5) {
    QT += openglwidgets
} else {
    QT += opengl
}

CONFIG += c++17

TARGET = RiffTabs
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    fretboardwidget.cpp \
    glwaveformwidget.cpp

HEADERS += \
    glwaveformwidget.h \
    mainwindow.h \
    fretboardwidget.h \
    knobwidget.h

#
# sudo apt update
# sudo apt install -y build-essential cmake pkg-config git \
#                     libfftw3-dev libyaml-cpp-dev libtag1-dev \
#                     libsamplerate0-dev libavcodec-dev libavformat-dev \
#                     python3 python3-pip  libeigen3-dev
# sudo apt update
#  sudo apt install libsoundtouch-dev
#   for: essentia.h
# 1. Clean previous build artifacts
#   python3 waf clean
#
#   # 2. Configure while ignoring FFmpeg-dependent AudioLoader algorithms
#   python3 waf configure --mode=release --ignore-algos=AudioLoader,MonoLoader
#
#   # 3. Build Essentia
#   python3 waf
#
#   # 4. Install system-wide
#   sudo python3 waf install
#   sudo apt install -y libeigen3-dev
#
#   sudo apt update
#   sudo apt install -y libsoundtouch-dev
#

unix:!macx {
    # Include paths for Eigen3, SoundTouch, and Vamp Host SDK
    INCLUDEPATH += /usr/local/include \
                   /usr/include/eigen3
    INCLUDEPATH += /usr/include/soundtouch \
                   /usr/include/vamp-hostsdk

    # Removed -lessentia, added -lvamp-hostsdk
    LIBS += -L/usr/local/lib -lSoundTouch -lvamp-hostsdk -lfftw3 -lyaml-cpp
}

win32 {
    # Windows (vcpkg)
    INCLUDEPATH += C:/vcpkg/installed/x64-windows/include \
                   C:/vcpkg/installed/x64-windows/include/eigen3
    INCLUDEPATH += C:/vcpkg/installed/x64-windows/include/soundtouch

    LIBS += -LC:/vcpkg/installed/x64-windows/lib \
            --lSoundTouch -lfftw3f-3 -lyaml-cpp
            #dpkg -L libsoundtouch-dev | grep -E '\.so|\.a'
}

# WINDOWS
# git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
# cd C:\vcpkg
# .\bootstrap-vcpkg.bat
# sudo apt install libvamp-hostsdk1v5 vamp-plugin-sdk
# .\vcpkg install fftw3:x64-windows yaml-cpp:x64-windows essentia:x64-windows
# .\vcpkg install soundtouch:x64-windows


# Copy the binary to ./bin after a successful build
BINDIR = $$PWD/bin

unix:!macx {
    QMAKE_POST_LINK += mkdir -p $$BINDIR && cp -f $$OUT_PWD/$$TARGET $$BINDIR/
}

win32 {
    QMAKE_POST_LINK += $$quote(if not exist $$shell_path($$BINDIR) mkdir $$shell_path($$BINDIR) && copy /Y $$shell_path($$OUT_PWD/$$TARGET.exe) $$shell_path($$BINDIR/))
}

DISTFILES += \
    bin/run.sh

