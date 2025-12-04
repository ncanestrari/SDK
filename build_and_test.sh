#!/bin/bash

# Script to demonstrate the JSON initialization code generator

set -e

echo "=== JSON Initialization Code Generator Demo ==="
echo

# Build the project
echo "Building project..."
mkdir -p build
cd build
cmake ..
make -j$(nproc)
cd ..

echo "✓ Build complete"
echo

# Run the code generator on example classes
echo "Running code generator on json_init_example.hpp..."
./build/json_init_generator json_init_example.hpp --output-dir generated

echo "✓ Code generation complete"
echo

# Display generated folder structure
echo "Generated folder structure:"
echo "=========================="
ls -la generated/
echo

# Display sample generated files
echo "Sample generated header (gameentity_initializer.hpp):"
echo "===================================================="
cat generated/gameentity_initializer.hpp
echo

echo "Sample generated implementation (gameentity_initializer.cpp):"
echo "============================================================"
head -30 generated/gameentity_initializer.cpp
echo "... (truncated)"
echo

echo "Sample generated config (gameentity_.conf):"
echo "==========================================="
cat generated/gameentity_.conf
echo

echo "All generated files:"
echo "==================="
find generated/ -type f | sort
echo

echo "=== Demo Complete ==="
echo
echo "Summary:"
echo "1. Created clang AST tool to find classes with __attribute__((annotate(\"initialize\")))"
echo "2. Generated separate files for each annotated class:"
echo "   - <class>_initializer.hpp (header with function declaration)"
echo "   - <class>_initializer.cpp (implementation)"
echo "   - <class>_.conf (example JSON configuration)"
echo "3. Handled Object-derived pointers via ObjectRegistry lookup"
echo "4. Created organized file structure in 'generated/' folder"
echo
echo "Usage in your code:"
echo "  #include \"generated/gameentity_initializer.hpp\""
echo "  GameEntity entity;"
echo "  JsonNodePtr node = parser.parse(jsonString);"
echo "  initializeFromJson(entity, node);"
echo
echo "Example JSON files are ready to use as configuration templates!"
