#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
export VAMP_PATH="$DIR/plugins"
# Optional: also keep user plugins
# export VAMP_PATH="$DIR/plugins:$HOME/.vamp"q
exec "$DIR/WebRiffStudio" "$@"
