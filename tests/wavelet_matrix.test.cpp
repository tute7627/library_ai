// Source: https://github.com/tute7627/library_ai/blob/main/tests/wavelet_matrix.test.cpp
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

struct xor_group {
    static std::uint64_t identity() { return 0; }
    static std::uint64_t op(std::uint64_t a, std::uint64_t b) { return a ^ b; }
    static std::uint64_t inverse(std::uint64_t a) { return a; }
};

struct weight {
    long long sum;
    int count;
    weight() = delete;
    weight(long long s, int c) : sum(s), count(c) {}
};

struct pair_group {
    static weight identity() { return {0, 0}; }
    static weight op(const weight& a, const weight& b) {
        return {a.sum + b.sum, a.count + b.count};
    }
    static weight inverse(const weight& a) { return {-a.sum, -a.count}; }
};

template <class Key>
Key transformed(Key value, std::make_unsigned_t<Key> mask) {
    using U = std::make_unsigned_t<Key>;
    U raw = static_cast<U>(value) ^ mask;
    Key result;
    std::memcpy(&result, &raw, sizeof(Key));
    return result;
}

template <class Key>
void differential(const std::vector<Key>& a, std::uint64_t seed, int queries = 200) {
    using U = std::make_unsigned_t<Key>;
    const int n = static_cast<int>(a.size());
    std::mt19937_64 rng(seed);
    cp::wavelet_matrix<Key> wm(a);
    cp::compressed_wavelet_matrix<Key> compressed(a);
    assert(wm.size() == n && compressed.size() == n);
    assert(wm.empty() == (n == 0) && compressed.empty() == (n == 0));
    assert(!wm.has_weights() && !compressed.has_weights());
    if (n) {
        auto sorted = a;
        std::sort(sorted.begin(), sorted.end());
        for (int k = 0; k < n; ++k) {
            assert(wm.kth_smallest(0, n, k) == sorted[k]);
            assert(compressed.kth_smallest(0, n, k) == sorted[k]);
        }
    }
    std::vector<long long> w(n);
    std::vector<std::uint64_t> xw(n);
    for (int i = 0; i < n; ++i) {
        w[i] = rng() % 7;
        xw[i] = rng();
    }
    wm.build_weights(w);
    compressed.build_weights(w);
    cp::wavelet_matrix<Key, std::uint64_t, xor_group> xwm(a);
    cp::compressed_wavelet_matrix<Key, std::uint64_t, xor_group> xc(a);
    xwm.build_weights(xw);
    xc.build_weights(xw);
    assert(wm.has_weights() && compressed.has_weights());
    for (int q = 0; q < queries; ++q) {
        int l = rng() % (n + 1), r = rng() % (n + 1);
        if (l > r) std::swap(l, r);
        Key lower = static_cast<Key>(rng()), upper = static_cast<Key>(rng());
        if (n && q % 3 == 0) lower = a[rng() % n];
        if (n && q % 3 == 1) upper = a[rng() % n];
        if (q % 5 && lower > upper) std::swap(lower, upper);
        const U mask = q % 2 ? static_cast<U>(rng()) : U{0};
        const long long target = 1 + rng() % (6 * (r - l) + 2);
        for (int variant = 0; variant < 2; ++variant) {
            const U m = variant ? U{0} : mask;
            std::vector<std::pair<Key, long long>> sorted;
            int count = 0;
            long long sum = 0, all = 0;
            std::uint64_t xsum = 0, xall = 0;
            for (int i = l; i < r; ++i) {
                const Key value = transformed(a[i], m);
                sorted.emplace_back(value, w[i]);
                all += w[i];
                xall ^= xw[i];
                if (lower <= value && value < upper) {
                    ++count;
                    sum += w[i];
                    xsum ^= xw[i];
                }
            }
            std::sort(sorted.begin(), sorted.end());
            std::optional<Key> expected;
            long long accumulated = 0;
            for (const auto& item : sorted) {
                if (item.first < lower) continue;
                accumulated += item.second;
                if (accumulated >= target) {
                    expected = item.first;
                    break;
                }
            }
            std::optional<Key> expected_last;
            accumulated = 0;
            for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
                if (it->first >= upper) continue;
                accumulated += it->second;
                if (accumulated >= target) {
                    expected_last = it->first;
                    break;
                }
            }
            const auto predicate = [target](long long s) { return s >= target; };
            if (variant == 0) {
                assert(wm.range_freq(l, r, lower, upper, m) == count);
                assert(wm.range_fold(l, r, lower, upper, m) == sum);
                assert(wm.range_fold(l, r) == all);
                assert(xwm.range_fold(l, r, lower, upper, m) == xsum);
                assert(xwm.range_fold(l, r) == xall);
                assert(value_only(wm.first_value(l, r, lower, predicate, m)) == expected);
                assert(value_only(wm.last_value(l, r, upper, predicate, m)) == expected_last);
                if (l < r) {
                    const int k = rng() % (r - l);
                    assert(wm.kth_smallest(l, r, k, m) == sorted[k].first);
                }
            } else {
                assert(compressed.range_freq(l, r, lower, upper) == count);
                assert(compressed.range_fold(l, r, lower, upper) == sum);
                assert(compressed.range_fold(l, r) == all);
                assert(xc.range_fold(l, r, lower, upper) == xsum);
                assert(xc.range_fold(l, r) == xall);
                assert(value_only(compressed.first_value(l, r, lower, predicate)) == expected);
                assert(value_only(compressed.last_value(l, r, upper, predicate)) == expected_last);
                if (l < r) {
                    const int k = rng() % (r - l);
                    assert(compressed.kth_smallest(l, r, k) == sorted[k].first);
                }
            }
        }
    }
    for (int i = 0; i < n; ++i) w[i] = static_cast<long long>(rng() % 31) - 15;
    wm.build_weights(w);
    compressed.build_weights(w);
    for (int l = 0; l <= n; ++l) {
        long long total = 0, selected = 0;
        for (int r = l; r <= n; ++r) {
            if (r > l) {
                total += w[r - 1];
                if (Key{0} <= a[r - 1] && a[r - 1] < Key{11}) selected += w[r - 1];
            }
            assert(wm.range_fold(l, r) == total);
            assert(compressed.range_fold(l, r) == total);
            assert(wm.range_fold(l, r, Key{0}, Key{11}) == selected);
            assert(compressed.range_fold(l, r, Key{0}, Key{11}) == selected);
        }
    }
    if (n) {
        const auto copy = wm;
        auto moved = compressed;
        auto destination = std::move(moved);
        assert(copy.range_fold(0, n) == wm.range_fold(0, n));
        assert(destination.kth_smallest(0, n, 0) == wm.kth_smallest(0, n, 0));
    }
}

void byte_masks() {
    const std::vector<std::int8_t> a{-128, -1, 0, 1, 127, -1};
    cp::wavelet_matrix<std::int8_t> wm(a);
    wm.build_weights(std::vector<long long>(a.size(), 1));
    for (unsigned mask = 0; mask < 256; ++mask) {
        std::vector<std::int8_t> sorted;
        for (auto v : a) sorted.push_back(transformed(v, static_cast<std::uint8_t>(mask)));
        std::sort(sorted.begin(), sorted.end());
        for (int k = 0; k < static_cast<int>(a.size()); ++k) {
            assert(wm.kth_smallest(0, a.size(), k, mask) == sorted[k]);
            const auto answer = value_only(wm.first_value(0, a.size(), std::int8_t{-128},
                [k](long long s) { return s >= k + 1; }, mask));
            assert(answer == sorted[k]);
        }
        for (int lower = -128; lower <= 127; ++lower) {
            const auto count = std::lower_bound(sorted.begin(), sorted.end(), lower) - sorted.begin();
            assert(wm.range_freq(0, a.size(), -128, lower, mask) == count);
            assert(wm.range_fold(0, a.size(), -128, lower, mask) == count);
            for (int k = 0; k <= count; ++k) {
                const auto answer = value_only(wm.last_value(0, a.size(), lower,
                    [k](long long s) { return s >= k + 1; }, mask));
                if (k == count) assert(!answer);
                else assert(answer == sorted[count - 1 - k]);
            }
        }
    }
}

void compound_weights() {
    cp::wavelet_matrix<int, weight, pair_group> wm({4, 1, 4, 2});
    wm.build_weights({{10, 1}, {20, 1}, {30, 1}, {40, 1}});
    const auto result = wm.range_fold(0, 4, 2, 5);
    assert(result.sum == 80 && result.count == 3);
    assert(value_only(wm.first_value(0, 4, 0, [](const weight& w) { return w.count >= 3; })) == 4);
    assert(value_only(wm.last_value(0, 4, 5, [](const weight& w) { return w.count >= 3; })) == 2);
    cp::compressed_wavelet_matrix<int, weight, pair_group> c({4, 1, 4, 2});
    c.build_weights({{10, 1}, {20, 1}, {30, 1}, {40, 1}});
    assert(c.range_fold(0, 4, 2, 5).sum == 80);
    assert(value_only(c.last_value(0, 4, 5, [](const weight& w) { return w.count >= 3; })) == 2);
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
    cp::wavelet_matrix<int, long long, throwing_group> wm({3, 1, 2});
    wm.build_weights({1, 2, 3});
    throwing_group::remaining = 2;
    bool caught = false;
    try {
        wm.build_weights({10, 20, 30});
    } catch (const std::runtime_error&) {
        caught = true;
    }
    throwing_group::remaining = -1;
    assert(caught && wm.has_weights());
    assert(wm.range_fold(0, 3) == 6 && wm.range_fold(0, 3, 1, 3) == 5);
}

template <template <class, class, class> class Matrix>
void reverse_boundaries() {
    const auto positive = [](long long sum) { return sum >= 1; };
    const auto five = [](long long sum) { return sum >= 5; };
    using Signed = Matrix<long long, long long, cp::wavelet_sum_group<long long>>;
    Signed s({LLONG_MIN, LLONG_MIN, LLONG_MIN + 1, LLONG_MAX});
    s.build_weights({0, 2, 3, 10});
    assert(!value_only(s.last_value(0, 4, LLONG_MIN, positive)));
    assert(value_only(s.last_value(0, 4, LLONG_MIN + 1, positive)) == LLONG_MIN);
    assert(value_only(s.last_value(0, 4, LLONG_MAX, five)) == LLONG_MIN);
    assert(!value_only(s.last_value(2, 4, LLONG_MAX, five)));
    s.build_weights({0, 0, 0, 0});
    assert(!value_only(s.last_value(0, 4, LLONG_MAX, positive)));
    s.build_weights({2, 0, 3, 10});
    assert(value_only(s.last_value(0, 4, LLONG_MAX, five)) == LLONG_MIN);
    using Unsigned = Matrix<std::uint64_t, long long, cp::wavelet_sum_group<long long>>;
    Unsigned u({0, 1, UINT64_MAX});
    u.build_weights({2, 3, 10});
    assert(!value_only(u.last_value(0, 3, 0, positive)));
    assert(value_only(u.last_value(0, 3, 1, positive)) == 0);
    assert(value_only(u.last_value(0, 3, UINT64_MAX, five)) == 0);
}

void large() {
    constexpr int n = 200000;
    const std::uint64_t base = UINT64_C(1) << 60;
    std::vector<std::uint64_t> a(n);
    std::iota(a.begin(), a.end(), base);
    cp::wavelet_matrix<std::uint64_t> wm(a);
    wm.build_weights(std::vector<long long>(n, 1));
    std::mt19937_64 rng(7627);
    for (int q = 0; q < 4000; ++q) {
        int l = rng() % n, r = l + 1 + rng() % (n - l);
        int k = rng() % (r - l);
        assert(wm.kth_smallest(l, r, k) == base + l + k);
        assert(wm.range_freq(l, r, base + l, base + r) == r - l);
        assert(wm.range_fold(l, r, base + l, base + r) == r - l);
        assert(value_only(wm.first_value(l, r, base + l,
            [k](long long s) { return s >= k + 1; })) == base + l + k);
        assert(value_only(wm.last_value(l, r, base + r,
            [k](long long s) { return s >= k + 1; })) == base + r - 1 - k);
        const auto mask = UINT64_C(0xfedcba9876543210) ^ rng();
        r = std::min(r, l + 80);
        auto sorted = std::vector<std::uint64_t>(a.begin() + l, a.begin() + r);
        for (auto& v : sorted) v ^= mask;
        std::sort(sorted.begin(), sorted.end());
        k = rng() % (r - l);
        assert(wm.kth_smallest(l, r, k, mask) == sorted[k]);
    }
}

int main() {
    cp::wavelet_matrix<int> empty;
    cp::compressed_wavelet_matrix<int> compressed_empty;
    empty.build_weights({});
    compressed_empty.build_weights({});
    assert(empty.range_fold(0, 0) == 0);
    assert(!value_only(empty.first_value(0, 0, 0, [](long long s) { return s > 0; })));
    assert(!value_only(compressed_empty.first_value(0, 0, 0, [](long long s) { return s > 0; })));
    assert(!value_only(empty.last_value(0, 0, 0, [](long long s) { return s > 0; })));
    assert(!value_only(compressed_empty.last_value(0, 0, 0, [](long long s) { return s > 0; })));
    std::mt19937_64 rng(20261031);
    for (int n : {0, 1, 2, 3, 63, 64, 65, 127, 128, 129}) {
        std::vector<long long> a(n);
        for (auto& v : a) v = static_cast<long long>(rng() % 51) - 25;
        differential(a, rng());
        for (auto& v : a) v = (1LL << 60) + rng() % 100;
        differential(a, rng());
        std::fill(a.begin(), a.end(), std::numeric_limits<long long>::max());
        differential(a, rng());
    }
    for (int iteration = 0; iteration < 60; ++iteration) {
        const int n = rng() % 100;
        std::vector<int> a(n);
        std::vector<std::uint64_t> b(n);
        for (auto& v : a) v = static_cast<int>(rng());
        for (auto& v : b) v = rng();
        differential(a, rng());
        differential(b, rng());
    }
    differential<long long>({LLONG_MIN, LLONG_MAX, -1, 0, 1, LLONG_MIN, LLONG_MAX}, 1, 1000);
    differential<std::uint64_t>({0, UINT64_MAX, UINT64_C(1) << 63, 1}, 2, 1000);
    differential<std::uint16_t>({0, 1, 32768, 65535}, 3);
    differential<std::int16_t>({-32768, -1, 0, 32767}, 4);
    byte_masks();
    compound_weights();
    reverse_boundaries<cp::wavelet_matrix>();
    reverse_boundaries<cp::compressed_wavelet_matrix>();
    exception_safety();
    large();
}
