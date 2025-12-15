#!/bin/bash
# Bulk convert std::cout and std::cerr to logger across the codebase
# Author: Olumuyiwa Oluwasanmi

set -e

PROJECT_ROOT="/home/muyiwa/Development/FastestJSONInTheWest"

echo "=== Converting cout/cerr to logger across codebase ==="
echo ""

# Find all .cpp files excluding external directory
find "$PROJECT_ROOT" -name "*.cpp" -not -path "*/external/*" -not -path "*/build/*" | while read -r file; do
    # Check if file contains cout or cerr
    if grep -q "std::cout\|std::cerr" "$file"; then
        echo "Processing: $file"
        
        # Backup original
        cp "$file" "${file}.bak"
        
        # Add logger import if not present and file uses cout/cerr
        if ! grep -q "import logger;" "$file"; then
            # Find the last #include line and add import after it
            sed -i '/^#include/a import logger;' "$file" | head -1
        fi
        
        # Add logger instance at start of main() if present
        if grep -q "^auto main\|^int main" "$file"; then
            sed -i '/^auto main.*{$/a\    auto\& log = logger::Logger::getInstance();' "$file"
            sed -i '/^int main.*{$/a\    auto\& log = logger::Logger::getInstance();' "$file"
        fi
        
        # Simple replacements for common patterns
        sed -i 's/std::cout << "\(.*\)\\n";/log.info("\1");/g' "$file"
        sed -i 's/std::cout << "\(.*\)" << std::endl;/log.info("\1");/g' "$file"
        sed -i 's/std::cerr << "\(.*\)\\n";/log.error("\1");/g' "$file"
        sed -i 's/std::cerr << "\(.*\)" << std::endl;/log.error("\1");/g' "$file"
        
        # For complex stream chains, add a TODO comment
        sed -i 's/std::cout <</log.info("") \/\/ TODO: convert <</' "$file"
        sed -i 's/std::cerr <</log.error("") \/\/ TODO: convert <</' "$file"
        
        echo "  → Converted (backup at ${file}.bak)"
    fi
done

echo ""
echo "=== Conversion complete ==="
echo "Review the changes and remove .bak files when satisfied"
echo "To review: git diff"
echo "To restore: find . -name '*.bak' -exec bash -c 'mv \"\$0\" \"\${0%.bak}\"' {} \\;"
