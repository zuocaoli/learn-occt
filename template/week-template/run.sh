#!/usr/bin/env bash
set -euo pipefail
course_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd -- "$course_dir"
cmake --preset default
cmake --build --preset default
exec "$course_dir/build/demo" "$@"
