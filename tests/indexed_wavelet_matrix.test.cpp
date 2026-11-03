// Source: https://github.com/tute7627/library_ai/blob/main/tests/indexed_wavelet_matrix.test.cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
// competitive-verifier: STANDALONE
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

struct weight {
    long long sum, progress;
    std::uint64_t bits;
    weight() = delete;
    weight(long long s, long long p, std::uint64_t b) : sum(s), progress(p), bits(b) {}
    bool operator==(const weight& other) const {
        return sum == other.sum && progress == other.progress && bits == other.bits;
    }
};

struct group {
    static weight identity() { return {0, 0, 0}; }
    static weight op(const weight& a, const weight& b) {
        return {a.sum + b.sum, a.progress + b.progress, a.bits ^ b.bits};
    }
    static weight inverse(const weight& a) { return {-a.sum, -a.progress, a.bits}; }
};

template <class T>
T transformed(T value, std::make_unsigned_t<T> mask) {
    const std::make_unsigned_t<T> bits = static_cast<std::make_unsigned_t<T>>(value) ^ mask;
    T answer;
    std::memcpy(&answer, &bits, sizeof(T));
    return answer;
}

template <class Key, class Weight>
bool same_result(const std::optional<cp::wavelet_search_result<Key, Weight>>& a,
                 const std::optional<cp::wavelet_search_result<Key, Weight>>& b) {
    if (!a || !b) return a.has_value() == b.has_value();
    return a->value == b->value && a->count == b->count && a->fold == b->fold;
}

template <bool Masked, class Index, class Key>
void differential(const std::vector<Index>& positions, const std::vector<Key>& values,
                  std::uint64_t seed, int queries = 70,
                  const std::vector<Index>& boundaries = {}) {
    using U = std::make_unsigned_t<Key>;
    using Core = std::conditional_t<Masked, cp::wavelet_matrix<Key, weight, group>,
                                           cp::compressed_wavelet_matrix<Key, weight, group>>;
    using Matrix = cp::indexed_wavelet_matrix<Index, Core>;
    using Result = std::optional<cp::wavelet_search_result<Key, weight>>;
    const int n = values.size();
    std::mt19937_64 rng(seed);
    const auto minimum = [&]() -> Index {
        if constexpr (std::is_integral_v<Index>) return std::numeric_limits<Index>::min();
        else { assert(!boundaries.empty()); return boundaries.front(); }
    }();
    const auto maximum = [&]() -> Index {
        if constexpr (std::is_integral_v<Index>) return std::numeric_limits<Index>::max();
        else { assert(!boundaries.empty()); return boundaries.back(); }
    }();
    const auto random_index = [&]() -> Index {
        if constexpr (std::is_integral_v<Index>)
            return transformed(Index{0}, static_cast<std::make_unsigned_t<Index>>(rng()));
        else return boundaries[rng() % boundaries.size()];
    };
    Matrix matrix(positions, values);
    assert(matrix.size() == n && matrix.empty() == (n == 0) && !matrix.has_weights());
    std::vector<weight> weights;
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return positions[a] < positions[b]; });
    for (int i = 0; i < n; ++i)
        weights.emplace_back(static_cast<long long>(rng() % 201) - 100, rng() % 6, rng());
    matrix.build_weights(weights);
    assert(matrix.has_weights());
    for (int q = 0; q < queries; ++q) {
        const bool single = q % 2 == 0;
        std::vector<typename Matrix::range_type> ranges;
        const int count = single ? 1 : rng() % 8;
        for (int j = 0; j < count; ++j) {
            Index l = random_index();
            Index r = random_index();
            if (n && q % 3 == 0) { l = positions[rng() % n]; r = positions[rng() % n]; }
            if (r < l) std::swap(l, r);
            if (q % 5 == 0) { l = minimum; r = maximum; }
            if (q % 11 == 0) r = l;
            if (j && q % 7 == 0) ranges.push_back(ranges.back());
            else ranges.emplace_back(l, r);
        }
        const auto original_ranges = ranges;
        const U mask = !Masked || q % 3 == 0 ? U{0} : q % 3 == 1 ? std::numeric_limits<U>::max() : static_cast<U>(rng());
        Key lower = transformed(Key{0}, static_cast<U>(rng()));
        Key upper = transformed(Key{0}, static_cast<U>(rng()));
        if (n && q % 3 == 0) { lower = transformed(values[rng() % n], mask); upper = lower; }
        if (q % 5 == 0) { lower = std::numeric_limits<Key>::min(); upper = std::numeric_limits<Key>::max(); }
        std::vector<std::pair<Key, weight>> small;
        for (const auto& interval : ranges) {
            int selected = 0;
            for (const int i : order) {
                if (!(positions[i] < interval.first) && positions[i] < interval.second) {
                    small.emplace_back(transformed(values[i], mask), weights[i]);
                    ++selected;
                }
            }
            assert(matrix.range_size(interval.first, interval.second) == selected);
        }
        const std::int64_t total = small.size();
        assert(matrix.range_size(ranges) == total);
        if (single) {
            const auto [l, r] = ranges[0];
            int count_in_values = 0;
            weight folded = group::identity(), all = group::identity();
            for (const auto& item : small) {
                all = group::op(all, item.second);
                if (lower <= item.first && item.first < upper) {
                    ++count_in_values;
                    folded = group::op(folded, item.second);
                }
            }
            assert(matrix.range_fold(l, r) == all);
            if constexpr (Masked) {
                assert(matrix.range_freq(l, r, lower, upper, mask) == count_in_values);
                assert(matrix.range_fold(l, r, lower, upper, mask) == folded);
            } else {
                assert(matrix.range_freq(l, r, lower, upper) == count_in_values);
                assert(matrix.range_fold(l, r, lower, upper) == folded);
            }
        }
        auto large = small;
        std::stable_sort(small.begin(), small.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        std::stable_sort(large.begin(), large.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        std::vector<std::int64_t> counts{0, total, total / 2};
        if (total) { counts.push_back(1); counts.push_back(total - 1); }
        if (total <= 10)
            for (std::int64_t k = 0; k <= total; ++k) counts.push_back(k);
        for (const auto k : counts) {
            weight expected_small = group::identity(), expected_large = group::identity();
            for (std::int64_t i = 0; i < k; ++i) {
                expected_small = group::op(expected_small, small[i].second);
                expected_large = group::op(expected_large, large[i].second);
            }
            if constexpr (Masked) {
                if (single) {
                    const auto [l, r] = ranges[0];
                    assert(matrix.fold_smallest(l, r, k, mask) == expected_small);
                    assert(matrix.fold_largest(l, r, k, mask) == expected_large);
                    if (k < total) {
                        assert(matrix.kth_smallest(l, r, k, mask) == small[k].first);
                        assert(matrix.kth_largest(l, r, k, mask) == large[k].first);
                    }
                } else {
                    assert(matrix.fold_smallest(ranges, k, mask) == expected_small);
                    assert(matrix.fold_largest(ranges, k, mask) == expected_large);
                    if (k < total) {
                        assert(matrix.kth_smallest(ranges, k, mask) == small[k].first);
                        assert(matrix.kth_largest(ranges, k, mask) == large[k].first);
                    }
                }
            } else {
                if (single) {
                    const auto [l, r] = ranges[0];
                    assert(matrix.fold_smallest(l, r, k) == expected_small);
                    assert(matrix.fold_largest(l, r, k) == expected_large);
                    if (k < total) {
                        assert(matrix.kth_smallest(l, r, k) == small[k].first);
                        assert(matrix.kth_largest(l, r, k) == large[k].first);
                    }
                } else {
                    assert(matrix.fold_smallest(ranges, k) == expected_small);
                    assert(matrix.fold_largest(ranges, k) == expected_large);
                    if (k < total) {
                        assert(matrix.kth_smallest(ranges, k) == small[k].first);
                        assert(matrix.kth_largest(ranges, k) == large[k].first);
                    }
                }
            }
        }
        long long progress = 0;
        for (const auto& item : small) progress += item.second.progress;
        const long long target = 1 + rng() % (progress + 3);
        const auto predicate = [target](const weight& value) { return value.progress >= target; };
        for (bool take_all_equal : {true, false}) {
            const auto search = [&](const auto& ordered, bool reverse) -> Result {
                weight sum = group::identity();
                std::int64_t taken = 0;
                for (std::size_t i = 0; i < ordered.size(); ++i) {
                    const auto& item = ordered[i];
                    const Key value = item.first;
                    if (reverse ? value < upper : value >= lower) {
                        sum = group::op(sum, item.second);
                        ++taken;
                        if (take_all_equal && i + 1 < ordered.size() && value == ordered[i + 1].first) continue;
                        if (predicate(sum)) return cp::wavelet_search_result<Key, weight>{value, taken, sum};
                    }
                }
                return std::nullopt;
            };
            const Result first = search(small, false), last = search(large, true);
            if constexpr (Masked) {
                if (single) {
                    const auto [l, r] = ranges[0];
                    assert(same_result(matrix.first_value(l, r, lower, predicate, take_all_equal, mask), first));
                    assert(same_result(matrix.last_value(l, r, upper, predicate, take_all_equal, mask), last));
                } else {
                    assert(same_result(matrix.first_value(ranges, lower, predicate, take_all_equal, mask), first));
                    assert(same_result(matrix.last_value(ranges, upper, predicate, take_all_equal, mask), last));
                }
            } else {
                if (single) {
                    const auto [l, r] = ranges[0];
                    assert(same_result(matrix.first_value(l, r, lower, predicate, take_all_equal), first));
                    assert(same_result(matrix.last_value(l, r, upper, predicate, take_all_equal), last));
                } else {
                    assert(same_result(matrix.first_value(ranges, lower, predicate, take_all_equal), first));
                    assert(same_result(matrix.last_value(ranges, upper, predicate, take_all_equal), last));
                }
            }
        }
        assert(ranges.size() == original_ranges.size());
        for (std::size_t i = 0; i < ranges.size(); ++i) {
            assert(!(ranges[i].first < original_ranges[i].first));
            assert(!(original_ranges[i].first < ranges[i].first));
            assert(!(ranges[i].second < original_ranges[i].second));
            assert(!(original_ranges[i].second < ranges[i].second));
        }
    }
    for (auto& value : weights) value = weight{17, 1, 42};
    matrix.build_weights(weights);
    const auto original = matrix;
    auto moved_from = matrix;
    auto moved = std::move(moved_from);
    int selected = 0;
    for (const auto& position : positions)
        if (!(position < minimum) && position < maximum) ++selected;
    const auto sum = moved.range_fold(minimum, maximum);
    assert(sum.sum == 17LL * selected && sum.progress == selected);
    assert(original.range_fold(minimum, maximum) == sum);
}

struct positive {
    bool operator()(long long value) const { return value > 0; }
};

template <class T, class = void>
struct has_xor : std::false_type {};
template <class T>
struct has_xor<T, std::void_t<decltype(std::declval<const T&>().kth_smallest(0, 1, 0, 1U)),
                              decltype(std::declval<const T&>().first_value(0, 1, 0, positive{}, 1U))>>
    : std::true_type {};

template <class T, class = void>
struct has_mask_type : std::false_type {};
template <class T>
struct has_mask_type<T, std::void_t<typename T::mask_type>> : std::true_type {};

struct unused_weight { unused_weight() = delete; };
struct unused_group {};

void boundaries() {
    using Raw = cp::indexed_wavelet_matrix<long long, cp::wavelet_matrix<int>>;
    using Compressed = cp::indexed_wavelet_matrix<long long, cp::compressed_wavelet_matrix<int>>;
    static_assert(has_xor<Raw>::value && !has_xor<Compressed>::value);
    static_assert(has_mask_type<Raw>::value && !has_mask_type<Compressed>::value);
    static_assert(std::is_same_v<Raw::range_type, std::pair<long long, long long>>);
    Raw matrix({5, -8, 5, 0, -8}, {1, 1, 1, 1, 1});
    assert(matrix.kth_smallest(-9, 6, 0) == 1);
    assert(matrix.kth_largest(-9, 6, 4) == 1);
    matrix.build_weights({10, 20, 30, 40, 50});
    assert(matrix.range_fold(-9, 6) == 150);
    assert(matrix.fold_smallest(-9, 6, 1) == 20);
    assert(matrix.fold_smallest(-9, 6, 2) == 70);
    assert(matrix.fold_largest(-9, 6, 3) == 110);
    const std::vector<Raw::range_type> ranges{{5, 6}, {-8, -7}, {5, 6}};
    assert(matrix.fold_smallest(ranges, 3) == 60);
    assert(matrix.range_size(ranges) == 6);
    matrix.build_weights({1, 2, 3, 4, 5});
    assert(matrix.range_fold(5, 6) == 4);
    const auto result = matrix.first_value(-9, 6, 0,
        [target = std::make_unique<long long>(8)](long long value) { return value >= *target; });
    assert(result && result->value == 1 && result->count == 5 && result->fold == 15);
    const auto partial = matrix.first_value(-9, 6, 0,
        [target = std::make_unique<long long>(8)](long long value) { return value >= *target; }, false);
    assert(partial && partial->value == 1 && partial->count == 3 && partial->fold == 11);
    Compressed compressed({5, -8, 5, 0, -8}, {1, 1, 1, 1, 1});
    compressed.build_weights({1, 2, 3, 4, 5});
    const auto reverse = compressed.last_value(ranges, 2,
        [target = std::make_unique<long long>(8)](long long value) { return value >= *target; });
    assert(reverse && reverse->value == 1 && reverse->count == 6 && reverse->fold == 15);
    Raw empty;
    assert(empty.empty() && empty.range_size(LLONG_MIN, LLONG_MAX) == 0);
    empty.build_weights({});
    assert(empty.fold_smallest({}, 0) == 0);
    assert(empty.fold_largest({}, 0) == 0);
    assert(empty.range_fold(1, 1) == 0);
    assert(!empty.first_value({}, 0, positive{}));
    assert(!empty.last_value({}, 1, positive{}));
    cp::indexed_wavelet_matrix<int, cp::wavelet_matrix<int, unused_weight, unused_group>> unweighted({4, 0}, {9, 3});
    assert(unweighted.kth_largest(0, 5, 0) == 9);
}

struct throwing_group {
    static inline int remaining = -1;
    static long long identity() { return 0; }
    static long long op(long long a, long long b) {
        if (remaining == 0) throw std::runtime_error("weight operation");
        if (remaining > 0) --remaining;
        return a + b;
    }
    static long long inverse(long long a) { return -a; }
};

void exception_safety() {
    cp::indexed_wavelet_matrix<int, cp::wavelet_matrix<int, long long, throwing_group>> matrix({8, 0, 4}, {3, 1, 2});
    matrix.build_weights({10, 20, 30});
    throwing_group::remaining = 1;
    bool caught = false;
    try { matrix.build_weights({100, 200, 300}); }
    catch (const std::runtime_error&) { caught = true; }
    throwing_group::remaining = -1;
    assert(caught && matrix.has_weights() && matrix.range_fold(0, 5) == 50);
}

void large_counts() {
    constexpr int n = 65536, count = 40000;
    using Matrix = cp::indexed_wavelet_matrix<long long, cp::compressed_wavelet_matrix<int>>;
    Matrix matrix(std::vector<long long>(n, 7), std::vector<int>(n, 11));
    const std::vector<Matrix::range_type> ranges(count, {7, 8});
    const std::int64_t total = static_cast<std::int64_t>(n) * count;
    const std::int64_t k = static_cast<std::int64_t>(INT_MAX) + 123;
    assert(matrix.range_size(ranges) == total);
    assert(matrix.kth_smallest(ranges, k) == 11);
    assert(matrix.kth_largest(ranges, k) == 11);
    matrix.build_weights(std::vector<long long>(n, 1));
    assert(matrix.fold_smallest(ranges, k) == k);
    assert(matrix.fold_largest(ranges, k) == k);
    const auto first = matrix.first_value(ranges, 11, [k](long long sum) { return sum >= k; });
    const auto last = matrix.last_value(ranges, 12, [k](long long sum) { return sum >= k; });
    assert(first && first->value == 11 && first->count == total && first->fold == total);
    assert(same_result(last, first));
    const auto partial = matrix.first_value(ranges, 11, [k](long long sum) { return sum >= k; }, false);
    assert(partial && partial->count == k && partial->fold == k);
}

struct ordered_index {
    int coordinate, tag;
    ordered_index() = delete;
    ordered_index(int c, int t) : coordinate(c), tag(t) {}
    bool operator<(const ordered_index& other) const { return coordinate < other.coordinate; }
    bool operator==(const ordered_index&) const = delete;
    bool operator!=(const ordered_index&) const = delete;
    bool operator<=(const ordered_index&) const = delete;
    bool operator>(const ordered_index&) const = delete;
    bool operator>=(const ordered_index&) const = delete;
};

template <class Index>
void general_indices(std::vector<Index> positions, const std::vector<Index>& boundaries,
                     std::uint64_t seed) {
    const std::vector<int> values{4, 4, 1, 4, 2, 1, 3};
    differential<true>(positions, values, seed, 100, boundaries);
    differential<false>(positions, values, seed + 1, 100, boundaries);
    std::stable_sort(positions.begin(), positions.end());
    differential<true>(positions, values, seed + 2, 100, boundaries);
    differential<false>(positions, values, seed + 3, 100, boundaries);
    differential<true, Index, int>({}, {}, seed + 4, 30, boundaries);
    differential<false, Index, int>({}, {}, seed + 5, 30, boundaries);
}

void generic_coordinates() {
    general_indices<std::pair<int, int>>(
        {{3, 0}, {0, 2}, {1, 4}, {1, 3}, {2, 0}, {0, 2}, {3, 0}},
        {{-1, 0}, {0, 1}, {0, 2}, {0, 3}, {1, 3}, {1, 4}, {1, 5}, {2, 0}, {3, 0}, {3, 1}, {4, 0}}, 7);
    general_indices<std::tuple<long long, int, int>>(
        {{3, 0, 1}, {0, 2, 1}, {1, 4, 2}, {1, 3, 0}, {2, 0, 0}, {0, 2, 1}, {3, 0, 1}},
        {{-1, 0, 0}, {0, 2, 0}, {0, 2, 1}, {1, 3, 0}, {1, 4, 2}, {2, 0, 0}, {3, 0, 1}, {4, 0, 0}}, 13);
    general_indices<std::string>(
        {"dog", "apple", "car", "cat", "banana", "apple", "dog"},
        {"", "apple", "apricot", "banana", "car", "cat", "cow", "dog", "zebra"}, 19);
    general_indices<ordered_index>(
        {{3, 91}, {0, 20}, {1, 7}, {1, -3}, {2, 0}, {0, 9}, {3, -11}},
        {{-1, 0}, {0, 77}, {1, 55}, {2, 33}, {3, 11}, {4, 0}}, 25);
}

int main() {
    std::mt19937_64 rng(20261105);
    for (int n : {0, 1, 2, 7, 63, 64, 65, 127, 128, 129}) {
        std::vector<long long> positions(n), values(n);
        for (auto& p : positions) p = static_cast<long long>(rng() % 13) - 6;
        for (auto& v : values) v = static_cast<long long>(rng() % 9) - 4;
        differential<true>(positions, values, rng());
        differential<false>(positions, values, rng());
        std::sort(positions.begin(), positions.end());
        differential<true>(positions, values, rng());
        differential<false>(positions, values, rng());
        std::fill(positions.begin(), positions.end(), 0);
        differential<true>(positions, values, rng());
        differential<false>(positions, values, rng());
    }
    const std::vector<long long> x{LLONG_MAX, LLONG_MIN, 0, LLONG_MIN, -1, LLONG_MAX - 1};
    const std::vector<long long> y{LLONG_MIN, LLONG_MAX, -1, 0, LLONG_MIN, LLONG_MAX};
    const std::vector<std::uint64_t> u{UINT64_MAX, 0, UINT64_C(1) << 63, 1, 0, UINT64_MAX - 1};
    differential<true>(x, y, 1, 300);
    differential<false>(x, y, 2, 300);
    differential<true>(u, u, 3, 300);
    differential<false>(u, u, 4, 300);
    differential<true, std::int8_t, std::uint8_t>({127, -128, 1, -1, 0, 0}, {0, 255, 128, 1, 255, 0}, 5, 256);
    differential<false, std::uint8_t, std::int8_t>({255, 0, 128, 1, 0, 1}, {-128, 127, 0, -1, 1, 0}, 6, 256);
    boundaries();
    exception_safety();
    large_counts();
    generic_coordinates();
}
