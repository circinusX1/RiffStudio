QT += core gui widgets multimedia network openglwidgets
CONFIG += c++17

TARGET = WebRiffStudio
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
#                     python3 python3-pip

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
    # Include paths for Essentia and Eigen3
    INCLUDEPATH += /usr/local/include \
                   /usr/include/eigen3
    INCLUDEPATH += /usr/include/soundtouch

    LIBS += -L/usr/local/lib -lSoundTouch -lessentia -lfftw3 -lyaml-cpp
}

win32 {
    # Windows (vcpkg)
    INCLUDEPATH += C:/vcpkg/installed/x64-windows/include \
                   C:/vcpkg/installed/x64-windows/include/eigen3
    INCLUDEPATH += C:/vcpkg/installed/x64-windows/include/soundtouch

    LIBS += -LC:/vcpkg/installed/x64-windows/lib \
            --lSoundTouch -lessentia -lfftw3f-3 -lyaml-cpp
            #dpkg -L libsoundtouch-dev | grep -E '\.so|\.a'
}

# WINDOWS
# git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
# cd C:\vcpkg
# .\bootstrap-vcpkg.bat
#
# .\vcpkg install fftw3:x64-windows yaml-cpp:x64-windows essentia:x64-windows
# .\vcpkg install soundtouch:x64-windows

