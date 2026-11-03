// Source: https://github.com/tute7627/library_ai/blob/main/tests/wavelet_matrix_multi.test.cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
// competitive-verifier: STANDALONE
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

template <class Key, class Weight>
std::optional<Key> value_only(const std::optional<cp::wavelet_search_result<Key, Weight>>& result) {
    if (!result) return std::nullopt;
    return result->value;
}

using Range = std::pair<int, int>;
using Ranges = std::vector<Range>;

template <class Key>
Key transformed(Key value, std::make_unsigned_t<Key> mask) {
    auto raw = static_cast<std::make_unsigned_t<Key>>(value) ^ mask;
    const std::make_unsigned_t<Key> bits = raw;
    Key result;
    std::memcpy(&result, &bits, sizeof(Key));
    return result;
}

template <class Key>
void differential(const std::vector<Key>& a, std::uint64_t seed, int queries = 150) {
    using U = std::make_unsigned_t<Key>;
    const int n = static_cast<int>(a.size());
    std::mt19937_64 rng(seed);
    cp::wavelet_matrix<Key> wm(a);
    cp::compressed_wavelet_matrix<Key> compressed(a);
    if (n) {
        const Ranges ranges{{0, n}, {n / 2, n}, {0, 0}};
        auto sorted = a;
        sorted.insert(sorted.end(), a.begin() + n / 2, a.end());
        std::sort(sorted.begin(), sorted.end());
        for (std::int64_t k : {std::int64_t{0}, static_cast<std::int64_t>(sorted.size() - 1)}) {
            assert(wm.kth_smallest(ranges, k) == sorted[k]);
            assert(compressed.kth_smallest(ranges, k) == sorted[k]);
        }
    }
    std::vector<long long> weights(n);
    for (auto& w : weights) w = rng() % 6;
    wm.build_weights(weights);
    compressed.build_weights(weights);
    for (int q = 0; q < queries; ++q) {
        Ranges ranges;
        const int count = rng() % 9;
        for (int j = 0; j < count; ++j) {
            int l = rng() % (n + 1), r = rng() % (n + 1);
            if (l > r) std::swap(l, r);
            if (q % 7 == 0) r = l;
            if (j && q % 3 == 0) ranges.push_back(ranges.back());
            else ranges.emplace_back(l, r);
        }
        std::shuffle(ranges.begin(), ranges.end(), rng);
        const auto original_ranges = ranges;
        Key lower = static_cast<Key>(rng()), upper = static_cast<Key>(rng());
        if (n && q % 2 == 0) lower = a[rng() % n];
        if (n && q % 3 == 0) upper = a[rng() % n];
        if (q % 11 == 0) lower = std::numeric_limits<Key>::min();
        if (q % 13 == 0) upper = std::numeric_limits<Key>::max();
        const U mask = q % 2 ? static_cast<U>(rng()) : U{0};
        for (int variant = 0; variant < 2; ++variant) {
            const U m = variant == 0 ? mask : U{0};
            std::vector<std::pair<Key, long long>> flat;
            for (const auto& interval : ranges)
                for (int i = interval.first; i < interval.second; ++i)
                    flat.emplace_back(transformed(a[i], m), weights[i]);
            std::sort(flat.begin(), flat.end());
            const std::int64_t k = flat.empty() ? 0 : rng() % flat.size();
            const long long target = 1 + rng() % (flat.size() * 5 + 2);
            const auto predicate = [target](long long sum) { return sum >= target; };
            long long sum = 0;
            std::optional<Key> first, last;
            for (const auto& item : flat) {
                if (item.first < lower) continue;
                sum += item.second;
                if (sum >= target) {
                    first = item.first;
                    break;
                }
            }
            sum = 0;
            for (auto it = flat.rbegin(); it != flat.rend(); ++it) {
                if (it->first >= upper) continue;
                sum += it->second;
                if (sum >= target) {
                    last = it->first;
                    break;
                }
            }
            if (variant == 0) {
                if (!flat.empty()) assert(wm.kth_smallest(ranges, k, m) == flat[k].first);
                assert(value_only(wm.first_value(ranges, lower, predicate, m)) == first);
                assert(value_only(wm.last_value(ranges, upper, predicate, m)) == last);
                if (ranges.size() == 1) {
                    const auto interval = ranges.front();
                    assert(value_only(wm.first_value(interval.first, interval.second, lower, predicate, m)) == first);
                    assert(value_only(wm.last_value(interval.first, interval.second, upper, predicate, m)) == last);
                }
            } else {
                if (!flat.empty()) assert(compressed.kth_smallest(ranges, k) == flat[k].first);
                assert(value_only(compressed.first_value(ranges, lower, predicate)) == first);
                assert(value_only(compressed.last_value(ranges, upper, predicate)) == last);
            }
        }
        assert(ranges == original_ranges);
    }
}

void byte_masks() {
    const std::vector<std::int8_t> a{-128, -1, 0, 1, 127, -1};
    const Ranges ranges{{1, 4}, {0, 6}, {2, 2}, {0, 1}, {5, 6}};
    cp::wavelet_matrix<std::int8_t> wm(a);
    wm.build_weights(std::vector<long long>(a.size(), 1));
    for (int mask = 0; mask < 256; ++mask) {
        std::vector<std::int8_t> sorted;
        for (const auto& interval : ranges)
            for (int i = interval.first; i < interval.second; ++i)
                sorted.push_back(transformed(a[i], static_cast<std::uint8_t>(mask)));
        std::sort(sorted.begin(), sorted.end());
        for (std::size_t k = 0; k < sorted.size(); ++k)
            assert(wm.kth_smallest(ranges, k, mask) == sorted[k]);
        for (int bound = -128; bound <= 127; ++bound) {
            const auto cut = std::lower_bound(sorted.begin(), sorted.end(), bound);
            for (int target : {1, 3, 11, 12}) {
                const auto predicate = [target](long long sum) { return sum >= target; };
                const auto first = value_only(wm.first_value(ranges, bound, predicate, mask));
                const auto last = value_only(wm.last_value(ranges, bound, predicate, mask));
                if (sorted.end() - cut >= target) assert(first == cut[target - 1]);
                else assert(!first);
                if (cut - sorted.begin() >= target) assert(last == cut[-target]);
                else assert(!last);
            }
        }
    }
}

struct weight {
    long long value;
    weight() = delete;
    explicit weight(long long v) : value(v) {}
};

struct group {
    static weight identity() { return weight(0); }
    static weight op(const weight& a, const weight& b) { return weight(a.value + b.value); }
    static weight inverse(const weight& a) { return weight(-a.value); }
};

void custom_group() {
    const Ranges ranges{{0, 2}, {2, 4}, {0, 2}};
    const auto predicate = [](const weight& w) { return w.value >= 81; };
    cp::wavelet_matrix<int, weight, group> wm({4, 1, 4, 2});
    wm.build_weights({weight(10), weight(20), weight(30), weight(40)});
    assert(value_only(wm.first_value(ranges, 0, predicate)) == 4);
    assert(value_only(wm.last_value(ranges, 5, predicate)) == 2);
    assert(value_only(wm.first_value(ranges, 1, predicate, 2U)) == 6);
    assert(value_only(wm.last_value(ranges, 7, predicate, 2U)) == 3);
    cp::compressed_wavelet_matrix<int, weight, group> compressed({4, 1, 4, 2});
    compressed.build_weights({weight(10), weight(20), weight(30), weight(40)});
    assert(value_only(compressed.first_value(ranges, 0, predicate)) == 4);
    assert(value_only(compressed.last_value(ranges, 5, predicate)) == 2);
}

struct xor_group {
    static std::uint64_t identity() { return 0; }
    static std::uint64_t op(std::uint64_t a, std::uint64_t b) { return a ^ b; }
    static std::uint64_t inverse(std::uint64_t a) { return a; }
};

void xor_weights() {
    const Ranges ranges{{0, 1}, {1, 4}};
    const auto predicate = [](std::uint64_t value) { return value >= 6; };
    cp::wavelet_matrix<int, std::uint64_t, xor_group> wm({4, 1, 4, 2});
    wm.build_weights({1, 2, 4, 8});
    assert(value_only(wm.first_value(ranges, 0, predicate)) == 2);
    assert(value_only(wm.first_value(ranges, 2, predicate)) == 2);
    assert(value_only(wm.last_value(ranges, 5, predicate)) == 2);
    assert(value_only(wm.first_value(ranges, 0, predicate, 2U)) == 0);
    assert(value_only(wm.last_value(ranges, 7, predicate, 2U)) == 3);
    cp::compressed_wavelet_matrix<int, std::uint64_t, xor_group> compressed({4, 1, 4, 2});
    compressed.build_weights({1, 2, 4, 8});
    assert(value_only(compressed.first_value(ranges, 2, predicate)) == 2);
    assert(value_only(compressed.last_value(ranges, 5, predicate)) == 2);
}

void empty() {
    cp::wavelet_matrix<int> wm;
    cp::compressed_wavelet_matrix<int> compressed;
    wm.build_weights({});
    compressed.build_weights({});
    for (const Ranges& ranges : {Ranges{}, Ranges{{0, 0}, {0, 0}}}) {
        const auto predicate = [](long long sum) { return sum > 0; };
        assert(!value_only(wm.first_value(ranges, 0, predicate)));
        assert(!value_only(wm.last_value(ranges, 1, predicate)));
        assert(!value_only(compressed.first_value(ranges, 0, predicate)));
        assert(!value_only(compressed.last_value(ranges, 1, predicate)));
    }
}

void large_count() {
    constexpr int n = 200000, count = 15000;
    const std::uint64_t base = UINT64_C(1) << 60;
    std::vector<std::uint64_t> a(n);
    std::iota(a.begin(), a.end(), base);
    const Ranges ranges(count, {0, n});
    const std::int64_t k = static_cast<std::int64_t>(INT_MAX) + 1337;
    const auto predicate = [k](long long sum) { return sum >= k + 1; };
    const std::uint64_t rank = k / count;
    cp::wavelet_matrix<std::uint64_t> wm(a);
    assert(wm.kth_smallest(ranges, k) == base + rank);
    assert(wm.kth_smallest(ranges, k, UINT64_MAX) == ~(base + n - 1 - rank));
    wm.build_weights(std::vector<long long>(n, 1));
    assert(value_only(wm.first_value(ranges, 0, predicate)) == base + rank);
    assert(value_only(wm.last_value(ranges, base + n, predicate)) == base + n - 1 - rank);
    assert(value_only(wm.first_value(ranges, 0, predicate, UINT64_MAX)) == ~(base + n - 1 - rank));
    assert(value_only(wm.last_value(ranges, UINT64_MAX, predicate, UINT64_MAX)) == ~(base + rank));
    cp::compressed_wavelet_matrix<std::uint64_t> compressed(a);
    assert(compressed.kth_smallest(ranges, k) == base + rank);
    compressed.build_weights(std::vector<long long>(n, 1));
    assert(value_only(compressed.first_value(ranges, 0, predicate)) == base + rank);
    assert(value_only(compressed.last_value(ranges, base + n, predicate)) == base + n - 1 - rank);
}

int main() {
    static_assert(std::is_same_v<cp::wavelet_matrix<int>::range_type, Range>);
    static_assert(std::is_same_v<cp::compressed_wavelet_matrix<int>::range_type, Range>);
    std::mt19937_64 rng(20261101);
    empty();
    for (int n : {0, 1, 2, 3, 63, 64, 65, 127, 128, 129}) {
        std::vector<long long> a(n);
        for (auto& value : a) value = static_cast<long long>(rng() % 41) - 20;
        differential(a, rng());
        std::fill(a.begin(), a.end(), LLONG_MAX);
        differential(a, rng());
        std::fill(a.begin(), a.end(), LLONG_MIN);
        differential(a, rng());
    }
    for (int iteration = 0; iteration < 35; ++iteration) {
        const int n = rng() % 100;
        std::vector<int> a(n);
        std::vector<std::uint64_t> b(n);
        for (auto& value : a) value = static_cast<int>(rng());
        for (auto& value : b) value = rng();
        differential(a, rng());
        differential(b, rng());
        for (auto& value : b) value = (UINT64_C(1) << 60) + rng() % 100;
        differential(b, rng());
    }
    differential<long long>({LLONG_MIN, LLONG_MIN + 1, -1, 0, 1, LLONG_MAX}, 1, 500);
    differential<std::uint64_t>({0, 1, UINT64_C(1) << 63, UINT64_MAX}, 2, 500);
    differential<std::int16_t>({-32768, -1, 0, 32767}, 3);
    differential<std::uint16_t>({0, 1, 32768, 65535}, 4);
    byte_masks();
    custom_group();
    xor_weights();
    large_count();
}
