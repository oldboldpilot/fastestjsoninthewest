// FastestJSONInTheWest Command Line Interface
// Author: Olumuyiwa Oluwasanmi
// High-performance JSON processing from command line

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <span>

// FastestJSONInTheWest modules
import fastjson.core;
import fastjson.parser;
import fastjson.serializer;

using namespace fastjson::core;
using namespace fastjson::parser;
using namespace fastjson::serializer;
namespace fs = std::filesystem;

// ============================================================================
// CLI Configuration and Options
// ============================================================================

struct cli_options {
    std::string input_file;
    std::string output_file;
    std::string operation = "validate";  // validate, minify, pretty, benchmark
    bool verbose = false;
    bool timing = false;
    bool validate_only = false;
    bool pretty_print = false;
    bool minify = false;
    bool benchmark = false;
    size_t iterations = 1;
    parser_config parse_config;
    serialize_config serial_config;
};

// ============================================================================
// Utility Functions
// ============================================================================

void print_usage(const char* program_name) {
    std::cout << "FastestJSONInTheWest - High-Performance C++23 JSON Processor\\n";
    std::cout << "Author: Olumuyiwa Oluwasanmi\\n";
    std::cout << "Version: 1.0.0\\n\\n";
    
    std::cout << "USAGE:\\n";
    std::cout << "  " << program_name << " [OPTIONS] [INPUT] [OUTPUT]\\n\\n";
    
    std::cout << "OPERATIONS:\\n";
    std::cout << "  validate     Validate JSON file (default)\\n";
    std::cout << "  minify       Minify JSON (remove whitespace)\\n";
    std::cout << "  pretty       Pretty-print JSON with indentation\\n";
    std::cout << "  benchmark    Performance benchmark\\n\\n";
    
    std::cout << "OPTIONS:\\n";
    std::cout << "  -h, --help              Show this help message\\n";
    std::cout << "  -v, --verbose           Enable verbose output\\n";
    std::cout << "  -t, --timing            Show timing information\\n";
    std::cout << "  -o, --operation OP      Operation: validate|minify|pretty|benchmark\\n";
    std::cout << "  -i, --input FILE        Input JSON file (or stdin)\\n";
    std::cout << "  -O, --output FILE       Output file (or stdout)\\n";
    std::cout << "  --iterations N          Benchmark iterations (default: 1)\\n";
    std::cout << "  --indent STR            Indentation string (default: \\\"  \\\")\\n";
    std::cout << "  --max-depth N           Maximum parsing depth (default: 256)\\n";
    std::cout << "  --allow-comments        Allow JavaScript-style comments\\n";
    std::cout << "  --allow-trailing-commas Allow trailing commas in arrays/objects\\n\\n";
    
    std::cout << "EXAMPLES:\\n";
    std::cout << "  " << program_name << " input.json                    # Validate input.json\\n";
    std::cout << "  " << program_name << " -o pretty input.json output.json  # Pretty-print\\n";
    std::cout << "  " << program_name << " -o minify input.json          # Minify to stdout\\n";
    std::cout << "  " << program_name << " -o benchmark input.json       # Benchmark parsing\\n";
    std::cout << "  " << program_name << " --timing -v input.json        # Validate with timing\\n\\n";
}

void print_version() {
    std::cout << "FastestJSONInTheWest 1.0.0\\n";
    std::cout << "Author: Olumuyiwa Oluwasanmi\\n";
    std::cout << "C++23 High-Performance JSON Library\\n";
    std::cout << "Built: " << __DATE__ << " " << __TIME__ << "\\n";
#ifdef __clang__
    std::cout << "Compiler: Clang " << __clang_version__ << "\\n";
#elif defined(__GNUC__)
    std::cout << "Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << "\\n";
#elif defined(_MSC_VER)
    std::cout << "Compiler: MSVC " << _MSC_VER << "\\n";
#endif
    std::cout << "Standard: C++23\\n";
    
#ifdef FASTJSON_HAS_AVX512
    std::cout << "SIMD: AVX-512 enabled\\n";
#elif defined(FASTJSON_HAS_AVX2)
    std::cout << "SIMD: AVX2 enabled\\n";
#elif defined(FASTJSON_HAS_SSE42)
    std::cout << "SIMD: SSE4.2 enabled\\n";
#else
    std::cout << "SIMD: Scalar only\\n";
#endif

#ifdef FASTJSON_ENABLE_OPENMP
    std::cout << "Parallel: OpenMP enabled\\n";
#else
    std::cout << "Parallel: Single-threaded\\n";
#endif
}

std::string read_file_or_stdin(const std::string& filename) {
    if (filename.empty() || filename == "-") {
        // Read from stdin
        std::string content;
        std::string line;
        while (std::getline(std::cin, line)) {
            content += line + "\\n";
        }
        return content;
    } else {
        // Read from file
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot open input file: " + filename);
        }
        
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::string content(size, '\\0');
        file.read(content.data(), size);
        return content;
    }
}

void write_file_or_stdout(const std::string& content, const std::string& filename) {
    if (filename.empty() || filename == "-") {
        // Write to stdout
        std::cout << content;
    } else {
        // Write to file
        std::ofstream file(filename);
        if (!file) {
            throw std::runtime_error("Cannot open output file: " + filename);
        }
        file << content;
    }
}

// ============================================================================
// CLI Operations
// ============================================================================

int validate_json(const cli_options& opts) {
    try {
        auto start = std::chrono::high_resolution_clock::now();
        std::string content = read_file_or_stdin(opts.input_file);
        auto read_end = std::chrono::high_resolution_clock::now();
        
        if (opts.verbose) {
            std::cerr << "Read " << content.size() << " bytes from " 
                      << (opts.input_file.empty() ? "stdin" : opts.input_file) << "\\n";
        }
        
        auto parse_start = std::chrono::high_resolution_clock::now();
        auto result = parse(content, opts.parse_config);
        auto parse_end = std::chrono::high_resolution_clock::now();
        
        if (!result) {
            std::cerr << "❌ JSON validation failed\\n";
            std::cerr << "Error: Invalid JSON syntax\\n";
            return 1;
        }
        
        if (opts.verbose) {
            std::cout << "✅ JSON is valid\\n";
            std::cout << "Type: " << result->type_name() << "\\n";
            if (result->is_object()) {
                std::cout << "Object keys: " << result->size() << "\\n";
            } else if (result->is_array()) {
                std::cout << "Array elements: " << result->size() << "\\n";
            }
        }
        
        if (opts.timing) {
            auto read_time = std::chrono::duration_cast<std::chrono::microseconds>(read_end - start);
            auto parse_time = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - parse_start);
            auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - start);
            
            std::cerr << "\\nTiming Information:\\n";
            std::cerr << "  File I/O: " << read_time.count() << " μs\\n";
            std::cerr << "  Parsing:  " << parse_time.count() << " μs\\n";
            std::cerr << "  Total:    " << total_time.count() << " μs\\n";
            std::cerr << "  Speed:    " << (content.size() * 1000000.0 / parse_time.count()) << " bytes/sec\\n";
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\\n";
        return 1;
    }
}

int process_json(const cli_options& opts) {
    try {
        auto start = std::chrono::high_resolution_clock::now();
        std::string content = read_file_or_stdin(opts.input_file);
        auto read_end = std::chrono::high_resolution_clock::now();
        
        if (opts.verbose) {
            std::cerr << "Read " << content.size() << " bytes from " 
                      << (opts.input_file.empty() ? "stdin" : opts.input_file) << "\\n";
        }
        
        auto parse_start = std::chrono::high_resolution_clock::now();
        auto result = parse(content, opts.parse_config);
        auto parse_end = std::chrono::high_resolution_clock::now();
        
        if (!result) {
            std::cerr << "❌ JSON parsing failed\\n";
            return 1;
        }
        
        auto serialize_start = std::chrono::high_resolution_clock::now();
        std::string output = to_string(*result, opts.serial_config);
        auto serialize_end = std::chrono::high_resolution_clock::now();
        
        write_file_or_stdout(output, opts.output_file);
        
        if (opts.verbose) {
            std::cout << "✅ JSON processed successfully\\n";
            std::cout << "Input size:  " << content.size() << " bytes\\n";
            std::cout << "Output size: " << output.size() << " bytes\\n";
            std::cout << "Compression: " << (100.0 * (content.size() - output.size()) / content.size()) << "%\\n";
        }
        
        if (opts.timing) {
            auto read_time = std::chrono::duration_cast<std::chrono::microseconds>(read_end - start);
            auto parse_time = std::chrono::duration_cast<std::chrono::microseconds>(parse_end - parse_start);
            auto serialize_time = std::chrono::duration_cast<std::chrono::microseconds>(serialize_end - serialize_start);
            auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(serialize_end - start);
            
            std::cerr << "\\nTiming Information:\\n";
            std::cerr << "  File I/O:      " << read_time.count() << " μs\\n";
            std::cerr << "  Parsing:       " << parse_time.count() << " μs\\n";
            std::cerr << "  Serialization: " << serialize_time.count() << " μs\\n";
            std::cerr << "  Total:         " << total_time.count() << " μs\\n";
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << "\\n";
        return 1;
    }
}

int benchmark_json(const cli_options& opts) {
    try {
        std::string content = read_file_or_stdin(opts.input_file);
        
        std::cout << "FastestJSONInTheWest Benchmark\\n";
        std::cout << "==============================\\n";
        std::cout << "File: " << (opts.input_file.empty() ? "stdin" : opts.input_file) << "\\n";
        std::cout << "Size: " << content.size() << " bytes\\n";
        std::cout << "Iterations: " << opts.iterations << "\\n\\n";
        
        // Warm-up
        std::cout << "Warming up...\\n";
        for (size_t i = 0; i < 3; ++i) {
            auto result = parse(content, opts.parse_config);
            if (!result) {
                std::cerr << "❌ JSON parsing failed during warm-up\\n";
                return 1;
            }
        }
        
        // Parsing benchmark
        std::cout << "\\nParsing Benchmark:\\n";
        std::vector<std::chrono::microseconds> parse_times;
        parse_times.reserve(opts.iterations);
        
        for (size_t i = 0; i < opts.iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            auto result = parse(content, opts.parse_config);
            auto end = std::chrono::high_resolution_clock::now();
            
            if (!result) {
                std::cerr << "❌ JSON parsing failed on iteration " << i << "\\n";
                return 1;
            }
            
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            parse_times.push_back(duration);
            
            if (opts.verbose && (i + 1) % 100 == 0) {
                std::cout << "  Completed " << (i + 1) << " iterations\\n";
            }
        }
        
        // Calculate statistics
        auto total_parse_time = std::accumulate(parse_times.begin(), parse_times.end(), 
                                               std::chrono::microseconds{0});
        auto avg_parse_time = total_parse_time / opts.iterations;
        auto min_parse_time = *std::min_element(parse_times.begin(), parse_times.end());
        auto max_parse_time = *std::max_element(parse_times.begin(), parse_times.end());
        
        std::cout << "\\nResults:\\n";
        std::cout << "  Average: " << avg_parse_time.count() << " μs\\n";
        std::cout << "  Minimum: " << min_parse_time.count() << " μs\\n";
        std::cout << "  Maximum: " << max_parse_time.count() << " μs\\n";
        std::cout << "  Speed:   " << (content.size() * 1000000.0 / avg_parse_time.count()) << " bytes/sec\\n";
        std::cout << "  Speed:   " << (content.size() / (1024.0 * 1024.0) * 1000000.0 / avg_parse_time.count()) << " MB/sec\\n";
        
        // Roundtrip benchmark
        if (opts.iterations >= 10) {
            std::cout << "\\nRoundtrip Benchmark (parse + serialize):\\n";
            std::vector<std::chrono::microseconds> roundtrip_times;
            roundtrip_times.reserve(opts.iterations);
            
            for (size_t i = 0; i < opts.iterations; ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                auto parsed = parse(content, opts.parse_config);
                auto serialized = to_string(*parsed, opts.serial_config);
                auto end = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                roundtrip_times.push_back(duration);
                
                // Prevent optimization
                volatile size_t result_size = serialized.size();
                (void)result_size;
            }
            
            auto avg_roundtrip_time = std::accumulate(roundtrip_times.begin(), roundtrip_times.end(),
                                                    std::chrono::microseconds{0}) / opts.iterations;
            
            std::cout << "  Average roundtrip: " << avg_roundtrip_time.count() << " μs\\n";
            std::cout << "  Roundtrip speed:   " << (content.size() / (1024.0 * 1024.0) * 1000000.0 / avg_roundtrip_time.count()) << " MB/sec\\n";
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Benchmark error: " << e.what() << "\\n";
        return 1;
    }
}

// ============================================================================
// Argument Parsing
// ============================================================================

bool parse_arguments(int argc, char* argv[], cli_options& opts) {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    
    for (size_t i = 0; i < args.size(); ++i) {
        std::string_view arg = args[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--version") {
            print_version();
            std::exit(0);
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-t" || arg == "--timing") {
            opts.timing = true;
        } else if (arg == "-o" || arg == "--operation") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: " << arg << " requires an argument\\n";
                return false;
            }
            opts.operation = args[++i];
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: " << arg << " requires an argument\\n";
                return false;
            }
            opts.input_file = args[++i];
        } else if (arg == "-O" || arg == "--output") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: " << arg << " requires an argument\\n";
                return false;
            }
            opts.output_file = args[++i];
        } else if (arg == "--iterations") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: " << arg << " requires an argument\\n";
                return false;
            }
            opts.iterations = std::stoull(std::string(args[++i]));
        } else if (arg == "--indent") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: " << arg << " requires an argument\\n";
                return false;
            }
            opts.serial_config.indent = args[++i];
        } else if (arg == "--max-depth") {
            if (i + 1 >= args.size()) {
                std::cerr << "Error: " << arg << " requires an argument\\n";
                return false;
            }
            opts.parse_config.max_depth = std::stoull(std::string(args[++i]));
        } else if (arg == "--allow-comments") {
            opts.parse_config.allow_comments = true;
        } else if (arg == "--allow-trailing-commas") {
            opts.parse_config.allow_trailing_commas = true;
        } else if (arg.starts_with("-")) {
            std::cerr << "Error: Unknown option: " << arg << "\\n";
            return false;
        } else {
            // Positional arguments
            if (opts.input_file.empty()) {
                opts.input_file = arg;
            } else if (opts.output_file.empty()) {
                opts.output_file = arg;
            } else {
                std::cerr << "Error: Too many positional arguments\\n";
                return false;
            }
        }
    }
    
    // Configure operation-specific settings
    if (opts.operation == "pretty") {
        opts.serial_config.pretty_print = true;
    } else if (opts.operation == "minify") {
        opts.serial_config.pretty_print = false;
    } else if (opts.operation == "benchmark") {
        if (opts.iterations == 1) {
            opts.iterations = 1000;  // Default for benchmarks
        }
    }
    
    return true;
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    cli_options opts;
    
    // Parse command line arguments
    if (!parse_arguments(argc, argv, opts)) {
        std::cerr << "Use -h or --help for usage information\\n";
        return 1;
    }
    
    // Execute the requested operation
    try {
        if (opts.operation == "validate") {
            return validate_json(opts);
        } else if (opts.operation == "minify" || opts.operation == "pretty") {
            return process_json(opts);
        } else if (opts.operation == "benchmark") {
            return benchmark_json(opts);
        } else {
            std::cerr << "Error: Unknown operation: " << opts.operation << "\\n";
            std::cerr << "Valid operations: validate, minify, pretty, benchmark\\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << "\\n";
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error\\n";
        return 1;
    }
}