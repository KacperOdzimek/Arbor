#!/usr/bin/env bash
set -e

# Usage:
#   ./run.sh <example_name>
#
# Example:
#   ./run.sh triangle
#
# Directory layout:
#   run.sh
#   engine/
#     main.c
#     thirdparty/
#       glad.c
#   triangle.c
#   cube.c

if [ $# -ne 1 ]; then
    echo "Usage: $0 <example_name>"
    exit 1
fi

EXAMPLE="$1"
ROOT="$(cd "$(dirname "$0")" && pwd)"

SOURCE="$ROOT/${EXAMPLE}.c"
OUTPUT="$ROOT/build/$EXAMPLE"

if [ ! -f "$SOURCE" ]; then
    echo "Error: example not found: ${EXAMPLE}.c"
    exit 1
fi

mkdir -p "$ROOT/build"

echo "Building $EXAMPLE..."

gcc \
    "-I../include" \
    "$ROOT/engine/main.c" \
    "$ROOT/engine/thirdparty/glad.c" \
    "$SOURCE" \
    -o "$OUTPUT" \
    -lglfw \
    -lGL \
    -lm

echo "Running $EXAMPLE..."
echo

exec "$OUTPUT"
