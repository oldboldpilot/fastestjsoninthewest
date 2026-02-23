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
static void BM_Fastjson_Turbo_IndexOnly_64(benchmark::State& state) {
    std::string json_data = generate_json();
    std::vector<uint32_t> structurals(json_data.size());
    
    for (auto _ : state) {
        const char* data = json_data.data();
        size_t len = json_data.size();
        size_t pos = 0;
        size_t struct_idx = 0;
        
        uint64_t prev_escaped = 0;
        uint64_t prev_string_mask = 0;
        uint64_t prev_scalar_bit = 0;
        
        const __m256i quote = _mm256_set1_epi8('"');
        const __m256i backslash = _mm256_set1_epi8('\\');
        const __m256i lbrace = _mm256_set1_epi8('{');
        const __m256i rbrace = _mm256_set1_epi8('}');
        const __m256i lbracket = _mm256_set1_epi8('[');
        const __m256i rbracket = _mm256_set1_epi8(']');
        const __m256i colon = _mm256_set1_epi8(':');
        const __m256i comma = _mm256_set1_epi8(',');
        const __m256i space = _mm256_set1_epi8(' ');
        const __m256i newline = _mm256_set1_epi8('\n');
        const __m256i cr = _mm256_set1_epi8('\r');
        const __m256i tab = _mm256_set1_epi8('\t');
        
        while (pos + 63 < len) {
            __m256i chunk0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
            __m256i chunk1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos + 32));
            
            uint64_t quote_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, quote)) |
                                 (static_cast<uint64_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, quote))) << 32);
            uint64_t backslash_mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk0, backslash)) |
                                     (static_cast<uint64_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk1, backslash))) << 32);
            
            uint64_t escaped_mask = 0;
            uint64_t bs = backslash_mask;
            if (prev_escaped) {
                escaped_mask |= 1;
            }
            while (bs) {
                uint64_t bit = bs & -bs;
                if (!(escaped_mask & bit)) {
                    escaped_mask |= (bit << 1);
                }
                bs &= bs - 1;
            }
            prev_escaped = (backslash_mask & (1ULL << 63)) && !(escaped_mask & (1ULL << 63));
            
            uint64_t unescaped_quotes = quote_mask & ~escaped_mask;
            
            __m128i v = _mm_set_epi64x(0, unescaped_quotes);
            __m128i ones = _mm_set1_epi8(0xFF);
            __m128i string_mask_vec = _mm_clmulepi64_si128(v, ones, 0);
            uint64_t string_mask = _mm_cvtsi128_si64(string_mask_vec);
            string_mask ^= prev_string_mask;
            prev_string_mask = static_cast<uint64_t>(static_cast<int64_t>(string_mask) >> 63);
            
            __m256i m_struct0 = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, lbrace), _mm256_cmpeq_epi8(chunk0, rbrace)),
                _mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, lbracket), _mm256_cmpeq_epi8(chunk0, rbracket)),
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, colon), _mm256_cmpeq_epi8(chunk0, comma))
                )
            );
            __m256i m_struct1 = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, lbrace), _mm256_cmpeq_epi8(chunk1, rbrace)),
                _mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, lbracket), _mm256_cmpeq_epi8(chunk1, rbracket)),
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, colon), _mm256_cmpeq_epi8(chunk1, comma))
                )
            );
            
            __m256i m_ws0 = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, space), _mm256_cmpeq_epi8(chunk0, newline)),
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk0, cr), _mm256_cmpeq_epi8(chunk0, tab))
            );
            __m256i m_ws1 = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, space), _mm256_cmpeq_epi8(chunk1, newline)),
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk1, cr), _mm256_cmpeq_epi8(chunk1, tab))
            );
            
            uint64_t struct_mask = _mm256_movemask_epi8(m_struct0) |
                                  (static_cast<uint64_t>(_mm256_movemask_epi8(m_struct1)) << 32);
            uint64_t ws_mask = _mm256_movemask_epi8(m_ws0) |
                              (static_cast<uint64_t>(_mm256_movemask_epi8(m_ws1)) << 32);
            
            uint64_t scalar_mask = ~(quote_mask | struct_mask | ws_mask);
            uint64_t shifted_scalar_mask = (scalar_mask << 1) | prev_scalar_bit;
            prev_scalar_bit = scalar_mask >> 63;
            
            uint64_t scalar_start_mask = scalar_mask & ~shifted_scalar_mask;
            
            struct_mask &= ~string_mask;
            scalar_start_mask &= ~string_mask;
            
            uint64_t combined_mask = unescaped_quotes | struct_mask | scalar_start_mask;
            
            while (combined_mask != 0) {
                uint64_t bit_pos = __builtin_ctzll(combined_mask);
                combined_mask &= combined_mask - 1;
                uint32_t pos_val = pos + bit_pos;
                uint32_t c = static_cast<uint8_t>(data[pos_val]);
                structurals[struct_idx++] = pos_val | (c << 24);
            }
            pos += 64;
        }
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_64);

BENCHMARK_MAIN();
