// Source: https://github.com/tute7627/library_ai/blob/main/tests/wavelet_matrix_optimized.test.cpp
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

struct ledger {
    long long balance;
    int progress;
    ledger() = delete;
    ledger(long long b, int p) : balance(b), progress(p) {}
};

struct ledger_group {
    static ledger identity() { return {0, 0}; }
    static ledger op(const ledger& a, const ledger& b) {
        return {a.balance + b.balance, a.progress + b.progress};
    }
    static ledger inverse(const ledger& a) { return {-a.balance, -a.progress}; }
};

template <class Key>
Key transformed(Key value, std::make_unsigned_t<Key> mask) {
    const std::make_unsigned_t<Key> bits = static_cast<std::make_unsigned_t<Key>>(value) ^ mask;
    Key result;
    std::memcpy(&result, &bits, sizeof(Key));
    return result;
}

template <class Key, class Weight, class Group, class Score>
void compare_selection(const std::vector<Key>& values, const std::vector<Weight>& weights,
                       Score score, std::uint64_t seed) {
    using U = std::make_unsigned_t<Key>;
    const int n = static_cast<int>(values.size());
    cp::wavelet_matrix<Key, Weight, Group> raw(values);
    cp::compressed_wavelet_matrix<Key, Weight, Group> compressed(values);
    raw.build_weights(weights);
    compressed.build_weights(weights);
    std::mt19937_64 rng(seed);
    for (int q = 0; q < 80; ++q) {
        int l = rng() % (n + 1), r = rng() % (n + 1);
        if (l > r) std::swap(l, r);
        if (q % 4 == 0) { l = 0; r = n; }
        if (q % 4 == 1) { l = 0; r = std::min(n, 64); }
        if (q % 4 == 2) { l = std::min(n, 63); r = std::min(n, 65); }
        long long total_score = 0;
        for (int i = l; i < r; ++i) total_score += score(weights[i]);
        const long long target = 1 + rng() % (total_score + 2);
        const auto predicate = [&](const Weight& value) { return score(value) >= target; };
        U mask = static_cast<U>(rng());
        if (q % 3 == 0) mask = 0;
        if (q % 3 == 1) mask = std::numeric_limits<U>::max();
        for (int variant = 0; variant < 2; ++variant) {
            const U applied = variant == 0 ? mask : U{0};
            Key lower = transformed(Key{0}, static_cast<U>(rng()));
            Key upper = transformed(Key{0}, static_cast<U>(rng()));
            if (q % 8 == 0) {
                lower = std::numeric_limits<Key>::min();
                upper = std::numeric_limits<Key>::max();
            }
            if (q % 8 == 1) {
                lower = std::numeric_limits<Key>::max();
                upper = std::numeric_limits<Key>::min();
            }
            if (n && q % 8 == 2) {
                lower = transformed(values[rng() % n], applied);
                upper = transformed(values[rng() % n], applied);
            }
            std::map<Key, Weight> grouped;
            for (int i = l; i < r; ++i) {
                const Key key = transformed(values[i], applied);
                auto it = grouped.find(key);
                if (it == grouped.end()) grouped.emplace(key, weights[i]);
                else it->second = Group::op(it->second, weights[i]);
            }
            Weight first_sum = Group::identity();
            std::optional<Key> first;
            for (auto it = grouped.lower_bound(lower); it != grouped.end(); ++it) {
                first_sum = Group::op(first_sum, it->second);
                if (predicate(first_sum)) { first = it->first; break; }
            }
            Weight last_sum = Group::identity();
            std::optional<Key> last;
            auto it = grouped.lower_bound(upper);
            while (it != grouped.begin()) {
                --it;
                last_sum = Group::op(last_sum, it->second);
                if (predicate(last_sum)) { last = it->first; break; }
            }
            if (variant == 0) {
                assert(value_only(raw.first_value(l, r, lower, predicate, applied)) == first);
                assert(value_only(raw.last_value(l, r, upper, predicate, applied)) == last);
            } else {
                assert(value_only(compressed.first_value(l, r, lower, predicate)) == first);
                assert(value_only(compressed.last_value(l, r, upper, predicate)) == last);
            }
        }
    }
}

template <class Key>
void structured_selection(const std::vector<Key>& values, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<long long> negative;
    std::vector<ledger> compound;
    for (std::size_t i = 0; i < values.size(); ++i) {
        negative.push_back(-static_cast<long long>(i % 7));
        compound.emplace_back(-static_cast<long long>(rng() % 32), 1 + i % 3);
    }
    compare_selection<Key, long long, cp::wavelet_sum_group<long long>>(
        values, negative, [](long long value) { return -value; }, seed);
    compare_selection<Key, ledger, ledger_group>(
        values, compound, [](const ledger& value) { return value.balance + 64LL * value.progress; }, seed + 1);
}

struct checked_sum_group {
    static long long identity() { return 0; }
    static long long op(long long a, long long b) {
        if (b > 0) assert(a <= LLONG_MAX - b);
        if (b < 0) assert(a >= LLONG_MIN - b);
        return a + b;
    }
    static long long inverse(long long value) {
        assert(value != LLONG_MIN);
        return -value;
    }
};

void inverse_regression() {
    const auto positive = [](long long sum) { return sum >= 1; };
    const std::vector<int> keys{0, 1, 2, 3, 4, 5, 6, 7};
    const std::vector<long long> weights{LLONG_MIN + 2, -1, -1, 1, 100, 1, 1, 1};
    cp::wavelet_matrix<int, long long, checked_sum_group> raw(keys);
    raw.build_weights(weights);
    assert(value_only(raw.first_value(0, 8, 3, positive)) == 3);
    cp::compressed_wavelet_matrix<int, long long, checked_sum_group> compressed(keys);
    compressed.build_weights(weights);
    assert(value_only(compressed.first_value(0, 8, 3, positive)) == 3);
    cp::wavelet_matrix<int, long long, checked_sum_group> flipped({0, 1});
    flipped.build_weights({1, LLONG_MIN});
    assert(value_only(flipped.first_value(0, 2, 1, positive, 1U)) == 1);
    assert(value_only(flipped.last_value(0, 2, 1, positive)) == 0);
}

void atomic_values() {
    const auto predicate = [](long long sum) { return sum >= 4; };
    cp::wavelet_matrix<int> raw({1, 1, 2, 3});
    raw.build_weights({100, -99, 2, 3});
    assert(value_only(raw.first_value(0, 4, 0, predicate)) == 3);
    assert(value_only(raw.last_value(0, 4, 4, predicate)) == 2);
    assert(value_only(raw.first_value(0, 4, 0, predicate, 1U)) == 2);
    assert(value_only(raw.last_value(0, 4, 4, predicate, 1U)) == 2);
    cp::compressed_wavelet_matrix<int> compressed({1, 1, 2, 3});
    compressed.build_weights({100, -99, 2, 3});
    assert(value_only(compressed.first_value(0, 4, 0, predicate)) == 3);
    assert(value_only(compressed.last_value(0, 4, 4, predicate)) == 2);
}

void full_width_boundary() {
    std::vector<std::uint64_t> keys;
    for (int bit = 0; bit < 64; ++bit) keys.push_back(UINT64_C(1) << bit);
    cp::wavelet_matrix<std::uint64_t> matrix(keys);
    matrix.build_weights(std::vector<long long>(64, 1));
    const auto all = [](long long sum) { return sum >= 64; };
    const auto absent = [](long long sum) { return sum >= 65; };
    assert(value_only(matrix.first_value(0, 64, 1, all)) == (UINT64_C(1) << 63));
    assert(!value_only(matrix.first_value(0, 64, 1, absent)));
    assert(value_only(matrix.last_value(0, 64, UINT64_MAX, all)) == 1);
    assert(!value_only(matrix.last_value(0, 64, UINT64_MAX, absent)));
}

int main() {
    const std::array<std::uint64_t, 9> pattern{
        0, UINT64_MAX, 1, UINT64_MAX - 1, UINT64_C(1) << 63,
        (UINT64_C(1) << 63) - 1, (UINT64_C(1) << 63) + 1,
        UINT64_C(1) << 40, (UINT64_C(1) << 40) + 1};
    std::mt19937_64 rng(20261103);
    for (int n : {0, 1, 2, 63, 64, 65, 127, 128, 129}) {
        std::vector<std::uint64_t> unsigned_values(n);
        std::vector<long long> signed_values(n);
        for (int i = 0; i < n; ++i) {
            unsigned_values[i] = pattern[i % pattern.size()];
            std::memcpy(&signed_values[i], &unsigned_values[i], sizeof(long long));
        }
        std::shuffle(unsigned_values.begin(), unsigned_values.end(), rng);
        std::shuffle(signed_values.begin(), signed_values.end(), rng);
        structured_selection(unsigned_values, rng());
        structured_selection(signed_values, rng());
    }
    structured_selection(std::vector<long long>(65, LLONG_MIN), 3);
    structured_selection(std::vector<std::uint64_t>(65, UINT64_MAX), 4);
    inverse_regression();
    atomic_values();
    full_width_boundary();
}
