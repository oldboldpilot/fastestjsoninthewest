// json_linq.cppm - LINQ-style operations for JSON data
// C++23 module for functional programming with JSON

module;

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <memory>
#include <numeric>
#include <optional>
#include <unordered_set>
#include <vector>
#include <omp.h>

export module json_linq;

export namespace fastjson {
namespace linq {

// ============================================================================
// LINQ Query Result - Lazy evaluation container
// ============================================================================

template <typename T> class query_result {
public:
    using value_type = T;
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    query_result() = default;

    explicit query_result(std::vector<T>&& data) : data_(std::move(data)) {}

    // Iterator support
    iterator begin() { return data_.begin(); }

    iterator end() { return data_.end(); }

    const_iterator begin() const { return data_.begin(); }

    const_iterator end() const { return data_.end(); }

    const_iterator cbegin() const { return data_.cbegin(); }

    const_iterator cend() const { return data_.cend(); }

    // Size and access
    size_t size() const { return data_.size(); }

    bool empty() const { return data_.empty(); }

    T& operator[](size_t idx) { return data_[idx]; }

    const T& operator[](size_t idx) const { return data_[idx]; }

    // LINQ operations - WHERE (filter)
    template <typename Predicate> query_result<T> where(Predicate pred) const {
        std::vector<T> result;
        result.reserve(data_.size());
        for (const auto& item : data_) {
            if (pred(item)) {
                result.push_back(item);
            }
        }
        return query_result<T>(std::move(result));
    }

    // Alias: filter (functional programming style)
    template <typename Predicate> query_result<T> filter(Predicate pred) const {
        return where(pred);
    }

    // LINQ operations - SELECT (map/transform)
    template <typename Func>
    auto select(Func func) const -> query_result<decltype(func(std::declval<T>()))> {
        using R = decltype(func(std::declval<T>()));
        std::vector<R> result;
        result.reserve(data_.size());
        for (const auto& item : data_) {
            result.push_back(func(item));
        }
        return query_result<R>(std::move(result));
    }

    // Alias: map (functional programming style)
    template <typename Func>
    auto map(Func func) const -> query_result<decltype(func(std::declval<T>()))> {
        return select(func);
    }

    // LINQ operations - ORDER BY (sort)
    template <typename KeySelector> query_result<T> order_by(KeySelector key_selector) const {
        std::vector<T> result = data_;
        std::sort(result.begin(), result.end(), [&key_selector](const T& a, const T& b) {
            return key_selector(a) < key_selector(b);
        });
        return query_result<T>(std::move(result));
    }

    template <typename KeySelector>
    query_result<T> order_by_descending(KeySelector key_selector) const {
        std::vector<T> result = data_;
        std::sort(result.begin(), result.end(), [&key_selector](const T& a, const T& b) {
            return key_selector(a) > key_selector(b);
        });
        return query_result<T>(std::move(result));
    }

    // LINQ operations - FIRST, LAST, SINGLE
    std::optional<T> first() const {
        if (data_.empty())
            return std::nullopt;
        return data_.front();
    }

    std::optional<T> first_or_default(const T& default_value) const {
        if (data_.empty())
            return default_value;
        return data_.front();
    }

    template <typename Predicate> std::optional<T> first(Predicate pred) const {
        for (const auto& item : data_) {
            if (pred(item))
                return item;
        }
        return std::nullopt;
    }

    std::optional<T> last() const {
        if (data_.empty())
            return std::nullopt;
        return data_.back();
    }

    template <typename Predicate> std::optional<T> last(Predicate pred) const {
        for (auto it = data_.rbegin(); it != data_.rend(); ++it) {
            if (pred(*it))
                return *it;
        }
        return std::nullopt;
    }

    std::optional<T> single() const {
        if (data_.size() != 1)
            return std::nullopt;
        return data_.front();
    }

    // FIND - Search for element matching predicate
    template <typename Predicate> std::optional<T> find(Predicate pred) const {
        auto it = std::find_if(data_.begin(), data_.end(), pred);
        if (it != data_.end()) {
            return *it;
        }
        return std::nullopt;
    }

    // FIND INDEX - Find index of first matching element
    template <typename Predicate> std::optional<size_t> find_index(Predicate pred) const {
        for (size_t i = 0; i < data_.size(); ++i) {
            if (pred(data_[i])) {
                return i;
            }
        }
        return std::nullopt;
    }

    // PREFIX_SUM (SCAN) - Cumulative sum/operation
    template <typename Func> query_result<T> prefix_sum(Func func) const {
        if (data_.empty())
            return query_result<T>();

        std::vector<T> result;
        result.reserve(data_.size());
        result.push_back(data_[0]);

        for (size_t i = 1; i < data_.size(); ++i) {
            result.push_back(func(result[i - 1], data_[i]));
        }

        return query_result<T>(std::move(result));
    }

    // PREFIX_SUM - Default addition
    query_result<T> prefix_sum() const {
        return prefix_sum([](const T& a, const T& b) { return a + b; });
    }

    // Alias: scan (functional programming style)
    template <typename Func> query_result<T> scan(Func func) const { return prefix_sum(func); }

    query_result<T> scan() const { return prefix_sum(); }

    // PREFIX_SUM with seed value
    template <typename Seed, typename Func>
    auto prefix_sum(Seed seed, Func func) const
        -> query_result<decltype(func(seed, std::declval<T>()))> {
        using R = decltype(func(seed, std::declval<T>()));
        std::vector<R> result;
        result.reserve(data_.size() + 1);
        result.push_back(seed);

        R accumulator = seed;
        for (const auto& item : data_) {
            accumulator = func(accumulator, item);
            result.push_back(accumulator);
        }

        return query_result<R>(std::move(result));
    }

    // LINQ operations - ANY, ALL, COUNT
    template <typename Predicate> bool any(Predicate pred) const {
        return std::any_of(data_.begin(), data_.end(), pred);
    }

    bool any() const { return !data_.empty(); }

    template <typename Predicate> bool all(Predicate pred) const {
        return std::all_of(data_.begin(), data_.end(), pred);
    }

    template <typename Predicate> size_t count(Predicate pred) const {
        return std::count_if(data_.begin(), data_.end(), pred);
    }

    size_t count() const { return data_.size(); }

    // LINQ operations - AGGREGATE (fold/reduce)
    template <typename Func> std::optional<T> aggregate(Func func) const {
        if (data_.empty())
            return std::nullopt;
        return std::accumulate(data_.begin() + 1, data_.end(), data_[0], func);
    }

    template <typename Acc, typename Func> Acc aggregate(Acc seed, Func func) const {
        return std::accumulate(data_.begin(), data_.end(), seed, func);
    }

    // Alias: reduce (functional programming style)
    template <typename Func> std::optional<T> reduce(Func func) const { return aggregate(func); }

    template <typename Acc, typename Func> Acc reduce(Acc seed, Func func) const {
        return aggregate(seed, func);
    }

    // FOREACH - Execute action for each element
    template <typename Action> void for_each(Action action) const {
        for (const auto& item : data_) {
            action(item);
        }
    }

    // Alias: forEach (JavaScript style)
    template <typename Action> void forEach(Action action) const { for_each(action); }

    // LINQ operations - SUM, MIN, MAX, AVERAGE
    template <typename Selector>
    auto sum(Selector selector) const -> decltype(selector(std::declval<T>())) {
        using R = decltype(selector(std::declval<T>()));
        return std::accumulate(data_.begin(), data_.end(), R{},
                               [&selector](R acc, const T& item) { return acc + selector(item); });
    }

    template <typename Selector>
    auto min(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        if (data_.empty())
            return std::nullopt;
        using R = decltype(selector(std::declval<T>()));
        R min_val = selector(data_[0]);
        for (size_t i = 1; i < data_.size(); ++i) {
            R val = selector(data_[i]);
            if (val < min_val)
                min_val = val;
        }
        return min_val;
    }

    template <typename Selector>
    auto max(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        if (data_.empty())
            return std::nullopt;
        using R = decltype(selector(std::declval<T>()));
        R max_val = selector(data_[0]);
        for (size_t i = 1; i < data_.size(); ++i) {
            R val = selector(data_[i]);
            if (val > max_val)
                max_val = val;
        }
        return max_val;
    }

    template <typename Selector> auto average(Selector selector) const -> std::optional<double> {
        if (data_.empty())
            return std::nullopt;
        using R = decltype(selector(std::declval<T>()));
        R sum_val = sum(selector);
        return static_cast<double>(sum_val) / data_.size();
    }

    // LINQ operations - DISTINCT
    query_result<T> distinct() const {
        std::unordered_set<T> unique(data_.begin(), data_.end());
        std::vector<T> result(unique.begin(), unique.end());
        return query_result<T>(std::move(result));
    }

    template <typename KeySelector> query_result<T> distinct_by(KeySelector key_selector) const {
        std::vector<T> result;
        std::unordered_set<decltype(key_selector(std::declval<T>()))> seen;
        for (const auto& item : data_) {
            auto key = key_selector(item);
            if (seen.insert(key).second) {
                result.push_back(item);
            }
        }
        return query_result<T>(std::move(result));
    }

    // LINQ operations - TAKE, SKIP
    query_result<T> take(size_t n) const {
        size_t count = std::min(n, data_.size());
        std::vector<T> result(data_.begin(), data_.begin() + count);
        return query_result<T>(std::move(result));
    }

    query_result<T> skip(size_t n) const {
        if (n >= data_.size())
            return query_result<T>();
        std::vector<T> result(data_.begin() + n, data_.end());
        return query_result<T>(std::move(result));
    }

    template <typename Predicate> query_result<T> take_while(Predicate pred) const {
        std::vector<T> result;
        for (const auto& item : data_) {
            if (!pred(item))
                break;
            result.push_back(item);
        }
        return query_result<T>(std::move(result));
    }

    template <typename Predicate> query_result<T> skip_while(Predicate pred) const {
        std::vector<T> result;
        bool skipping = true;
        for (const auto& item : data_) {
            if (skipping && pred(item))
                continue;
            skipping = false;
            result.push_back(item);
        }
        return query_result<T>(std::move(result));
    }

    // LINQ operations - GROUP BY
    template <typename KeySelector>
    auto group_by(KeySelector key_selector) const
        -> std::unordered_map<decltype(key_selector(std::declval<T>())), std::vector<T>> {
        using K = decltype(key_selector(std::declval<T>()));
        std::unordered_map<K, std::vector<T>> groups;
        for (const auto& item : data_) {
            groups[key_selector(item)].push_back(item);
        }
        return groups;
    }

    // LINQ operations - JOIN
    template <typename TOther, typename KeySelector1, typename KeySelector2,
              typename ResultSelector>
    auto join(const query_result<TOther>& other, KeySelector1 key_selector1,
              KeySelector2 key_selector2, ResultSelector result_selector) const
        -> query_result<decltype(result_selector(std::declval<T>(), std::declval<TOther>()))> {
        using R = decltype(result_selector(std::declval<T>(), std::declval<TOther>()));
        std::vector<R> result;

        for (const auto& item1 : data_) {
            auto key1 = key_selector1(item1);
            for (const auto& item2 : other) {
                auto key2 = key_selector2(item2);
                if (key1 == key2) {
                    result.push_back(result_selector(item1, item2));
                }
            }
        }
        return query_result<R>(std::move(result));
    }

    // ZIP - Combine two sequences element-wise
    template <typename TOther, typename ResultSelector>
    auto zip(const query_result<TOther>& other, ResultSelector result_selector) const
        -> query_result<decltype(result_selector(std::declval<T>(), std::declval<TOther>()))> {
        using R = decltype(result_selector(std::declval<T>(), std::declval<TOther>()));
        std::vector<R> result;
        size_t min_size = std::min(data_.size(), other.size());
        result.reserve(min_size);

        for (size_t i = 0; i < min_size; ++i) {
            result.push_back(result_selector(data_[i], other[i]));
        }
        return query_result<R>(std::move(result));
    }

    // ZIP - Simple pair version
    template <typename TOther>
    auto zip(const query_result<TOther>& other) const -> query_result<std::pair<T, TOther>> {
        return zip(other, [](const T& a, const TOther& b) { return std::make_pair(a, b); });
    }

    // ZIP with vector
    template <typename TOther, typename ResultSelector>
    auto zip(const std::vector<TOther>& other, ResultSelector result_selector) const
        -> query_result<decltype(result_selector(std::declval<T>(), std::declval<TOther>()))> {
        using R = decltype(result_selector(std::declval<T>(), std::declval<TOther>()));
        std::vector<R> result;
        size_t min_size = std::min(data_.size(), other.size());
        result.reserve(min_size);

        for (size_t i = 0; i < min_size; ++i) {
            result.push_back(result_selector(data_[i], other[i]));
        }
        return query_result<R>(std::move(result));
    }

    // LINQ operations - CONCAT, UNION
    query_result<T> concat(const query_result<T>& other) const {
        std::vector<T> result = data_;
        result.insert(result.end(), other.begin(), other.end());
        return query_result<T>(std::move(result));
    }

    query_result<T> union_with(const query_result<T>& other) const {
        std::unordered_set<T> unique(data_.begin(), data_.end());
        unique.insert(other.begin(), other.end());
        std::vector<T> result(unique.begin(), unique.end());
        return query_result<T>(std::move(result));
    }

    // LINQ operations - INTERSECT, EXCEPT
    query_result<T> intersect(const query_result<T>& other) const {
        std::unordered_set<T> set1(data_.begin(), data_.end());
        std::unordered_set<T> set2(other.begin(), other.end());
        std::vector<T> result;
        for (const auto& item : set1) {
            if (set2.find(item) != set2.end()) {
                result.push_back(item);
            }
        }
        return query_result<T>(std::move(result));
    }

    query_result<T> except(const query_result<T>& other) const {
        std::unordered_set<T> set1(data_.begin(), data_.end());
        std::unordered_set<T> set2(other.begin(), other.end());
        std::vector<T> result;
        for (const auto& item : set1) {
            if (set2.find(item) == set2.end()) {
                result.push_back(item);
            }
        }
        return query_result<T>(std::move(result));
    }

    // Conversion
    std::vector<T> to_vector() const { return data_; }

    template <typename Container> Container to() const {
        return Container(data_.begin(), data_.end());
    }

private:
    std::vector<T> data_;
};

// ============================================================================
// Parallel LINQ Query Result - Thread-safe parallel operations
// ============================================================================

template <typename T> class parallel_query_result {
public:
    using value_type = T;

    explicit parallel_query_result(std::vector<T>&& data) : data_(std::move(data)) {}

    // Parallel WHERE
    template <typename Predicate> parallel_query_result<T> where(Predicate pred) const {
        std::vector<T> result(data_.size());
        std::vector<bool> keep(data_.size());

#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < data_.size(); ++i) {
            keep[i] = pred(data_[i]);
        }

        // Sequential compaction
        std::vector<T> compacted;
        compacted.reserve(data_.size());
        for (size_t i = 0; i < data_.size(); ++i) {
            if (keep[i]) {
                compacted.push_back(data_[i]);
            }
        }

        return parallel_query_result<T>(std::move(compacted));
    }

    // Alias: filter (functional programming style)
    template <typename Predicate> parallel_query_result<T> filter(Predicate pred) const {
        return where(pred);
    }

    // Parallel SELECT
    template <typename Func>
    auto select(Func func) const -> parallel_query_result<decltype(func(std::declval<T>()))> {
        using R = decltype(func(std::declval<T>()));
        std::vector<R> result(data_.size());

#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < data_.size(); ++i) {
            result[i] = func(data_[i]);
        }

        return parallel_query_result<R>(std::move(result));
    }

    // Alias: map (functional programming style)
    template <typename Func>
    auto map(Func func) const -> parallel_query_result<decltype(func(std::declval<T>()))> {
        return select(func);
    }

    // Parallel AGGREGATE
    template <typename Acc, typename Func> Acc aggregate(Acc seed, Func func) const {
        Acc result = seed;

#pragma omp parallel for reduction(+ : result)
        for (size_t i = 0; i < data_.size(); ++i) {
            result = func(result, data_[i]);
        }

        return result;
    }

    // Alias: reduce (functional programming style)
    template <typename Acc, typename Func> Acc reduce(Acc seed, Func func) const {
        return aggregate(seed, func);
    }

    // Parallel FOREACH - Execute action for each element
    template <typename Action> void for_each(Action action) const {
#pragma omp parallel for schedule(static)
        for (size_t i = 0; i < data_.size(); ++i) {
            action(data_[i]);
        }
    }

    // Alias: forEach (JavaScript style)
    template <typename Action> void forEach(Action action) const { for_each(action); }

    // Parallel PREFIX_SUM (SCAN) - Parallel cumulative operation
    template <typename Func> parallel_query_result<T> prefix_sum(Func func) const {
        if (data_.empty())
            return parallel_query_result<T>(std::vector<T>());
        if (data_.size() == 1)
            return parallel_query_result<T>(std::vector<T>(data_));

        std::vector<T> result(data_.size());
        result[0] = data_[0];

        // Parallel prefix sum using OpenMP
        // Phase 1: Compute block-wise prefix sums
        int num_threads = omp_get_max_threads();
        size_t block_size = (data_.size() + num_threads - 1) / num_threads;
        std::vector<T> block_sums(num_threads);

#pragma omp parallel
        {
            int tid = omp_get_thread_num();
            size_t start = tid * block_size;
            size_t end = std::min(start + block_size, data_.size());

            if (start < end) {
                T local_sum = (start == 0) ? data_[0] : data_[start];
                result[start] = local_sum;

                for (size_t i = start + 1; i < end; ++i) {
                    local_sum = func(local_sum, data_[i]);
                    result[i] = local_sum;
                }

                block_sums[tid] = local_sum;
            }
        }

        // Phase 2: Compute prefix of block sums (sequential)
        // block_offsets[i] contains the cumulative value up to (but not including) block i
        std::vector<T> block_offsets(num_threads);
        if (num_threads > 0 && !data_.empty()) {
            // First block has no offset (starts from beginning)
            // For subsequent blocks, offset is cumulative of all previous blocks
            for (int i = 0; i < num_threads; ++i) {
                if (i == 0) {
                    block_offsets[0] = T{};  // Not used, first block needs no adjustment
                } else {
                    // Cumulative value is result of applying func to all previous block sums
                    if (i == 1) {
                        block_offsets[1] = block_sums[0];
                    } else {
                        block_offsets[i] = func(block_offsets[i - 1], block_sums[i - 1]);
                    }
                }
            }
        }

// Phase 3: Apply block offsets to results (skip first block)
#pragma omp parallel
        {
            int tid = omp_get_thread_num();
            if (tid > 0) {
                size_t start = tid * block_size;
                size_t end = std::min(start + block_size, data_.size());

                for (size_t i = start; i < end; ++i) {
                    result[i] = func(block_offsets[tid], result[i]);
                }
            }
        }

        return parallel_query_result<T>(std::move(result));
    }

    // Parallel PREFIX_SUM - Default addition
    parallel_query_result<T> prefix_sum() const {
        return prefix_sum([](const T& a, const T& b) { return a + b; });
    }

    // Alias: scan (functional programming style)
    template <typename Func> parallel_query_result<T> scan(Func func) const {
        return prefix_sum(func);
    }

    parallel_query_result<T> scan() const { return prefix_sum(); }

    // Parallel COUNT
    template <typename Selector>
    auto sum(Selector selector) const -> decltype(selector(std::declval<T>())) {
        using R = decltype(selector(std::declval<T>()));
        R result = R{};

#pragma omp parallel for reduction(+ : result)
        for (size_t i = 0; i < data_.size(); ++i) {
            result += selector(data_[i]);
        }

        return result;
    }

    // Parallel MIN
    template <typename Selector>
    auto min(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        if (data_.empty())
            return std::nullopt;

        using R = decltype(selector(std::declval<T>()));
        R min_val = selector(data_[0]);

#pragma omp parallel
        {
            R local_min = min_val;
#pragma omp for nowait
            for (size_t i = 0; i < data_.size(); ++i) {
                R val = selector(data_[i]);
                if (val < local_min)
                    local_min = val;
            }

#pragma omp critical
            {
                if (local_min < min_val)
                    min_val = local_min;
            }
        }

        return min_val;
    }

    // Parallel MAX
    template <typename Selector>
    auto max(Selector selector) const -> std::optional<decltype(selector(std::declval<T>()))> {
        if (data_.empty())
            return std::nullopt;

        using R = decltype(selector(std::declval<T>()));
        R max_val = selector(data_[0]);

#pragma omp parallel
        {
            R local_max = max_val;
#pragma omp for nowait
            for (size_t i = 0; i < data_.size(); ++i) {
                R val = selector(data_[i]);
                if (val > local_max)
                    local_max = val;
            }

#pragma omp critical
            {
                if (local_max > max_val)
                    max_val = local_max;
            }
        }

        return max_val;
    }

    // Parallel COUNT
    template <typename Predicate> size_t count(Predicate pred) const {
        size_t result = 0;

#pragma omp parallel for reduction(+ : result)
        for (size_t i = 0; i < data_.size(); ++i) {
            if (pred(data_[i])) {
                result++;
            }
        }

        return result;
    }

    // Parallel ANY
    template <typename Predicate> bool any(Predicate pred) const {
        bool found = false;

#pragma omp parallel for shared(found)
        for (size_t i = 0; i < data_.size(); ++i) {
            if (found)
                continue;
            if (pred(data_[i])) {
#pragma omp critical
                { found = true; }
            }
        }

        return found;
    }

    // Parallel ALL
    template <typename Predicate> bool all(Predicate pred) const {
        bool all_true = true;

#pragma omp parallel for shared(all_true)
        for (size_t i = 0; i < data_.size(); ++i) {
            if (!all_true)
                continue;
            if (!pred(data_[i])) {
#pragma omp critical
                { all_true = false; }
            }
        }

        return all_true;
    }

    // Parallel ORDER BY (parallel sort)
    template <typename KeySelector>
    parallel_query_result<T> order_by(KeySelector key_selector) const {
        std::vector<T> result = data_;

// Use parallel sort with OpenMP tasks
#pragma omp parallel
        {
#pragma omp single
            {
                std::sort(result.begin(), result.end(), [&key_selector](const T& a, const T& b) {
                    return key_selector(a) < key_selector(b);
                });
            }
        }

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
// Helper function to create query_result from JSON array
// ============================================================================

// Forward declaration - will be specialized for json_value
template <typename T> query_result<T> from(const std::vector<T>& vec) {
    return query_result<T>(std::vector<T>(vec));
}

template <typename T> parallel_query_result<T> from_parallel(const std::vector<T>& vec) {
    return parallel_query_result<T>(std::vector<T>(vec));
}

}  // namespace linq
}  // namespace fastjson
