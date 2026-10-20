// Source: https://github.com/tute7627/library_ai/blob/main/benchmarks/hashmap.cpp
#include "library/template.hpp"
#include "library/hashmap.hpp"

using Key = long long;
using Clock = std::chrono::steady_clock;
using Standard = std::unordered_map<Key, long long>;
using Flat = cp::hashmap<Key, long long>;

struct Measurement {
    double ms;
    std::uint64_t checksum;
};

template <class Map>
Measurement measure(const std::string& operation, const std::vector<Key>& keys,
                    const std::vector<Key>& queries) {
    Map map = [] {
        if constexpr (std::is_same_v<Map, Flat>) {
            return Map(0, UINT64_C(20261020));
        } else {
            return Map();
        }
    }();
    std::uint64_t checksum = 0;
    Clock::time_point start;
    if (operation == "churn_long") {
        map.reserve(keys.size());
        for (std::size_t i = 0; i < keys.size(); ++i) {
            map.insert({64LL * i, static_cast<long long>(i)});
        }
        start = Clock::now();
        for (int round = 0; round < 8; ++round) {
            for (std::size_t i = 0; i < keys.size(); ++i) {
                checksum += map.erase(64LL * i + round);
                map.insert({64LL * i + round + 1, static_cast<long long>(i)});
            }
        }
        checksum += map.size();
    } else if (operation == "insert" || operation == "insert_grow") {
        start = Clock::now();
        if (operation == "insert") map.reserve(keys.size());
        for (std::size_t i = 0; i < keys.size(); ++i) map.insert({keys[i], static_cast<long long>(i)});
        checksum = map.size();
    } else {
        map.reserve(keys.size());
        for (std::size_t i = 0; i < keys.size(); ++i) map.insert({keys[i], static_cast<long long>(i)});
        start = Clock::now();
        if (operation == "find_hit" || operation == "find_miss") {
            for (int repeat = 0; repeat < 4; ++repeat) {
                for (const auto key : queries) {
                    const auto it = map.find(key);
                    if (it != map.end()) checksum += static_cast<std::uint64_t>(it->second) + 1;
                }
            }
        } else if (operation == "update") {
            for (const auto key : queries) checksum += static_cast<std::uint64_t>(++map[key]);
        } else if (operation == "erase_insert") {
            for (std::size_t i = 0; i < keys.size(); ++i) {
                checksum += map.erase(keys[i]);
                map.insert({keys[i] | 1, static_cast<long long>(i)});
            }
            checksum += map.size();
        } else if (operation == "rehash") {
            map.rehash(map.bucket_count() * 2);
            checksum = map.size();
        } else if (operation == "iterate") {
            for (int repeat = 0; repeat < 4; ++repeat) {
                for (const auto& [key, value] : map) {
                    checksum += static_cast<std::uint64_t>(key) ^ static_cast<std::uint64_t>(value);
                }
            }
        } else {
            std::abort();
        }
    }
    const auto stop = Clock::now();
    std::uint64_t content = 0;
    for (const auto& [key, value] : map) {
        content += static_cast<std::uint64_t>(key) * UINT64_C(0x9e3779b97f4a7c15)
                   ^ static_cast<std::uint64_t>(value);
    }
    checksum ^= content;
    return {std::chrono::duration<double, std::milli>(stop - start).count(), checksum};
}

void compare(const std::string& distribution, const std::string& operation,
             const std::vector<Key>& keys, const std::vector<Key>& queries, int rounds) {
    std::vector<double> standard, flat;
    std::uint64_t checksum = 0;
    for (int i = 0; i < rounds; ++i) {
        Measurement a, b;
        if (i % 2 == 0) {
            a = measure<Standard>(operation, keys, queries);
            b = measure<Flat>(operation, keys, queries);
        } else {
            b = measure<Flat>(operation, keys, queries);
            a = measure<Standard>(operation, keys, queries);
        }
        if (a.checksum != b.checksum) {
            std::cerr << "Checksum mismatch: " << distribution << ',' << operation << '\n';
            std::exit(1);
        }
        checksum = a.checksum;
        standard.push_back(a.ms);
        flat.push_back(b.ms);
    }
    std::sort(standard.begin(), standard.end());
    std::sort(flat.begin(), flat.end());
    const double a = standard[rounds / 2], b = flat[rounds / 2];
    std::cout << distribution << ',' << operation << ',' << keys.size() << ','
              << a << ',' << b << ',' << a / b << ',' << checksum << '\n';
}

int main(int argc, char** argv) {
    const int n = argc > 1 ? std::stoi(argv[1]) : 200000;
    const int rounds = argc > 2 ? std::stoi(argv[2]) : 5;
    if (n <= 0 || rounds <= 0) return 1;
    std::mt19937_64 random(20261020);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "distribution,operation,n,std_ms,hashmap_ms,speedup,checksum\n";
    for (const std::string distribution : {"sequential", "random64"}) {
        std::vector<Key> keys(n);
        if (distribution == "sequential") {
            for (int i = 0; i < n; ++i) keys[i] = 2LL * i;
        } else {
            std::unordered_map<Key, bool> used;
            used.reserve(n);
            for (int i = 0; i < n;) {
                const Key key = static_cast<Key>(random() & UINT64_C(0x7ffffffffffffffe));
                if (used.insert({key, true}).second) keys[i++] = key;
            }
        }
        auto hits = keys;
        std::shuffle(hits.begin(), hits.end(), random);
        auto misses = hits;
        for (auto& key : misses) key |= 1;
        for (const std::string operation : {"insert", "insert_grow", "find_hit", "find_miss", "update", "erase_insert", "iterate", "rehash"}) {
            compare(distribution, operation, keys, operation == "find_miss" ? misses : hits, rounds);
        }
    }
    std::vector<Key> churn_keys(n);
    compare("fresh_integer", "churn_long", churn_keys, churn_keys, rounds);
    const int attack_n = std::min(n, 6000);
    Standard baseline;
    baseline.reserve(attack_n);
    std::vector<Key> attack(attack_n);
    for (int i = 0; i < attack_n; ++i) attack[i] = static_cast<Key>(i) * baseline.bucket_count();
    compare("std_bucket_multiples", "insert", attack, attack, rounds);
}
