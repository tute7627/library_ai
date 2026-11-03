// Source: https://github.com/tute7627/library_ai/blob/main/benchmarks/wavelet_matrix.cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

using Key = std::uint64_t;
using Clock = std::chrono::steady_clock;

struct Query {
    int l, r, k;
    Key lower, upper, mask;
};

template <class Function>
void measure(const std::string& label, Function function) {
    const auto start = Clock::now();
    const auto checksum = function();
    const auto end = Clock::now();
    const double ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << label << " " << std::fixed << std::setprecision(3)
              << ms << " ms checksum=" << checksum << '\n';
}

void run(const std::string& scenario, int n, int q, int mode) {
    std::mt19937_64 rng(20261031);
    std::vector<Key> values(n);
    for (auto& value : values) {
        value = rng();
        if (mode < 2) value &= (Key{1} << 20) - 1;
        if (mode == 1) value |= Key{1} << 60;
    }
    std::vector<Query> queries(q);
    for (auto& query : queries) {
        query.l = rng() % n;
        query.r = query.l + 1 + rng() % (n - query.l);
        query.k = rng() % (query.r - query.l);
        query.lower = values[rng() % n];
        query.upper = values[rng() % n];
        if (query.lower > query.upper) std::swap(query.lower, query.upper);
        query.mask = rng();
        if (mode < 2) query.mask &= (Key{1} << 20) - 1;
    }
    std::cout << scenario << " N=" << n << " Q=" << q << '\n';
    std::optional<cp::wavelet_matrix<Key>> wm;
    measure("build", [&] { wm.emplace(values); return wm->size(); });
    measure("kth_smallest", [&] {
        Key sum = 0;
        for (const auto& x : queries) sum += wm->kth_smallest(x.l, x.r, x.k);
        return sum;
    });
    measure("range_freq", [&] {
        Key sum = 0;
        for (const auto& x : queries) sum += wm->range_freq(x.l, x.r, x.lower, x.upper);
        return sum;
    });
    measure("kth_smallest_xor", [&] {
        Key sum = 0;
        for (const auto& x : queries) sum += wm->kth_smallest(x.l, x.r, x.k, x.mask);
        return sum;
    });
    std::vector<long long> weights(n, 1);
    measure("build_weights", [&] { wm->build_weights(weights); return wm->has_weights(); });
    measure("range_fold", [&] {
        Key sum = 0;
        for (const auto& x : queries) sum += wm->range_fold(x.l, x.r, x.lower, x.upper);
        return sum;
    });
    measure("range_fold_xor", [&] {
        Key sum = 0;
        for (const auto& x : queries)
            sum += wm->range_fold(x.l, x.r, x.lower, x.upper, x.mask);
        return sum;
    });
    measure("first_value", [&] {
        Key sum = 0;
        for (const auto& x : queries) {
            const auto result = wm->first_value(x.l, x.r, Key{0},
                [&x](long long s) { return s >= x.k + 1; });
            assert(result);
            sum += result->value;
        }
        return sum;
    });
    measure("first_value_xor", [&] {
        Key sum = 0;
        for (const auto& x : queries) {
            const auto result = wm->first_value(x.l, x.r, Key{0},
                [&x](long long s) { return s >= x.k + 1; }, x.mask);
            assert(result);
            sum += result->value;
        }
        return sum;
    });
    measure("last_value", [&] {
        Key sum = 0;
        for (const auto& x : queries) {
            const auto result = wm->last_value(x.l, x.r, std::numeric_limits<Key>::max(),
                [&x](long long s) { return s >= x.k + 1; });
            sum += result ? result->value : 0;
        }
        return sum;
    });
    measure("last_value_xor", [&] {
        Key sum = 0;
        for (const auto& x : queries) {
            const auto result = wm->last_value(x.l, x.r, std::numeric_limits<Key>::max(),
                [&x](long long s) { return s >= x.k + 1; }, x.mask);
            sum += result ? result->value : 0;
        }
        return sum;
    });
    const int multi_queries = std::min(q, 10000);
    for (int count : {1, 4, 16}) {
        std::vector<std::pair<int, int>> ranges(count);
        const auto split = [&](const Query& x) {
            for (int j = 0; j < count; ++j) {
                const auto length = static_cast<std::int64_t>(x.r - x.l);
                ranges[j] = {x.l + static_cast<int>(length * j / count),
                             x.l + static_cast<int>(length * (j + 1) / count)};
            }
        };
        std::cout << "multi R=" << count << " Q=" << multi_queries << std::endl;
        measure("multi_kth_smallest", [&] {
            Key sum = 0;
            for (int j = 0; j < multi_queries; ++j) {
                const auto& x = queries[j];
                split(x);
                sum += wm->kth_smallest(ranges, x.k);
            }
            return sum;
        });
        measure("multi_first_value", [&] {
            Key sum = 0;
            for (int j = 0; j < multi_queries; ++j) {
                const auto& x = queries[j];
                split(x);
                const auto result = wm->first_value(ranges, Key{0},
                    [&x](long long s) { return s >= x.k + 1; });
                assert(result);
                sum += result->value;
            }
            return sum;
        });
        measure("multi_last_value", [&] {
            Key sum = 0;
            for (int j = 0; j < multi_queries; ++j) {
                const auto& x = queries[j];
                split(x);
                const auto result = wm->last_value(ranges, std::numeric_limits<Key>::max(),
                    [&x](long long s) { return s >= x.k + 1; });
                sum += result ? result->value : 0;
            }
            return sum;
        });
    }
    wm.reset();
    std::optional<cp::compressed_wavelet_matrix<Key>> compressed;
    measure("compressed_build", [&] { compressed.emplace(values); return compressed->size(); });
    measure("compressed_kth_smallest", [&] {
        Key sum = 0;
        for (const auto& x : queries) sum += compressed->kth_smallest(x.l, x.r, x.k);
        return sum;
    });
    measure("compressed_range_freq", [&] {
        Key sum = 0;
        for (const auto& x : queries)
            sum += compressed->range_freq(x.l, x.r, x.lower, x.upper);
        return sum;
    });
}

int main(int argc, char** argv) {
    const int n = argc > 1 ? std::stoi(argv[1]) : 200000;
    const int q = argc > 2 ? std::stoi(argv[2]) : 100000;
    assert(n > 0 && q > 0);
    run("small_values", n, q, 0);
    run("common_high_bits", n, q, 1);
    run("full_width", n, q, 2);
}
