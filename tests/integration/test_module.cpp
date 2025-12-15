// Test the C++23 SIMD Multiregister Module
// ==========================================

import fastjson_simd_multiregister;
import std;

using namespace fastjson::simd::multi;

auto main() -> int {
    std::println("🚀 FastJSON SIMD Multi-Register C++23 Module Test");
    std::println("================================================");
    
    // Test data with whitespace and structural characters
    std::string test_data = R"(  
        {"name": "test", "value": 42, "active": true}
    )";
    
    // Display CPU capabilities
    std::println("\n🔍 CPU Capability Detection:");
    std::println("  AVX2 Support:    {}", has_avx2_support() ? "✓ Yes" : "✗ No");
    std::println("  AVX-512 Support: {}", has_avx512_support() ? "✓ Yes" : "✗ No");
    
    std::println("\n📊 Testing Multi-Register SIMD Functions:");
    std::println("=========================================");
    
    // Test whitespace skipping
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t non_ws_pos = skip_whitespace_multiregister(test_data.data(), test_data.size(), 0);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    std::println("  Whitespace Skip: Position {} (char: '{}') - {} ns", 
                non_ws_pos, test_data[non_ws_pos], duration.count());
    
    // Test string scanning  
    if (test_data[non_ws_pos] == '{') {
        size_t quote_pos = test_data.find('"', non_ws_pos);
        if (quote_pos != std::string::npos) {
            start_time = std::chrono::high_resolution_clock::now();
            size_t string_end = find_string_end_multiregister(test_data.data(), test_data.size(), quote_pos + 1);
            end_time = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
            
            std::string extracted(test_data.begin() + quote_pos + 1, test_data.begin() + string_end);
            std::println("  String Scan:     '{}' - {} ns", extracted, duration.count());
        }
    }
    
    // Test structural character detection
    start_time = std::chrono::high_resolution_clock::now();
    auto structural = find_structural_chars_multiregister(test_data.data(), test_data.size(), 0);
    end_time = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    
    std::println("  Structural Chars: Found {} characters - {} ns", structural.count, duration.count());
    
    // Display structural characters found
    std::println("\n🎯 Structural Characters Found:");
    const char* type_names[] = {"", "{", "}", "[", "]", ":", ","};
    for (size_t i = 0; i < structural.count && i < 10; ++i) {  // Show first 10
        size_t pos = structural.positions[i];
        uint8_t type = structural.types[i];
        const char* type_name = (type < 7) ? type_names[type] : "?";
        std::println("    Position {}: '{}' ({})", pos, test_data[pos], type_name);
    }
    
    // Performance comparison test
    std::println("\n⚡ Performance Comparison (1000 iterations):");
    std::println("===========================================");
    
    const size_t iterations = 1000;
    
    // Multi-register performance
    start_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        volatile auto result = skip_whitespace_multiregister(test_data.data(), test_data.size(), 0);
        (void)result;  // Prevent optimization
    }
    end_time = std::chrono::high_resolution_clock::now();
    auto multi_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // AVX2 specific performance (if available)
    if (has_avx2_support()) {
        start_time = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            volatile auto result = skip_whitespace_4x_avx2(test_data.data(), test_data.size(), 0);
            (void)result;
        }
        end_time = std::chrono::high_resolution_clock::now();
        auto avx2_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::println("  Auto-Dispatch: {} μs", multi_duration.count());
        std::println("  AVX2 Direct:   {} μs", avx2_duration.count());
        std::println("  Overhead:      {} μs ({:.1f}%)", 
                    multi_duration.count() - avx2_duration.count(),
                    100.0 * (multi_duration.count() - avx2_duration.count()) / avx2_duration.count());
    } else {
        std::println("  Auto-Dispatch: {} μs (AVX2 not available)", multi_duration.count());
    }
    
    std::println("\n✅ C++23 SIMD Multi-Register Module Test Complete!");
    return 0;
}