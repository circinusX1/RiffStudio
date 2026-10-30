#!/bin/bash
# qqmake.bat — wrapper that forces Qt 6.7.3 for qmake
# Usage: ./qqmake.bat [qmake arguments]
# Example: ./qqmake.bat ../WebRiffStudio.pro

QT_ROOT="${HOME}/Qt/6.7.3/gcc_64"

# Make sure the Qt 6.7.3 tools and libraries are used
export PATH="$QT_ROOT/bin:$PATH"
export LD_LIBRARY_PATH="$QT_ROOT/lib:${LD_LIBRARY_PATH}"
export QTDIR="$QT_ROOT"
export QMAKE="$QT_ROOT/bin/qmake"

# Optional but useful so qmake finds the correct mkspecs
export QMAKESPEC="$QT_ROOT/mkspecs/linux-g++"

# Run the real qmake with all arguments that were passed to this script
exec "$QMAKE" "$@"

