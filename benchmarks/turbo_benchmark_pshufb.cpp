#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include <immintrin.h>

std::string generate_json() {
    std::string json = "[";
    for (int i = 0; i < 10000; i++) {
        if (i > 0) json += ",";
        json += R"({"id":)" + std::to_string(i) + R"(,"name":"User )" + std::to_string(i) + R"(","active":true,"score":)" + std::to_string(i * 1.5) + R"(})";
    }
    json += "]";
    return json;
}

__attribute__((target("avx2,pclmul")))
static void BM_Fastjson_Turbo_IndexOnly_Pshufb(benchmark::State& state) {
    std::string json_data = generate_json();
    std::vector<uint32_t> structurals(json_data.size());
    
    for (auto _ : state) {
        const char* data = json_data.data();
        size_t len = json_data.size();
        size_t pos = 0;
        size_t struct_idx = 0;
        
        bool prev_escaped = false;
        uint32_t prev_string_mask = 0;
        uint32_t prev_scalar_bit = 0;
        
        const __m256i quote = _mm256_set1_epi8('"');
        const __m256i backslash = _mm256_set1_epi8('\\');
        
        const __m256i lower_tbl = _mm256_setr_epi8(16, 0, 0, 0, 0, 0, 0, 0, 0, 32, 36, 1, 8, 34, 0, 0,
                                                   16, 0, 0, 0, 0, 0, 0, 0, 0, 32, 36, 1, 8, 34, 0, 0);
        const __m256i upper_tbl = _mm256_setr_epi8(32, 0, 24, 4, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   32, 0, 24, 4, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0);
        const __m256i lower_mask = _mm256_set1_epi8(0x0F);
        const __m256i struct_bitmask = _mm256_set1_epi8(0x0F);
        const __m256i ws_bitmask = _mm256_set1_epi8(0x30);
        const __m256i zero = _mm256_setzero_si256();
        
        while (pos + 31 < len) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
            
            __m256i m_quote = _mm256_cmpeq_epi8(chunk, quote);
            __m256i m_backslash = _mm256_cmpeq_epi8(chunk, backslash);
            
            uint32_t quote_mask = _mm256_movemask_epi8(m_quote);
            uint32_t backslash_mask = _mm256_movemask_epi8(m_backslash);
            
            uint32_t escaped_mask = 0;
            uint32_t bs = backslash_mask;
            if (prev_escaped) {
                escaped_mask |= 1;
            }
            while (bs) {
                uint32_t bit = bs & -bs;
                if (!(escaped_mask & bit)) {
                    escaped_mask |= (bit << 1);
                }
                bs &= bs - 1;
            }
            prev_escaped = (backslash_mask & (1U << 31)) && !(escaped_mask & (1U << 31));
            
            uint32_t unescaped_quotes = quote_mask & ~escaped_mask;
            
            __m128i v = _mm_cvtsi32_si128(unescaped_quotes);
            __m128i ones = _mm_set1_epi8(0xFF);
            __m128i string_mask_vec = _mm_clmulepi64_si128(v, ones, 0);
            uint32_t string_mask = _mm_cvtsi128_si32(string_mask_vec);
            string_mask ^= prev_string_mask;
            prev_string_mask = static_cast<uint32_t>(static_cast<int32_t>(string_mask) >> 31);
            
            __m256i lower_nibbles = _mm256_and_si256(chunk, lower_mask);
            __m256i upper_nibbles = _mm256_and_si256(_mm256_srli_epi16(chunk, 4), lower_mask);
            
            __m256i lower_class = _mm256_shuffle_epi8(lower_tbl, lower_nibbles);
            __m256i upper_class = _mm256_shuffle_epi8(upper_tbl, upper_nibbles);
            __m256i class_mask = _mm256_and_si256(lower_class, upper_class);
            
            __m256i struct_vec = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask, struct_bitmask), zero);
            __m256i ws_vec = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask, ws_bitmask), zero);
            
            uint32_t struct_mask = _mm256_movemask_epi8(struct_vec);
            uint32_t ws_mask = _mm256_movemask_epi8(ws_vec);
            
            uint32_t scalar_mask = ~(quote_mask | struct_mask | ws_mask);
            uint32_t shifted_scalar_mask = (scalar_mask << 1) | prev_scalar_bit;
            prev_scalar_bit = scalar_mask >> 31;
            
            uint32_t scalar_start_mask = scalar_mask & ~shifted_scalar_mask;
            
            struct_mask &= ~string_mask;
            scalar_start_mask &= ~string_mask;
            
            uint32_t combined_mask = unescaped_quotes | struct_mask | scalar_start_mask;
            
            while (combined_mask != 0) {
                uint32_t bit_pos = __builtin_ctz(combined_mask);
                combined_mask &= combined_mask - 1;
                structurals[struct_idx++] = pos + bit_pos;
            }
            pos += 32;
        }
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_Pshufb);

BENCHMARK_MAIN();
