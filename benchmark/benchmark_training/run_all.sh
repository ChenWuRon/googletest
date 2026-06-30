#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD="$DIR/build"

echo "=== Building all ==="
cmake -S "$DIR" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -1
cmake --build "$BUILD" 2>&1 | tail -1

echo ""
echo "=== Running all benchmarks ==="
for f in "$BUILD"/0*; do
    name=$(basename "$f")
    echo ""
    echo ">>>> $name <<<<"
    "$f" 2>&1 | grep -v "^$" | grep -v "WARNING"
done
