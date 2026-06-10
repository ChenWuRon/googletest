#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"

cmake -B "$BUILD_DIR" -DBUILD_TESTS=ON
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
