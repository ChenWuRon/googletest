#!/usr/bin/env bash
set -euo pipefail

find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i --dry-run --Werror
echo "Lint OK"
