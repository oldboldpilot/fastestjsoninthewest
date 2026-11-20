/**
 * member_index_benchmark -- what the top-level member index costs against a full parse of the same text.
 *
 * Usage: member_index_benchmark [rounds]      prints, per body size, the median nanoseconds per call and MB/s of
 *                                             index_members_into (one reused index), index_elements_into (the same
 *                                             text's `messages` array) and fastjson::parse.
 *
 * The body is the shape a large request has: a handful of short members (model, stream, max_tokens, ...), a nested
 * `messages` array and one long string that carries the padding, so the cost measured is mostly the walk over a long
 * string -- which the index crosses with the parser's SIMD string-end kernel (FASTJSON_SIMD caps the tier).
 *
 * Author: Olumuyiwa Oluwasanmi
 */
import std;
import fastjson;

namespace {

[[nodiscard]] auto bodyOf(std::size_t bytes) -> std::string {
    std::string text{"{\"model\":\"qwen3.8-27b\",\"stream\":false,\"max_tokens\":64,\"temperature\":0.7,\"messages\":[{\"role\":\"system\",\"content\":\"be brief\"},{\"role\":\"user\",\"content\":\""};
    const std::string tail = "\"}],\"stop\":[\"\\n\\n\"],\"user\":\"alice\"}";
    const std::string unit = "The quick brown fox jumps over the lazy dog, then \\\"quotes\\\" and a \\u00e9 and a tab\\t. ";
    while (text.size() + tail.size() < bytes) { text += unit; }
    return text + tail;
}

[[nodiscard]] auto median(std::vector<std::int64_t> v) -> std::int64_t {
    std::ranges::sort(v);
    return v[v.size() / 2];
}

template <class F>
[[nodiscard]] auto timeIt(int calls, F&& one) -> std::int64_t {
    for (int warm = 0; warm < 3; ++warm) { one(); }
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < calls; ++i) { one(); }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / calls;
}

}  // namespace

auto main(int argc, char** argv) -> int {
    const int rounds = argc > 1 ? std::atoi(argv[1]) : 5;
    std::println("member_index_benchmark: string-scan kernel '{}'", fastjson::string_scan_tier());
    struct Size { std::size_t bytes; int calls; };
    std::int64_t sink = 0;
    for (const Size size : {Size{1U << 10, 20000}, Size{1U << 20, 200}, Size{8U << 20, 25}}) {
        const std::string body = bodyOf(size.bytes);
        fastjson::member_index index;
        fastjson::member_index items;
        std::vector<std::int64_t> index_ns, element_ns, parse_ns;
        for (int round = 0; round < rounds; ++round) {
            index_ns.push_back(timeIt(size.calls, [&] {
                fastjson::index_members_into(index, body);
                sink += static_cast<std::int64_t>(index.members().size());
            }));
            const auto messages = index.find("messages")->value_in(body);
            element_ns.push_back(timeIt(size.calls, [&] {
                fastjson::index_elements_into(items, messages);
                sink += static_cast<std::int64_t>(items.members().size());
            }));
            parse_ns.push_back(timeIt(size.calls, [&] {
                const auto parsed = fastjson::parse(body);
                sink += parsed.has_value() ? 1 : 0;
            }));
        }
        const auto mbs = [&](std::int64_t ns) { return static_cast<double>(body.size()) * 1000.0 / static_cast<double>(std::max<std::int64_t>(ns, 1)); };
        std::println("{:>8} bytes: index_members {:>10} ns ({:>8.1f} MB/s) | index_elements(messages) {:>10} ns | parse {:>10} ns ({:>8.1f} MB/s) | parse/index {:.2f}x",
                     body.size(), median(index_ns), mbs(median(index_ns)), median(element_ns), median(parse_ns), mbs(median(parse_ns)),
                     static_cast<double>(median(parse_ns)) / static_cast<double>(std::max<std::int64_t>(median(index_ns), 1)));
    }
    return sink == 0 ? 1 : 0;
}
