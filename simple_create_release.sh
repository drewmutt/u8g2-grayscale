#!/bin/bash

# Path to library.json
LIBRARY_JSON="library.json"

# Check if library.json exists
if [ ! -f "$LIBRARY_JSON" ]; then
    echo "Error: library.json not found!"
    exit 1
fi

# Check if jq is installed (better JSON parsing)
if command -v jq &> /dev/null; then
    # Use jq for robust JSON parsing
    LIBRARY_NAME=$(jq -r '.name' "$LIBRARY_JSON")
    VERSION=$(jq -r '.version' "$LIBRARY_JSON")
    DESCRIPTION=$(jq -r '.description' "$LIBRARY_JSON")
    AUTHOR=$(jq -r '.authors[0].name' "$LIBRARY_JSON")
    URL=$(jq -r '.repository.url' "$LIBRARY_JSON")
else
    # Fallback to grep/sed if jq not available
    echo "Warning: jq not found, using basic parsing (install jq for better results)"
    LIBRARY_NAME=$(grep '"name"' "$LIBRARY_JSON" | head -1 | sed -E 's/.*"name"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')
    VERSION=$(grep '"version"' "$LIBRARY_JSON" | sed -E 's/.*"version"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')
    DESCRIPTION=$(grep '"description"' "$LIBRARY_JSON" | sed -E 's/.*"description"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')
    AUTHOR=$(grep '"name"' "$LIBRARY_JSON" | tail -1 | sed -E 's/.*"name"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')
    URL=$(grep '"url"' "$LIBRARY_JSON" | sed -E 's/.*"url"[[:space:]]*:[[:space:]]*"([^"]+)".*/\1/')
fi

OUTPUT_DIR="U8g2_Arduino"

echo "Creating Arduino library: ${LIBRARY_NAME} v${VERSION}"

# Create Arduino library structure
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/src/clib"
mkdir -p "$OUTPUT_DIR/examples"

echo "Copying source files..."

# Copy C++ wrapper files
cp cppsrc/*.cpp "$OUTPUT_DIR/src/" 2>/dev/null || true
cp cppsrc/*.h "$OUTPUT_DIR/src/" 2>/dev/null || true

# Copy ALL C library files INCLUDING FONTS
echo "Copying C files (including fonts)..."
cp csrc/*.c "$OUTPUT_DIR/src/clib/"
cp csrc/*.h "$OUTPUT_DIR/src/clib/"

# Remove the stdio driver (not needed for Arduino)
rm -f "$OUTPUT_DIR/src/clib/u8x8_d_stdio.c"

# Verify fonts were copied
if [ -f "$OUTPUT_DIR/src/clib/u8g2_fonts.c" ]; then
    FONT_SIZE=$(wc -c < "$OUTPUT_DIR/src/clib/u8g2_fonts.c")
    echo "✓ Fonts copied successfully (u8g2_fonts.c: ${FONT_SIZE} bytes)"
else
    echo "⚠ WARNING: u8g2_fonts.c not found!"
fi

if [ -f "$OUTPUT_DIR/src/clib/u8x8_fonts.c" ]; then
    FONT_SIZE=$(wc -c < "$OUTPUT_DIR/src/clib/u8x8_fonts.c")
    echo "✓ u8x8 Fonts copied successfully (u8x8_fonts.c: ${FONT_SIZE} bytes)"
else
    echo "⚠ WARNING: u8x8_fonts.c not found!"
fi

echo "Fixing include paths..."

# Fix include paths in C++ headers (Mac-compatible)
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    sed -i '' 's|"u8g2.h"|"clib/u8g2.h"|g' "$OUTPUT_DIR/src/U8g2lib.h" 2>/dev/null || true
    sed -i '' 's|"u8x8.h"|"clib/u8x8.h"|g' "$OUTPUT_DIR/src/U8x8lib.h" 2>/dev/null || true
    sed -i '' 's|"mui.h"|"clib/mui.h"|g' "$OUTPUT_DIR/src/MUIU8g2.h" 2>/dev/null || true
    sed -i '' 's|"mui_u8g2.h"|"clib/mui_u8g2.h"|g' "$OUTPUT_DIR/src/MUIU8g2.h" 2>/dev/null || true
else
    # Linux
    sed -i 's|"u8g2.h"|"clib/u8g2.h"|g' "$OUTPUT_DIR/src/U8g2lib.h" 2>/dev/null || true
    sed -i 's|"u8x8.h"|"clib/u8x8.h"|g' "$OUTPUT_DIR/src/U8x8lib.h" 2>/dev/null || true
    sed -i 's|"mui.h"|"clib/mui.h"|g' "$OUTPUT_DIR/src/MUIU8g2.h" 2>/dev/null || true
    sed -i 's|"mui_u8g2.h"|"clib/mui_u8g2.h"|g' "$OUTPUT_DIR/src/MUIU8g2.h" 2>/dev/null || true
fi

# Copy examples (if you have them)
if [ -d "sys/arduino" ]; then
    echo "Copying examples..."
    cp -r sys/arduino/u8g2_page_buffer/* "$OUTPUT_DIR/examples/" 2>/dev/null || true
fi

# Copy docs
cp LICENSE "$OUTPUT_DIR/" 2>/dev/null || true
cp README.md "$OUTPUT_DIR/" 2>/dev/null || true
cp library.json "$OUTPUT_DIR/" 2>/dev/null || true

# Create library.properties from library.json
cat > "$OUTPUT_DIR/library.properties" << EOF
name=${LIBRARY_NAME}
version=${VERSION}
author=${AUTHOR}
maintainer=${AUTHOR}
sentence=${DESCRIPTION}
paragraph=${DESCRIPTION}
category=Display
url=${URL}
architectures=*
includes=U8g2lib.h
EOF

echo "Creating ZIP file..."

# Create ZIP
ZIP_NAME="${LIBRARY_NAME}_${VERSION}.zip"
cd "$OUTPUT_DIR"
zip -r "../${ZIP_NAME}" . -x "*.git*"
cd ..

echo ""
echo "✓ Arduino library created successfully!"
echo "  Output: ${ZIP_NAME}"
echo "  Name: ${LIBRARY_NAME}"
echo "  Version: ${VERSION}"
echo "  Author: ${AUTHOR}"
echo ""
echo "File count in library:"
echo "  Headers: $(find "$OUTPUT_DIR/src" -name "*.h" | wc -l)"
echo "  C files: $(find "$OUTPUT_DIR/src" -name "*.c" | wc -l)"
echo "  C++ files: $(find "$OUTPUT_DIR/src" -name "*.cpp" | wc -l)"
echo ""
echo "Install in Arduino IDE: Sketch → Include Library → Add .ZIP Library"