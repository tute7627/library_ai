// Source: https://github.com/tute7627/library_ai/blob/main/tests/compressed_wavelet_matrix.test.cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
// competitive-verifier: STANDALONE
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

struct aggregate {
    long long sum;
    std::uint64_t bits;
    bool operator==(const aggregate& other) const {
        return sum == other.sum && bits == other.bits;
    }
};

struct aggregate_group {
    static aggregate identity() { return {0, 0}; }
    static aggregate op(const aggregate& a, const aggregate& b) {
        return {a.sum + b.sum, a.bits ^ b.bits};
    }
    static aggregate inverse(const aggregate& a) { return {-a.sum, a.bits}; }
};

template <class Key>
bool equivalent(const Key& a, const Key& b) {
    return !(a < b) && !(b < a);
}

template <class Key>
void same_result(const std::optional<cp::wavelet_search_result<Key, aggregate>>& actual,
                 const std::optional<cp::wavelet_search_result<Key, aggregate>>& expected) {
    assert(actual.has_value() == expected.has_value());
    if (actual) {
        assert(equivalent(actual->value, expected->value));
        assert(actual->count == expected->count);
        assert(actual->fold == expected->fold);
    }
}

template <class Key>
void differential(const std::vector<Key>& pool, const std::vector<Key>& boundaries,
                  std::uint64_t seed) {
    using Matrix = cp::compressed_wavelet_matrix<Key, aggregate, aggregate_group>;
    using Result = std::optional<cp::wavelet_search_result<Key, aggregate>>;
    std::mt19937_64 rng(seed);
    Matrix empty;
    assert(empty.size() == 0 && empty.empty() && !empty.has_weights());
    empty.build_weights({});
    assert(empty.range_freq(0, 0, boundaries.front(), boundaries.back()) == 0);
    assert(empty.range_fold(0, 0) == aggregate_group::identity());
    for (int n : {0, 1, 2, 7, 63, 64, 65, 100}) {
        std::vector<Key> values;
        std::vector<aggregate> weights;
        for (int i = 0; i < n; ++i) {
            values.push_back(pool[rng() % pool.size()]);
            weights.push_back({static_cast<long long>(rng() % 6), rng()});
        }
        Matrix matrix(values);
        assert(matrix.size() == n && matrix.empty() == (n == 0) && !matrix.has_weights());
        if (n) {
            const auto minimum = std::min_element(values.begin(), values.end());
            assert(equivalent(matrix.kth_smallest(0, n, 0), *minimum));
        }
        matrix.build_weights(weights);
        assert(matrix.has_weights());
        for (int q = 0; q < 60; ++q) {
            const bool single = q % 2 == 0;
            std::vector<typename Matrix::range_type> ranges;
            const int count = single ? 1 : rng() % 7;
            for (int j = 0; j < count; ++j) {
                int l = rng() % (n + 1), r = rng() % (n + 1);
                if (r < l) std::swap(l, r);
                if (q % 5 == 0) { l = 0; r = n; }
                if (q % 11 == 0) r = l;
                if (j && q % 7 == 0) ranges.push_back(ranges.back());
                else ranges.emplace_back(l, r);
            }
            const auto original_ranges = ranges;
            const Key& lower = boundaries[rng() % boundaries.size()];
            const Key& upper = boundaries[rng() % boundaries.size()];
            std::vector<int> small;
            for (const auto& interval : ranges) {
                aggregate total = aggregate_group::identity();
                aggregate selected = aggregate_group::identity();
                int frequency = 0;
                for (int i = interval.first; i < interval.second; ++i) {
                    small.push_back(i);
                    total = aggregate_group::op(total, weights[i]);
                    if (!(values[i] < lower) && values[i] < upper) {
                        ++frequency;
                        selected = aggregate_group::op(selected, weights[i]);
                    }
                }
                assert(matrix.range_freq(interval.first, interval.second, lower, upper) == frequency);
                assert(matrix.range_fold(interval.first, interval.second) == total);
                assert(matrix.range_fold(interval.first, interval.second, lower, upper) == selected);
            }
            std::stable_sort(small.begin(), small.end(),
                [&](int a, int b) { return values[a] < values[b]; });
            auto large = small;
            std::stable_sort(large.begin(), large.end(),
                [&](int a, int b) { return values[b] < values[a]; });
            aggregate small_fold = aggregate_group::identity();
            aggregate large_fold = aggregate_group::identity();
            const int m = small.size();
            for (int k = 0; k <= m; ++k) {
                if (single) {
                    const auto [l, r] = ranges.front();
                    assert(matrix.fold_smallest(l, r, k) == small_fold);
                    assert(matrix.fold_largest(l, r, k) == large_fold);
                    if (k < m) {
                        assert(equivalent(matrix.kth_smallest(l, r, k), values[small[k]]));
                        assert(equivalent(matrix.kth_largest(l, r, k), values[large[k]]));
                    }
                } else {
                    assert(matrix.fold_smallest(ranges, k) == small_fold);
                    assert(matrix.fold_largest(ranges, k) == large_fold);
                    if (k < m) {
                        assert(equivalent(matrix.kth_smallest(ranges, k), values[small[k]]));
                        assert(equivalent(matrix.kth_largest(ranges, k), values[large[k]]));
                    }
                }
                if (k < m) {
                    small_fold = aggregate_group::op(small_fold, weights[small[k]]);
                    large_fold = aggregate_group::op(large_fold, weights[large[k]]);
                }
            }
            for (long long target : {1LL, small_fold.sum / 2 + 1, small_fold.sum + 1}) {
                const auto predicate = [target](const aggregate& value) { return value.sum >= target; };
                for (bool take_all_equal : {true, false}) {
                    Result first, last;
                    for (int direction = 0; direction < 2; ++direction) {
                        const auto& ordered = direction == 0 ? small : large;
                        aggregate accumulated = aggregate_group::identity();
                        std::int64_t taken = 0;
                        for (std::size_t i = 0; i < ordered.size(); ++i) {
                            const int index = ordered[i];
                            const Key& key = values[index];
                            if (direction == 0 ? key < lower : !(key < upper)) continue;
                            accumulated = aggregate_group::op(accumulated, weights[index]);
                            ++taken;
                            if (take_all_equal && i + 1 < ordered.size() && equivalent(key, values[ordered[i + 1]])) continue;
                            if (predicate(accumulated)) {
                                (direction == 0 ? first : last) = cp::wavelet_search_result<Key, aggregate>{key, taken, accumulated};
                                break;
                            }
                        }
                    }
                    if (single) {
                        const auto [l, r] = ranges.front();
                        same_result(matrix.first_value(l, r, lower, predicate, take_all_equal), first);
                        same_result(matrix.last_value(l, r, upper, predicate, take_all_equal), last);
                    } else {
                        same_result(matrix.first_value(ranges, lower, predicate, take_all_equal), first);
                        same_result(matrix.last_value(ranges, upper, predicate, take_all_equal), last);
                    }
                }
            }
            assert(ranges == original_ranges);
        }
        for (auto& value : weights) value.sum = 0;
        matrix.build_weights(weights);
        aggregate total = aggregate_group::identity();
        for (const auto& value : weights) total = aggregate_group::op(total, value);
        assert(matrix.range_fold(0, n) == total);
        const auto positive = [](const aggregate& value) { return value.sum > 0; };
        assert(!matrix.first_value(0, n, boundaries.front(), positive));
        assert(!matrix.last_value(0, n, boundaries.back(), positive));
        const auto copy = matrix;
        auto moved = std::move(matrix);
        assert(copy.range_fold(0, n) == total);
        assert(moved.range_fold(0, n) == total);
    }
}

struct ordered_key {
    int value, tag;
    ordered_key() = delete;
    ordered_key(int v, int t) : value(v), tag(t) {}
    bool operator<(const ordered_key& other) const { return value < other.value; }
    bool operator==(const ordered_key&) const = delete;
    bool operator!=(const ordered_key&) const = delete;
    bool operator<=(const ordered_key&) const = delete;
    bool operator>(const ordered_key&) const = delete;
    bool operator>=(const ordered_key&) const = delete;
};

void indexed_values() {
    using Key = std::pair<int, int>;
    using Matrix = cp::indexed_wavelet_matrix<std::string, cp::compressed_wavelet_matrix<Key>>;
    Matrix matrix({"dog", "apple", "cat", "apple"}, {{2, 0}, {1, 4}, {1, 2}, {1, 2}});
    matrix.build_weights({3, 1, 2, 4});
    const Key lower{1, 2}, upper{2, 0}, middle{1, 4}, maximum{2, 0};
    assert(matrix.range_size("apple", "dog") == 3);
    assert(matrix.range_freq("apple", "zebra", lower, upper) == 3);
    assert(matrix.kth_smallest("apple", "zebra", 2) == middle);
    assert(matrix.kth_largest("apple", "zebra", 0) == maximum);
    assert(matrix.range_fold("apple", "zebra", lower, upper) == 7);
    assert(matrix.fold_smallest("apple", "zebra", 2) == 6);
    assert(matrix.fold_largest("apple", "zebra", 2) == 4);
    const std::vector<Matrix::range_type> ranges{{"apple", "cat"}, {"dog", "zebra"}};
    assert(matrix.kth_smallest(ranges, 1) == middle);
    assert(matrix.kth_largest(ranges, 0) == maximum);
    assert(matrix.fold_smallest(ranges, 2) == 5);
    assert(matrix.fold_largest(ranges, 2) == 4);
    const auto four = [](long long sum) { return sum >= 4; };
    const auto first = matrix.first_value(ranges, lower, four);
    const auto last = matrix.last_value(ranges, upper, four);
    const auto single_first = matrix.first_value("apple", "zebra", lower, four);
    const auto single_last = matrix.last_value("apple", "zebra", upper, four);
    assert(first && first->value == lower && first->count == 1 && first->fold == 4);
    assert(last && last->value == lower && last->count == 2 && last->fold == 5);
    assert(single_first && single_first->value == lower && single_first->count == 2 && single_first->fold == 6);
    assert(single_last && single_last->value == lower && single_last->count == 3 && single_last->fold == 7);
    const auto partial = matrix.first_value("apple", "zebra", lower, four, false);
    assert(partial && partial->value == lower && partial->count == 1 && partial->fold == 4);
}

int main() {
    differential<std::pair<int, int>>(
        {{2, 0}, {1, 4}, {1, 2}, {1, 2}, {0, 1}},
        {{-1, 0}, {0, 1}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {2, 0}, {2, 1}, {3, 0}}, 2026110301);
    differential<std::tuple<long long, int, int>>(
        {{3, 1, 0}, {1, 4, 2}, {1, 4, 0}, {0, 0, 0}, {1, 4, 0}},
        {{-1, 0, 0}, {0, 0, 0}, {1, 4, 0}, {1, 4, 1}, {1, 4, 2}, {3, 1, 0}, {4, 0, 0}}, 2026110302);
    differential<std::string>(
        {"dog", "apple", "car", "cat", "apple", "banana"},
        {"", "apple", "apricot", "banana", "car", "cat", "cow", "dog", "zebra"}, 2026110303);
    differential<ordered_key>(
        {{-2, 9}, {0, 77}, {0, -3}, {4, 5}, {6, 11}, {6, -2}},
        {{-3, 8}, {-2, 7}, {-1, 6}, {0, 5}, {1, 4}, {4, 3}, {6, 2}, {7, 1}}, 2026110304);
    differential<double>(
        {-std::numeric_limits<double>::infinity(), -1.5, -0.0, 0.0, 2.5,
         std::numeric_limits<double>::infinity()},
        {-std::numeric_limits<double>::infinity(), -2.0, -1.5, 0.0, 1.0, 2.5,
         std::numeric_limits<double>::infinity()}, 2026110305);
    indexed_values();
}
