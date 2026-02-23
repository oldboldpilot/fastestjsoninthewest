#include <benchmark/benchmark.h>
#include <string>
#include <simdjson.h>

import fastjson_turbo;
import fastjson;

std::string generate_json() {
    std::string json = "[";
    for (int i = 0; i < 10000; i++) {
        if (i > 0) json += ",";
        json += R"({"id":)" + std::to_string(i) + R"(,"name":"User )" + std::to_string(i) + R"(","active":true,"score":)" + std::to_string(i * 1.5) + R"(})";
    }
    json += "]";
    return json;
}

static void BM_Simdjson_Parse(benchmark::State& state) {
    std::string json = generate_json();
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded_json(json);
    
    for (auto _ : state) {
        simdjson::ondemand::document doc;
        auto error = parser.iterate(padded_json).get(doc);
        if (error) {
            state.SkipWithError("simdjson parse failed");
            break;
        }
        
        int count = 0;
        for (auto item : doc.get_array()) {
            std::string_view name;
            if (!item["name"].get_string().get(name)) {
                benchmark::DoNotOptimize(name);
            }
            count++;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * json.size());
}

static void BM_Fastjson_Turbo_ParseOnly(benchmark::State& state) {
    std::string json = generate_json();
    fastjson::turbo::turbo_parser parser;

    for (auto _ : state) {
        auto doc_res = fastjson::turbo::parse(json, parser);
        benchmark::DoNotOptimize(doc_res);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * json.size());
}

static void BM_Fastjson_Turbo_Parse(benchmark::State& state) {
    std::string json = generate_json();
    fastjson::turbo::turbo_parser parser;
    
    for (auto _ : state) {
        auto doc_res = fastjson::turbo::parse(json, parser);
        if (!doc_res) {
            state.SkipWithError("fastjson turbo parse failed");
            break;
        }
        
        auto root = doc_res->root();
        auto arr = root.get_array();
        if (!arr) continue;
        
        int count = 0;
        for (auto item : *arr) {
            auto obj = item.get_object();
            if (!obj) continue;
            
            auto name_val = obj->find_field("name");
            if (!name_val) continue;
            
            auto name = name_val->get_string();
            if (name) {
                benchmark::DoNotOptimize(*name);
            }
            count++;
        }
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * json.size());
}

static void BM_Fastjson_Turbo_IndexOnly(benchmark::State& state) {
    std::string json = generate_json();
    // scan-only: no match table (nullptr). Uses AVX-512 or AVX2 via runtime dispatch.
    std::unique_ptr<uint32_t[]> sp = std::make_unique<uint32_t[]>(json.size() + 32);

    for (auto _ : state) {
        size_t count = fastjson::turbo::detail::build_structural_index(json, sp.get(), nullptr);
        benchmark::DoNotOptimize(count);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * json.size());
}

BENCHMARK(BM_Simdjson_Parse);
BENCHMARK(BM_Fastjson_Turbo_ParseOnly);
BENCHMARK(BM_Fastjson_Turbo_Parse);
BENCHMARK(BM_Fastjson_Turbo_IndexOnly);

BENCHMARK_MAIN();

// Depth-scan skip benchmark (replaces old bracket-match benchmark).
static void BM_Fastjson_Turbo_DepthScan(benchmark::State& state) {
    std::string json = generate_json();
    fastjson::turbo::turbo_parser parser;
    auto doc_res = fastjson::turbo::parse(json, parser);
    auto& doc = doc_res.value();
    
    for (auto _ : state) {
        // Exercise skip_value for every array element (depth-scan skip).
        size_t total = 0;
        auto root = doc.root();
        auto arr = root.get_array();
        if (arr) {
            for (auto item : *arr) {
                benchmark::DoNotOptimize(item);
                ++total;
            }
        }
        benchmark::DoNotOptimize(total);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * json.size());
}
BENCHMARK(BM_Fastjson_Turbo_DepthScan);

__attribute__((target("avx2")))
static void BM_Fastjson_Turbo_IndexOnly_NoChar(benchmark::State& state) {
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
            
            __m256i m_struct = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk, lbrace), _mm256_cmpeq_epi8(chunk, rbrace)),
                _mm256_or_si256(
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, lbracket), _mm256_cmpeq_epi8(chunk, rbracket)),
                    _mm256_or_si256(_mm256_cmpeq_epi8(chunk, colon), _mm256_cmpeq_epi8(chunk, comma))
                )
            );
            
            __m256i m_ws = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk, space), _mm256_cmpeq_epi8(chunk, newline)),
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk, cr), _mm256_cmpeq_epi8(chunk, tab))
            );
            
            uint32_t struct_mask = _mm256_movemask_epi8(m_struct);
            uint32_t ws_mask = _mm256_movemask_epi8(m_ws);
            
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
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_NoChar);

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

__attribute__((target("avx2,pclmul")))
static void BM_Fastjson_Turbo_IndexOnly_64_NoChar(benchmark::State& state) {
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
                structurals[struct_idx++] = pos + bit_pos;
            }
            pos += 64;
        }
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_64_NoChar);

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

__attribute__((target("avx2,pclmul")))
static void BM_Fastjson_Turbo_IndexOnly_64_Pshufb(benchmark::State& state) {
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
        
        const __m256i lower_tbl = _mm256_setr_epi8(16, 0, 0, 0, 0, 0, 0, 0, 0, 32, 36, 1, 8, 34, 0, 0,
                                                   16, 0, 0, 0, 0, 0, 0, 0, 0, 32, 36, 1, 8, 34, 0, 0);
        const __m256i upper_tbl = _mm256_setr_epi8(32, 0, 24, 4, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
                                                   32, 0, 24, 4, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0);
        const __m256i lower_mask = _mm256_set1_epi8(0x0F);
        const __m256i struct_bitmask = _mm256_set1_epi8(0x0F);
        const __m256i ws_bitmask = _mm256_set1_epi8(0x30);
        const __m256i zero = _mm256_setzero_si256();
        
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
            
            __m256i lower_nibbles0 = _mm256_and_si256(chunk0, lower_mask);
            __m256i upper_nibbles0 = _mm256_and_si256(_mm256_srli_epi16(chunk0, 4), lower_mask);
            __m256i lower_class0 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles0);
            __m256i upper_class0 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles0);
            __m256i class_mask0 = _mm256_and_si256(lower_class0, upper_class0);
            __m256i struct_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, struct_bitmask), zero);
            __m256i ws_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, ws_bitmask), zero);
            
            __m256i lower_nibbles1 = _mm256_and_si256(chunk1, lower_mask);
            __m256i upper_nibbles1 = _mm256_and_si256(_mm256_srli_epi16(chunk1, 4), lower_mask);
            __m256i lower_class1 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles1);
            __m256i upper_class1 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles1);
            __m256i class_mask1 = _mm256_and_si256(lower_class1, upper_class1);
            __m256i struct_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, struct_bitmask), zero);
            __m256i ws_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, ws_bitmask), zero);
            
            uint64_t struct_mask = _mm256_movemask_epi8(struct_vec0) |
                                  (static_cast<uint64_t>(_mm256_movemask_epi8(struct_vec1)) << 32);
            uint64_t ws_mask = _mm256_movemask_epi8(ws_vec0) |
                              (static_cast<uint64_t>(_mm256_movemask_epi8(ws_vec1)) << 32);
            
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
                structurals[struct_idx++] = pos + bit_pos;
            }
            pos += 64;
        }
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_64_Pshufb);

__attribute__((target("avx2,pclmul")))
static void BM_Fastjson_Turbo_IndexOnly_Pshufb_Char(benchmark::State& state) {
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
                uint32_t pos_val = pos + bit_pos;
                uint32_t c = static_cast<uint8_t>(data[pos_val]);
                structurals[struct_idx++] = pos_val | (c << 24);
            }
            pos += 32;
        }
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_Pshufb_Char);


__attribute__((target("avx2,pclmul")))
static void BM_Fastjson_Turbo_IndexOnly_Decoupled(benchmark::State& state) {
    std::string json_data = generate_json();
    std::vector<uint32_t> structurals(json_data.size());
    std::vector<uint64_t> structural_masks(json_data.size() / 64 + 1);
    
    for (auto _ : state) {
        const char* data = json_data.data();
        size_t len = json_data.size();
        size_t pos = 0;
        size_t mask_idx = 0;
        
        uint64_t prev_escaped = 0;
        uint64_t prev_string_mask = 0;
        uint64_t prev_scalar_bit = 0;
        
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
        
        // Pass 1: SIMD bitmask generation
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
            
            __m256i lower_nibbles0 = _mm256_and_si256(chunk0, lower_mask);
            __m256i upper_nibbles0 = _mm256_and_si256(_mm256_srli_epi16(chunk0, 4), lower_mask);
            __m256i lower_class0 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles0);
            __m256i upper_class0 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles0);
            __m256i class_mask0 = _mm256_and_si256(lower_class0, upper_class0);
            __m256i struct_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, struct_bitmask), zero);
            __m256i ws_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, ws_bitmask), zero);
            
            __m256i lower_nibbles1 = _mm256_and_si256(chunk1, lower_mask);
            __m256i upper_nibbles1 = _mm256_and_si256(_mm256_srli_epi16(chunk1, 4), lower_mask);
            __m256i lower_class1 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles1);
            __m256i upper_class1 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles1);
            __m256i class_mask1 = _mm256_and_si256(lower_class1, upper_class1);
            __m256i struct_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, struct_bitmask), zero);
            __m256i ws_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, ws_bitmask), zero);
            
            uint64_t struct_mask = _mm256_movemask_epi8(struct_vec0) |
                                  (static_cast<uint64_t>(_mm256_movemask_epi8(struct_vec1)) << 32);
            uint64_t ws_mask = _mm256_movemask_epi8(ws_vec0) |
                              (static_cast<uint64_t>(_mm256_movemask_epi8(ws_vec1)) << 32);
            
            uint64_t scalar_mask = ~(quote_mask | struct_mask | ws_mask);
            uint64_t shifted_scalar_mask = (scalar_mask << 1) | prev_scalar_bit;
            prev_scalar_bit = scalar_mask >> 63;
            
            uint64_t scalar_start_mask = scalar_mask & ~shifted_scalar_mask;
            
            struct_mask &= ~string_mask;
            scalar_start_mask &= ~string_mask;
            
            uint64_t combined_mask = unescaped_quotes | struct_mask | scalar_start_mask;
            
            structural_masks[mask_idx++] = combined_mask;
            pos += 64;
        }
        
        // Pass 2: Scalar bit extraction
        size_t struct_idx = 0;
        for (size_t i = 0; i < mask_idx; i++) {
            uint64_t combined_mask = structural_masks[i];
            uint32_t base_pos = i * 64;
            while (combined_mask != 0) {
                uint64_t bit_pos = __builtin_ctzll(combined_mask);
                combined_mask &= combined_mask - 1;
                structurals[struct_idx++] = base_pos + bit_pos;
            }
        }
        
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_Decoupled);

__attribute__((target("avx2,pclmul")))
static void BM_Fastjson_Turbo_IndexOnly_Decoupled_Char(benchmark::State& state) {
    std::string json_data = generate_json();
    std::vector<uint32_t> structurals(json_data.size());
    std::vector<uint64_t> structural_masks(json_data.size() / 64 + 1);
    
    for (auto _ : state) {
        const char* data = json_data.data();
        size_t len = json_data.size();
        size_t pos = 0;
        size_t mask_idx = 0;
        
        uint64_t prev_escaped = 0;
        uint64_t prev_string_mask = 0;
        uint64_t prev_scalar_bit = 0;
        
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
        
        // Pass 1: SIMD bitmask generation
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
            
            __m256i lower_nibbles0 = _mm256_and_si256(chunk0, lower_mask);
            __m256i upper_nibbles0 = _mm256_and_si256(_mm256_srli_epi16(chunk0, 4), lower_mask);
            __m256i lower_class0 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles0);
            __m256i upper_class0 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles0);
            __m256i class_mask0 = _mm256_and_si256(lower_class0, upper_class0);
            __m256i struct_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, struct_bitmask), zero);
            __m256i ws_vec0 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask0, ws_bitmask), zero);
            
            __m256i lower_nibbles1 = _mm256_and_si256(chunk1, lower_mask);
            __m256i upper_nibbles1 = _mm256_and_si256(_mm256_srli_epi16(chunk1, 4), lower_mask);
            __m256i lower_class1 = _mm256_shuffle_epi8(lower_tbl, lower_nibbles1);
            __m256i upper_class1 = _mm256_shuffle_epi8(upper_tbl, upper_nibbles1);
            __m256i class_mask1 = _mm256_and_si256(lower_class1, upper_class1);
            __m256i struct_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, struct_bitmask), zero);
            __m256i ws_vec1 = _mm256_cmpgt_epi8(_mm256_and_si256(class_mask1, ws_bitmask), zero);
            
            uint64_t struct_mask = _mm256_movemask_epi8(struct_vec0) |
                                  (static_cast<uint64_t>(_mm256_movemask_epi8(struct_vec1)) << 32);
            uint64_t ws_mask = _mm256_movemask_epi8(ws_vec0) |
                              (static_cast<uint64_t>(_mm256_movemask_epi8(ws_vec1)) << 32);
            
            uint64_t scalar_mask = ~(quote_mask | struct_mask | ws_mask);
            uint64_t shifted_scalar_mask = (scalar_mask << 1) | prev_scalar_bit;
            prev_scalar_bit = scalar_mask >> 63;
            
            uint64_t scalar_start_mask = scalar_mask & ~shifted_scalar_mask;
            
            struct_mask &= ~string_mask;
            scalar_start_mask &= ~string_mask;
            
            uint64_t combined_mask = unescaped_quotes | struct_mask | scalar_start_mask;
            
            structural_masks[mask_idx++] = combined_mask;
            pos += 64;
        }
        
        // Pass 2: Scalar bit extraction and character packing
        size_t struct_idx = 0;
        for (size_t i = 0; i < mask_idx; i++) {
            uint64_t combined_mask = structural_masks[i];
            uint32_t base_pos = i * 64;
            while (combined_mask != 0) {
                uint64_t bit_pos = __builtin_ctzll(combined_mask);
                combined_mask &= combined_mask - 1;
                uint32_t pos_val = base_pos + bit_pos;
                uint32_t c = static_cast<uint8_t>(data[pos_val]);
                structurals[struct_idx++] = pos_val | (c << 24);
            }
        }
        
        benchmark::DoNotOptimize(structurals);
    }
    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(json_data.size()));
}
BENCHMARK(BM_Fastjson_Turbo_IndexOnly_Decoupled_Char);
