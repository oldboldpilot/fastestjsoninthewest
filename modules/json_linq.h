#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <execution>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_reduce.h>
#include <tbb/parallel_scan.h>
#include <tbb/parallel_sort.h>

namespace fastjson {
namespace linq {

// ============================================================================
// LINQ Query Result - Lazy evaluation container (push-transducer pipeline)
// ============================================================================

template <typename T> class query_result {
public:
    using value_type = T;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;
    using sink_fn = std::function<bool(const T&)>;
    using stream_fn = std::function<void(const sink_fn&)>;

    query_result() = default;

    explicit query_result(std::vector<T>&& data)
        : data_(std::make_shared<std::vector<T>>(std::move(data))) {}

    explicit query_result(const std::vector<T>& data)
        : data_(std::make_shared<std::vector<T>>(data)) {}

    explicit query_result(const std::vector<T>* ext_ref) noexcept
        : ext_data_(ext_ref) {}

    explicit query_result(stream_fn generator) noexcept
        : generator_(std::move(generator)) {}

    // Internal lazy consumption driver: returns false from sink to early-terminate
    template <typename Sink>
    void consume(Sink&& sink) const {
        if (generator_) {
            generator_(sink_fn(std::forward<Sink>(sink)));
        } else if (ext_data_) {
            for (const auto& item : *ext_data_) {
                if (!sink(item)) break;
            }
        } else if (data_) {
            for (const auto& item : *data_) {
                if (!sink(item)) break;
            }
        }
    }

    bool is_lazy() const noexcept { return static_cast<bool>(generator_); }

    // Iterator support (materializes lazily on first random-access / iterator request)
    iterator begin() { return ensure_mutable().begin(); }

    iterator end() { return ensure_mutable().end(); }

    const_iterator begin() const { return ensure_materialized().begin(); }

    const_iterator end() const { return ensure_materialized().end(); }

    const_iterator cbegin() const { return ensure_materialized().cbegin(); }

    const_iterator cend() const { return ensure_materialized().cend(); }

    // Size and access
    size_t size() const { return count(); }

    bool empty() const { return !any(); }

    T& operator[](size_t idx) { return ensure_mutable()[idx]; }

    const T& operator[](size_t idx) const { return ensure_materialized()[idx]; }

    // LINQ operations - WHERE (lazy filter)
    template <typename Predicate> query_result<T> where(Predicate pred) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, pred = std::move(pred)](const sink_fn& downstream) {
            self.consume([&](const T& item) -> bool {
                if (pred(item)) {
                    return downstream(item);
                }
                return true;
            });
        }));
    }

    // Alias: filter (functional programming style)
    template <typename Predicate> query_result<T> filter(Predicate pred) const {
        return where(std::move(pred));
    }

    // LINQ operations - SELECT (lazy map/transform)
    template <typename Func>
    auto select(Func func) const -> query_result<decltype(func(std::declval<T>()))> {
        using R = decltype(func(std::declval<T>()));
        auto self = *this;
        return query_result<R>(typename query_result<R>::stream_fn(
            [self, func = std::move(func)](const typename query_result<R>::sink_fn& downstream) {
                self.consume([&](const T& item) -> bool {
                    return downstream(func(item));
                });
            }));
    }

    // Alias: map (functional programming style)
    template <typename Func>
    auto map(Func func) const -> query_result<decltype(func(std::declval<T>()))> {
        return select(std::move(func));
    }

    // FLAT_MAP / SELECT_MANY (lazy transform and flatten)
    template <typename Func>
    auto flat_map(Func func) const
        -> query_result<typename std::invoke_result_t<Func, const T&>::value_type> {
        using Inner = typename std::invoke_result_t<Func, const T&>::value_type;
        auto self = *this;
        return query_result<Inner>(typename query_result<Inner>::stream_fn(
            [self, func = std::move(func)](const typename query_result<Inner>::sink_fn& downstream) {
                self.consume([&](const T& item) -> bool {
                    auto inner = func(item);
                    for (const auto& elem : inner) {
                        if (!downstream(elem)) return false;
                    }
                    return true;
                });
            }));
    }

    template <typename Func>
    auto select_many(Func func) const
        -> query_result<typename std::invoke_result_t<Func, const T&>::value_type> {
        return flat_map(std::move(func));
    }

    // LINQ operations - ORDER BY (sort - deferred until consumed)
    template <typename KeySelector> query_result<T> order_by(KeySelector key_selector) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, key_selector = std::move(key_selector)](const sink_fn& downstream) {
            std::vector<T> sorted = self.to_vector();
            std::sort(sorted.begin(), sorted.end(), [&key_selector](const T& a, const T& b) {
                return key_selector(a) < key_selector(b);
            });
            for (const auto& item : sorted) {
                if (!downstream(item)) break;
            }
        }));
    }

    template <typename KeySelector>
    query_result<T> order_by_descending(KeySelector key_selector) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, key_selector = std::move(key_selector)](const sink_fn& downstream) {
            std::vector<T> sorted = self.to_vector();
            std::sort(sorted.begin(), sorted.end(), [&key_selector](const T& a, const T& b) {
                return key_selector(a) > key_selector(b);
            });
            for (const auto& item : sorted) {
                if (!downstream(item)) break;
            }
        }));
    }

    // REVERSE (deferred until consumed)
    query_result<T> reverse() const {
        auto self = *this;
        return query_result<T>(stream_fn([self](const sink_fn& downstream) {
            const auto& vec = self.ensure_materialized();
            for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                if (!downstream(*it)) break;
            }
        }));
    }

    // LINQ operations - FIRST, LAST, SINGLE (early-terminating!)
    std::optional<T> first() const {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            if (vec.empty()) return std::nullopt;
            return vec.front();
        }
        std::optional<T> res;
        consume([&res](const T& item) -> bool {
            res.emplace(item);
            return false;
        });
        return res;
    }

    std::optional<T> first_or_default(const T& default_value) const {
        auto res = first();
        return res.has_value() ? res : std::optional<T>(default_value);
    }

    template <typename Predicate> std::optional<T> first(Predicate pred) const {
        std::optional<T> res;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                res.emplace(item);
                return false;
            }
            return true;
        });
        return res;
    }

    std::optional<T> last() const {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            if (vec.empty()) return std::nullopt;
            return vec.back();
        }
        std::optional<T> res;
        consume([&res](const T& item) -> bool {
            res = item;
            return true;
        });
        return res;
    }

    template <typename Predicate> std::optional<T> last(Predicate pred) const {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                if (pred(*it)) return *it;
            }
            return std::nullopt;
        }
        std::optional<T> res;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                res = item;
            }
            return true;
        });
        return res;
    }

    std::optional<T> single() const {
        if (!generator_) {
            const auto& vec = ensure_materialized();
            if (vec.size() != 1) return std::nullopt;
            return vec.front();
        }
        std::optional<T> res;
        size_t seen = 0;
        consume([&](const T& item) -> bool {
            ++seen;
            if (seen == 1) {
                res.emplace(item);
                return true;
            }
            res.reset();
            return false;
        });
        return seen == 1 ? res : std::nullopt;
    }

    // FIND - Search for element matching predicate (early-terminating!)
    template <typename Predicate> std::optional<T> find(Predicate pred) const {
        return first(std::move(pred));
    }

    // FIND INDEX - Find index of first matching element (early-terminating!)
    template <typename Predicate> std::optional<size_t> find_index(Predicate pred) const {
        std::optional<size_t> found_idx;
        size_t idx = 0;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                found_idx = idx;
                return false;
            }
            ++idx;
            return true;
        });
        return found_idx;
    }

    // PREFIX_SUM (SCAN) - Lazy cumulative sum/operation
    template <typename Func> query_result<T> prefix_sum(Func func) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, func = std::move(func)](const sink_fn& downstream) {
            std::optional<T> acc;
            self.consume([&](const T& item) -> bool {
                if (!acc.has_value()) {
                    acc.emplace(item);
                } else {
                    acc = func(*acc, item);
                }
                return downstream(*acc);
            });
        }));
    }

    // PREFIX_SUM - Default addition
    query_result<T> prefix_sum() const {
        return prefix_sum([](const T& a, const T& b) { return a + b; });
    }

    // Alias: scan (functional programming style)
    template <typename Func> query_result<T> scan(Func func) const {
        return prefix_sum(std::move(func));
    }

    query_result<T> scan() const { return prefix_sum(); }

    // PREFIX_SUM with seed value (lazy!)
    template <typename Seed, typename Func>
    auto prefix_sum(Seed seed, Func func) const
        -> query_result<decltype(func(seed, std::declval<T>()))> {
        using R = decltype(func(seed, std::declval<T>()));
        auto self = *this;
        return query_result<R>(typename query_result<R>::stream_fn(
            [self, seed = std::move(seed), func = std::move(func)](
                const typename query_result<R>::sink_fn& downstream) {
                R accumulator = seed;
                if (!downstream(accumulator)) return;
                self.consume([&](const T& item) -> bool {
                    accumulator = func(accumulator, item);
                    return downstream(accumulator);
                });
            }));
    }

    // LINQ operations - ANY, ALL, COUNT (early-terminating!)
    template <typename Predicate> bool any(Predicate pred) const {
        bool found = false;
        consume([&](const T& item) -> bool {
            if (pred(item)) {
                found = true;
                return false;
            }
            return true;
        });
        return found;
    }

    bool any() const {
        if (!generator_) {
            return !ensure_materialized().empty();
        }
        bool found = false;
        consume([&](const T&) -> bool {
            found = true;
            return false;
        });
        return found;
    }

    template <typename Predicate> bool all(Predicate pred) const {
        bool all_match = true;
        consume([&](const T& item) -> bool {
            if (!pred(item)) {
                all_match = false;
                return false;
            }
            return true;
        });
        return all_match;
    }

    template <typename Predicate> size_t count(Predicate pred) const {
        size_t c = 0;
        consume([&](const T& item) -> bool {
            if (pred(item)) ++c;
            return true;
        });
        return c;
    }

    size_t count() const {
        if (!generator_) {
            return ensure_materialized().size();
        }
        size_t c = 0;
        consume([&](const T&) -> bool {
            ++c;
            return true;
        });
        return c;
    }

    // LINQ operations - AGGREGATE (fold/reduce)
    template <typename Func> std::optional<T> aggregate(Func func) const {
        std::optional<T> acc;
        consume([&](const T& item) -> bool {
            if (!acc.has_value()) {
                acc.emplace(item);
            } else {
                acc = func(*acc, item);
            }
            return true;
        });
        return acc;
    }

    template <typename Acc, typename Func> Acc aggregate(Acc seed, Func func) const {
        Acc acc = std::move(seed);
        consume([&](const T& item) -> bool {
            acc = func(acc, item);
            return true;
        });
        return acc;
    }

    // Alias: reduce (functional programming style)
    template <typename Func> std::optional<T> reduce(Func func) const {
        return aggregate(std::move(func));
    }

    template <typename Acc, typename Func> Acc reduce(Acc seed, Func func) const {
        return aggregate(std::move(seed), std::move(func));
    }

    template <typename Acc, typename Func> Acc fold(Acc seed, Func func) const {
        return aggregate(std::move(seed), std::move(func));
    }

    // FOREACH - Execute action for each element
    template <typename Action> void for_each(Action action) const {
        consume([&](const T& item) -> bool {
            action(item);
            return true;
        });
    }

    // Alias: forEach (JavaScript style)
    template <typename Action> void forEach(Action action) const { for_each(std::move(action)); }

    // LINQ operations - SUM, MIN, MAX, AVERAGE
    template <typename Selector>
    auto sum(Selector selector) const -> decltype(selector(std::declval<T>())) {
        using R = decltype(selector(std::declval<T>()));
        R acc{};
        consume([&](const T& item) -> bool {
            acc = acc + selector(item);
            return true;
        });
        return acc;
    }

    template <typename Selector>
    auto min(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        using R = decltype(selector(std::declval<T>()));
        std::optional<R> min_val;
        consume([&](const T& item) -> bool {
            R val = selector(item);
            if (!min_val.has_value() || val < *min_val) {
                min_val.emplace(std::move(val));
            }
            return true;
        });
        return min_val;
    }

    template <typename Selector>
    auto max(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        using R = decltype(selector(std::declval<T>()));
        std::optional<R> max_val;
        consume([&](const T& item) -> bool {
            R val = selector(item);
            if (!max_val.has_value() || val > *max_val) {
                max_val.emplace(std::move(val));
            }
            return true;
        });
        return max_val;
    }

    template <typename Selector> auto average(Selector selector) const -> std::optional<double> {
        double sum_val = 0.0;
        size_t cnt = 0;
        consume([&](const T& item) -> bool {
            sum_val += static_cast<double>(selector(item));
            ++cnt;
            return true;
        });
        if (cnt == 0) return std::nullopt;
        return sum_val / static_cast<double>(cnt);
    }

    // LINQ operations - DISTINCT (lazy, order-preserving)
    query_result<T> distinct() const {
        auto self = *this;
        return query_result<T>(stream_fn([self](const sink_fn& downstream) {
            if constexpr (requires(const T& a, const T& b) { a < b; }) {
                std::set<T> seen;
                self.consume([&](const T& item) -> bool {
                    if (seen.insert(item).second) {
                        return downstream(item);
                    }
                    return true;
                });
            } else {
                std::vector<T> seen;
                self.consume([&](const T& item) -> bool {
                    if (std::find(seen.begin(), seen.end(), item) == seen.end()) {
                        seen.push_back(item);
                        return downstream(item);
                    }
                    return true;
                });
            }
        }));
    }

    template <typename KeySelector> query_result<T> distinct_by(KeySelector key_selector) const {
        using K = decltype(key_selector(std::declval<T>()));
        auto self = *this;
        return query_result<T>(stream_fn([self, key_selector = std::move(key_selector)](const sink_fn& downstream) {
            if constexpr (requires(const K& a, const K& b) { a < b; }) {
                std::set<K> seen;
                self.consume([&](const T& item) -> bool {
                    if (seen.insert(key_selector(item)).second) {
                        return downstream(item);
                    }
                    return true;
                });
            } else {
                std::vector<K> seen;
                self.consume([&](const T& item) -> bool {
                    auto key = key_selector(item);
                    if (std::find(seen.begin(), seen.end(), key) == seen.end()) {
                        seen.push_back(std::move(key));
                        return downstream(item);
                    }
                    return true;
                });
            }
        }));
    }

    // LINQ operations - TAKE, SKIP (lazy with early termination!)
    query_result<T> take(size_t n) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, n](const sink_fn& downstream) {
            if (n == 0) return;
            size_t taken = 0;
            self.consume([&](const T& item) -> bool {
                if (!downstream(item)) return false;
                return ++taken < n;
            });
        }));
    }

    query_result<T> skip(size_t n) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, n](const sink_fn& downstream) {
            size_t skipped = 0;
            self.consume([&](const T& item) -> bool {
                if (skipped < n) {
                    ++skipped;
                    return true;
                }
                return downstream(item);
            });
        }));
    }

    template <typename Predicate> query_result<T> take_while(Predicate pred) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, pred = std::move(pred)](const sink_fn& downstream) {
            self.consume([&](const T& item) -> bool {
                if (!pred(item)) return false;
                return downstream(item);
            });
        }));
    }

    template <typename Predicate> query_result<T> skip_while(Predicate pred) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, pred = std::move(pred)](const sink_fn& downstream) {
            bool skipping = true;
            self.consume([&](const T& item) -> bool {
                if (skipping) {
                    if (pred(item)) return true;
                    skipping = false;
                }
                return downstream(item);
            });
        }));
    }

    // LINQ operations - GROUP BY
    template <typename KeySelector>
    auto group_by(KeySelector key_selector) const
        -> std::map<decltype(key_selector(std::declval<T>())), std::vector<T>> {
        using K = decltype(key_selector(std::declval<T>()));
        std::map<K, std::vector<T>> groups;
        consume([&](const T& item) -> bool {
            groups[key_selector(item)].push_back(item);
            return true;
        });
        return groups;
    }

    // LINQ operations - JOIN (lazy!)
    template <typename TOther, typename KeySelector1, typename KeySelector2,
              typename ResultSelector>
    auto join(const query_result<TOther>& other, KeySelector1 key_selector1,
              KeySelector2 key_selector2, ResultSelector result_selector) const
        -> query_result<decltype(result_selector(std::declval<T>(), std::declval<TOther>()))> {
        using R = decltype(result_selector(std::declval<T>(), std::declval<TOther>()));
        auto self = *this;
        return query_result<R>(typename query_result<R>::stream_fn(
            [self, other, key_selector1 = std::move(key_selector1),
             key_selector2 = std::move(key_selector2),
             result_selector = std::move(result_selector)](
                const typename query_result<R>::sink_fn& downstream) {
                const auto& other_vec = other.ensure_materialized();
                self.consume([&](const T& item1) -> bool {
                    auto key1 = key_selector1(item1);
                    for (const auto& item2 : other_vec) {
                        if (key1 == key_selector2(item2)) {
                            if (!downstream(result_selector(item1, item2))) return false;
                        }
                    }
                    return true;
                });
            }));
    }

    // ZIP - Combine two sequences element-wise (lazy with early termination!)
    template <typename TOther, typename ResultSelector>
    auto zip(const query_result<TOther>& other, ResultSelector result_selector) const
        -> query_result<decltype(result_selector(std::declval<T>(), std::declval<TOther>()))> {
        using R = decltype(result_selector(std::declval<T>(), std::declval<TOther>()));
        auto self = *this;
        return query_result<R>(typename query_result<R>::stream_fn(
            [self, other, result_selector = std::move(result_selector)](
                const typename query_result<R>::sink_fn& downstream) {
                const auto& other_vec = other.ensure_materialized();
                if (other_vec.empty()) return;
                size_t idx = 0;
                self.consume([&](const T& item) -> bool {
                    if (idx >= other_vec.size()) return false;
                    if (!downstream(result_selector(item, other_vec[idx]))) return false;
                    return ++idx < other_vec.size();
                });
            }));
    }

    // ZIP - Simple pair version
    template <typename TOther>
    auto zip(const query_result<TOther>& other) const -> query_result<std::pair<T, TOther>> {
        return zip(other, [](const T& a, const TOther& b) { return std::make_pair(a, b); });
    }

    // ZIP with vector (lazy!)
    template <typename TOther, typename ResultSelector>
    auto zip(const std::vector<TOther>& other, ResultSelector result_selector) const
        -> query_result<decltype(result_selector(std::declval<T>(), std::declval<TOther>()))> {
        return zip(query_result<TOther>(&other), std::move(result_selector));
    }

    // LINQ operations - CONCAT, UNION (lazy!)
    query_result<T> concat(const query_result<T>& other) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, other](const sink_fn& downstream) {
            bool keep_going = true;
            self.consume([&](const T& item) -> bool {
                if (!downstream(item)) {
                    keep_going = false;
                    return false;
                }
                return true;
            });
            if (keep_going) {
                other.consume([&](const T& item) -> bool {
                    return downstream(item);
                });
            }
        }));
    }

    query_result<T> union_with(const query_result<T>& other) const {
        return concat(other).distinct();
    }

    // LINQ operations - INTERSECT, EXCEPT (lazy!)
    query_result<T> intersect(const query_result<T>& other) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, other](const sink_fn& downstream) {
            const auto& other_vec = other.ensure_materialized();
            std::set<T> set2(other_vec.begin(), other_vec.end());
            std::set<T> emitted;
            self.consume([&](const T& item) -> bool {
                if (set2.find(item) != set2.end() && emitted.insert(item).second) {
                    return downstream(item);
                }
                return true;
            });
        }));
    }

    query_result<T> except(const query_result<T>& other) const {
        auto self = *this;
        return query_result<T>(stream_fn([self, other](const sink_fn& downstream) {
            const auto& other_vec = other.ensure_materialized();
            std::set<T> set2(other_vec.begin(), other_vec.end());
            std::set<T> emitted;
            self.consume([&](const T& item) -> bool {
                if (set2.find(item) == set2.end() && emitted.insert(item).second) {
                    return downstream(item);
                }
                return true;
            });
        }));
    }

    // Conversion
    std::vector<T> to_vector() const {
        if (!generator_) {
            if (ext_data_) return *ext_data_;
            if (data_) return *data_;
            return {};
        }
        std::vector<T> result;
        consume([&result](const T& item) -> bool {
            result.push_back(item);
            return true;
        });
        return result;
    }

    template <typename Container> Container to() const {
        const auto& vec = ensure_materialized();
        return Container(vec.begin(), vec.end());
    }

    const std::vector<T>& ensure_materialized() const {
        if (!data_) {
            if (ext_data_ && !generator_) {
                return *ext_data_;
            }
            auto vec = std::make_shared<std::vector<T>>();
            if (generator_) {
                generator_([&vec](const T& item) -> bool {
                    vec->push_back(item);
                    return true;
                });
                generator_ = nullptr;
            }
            data_ = std::move(vec);
        }
        return *data_;
    }

private:
    std::vector<T>& ensure_mutable() {
        if (!data_) {
            auto vec = std::make_shared<std::vector<T>>();
            if (generator_) {
                generator_([&vec](const T& item) -> bool {
                    vec->push_back(item);
                    return true;
                });
                generator_ = nullptr;
            } else if (ext_data_) {
                *vec = *ext_data_;
                ext_data_ = nullptr;
            }
            data_ = std::move(vec);
        } else if (data_.use_count() > 1) {
            data_ = std::make_shared<std::vector<T>>(*data_);
        }
        return *data_;
    }

    mutable std::shared_ptr<std::vector<T>> data_;
    mutable const std::vector<T>* ext_data_ = nullptr;
    mutable stream_fn generator_;
};

// ============================================================================
// Parallel LINQ Query Result - Thread-safe parallel operations
// ============================================================================

template <typename T> class parallel_query_result {
public:
    using value_type = T;

    parallel_query_result() = default;

    explicit parallel_query_result(std::vector<T>&& data) : data_(std::move(data)) {}

    explicit parallel_query_result(const std::vector<T>& data) : data_(data) {}

    // Parallel WHERE (thread-safe byte flags - zero std::vector<bool> bit-packing data race)
    template <typename Predicate> parallel_query_result<T> where(Predicate pred) const {
        if (data_.empty()) {
            return parallel_query_result<T>(std::vector<T>{});
        }
        auto keep = std::make_unique_for_overwrite<uint8_t[]>(data_.size());

        tbb::parallel_for(tbb::blocked_range<size_t>(0, data_.size()),
                          [&](const tbb::blocked_range<size_t>& r) {
                              for (size_t i = r.begin(); i < r.end(); ++i) {
                                  keep[i] = pred(data_[i]) ? uint8_t{1} : uint8_t{0};
                              }
                          });

        // Sequential compaction
        std::vector<T> compacted;
        compacted.reserve(data_.size());
        for (size_t i = 0; i < data_.size(); ++i) {
            if (keep[i] != 0) {
                compacted.push_back(data_[i]);
            }
        }

        return parallel_query_result<T>(std::move(compacted));
    }

    // Alias: filter (functional programming style)
    template <typename Predicate> parallel_query_result<T> filter(Predicate pred) const {
        return where(std::move(pred));
    }

    // Parallel SELECT
    template <typename Func>
    auto select(Func func) const -> parallel_query_result<decltype(func(std::declval<T>()))> {
        using R = decltype(func(std::declval<T>()));
        std::vector<R> result(data_.size());

        tbb::parallel_for(tbb::blocked_range<size_t>(0, data_.size()),
                          [&](const tbb::blocked_range<size_t>& r) {
                              for (size_t i = r.begin(); i < r.end(); ++i) {
                                  result[i] = func(data_[i]);
                              }
                          });

        return parallel_query_result<R>(std::move(result));
    }

    // Alias: map (functional programming style)
    template <typename Func>
    auto map(Func func) const -> parallel_query_result<decltype(func(std::declval<T>()))> {
        return select(std::move(func));
    }

    // Parallel AGGREGATE
    template <typename Acc, typename Func> Acc aggregate(Acc seed, Func func) const {
        if (data_.empty()) return seed;
        return tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, data_.size()), seed,
            [&](const tbb::blocked_range<size_t>& r, Acc local_acc) -> Acc {
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    local_acc = func(local_acc, data_[i]);
                }
                return local_acc;
            },
            [&](Acc a, Acc b) -> Acc { return func(a, b); });
    }

    // Alias: reduce (functional programming style)
    template <typename Acc, typename Func> Acc reduce(Acc seed, Func func) const {
        return aggregate(std::move(seed), std::move(func));
    }

    // Parallel FOREACH - Execute action for each element
    template <typename Action> void for_each(Action action) const {
        tbb::parallel_for(tbb::blocked_range<size_t>(0, data_.size()),
                          [&](const tbb::blocked_range<size_t>& r) {
                              for (size_t i = r.begin(); i < r.end(); ++i) {
                                  action(data_[i]);
                              }
                          });
    }

    // Alias: forEach (JavaScript style)
    template <typename Action> void forEach(Action action) const { for_each(std::move(action)); }

    // Parallel PREFIX_SUM (SCAN) - Parallel cumulative operation via TBB parallel_scan
    template <typename Func> parallel_query_result<T> prefix_sum(Func func) const {
        if (data_.empty()) return parallel_query_result<T>(std::vector<T>());
        if (data_.size() == 1) return parallel_query_result<T>(std::vector<T>(data_));

        std::vector<T> result(data_.size());
        tbb::parallel_scan(
            tbb::blocked_range<size_t>(0, data_.size()),
            std::optional<T>{},
            [&](const tbb::blocked_range<size_t>& r, std::optional<T> acc, bool is_final_scan) -> std::optional<T> {
                for (size_t i = r.begin(); i < r.end(); ++i) {
                    if (!acc.has_value()) {
                        acc.emplace(data_[i]);
                    } else {
                        acc = func(*acc, data_[i]);
                    }
                    if (is_final_scan) {
                        result[i] = *acc;
                    }
                }
                return acc;
            },
            [&](const std::optional<T>& left, const std::optional<T>& right) -> std::optional<T> {
                if (!left.has_value()) return right;
                if (!right.has_value()) return left;
                return std::optional<T>(func(*left, *right));
            });
        return parallel_query_result<T>(std::move(result));
    }

    // Parallel PREFIX_SUM - Default addition
    parallel_query_result<T> prefix_sum() const {
        return prefix_sum([](const T& a, const T& b) { return a + b; });
    }

    // Alias: scan (functional programming style)
    template <typename Func> parallel_query_result<T> scan(Func func) const {
        return prefix_sum(std::move(func));
    }

    parallel_query_result<T> scan() const { return prefix_sum(); }

    // Parallel SUM
    template <typename Selector>
    auto sum(Selector selector) const -> decltype(selector(std::declval<T>())) {
        using R = decltype(selector(std::declval<T>()));
        R result = R{};
        if (data_.empty()) return result;

        return tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, data_.size()), result,
            [&](const tbb::blocked_range<size_t>& r, R local_val) -> R {
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    local_val += selector(data_[i]);
                }
                return local_val;
            },
            std::plus<R>());
    }

    // Parallel MIN
    template <typename Selector>
    auto min(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        if (data_.empty()) return std::nullopt;

        using R = decltype(selector(std::declval<T>()));
        R min_val = selector(data_[0]);

        return tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, data_.size()), min_val,
            [&](const tbb::blocked_range<size_t>& r, R local_min) -> R {
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    R val = selector(data_[i]);
                    if (val < local_min) local_min = val;
                }
                return local_min;
            },
            [](R a, R b) { return std::min(a, b); });
    }

    // Parallel MAX
    template <typename Selector>
    auto max(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        if (data_.empty()) return std::nullopt;

        using R = decltype(selector(std::declval<T>()));
        R max_val = selector(data_[0]);

        return tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, data_.size()), max_val,
            [&](const tbb::blocked_range<size_t>& r, R local_max) -> R {
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    R val = selector(data_[i]);
                    if (val > local_max) local_max = val;
                }
                return local_max;
            },
            [](R a, R b) { return std::max(a, b); });
    }

    // Parallel COUNT
    template <typename Predicate> size_t count(Predicate pred) const {
        if (data_.empty()) return 0;
        return tbb::parallel_reduce(
            tbb::blocked_range<size_t>(0, data_.size()), size_t{0},
            [&](const tbb::blocked_range<size_t>& r, size_t local_count) -> size_t {
                for (size_t i = r.begin(); i != r.end(); ++i) {
                    if (pred(data_[i])) {
                        local_count++;
                    }
                }
                return local_count;
            },
            std::plus<size_t>());
    }

    // Parallel ANY
    template <typename Predicate> bool any(Predicate pred) const {
        if (data_.empty()) return false;
        std::atomic<bool> found{false};

        tbb::parallel_for(tbb::blocked_range<size_t>(0, data_.size()),
                          [&](const tbb::blocked_range<size_t>& r) {
                              for (size_t i = r.begin(); i != r.end(); ++i) {
                                  if (found.load(std::memory_order_relaxed)) break;
                                  if (pred(data_[i])) {
                                      found.store(true, std::memory_order_relaxed);
                                      break;
                                  }
                              }
                          });

        return found.load(std::memory_order_relaxed);
    }

    // Parallel ALL
    template <typename Predicate> bool all(Predicate pred) const {
        if (data_.empty()) return true;
        std::atomic<bool> all_true{true};

        tbb::parallel_for(tbb::blocked_range<size_t>(0, data_.size()),
                          [&](const tbb::blocked_range<size_t>& r) {
                              for (size_t i = r.begin(); i != r.end(); ++i) {
                                  if (!all_true.load(std::memory_order_relaxed)) break;
                                  if (!pred(data_[i])) {
                                      all_true.store(false, std::memory_order_relaxed);
                                      break;
                                  }
                              }
                          });

        return all_true.load(std::memory_order_relaxed);
    }

    // Parallel ORDER BY (parallel sort)
    template <typename KeySelector>
    parallel_query_result<T> order_by(KeySelector key_selector) const {
        std::vector<T> result = data_;
        tbb::parallel_sort(result.begin(), result.end(),
                           [&key_selector](const T& a, const T& b) {
                               return key_selector(a) < key_selector(b);
                           });
        return parallel_query_result<T>(std::move(result));
    }

    // Convert to sequential query
    query_result<T> as_sequential() const { return query_result<T>(std::vector<T>(data_)); }

    // Conversion
    std::vector<T> to_vector() const { return data_; }

    size_t size() const { return data_.size(); }

    bool empty() const { return data_.empty(); }

private:
    std::vector<T> data_;
};

// ============================================================================
// Helper functions to create query_result from containers (zero-copy for lvalues)
// ============================================================================

template <typename T> query_result<T> from(const std::vector<T>& vec) noexcept {
    return query_result<T>(&vec);
}

template <typename T> query_result<T> from(std::vector<T>&& vec) {
    return query_result<T>(std::move(vec));
}

template <typename T> parallel_query_result<T> from_parallel(const std::vector<T>& vec) {
    return parallel_query_result<T>(vec);
}

template <typename T> parallel_query_result<T> from_parallel(std::vector<T>&& vec) {
    return parallel_query_result<T>(std::move(vec));
}

}  // namespace linq
}  // namespace fastjson
