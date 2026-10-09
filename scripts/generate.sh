#!/usr/bin/env bash
# Regenerates every page under docs/reference from EposLib's sources.
#
#   scripts/generate.sh [path/to/EposLib] [path/to/ros2units]
#
# The error codes and hardware names come from the library's own tables,
# through a small program built in a container (IMAGE, default
# ros2_humble_gazebo); the rest is parsed from the sources.
set -euo pipefail

DOCS="$(cd "$(dirname "$0")/.." && pwd)"
EPOSLIB="$(cd "${1:-$HOME/ros2_ws/src/EposLib}" && pwd)"
UNITS="$(cd "${2:-$HOME/ros2_ws/src/ros2units}" && pwd)"
IMAGE="${IMAGE:-ros2_humble_gazebo}"
OUT="$DOCS/docs/reference"

python3 "$DOCS/scripts/gen_reference.py" "$EPOSLIB" "$OUT"

docker run --rm --user root \
  -v "$EPOSLIB":/src/EposLib:ro -v "$UNITS":/src/ros2units:ro \
  -v "$DOCS/scripts":/scripts:ro -v "$OUT":/out -v "$DOCS/includes":/includes \
  "$IMAGE" bash -c '
set -e
cd /tmp && source /opt/ros/humble/setup.bash
g++ -std=c++17 /scripts/gen_errors.cpp /src/EposLib/src/signals/Errors.cpp \
  /src/EposLib/src/signals/Identity.cpp -I /src/EposLib/include -o gen_errors
./gen_errors > /out/error-codes.md
./gen_errors --hardware > /includes/hardware-codes.md
chown --reference=/out /out/error-codes.md /includes/hardware-codes.md
'
# The changelog page includes EposLib's own, minus its top-level title.
tail -n +2 "$EPOSLIB/CHANGELOG.md" > "$DOCS/includes/CHANGELOG.md"
echo "reference regenerated in $OUT"
