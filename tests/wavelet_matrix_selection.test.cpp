// Source: https://github.com/tute7627/library_ai/blob/main/tests/wavelet_matrix_selection.test.cpp
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

template <class Key>
Key transformed(Key value, std::make_unsigned_t<Key> mask) {
    const std::make_unsigned_t<Key> bits = static_cast<std::make_unsigned_t<Key>>(value) ^ mask;
    Key answer;
    std::memcpy(&answer, &bits, sizeof(Key));
    return answer;
}

template <class Key, class Weight>
bool same_result(const std::optional<cp::wavelet_search_result<Key, Weight>>& a,
                 const std::optional<cp::wavelet_search_result<Key, Weight>>& b) {
    if (!a || !b) return a.has_value() == b.has_value();
    return a->value == b->value && a->count == b->count && a->fold == b->fold;
}

template <class Key, class Matrix>
void verify(const Matrix& matrix, const std::vector<Key>& values,
            const std::vector<weight>& weights,
            const std::vector<std::pair<int, int>>& ranges,
            Key lower, Key upper, long long target,
            std::make_unsigned_t<Key> mask, bool single, bool take_all_equal) {
    using Result = std::optional<cp::wavelet_search_result<Key, weight>>;
    std::vector<std::pair<Key, weight>> ascending;
    for (const auto& interval : ranges)
        for (int i = interval.first; i < interval.second; ++i)
            ascending.emplace_back(transformed(values[i], mask), weights[i]);
    auto descending = ascending;
    std::stable_sort(ascending.begin(), ascending.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    std::stable_sort(descending.begin(), descending.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    const auto predicate = [target](const weight& value) { return value.progress >= target; };
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
    const Result first = search(ascending, false), last = search(descending, true);
    const auto first_query = [&]() -> Result {
        if constexpr (std::is_same_v<Matrix, cp::wavelet_matrix<Key, weight, group>>) {
            if (single) return matrix.first_value(ranges[0].first, ranges[0].second, lower, predicate, take_all_equal, mask);
            return matrix.first_value(ranges, lower, predicate, take_all_equal, mask);
        } else {
            if (single) return matrix.first_value(ranges[0].first, ranges[0].second, lower, predicate, take_all_equal);
            return matrix.first_value(ranges, lower, predicate, take_all_equal);
        }
    };
    const auto last_query = [&]() -> Result {
        if constexpr (std::is_same_v<Matrix, cp::wavelet_matrix<Key, weight, group>>) {
            if (single) return matrix.last_value(ranges[0].first, ranges[0].second, upper, predicate, take_all_equal, mask);
            return matrix.last_value(ranges, upper, predicate, take_all_equal, mask);
        } else {
            if (single) return matrix.last_value(ranges[0].first, ranges[0].second, upper, predicate, take_all_equal);
            return matrix.last_value(ranges, upper, predicate, take_all_equal);
        }
    };
    assert(same_result(first_query(), first));
    assert(same_result(last_query(), last));
    if (take_all_equal) {
        if constexpr (std::is_same_v<Matrix, cp::wavelet_matrix<Key, weight, group>>) {
            if (single) {
                assert(same_result(matrix.first_value(ranges[0].first, ranges[0].second, lower, predicate, mask), first));
                assert(same_result(matrix.last_value(ranges[0].first, ranges[0].second, upper, predicate, mask), last));
            } else {
                assert(same_result(matrix.first_value(ranges, lower, predicate, mask), first));
                assert(same_result(matrix.last_value(ranges, upper, predicate, mask), last));
            }
        } else {
            if (single) {
                assert(same_result(matrix.first_value(ranges[0].first, ranges[0].second, lower, predicate), first));
                assert(same_result(matrix.last_value(ranges[0].first, ranges[0].second, upper, predicate), last));
            } else {
                assert(same_result(matrix.first_value(ranges, lower, predicate), first));
                assert(same_result(matrix.last_value(ranges, upper, predicate), last));
            }
        }
    }
    const std::int64_t total = ascending.size();
    std::vector<std::int64_t> counts{0, total, total / 2};
    if (total) { counts.push_back(1); counts.push_back(total - 1); }
    if (total <= 12)
        for (std::int64_t k = 0; k <= total; ++k) counts.push_back(k);
    for (const auto k : counts) {
        weight small = group::identity(), large = group::identity();
        for (std::int64_t i = 0; i < k; ++i) {
            small = group::op(small, ascending[i].second);
            large = group::op(large, descending[i].second);
        }
        if constexpr (std::is_same_v<Matrix, cp::wavelet_matrix<Key, weight, group>>) {
            if (single) {
                const auto [l, r] = ranges[0];
                assert(matrix.fold_smallest(l, r, k, mask) == small);
                assert(matrix.fold_largest(l, r, k, mask) == large);
                if (k < total) assert(matrix.kth_largest(l, r, k, mask) == descending[k].first);
            } else {
                assert(matrix.fold_smallest(ranges, k, mask) == small);
                assert(matrix.fold_largest(ranges, k, mask) == large);
                if (k < total) assert(matrix.kth_largest(ranges, k, mask) == descending[k].first);
            }
        } else {
            if (single) {
                const auto [l, r] = ranges[0];
                assert(matrix.fold_smallest(l, r, k) == small);
                assert(matrix.fold_largest(l, r, k) == large);
                if (k < total) assert(matrix.kth_largest(l, r, k) == descending[k].first);
            } else {
                assert(matrix.fold_smallest(ranges, k) == small);
                assert(matrix.fold_largest(ranges, k) == large);
                if (k < total) assert(matrix.kth_largest(ranges, k) == descending[k].first);
            }
        }
    }
}

template <class Key>
void differential(const std::vector<Key>& values, std::uint64_t seed, int queries = 50) {
    using U = std::make_unsigned_t<Key>;
    std::mt19937_64 rng(seed);
    const int n = values.size();
    std::vector<weight> weights;
    for (int i = 0; i < n; ++i)
        weights.emplace_back(static_cast<long long>(rng() % 201) - 100, rng() % 6, rng());
    cp::wavelet_matrix<Key, weight, group> raw(values);
    cp::compressed_wavelet_matrix<Key, weight, group> compressed(values);
    raw.build_weights(weights);
    compressed.build_weights(weights);
    for (int q = 0; q < queries; ++q) {
        std::vector<std::pair<int, int>> ranges;
        const bool single = q % 2 == 0;
        const int count = single ? 1 : rng() % 8;
        for (int j = 0; j < count; ++j) {
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) std::swap(l, r);
            if (q % 7 == 0) { l = 0; r = n; }
            if (q % 11 == 0) r = l;
            if (j && q % 3 == 0) ranges.push_back(ranges.back());
            else ranges.emplace_back(l, r);
        }
        const auto original_ranges = ranges;
        const U mask = q % 3 == 0 ? U{0} : q % 3 == 1 ? std::numeric_limits<U>::max() : static_cast<U>(rng());
        Key lower = transformed(Key{0}, static_cast<U>(rng()));
        Key upper = transformed(Key{0}, static_cast<U>(rng()));
        if (q % 5 == 0) { lower = std::numeric_limits<Key>::min(); upper = std::numeric_limits<Key>::max(); }
        if (n && q % 5 == 1) { lower = transformed(values[rng() % n], mask); upper = lower; }
        const long long target = 1 + rng() % (20 * (n + 1));
        for (bool take_all_equal : {true, false}) {
            verify(raw, values, weights, ranges, lower, upper, target, mask, single, take_all_equal);
            verify(compressed, values, weights, ranges, lower, upper, target, U{0}, single, take_all_equal);
        }
        assert(ranges == original_ranges);
    }
}

void atomic_counts() {
    cp::wavelet_matrix<int> raw({1, 1, 2, 3});
    raw.build_weights({100, -99, 2, 3});
    const auto predicate = [](long long sum) { return sum >= 4; };
    using Result = std::optional<cp::wavelet_search_result<int, long long>>;
    assert(same_result(raw.first_value(0, 4, 0, predicate), Result{{3, 4, 6}}));
    assert(same_result(raw.last_value(0, 4, 4, predicate), Result{{2, 2, 5}}));
    const std::vector<std::pair<int, int>> ranges{{0, 4}, {0, 4}, {2, 2}};
    assert(same_result(raw.first_value(ranges, 0, predicate), Result{{2, 6, 6}}));
    assert(same_result(raw.last_value(ranges, 4, predicate), Result{{3, 2, 6}}));
    cp::compressed_wavelet_matrix<int> compressed({1, 1, 2, 3});
    compressed.build_weights({100, -99, 2, 3});
    assert(same_result(compressed.first_value(ranges, 0, predicate), Result{{2, 6, 6}}));
    assert(same_result(compressed.last_value(ranges, 4, predicate), Result{{3, 2, 6}}));
}

template <template <class, class, class> class Core>
void partial_ties() {
    using Matrix = Core<int, long long, cp::wavelet_sum_group<long long>>;
    using Result = std::optional<typename Matrix::search_result_type>;
    Matrix matrix({10, 10, 10});
    matrix.build_weights({10, 10, 10});
    for (int target : {5, 15, 25}) {
        const auto predicate = [target](long long sum) { return sum > target; };
        const int taken = target / 10 + 1;
        const Result expected{{10, taken, 10LL * taken}};
        assert(same_result(matrix.first_value(0, 3, 0, predicate), Result{{10, 3, 30}}));
        assert(same_result(matrix.last_value(0, 3, 11, predicate, true), Result{{10, 3, 30}}));
        assert(same_result(matrix.first_value(0, 3, 0, predicate, false), expected));
        assert(same_result(matrix.last_value(0, 3, 11, predicate, false), expected));
        const std::vector<std::pair<int, int>> ranges{{0, 0}, {1, 3}, {0, 1}, {2, 2}};
        assert(same_result(matrix.first_value(ranges, 0, predicate), Result{{10, 3, 30}}));
        assert(same_result(matrix.last_value(ranges, 11, predicate, true), Result{{10, 3, 30}}));
        assert(same_result(matrix.first_value(ranges, 0, predicate, false), expected));
        assert(same_result(matrix.last_value(ranges, 11, predicate, false), expected));
    }
    const auto absent = [](long long sum) { return sum > 30; };
    assert(!matrix.first_value(0, 3, 0, absent, false));
    assert(!matrix.last_value(0, 3, 11, absent, false));
    Matrix zeros({7, 7, 7, 7});
    zeros.build_weights({0, 3, 0, 5});
    const auto three = [](long long sum) { return sum >= 3; };
    const auto six = [](long long sum) { return sum >= 6; };
    assert(same_result(zeros.first_value(0, 4, 0, three, false), Result{{7, 2, 3}}));
    assert(same_result(zeros.last_value(0, 4, 8, three, false), Result{{7, 2, 3}}));
    const std::vector<std::pair<int, int>> ranges{{2, 4}, {0, 2}};
    assert(same_result(zeros.first_value(ranges, 0, three, false), Result{{7, 2, 5}}));
    assert(same_result(zeros.last_value(ranges, 8, three, false), Result{{7, 2, 5}}));
    assert(same_result(zeros.first_value(ranges, 0, six, false), Result{{7, 4, 8}}));
    assert(same_result(zeros.last_value(ranges, 8, six, false), Result{{7, 4, 8}}));
}

struct xor_group {
    static std::uint64_t identity() { return 0; }
    static std::uint64_t op(std::uint64_t a, std::uint64_t b) { return a ^ b; }
    static std::uint64_t inverse(std::uint64_t value) { return value; }
};

template <template <class, class, class> class Core>
void grouped_xor() {
    using Matrix = Core<int, std::uint64_t, xor_group>;
    using Result = std::optional<typename Matrix::search_result_type>;
    Matrix matrix({1, 1, 2, 2, 3, 3, 3, 4, 4});
    matrix.build_weights({1, 1, 2, 2, 4, 4, 4, 8, 8});
    const auto nonzero = [](std::uint64_t value) { return value != 0; };
    assert(same_result(matrix.first_value(0, 9, 0, nonzero), Result{{3, 7, 4}}));
    assert(same_result(matrix.last_value(0, 9, 5, nonzero, true), Result{{3, 5, 4}}));
    const std::vector<std::pair<int, int>> ranges{{0, 9}, {0, 4}};
    assert(same_result(matrix.first_value(ranges, 0, nonzero, true), Result{{3, 11, 4}}));
    assert(same_result(matrix.last_value(ranges, 5, nonzero), Result{{3, 5, 4}}));
}

void large_counts() {
    constexpr int n = 65536, count = 40000;
    const std::int64_t total = static_cast<std::int64_t>(n) * count;
    const std::int64_t k = static_cast<std::int64_t>(INT_MAX) + 153;
    assert(total > INT_MAX && k < total);
    const std::vector<std::pair<int, int>> ranges(count, {0, n});
    const std::vector<long long> values(n, 7), weights(n, 1);
    const auto predicate = [k](long long sum) { return sum >= k; };
    cp::wavelet_matrix<long long> raw(values);
    cp::compressed_wavelet_matrix<long long> compressed(values);
    raw.build_weights(weights);
    compressed.build_weights(weights);
    using Result = std::optional<cp::wavelet_search_result<long long, long long>>;
    assert(same_result(raw.first_value(ranges, 7, predicate), Result{{7, total, total}}));
    assert(same_result(raw.last_value(ranges, 8, predicate), Result{{7, total, total}}));
    assert(same_result(compressed.first_value(ranges, 7, predicate), Result{{7, total, total}}));
    assert(same_result(compressed.last_value(ranges, 8, predicate), Result{{7, total, total}}));
    assert(same_result(raw.first_value(ranges, 7, predicate, false), Result{{7, k, k}}));
    assert(same_result(raw.last_value(ranges, 8, predicate, false), Result{{7, k, k}}));
    assert(same_result(compressed.first_value(ranges, 7, predicate, false), Result{{7, k, k}}));
    assert(same_result(compressed.last_value(ranges, 8, predicate, false), Result{{7, k, k}}));
    assert(raw.kth_largest(ranges, k) == 7);
    assert(compressed.kth_largest(ranges, k) == 7);
    assert(raw.fold_smallest(ranges, k) == k);
    assert(raw.fold_largest(ranges, k) == k);
    assert(compressed.fold_smallest(ranges, k) == k);
    assert(compressed.fold_largest(ranges, k) == k);
}

int main() {
    std::mt19937_64 rng(20261104);
    for (int n : {0, 1, 2, 7, 63, 64, 65, 127, 128, 129}) {
        std::vector<long long> values(n);
        for (auto& value : values) value = static_cast<long long>(rng() % 9) - 4;
        differential(values, rng());
        std::fill(values.begin(), values.end(), LLONG_MAX);
        differential(values, rng());
        for (auto& value : values) value = transformed(0LL, rng());
        differential(values, rng());
        std::vector<std::uint64_t> unsigned_values(n);
        for (auto& value : unsigned_values) value = (UINT64_C(1) << 60) + rng() % 17;
        differential(unsigned_values, rng());
    }
    differential<long long>({LLONG_MIN, -1, LLONG_MAX, 0, LLONG_MIN, LLONG_MAX}, 1, 200);
    differential<std::uint64_t>({0, UINT64_MAX, UINT64_C(1) << 63, 1, 1}, 2, 200);
    differential<std::int8_t>({-128, -1, 0, 127, -1}, 3, 256);
    differential<std::uint8_t>({0, 255, 1, 128, 1}, 4, 256);
    atomic_counts();
    partial_ties<cp::wavelet_matrix>();
    partial_ties<cp::compressed_wavelet_matrix>();
    grouped_xor<cp::wavelet_matrix>();
    grouped_xor<cp::compressed_wavelet_matrix>();
    large_counts();
}
