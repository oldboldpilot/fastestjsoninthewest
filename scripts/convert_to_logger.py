#!/usr/bin/env python3
"""
Convert std::cout and std::cerr statements to cpp23-logger API
Author: Olumuyiwa Oluwasanmi
"""

import re
import sys
from pathlib import Path

def convert_file(filepath):
    """Convert cout/cerr to logger in a single file"""
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    # Check if file already has logger import
    has_logger_import = 'import logger;' in content
    
    # Check if file uses cout or cerr
    uses_cout = 'std::cout' in content or 'std::cerr' in content or 'printf' in content
    
    if not uses_cout:
        return False  # No changes needed
    
    # Add logger import after other imports if not present
    if not has_logger_import:
        # Find the last #include or import statement
        import_pattern = r'((?:#include[^\n]*\n|import [^\n]*\n)+)'
        match = re.search(import_pattern, content)
        if match:
            last_import_pos = match.end()
            content = content[:last_import_pos] + 'import logger;\n' + content[last_import_pos:]
        else:
            # No imports found, add at the beginning after comments
            lines = content.split('\n')
            insert_pos = 0
            for i, line in enumerate(lines):
                if line.strip() and not line.strip().startswith('//') and not line.strip().startswith('/*'):
                    insert_pos = i
                    break
            lines.insert(insert_pos, 'import logger;')
            content = '\n'.join(lines)
    
    # Add logger instance at the beginning of main() or test functions
    # Simple pattern: add after opening brace of main or test functions
    main_pattern = r'((?:auto|int)\s+main\s*\([^)]*\)\s*(?:->.*?)?\s*\{)'
    test_pattern = r'((?:void|auto|int)\s+test_\w+\s*\([^)]*\)\s*\{)'
    
    def add_logger_instance(match):
        return match.group(1) + '\n    auto& log = logger::Logger::getInstance();'
    
    content = re.sub(main_pattern, add_logger_instance, content)
    content = re.sub(test_pattern, add_logger_instance, content)
    
    # Convert std::cout patterns to log.info()
    # Handle: std::cout << ... << std::endl; or << "\n";
    cout_patterns = [
        # Pattern: std::cout << "text" << std::endl;
        (r'std::cout\s*<<\s*"([^"]*)"[\s*<<\s*std::endl]*;', r'log.info("{}");'),
        # Pattern: std::cout << "text\n";
        (r'std::cout\s*<<\s*"([^"]*)\\n"\s*;', r'log.info("{}");'),
        # Pattern: std::cout << variable << std::endl;
        (r'std::cout\s*<<\s*([^\s;]+)\s*<<\s*std::endl\s*;', r'log.info("{}", {});'),
        # Pattern: std::cout << "text" << variable << std::endl;
        (r'std::cout\s*<<\s*"([^"]*)"\s*<<\s*([^\s;]+)\s*<<\s*std::endl\s*;', r'log.info("{}{}", "{}", {});'),
    ]
    
    # Convert std::cerr patterns to log.error()
    cerr_patterns = [
        (r'std::cerr\s*<<\s*"([^"]*)"[\s*<<\s*std::endl]*;', r'log.error("{}");'),
        (r'std::cerr\s*<<\s*"([^"]*)\\n"\s*;', r'log.error("{}");'),
        (r'std::cerr\s*<<\s*([^\s;]+)\s*<<\s*std::endl\s*;', r'log.error("{}", {});'),
    ]
    
    # This is a simplified conversion - for complex cases, manual review needed
    # For now, just replace basic patterns
    
    # Replace cout with info level
    content = content.replace('std::cout', 'log.info("")  // TODO: convert stream to format string')
    content = content.replace('std::cerr', 'log.error("")  // TODO: convert stream to format string')
    content = content.replace('printf(', 'log.info(')  # Basic printf conversion
    
    if content != original_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

def main():
    if len(sys.argv) < 2:
        print("Usage: convert_to_logger.py <file_or_directory>")
        sys.exit(1)
    
    path = Path(sys.argv[1])
    
    if path.is_file():
        if convert_file(path):
            print(f"Converted: {path}")
    elif path.is_directory():
        cpp_files = list(path.rglob('*.cpp')) + list(path.rglob('*.h'))
        converted = 0
        for cpp_file in cpp_files:
            if 'external' in str(cpp_file):  # Skip external dependencies
                continue
            if convert_file(cpp_file):
                converted += 1
                print(f"Converted: {cpp_file}")
        print(f"\nTotal files converted: {converted}/{len(cpp_files)}")
    else:
        print(f"Error: {path} not found")
        sys.exit(1)

if __name__ == '__main__':
    main()
