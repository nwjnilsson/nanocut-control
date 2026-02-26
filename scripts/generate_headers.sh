# Script to generate C header files from binary assets using xxd
# This should be run whenever the source assets are updated

set -e

# Check if pch directory exists
if [ -d "src/pch" ]; then
    rm src/pch/* -rf
else
    echo "pch dir doesn't exist"
    exit
fi

# Generate icon headers
echo "Generating icon headers..."
xxd -i assets/nanocut-64.png  src/pch/nanocut_64_png.h
xxd -i assets/nanocut-128.png src/pch/nanocut_128_png.h
xxd -i assets/nanocut-256.png src/pch/nanocut_256_png.h

# Generate logo header
echo "Generating logo header..."
xxd -i assets/logo_feathered.png src/pch/logo_png.h

echo "Header generation complete!"

# Show file sizes
echo "Generated files:"
ls -lh src/pch/*.h
