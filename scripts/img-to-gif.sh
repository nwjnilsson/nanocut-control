#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "Usage: $0 <directory> [output.gif]"
    exit 1
fi

dir="${1%/}"

if [ ! -d "$dir" ]; then
    echo "Error: '$dir' is not a directory"
    exit 1
fi

# Detect image extension from first image found
ext=""
for candidate in png jpg jpeg bmp tiff webp; do
    count=$(find "$dir" -maxdepth 1 -iname "*.$candidate" | wc -l)
    if [ "$count" -gt 0 ]; then
        ext="$candidate"
        break
    fi
done

if [ -z "$ext" ]; then
    echo "Error: no supported images found in '$dir'"
    exit 1
fi

# Determine input pattern — use numbered if files are 1.ext, 2.ext, etc., otherwise glob
if [ -f "$dir/1.$ext" ]; then
    input=(-i "$dir/%d.$ext")
else
    input=(-pattern_type glob -i "$dir/*.$ext")
fi

output="${2:-$dir/output.gif}"
palette=$(mktemp /tmp/palette-XXXX.png)
trap "rm -f '$palette'" EXIT

echo "Found .$ext images in $dir"
echo "Generating palette..."
ffmpeg -framerate 1 "${input[@]}" -vf "palettegen=max_colors=256:stats_mode=full" -y "$palette" 2>/dev/null

echo "Creating GIF..."
ffmpeg -framerate 1 "${input[@]}" -i "$palette" -lavfi "paletteuse=dither=floyd_steinberg" -loop 0 -y "$output" 2>/dev/null

echo "Done: $output"
