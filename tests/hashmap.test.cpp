// Source: https://github.com/tute7627/library_ai/blob/main/tests/hashmap.test.cpp
// competitive-verifier: STANDALONE
#include "library/template.hpp"
#include "library/hashmap.hpp"

using Map = cp::hashmap<long long, long long>;
static_assert(std::is_base_of_v<std::forward_iterator_tag,
                               std::iterator_traits<Map::iterator>::iterator_category>);
static_assert(std::is_base_of_v<std::forward_iterator_tag,
                               std::iterator_traits<Map::const_iterator>::iterator_category>);
#if __cplusplus >= 202002L
static_assert(std::forward_iterator<Map::iterator>);
static_assert(std::forward_iterator<Map::const_iterator>);
#endif
static_assert(std::is_same_v<decltype(Map::value_type::first), const long long>);
static_assert(std::is_same_v<decltype(std::declval<const Map&>().begin()->second), long long>);
static_assert(!std::is_assignable_v<decltype((std::declval<const Map&>().begin()->second)), long long>);

template <class Key>
void equal_maps(const cp::hashmap<Key, long long>& actual,
                const std::unordered_map<Key, long long>& expected) {
    assert(actual.size() == expected.size());
    std::unordered_map<Key, long long> seen;
    for (const auto& [key, value] : actual) {
        assert(seen.insert({key, value}).second);
        const auto it = expected.find(key);
        assert(it != expected.end() && it->second == value);
        assert(actual.find(key)->second == value);
        assert(actual.count(key) == 1 && actual.contains(key));
    }
    assert(seen == expected);
    assert(actual.empty() == expected.empty());
}

void basics() {
    Map map;
    assert(map.max_load_factor() == 0.5F);
    assert(map.empty() && map.size() == 0);
    assert(map.begin() == map.end() && map.cbegin() == map.cend());
    assert(map.count(42) == 0 && map.find(42) == map.end());
    assert(map.erase(42) == 0);
    map.clear();
    map.reserve(0);
    map.rehash(0);
    std::ostringstream output;
    output << map;
    assert(output.str() == "{}");
    const std::vector<long long> keys{
        0, 1, -1, std::numeric_limits<long long>::min(),
        std::numeric_limits<long long>::max(), 1LL << 32, -(1LL << 32)};
    for (const auto key : keys) {
        assert(map.insert({key, 3}).second);
        assert(!map.insert({key, 9}).second);
        assert(map.at(key) == 3);
        map[key] += 2;
    }
    for (auto& [key, value] : map) {
        (void)key;
        ++value;
    }
    const Map& constant = map;
    for (const auto key : keys) assert(constant.at(key) == 6);
    bool threw = false;
    try {
        (void)constant.at(1234);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    assert(threw && map.size() == keys.size());
    assert(map.begin() == constant.begin() && constant.begin() == map.begin());
    for (auto it = map.begin(); it != map.end();) it = map.erase(it);
    assert(map.empty());
    map.insert({{10, 20}, {30, 40}});
    map.erase(map.cbegin());
    assert(map.size() == 1);
    map.clear();
    map[7] = 8;
    output.str("");
    map.dump(output);
    assert(output.str() == "{7: 8}");
    Map initialized{{1, 2}, {3, 4}, {1, 9}};
    assert(initialized.size() == 2 && initialized.at(1) == 2);
    Map with_buckets(50);
    assert(with_buckets.bucket_count() >= 50 && with_buckets.empty());
    const std::vector<std::pair<long long, long long>> entries{{11, 12}, {13, 14}};
    with_buckets.insert(entries.begin(), entries.end());
    assert(with_buckets.at(11) == 12 && with_buckets.at(13) == 14);
    map = map;
    map = std::move(map);
    assert(map.at(7) == 8);
    Map other{{100, 200}};
    using std::swap;
    swap(map, other);
    assert(map.at(100) == 200 && other.at(7) == 8);
}

template <class Key>
void differential(std::uint64_t seed) {
    std::mt19937_64 random(seed);
    cp::hashmap<Key, long long> actual(0, seed);
    std::unordered_map<Key, long long> expected;
    actual.reserve(28);
    for (int i = 0; i < 28; ++i) actual[i] = expected[i] = i;
    for (int step = 0; step < 140000; ++step) {
        const Key key = static_cast<Key>(static_cast<int>(random() % 8192) - 4096);
        const long long value = static_cast<long long>(random() % 1000000);
        switch (random() % 10) {
            case 0:
            case 1:
            case 2:
                actual[key] += value;
                expected[key] += value;
                break;
            case 3: {
                const auto a = actual.insert({key, value});
                const auto b = expected.insert({key, value});
                assert(a.second == b.second && a.first->first == b.first->first &&
                       a.first->second == b.first->second);
                break;
            }
            case 4:
                assert(actual.erase(key) == expected.erase(key));
                break;
            case 5: {
                const auto a = actual.find(key);
                const auto b = expected.find(key);
                assert((a == actual.end()) == (b == expected.end()));
                if (a != actual.end()) {
                    a->second = value;
                    b->second = value;
                }
                break;
            }
            case 6:
                assert(actual.count(key) == expected.count(key));
                break;
            case 7:
                if (step % 97 == 0) actual.reserve(random() % 20000);
                break;
            case 8:
                if (!actual.empty()) {
                    auto it = actual.begin();
                    expected.erase(it->first);
                    actual.erase(it);
                }
                break;
            case 9:
                if (step % 541 == 0) {
                    auto copy = actual;
                    equal_maps(copy, expected);
                    auto moved = std::move(copy);
                    assert(copy.empty() && copy.begin() == copy.end());
                    copy[123] = 456;
                    assert(copy.at(123) == 456);
                    actual = moved;
                    actual = std::move(moved);
                    actual.rehash(0);
                }
                if (step % 10007 == 0) {
                    actual.clear();
                    expected.clear();
                }
                break;
        }
        if (step % 1024 == 0) equal_maps(actual, expected);
    }
    equal_maps(actual, expected);
}

void churn_and_growth() {
    Map map(0, 43210);
    for (int round = 0; round < 12; ++round) {
        for (int i = 0; i < 12000; ++i) map[i] = i + round;
        for (int i = 0; i < 12000; i += 2) assert(map.erase(i) == 1);
        for (int i = 0; i < 12000; ++i) {
            assert(map.count(i) == static_cast<unsigned>(i % 2));
            if (i % 2 != 0) assert(map.at(i) == i + round);
        }
        for (int i = 0; i < 12000; i += 2) assert(map.insert({i, round}).second);
        assert(map.size() == 12000);
        if (round % 2 == 0) map.rehash(map.bucket_count());
    }
    map.clear();
    map.rehash(0);
    assert(map.bucket_count() == 0);
    map[-1] = 5;
    assert(map.at(-1) == 5);
    for (long long i = 0; i < 20000; ++i) map[i * 107897] = i;
    for (long long i = 0; i < 20000; ++i) assert(map.at(i * 107897) == i);
    cp::hashmap<unsigned long long, int> unsigned_keys;
    unsigned_keys[0] = 1;
    unsigned_keys[std::numeric_limits<unsigned long long>::max()] = 2;
    assert(unsigned_keys.size() == 2);
}

void dense_churn() {
    std::mt19937_64 random(567890);
    for (const int buckets : {16, 32, 64, 128}) {
        Map actual(buckets, static_cast<std::uint64_t>(buckets));
        std::unordered_map<long long, long long> expected;
        const int n = static_cast<int>(actual.bucket_count() * actual.max_load_factor());
        const auto capacity = actual.bucket_count();
        std::vector<long long> keys(n);
        for (int i = 0; i < n; ++i) {
            keys[i] = i;
            actual[i] = expected[i] = i;
        }
        for (long long step = 0; step < 20000; ++step) {
            auto& old_key = keys[random() % n];
            assert(actual.erase(old_key) == expected.erase(old_key));
            old_key = step + 1000;
            assert(actual.insert({old_key, step}).second);
            expected[old_key] = step;
            assert(actual.bucket_count() == capacity);
            if (step % 256 == 0) equal_maps(actual, expected);
        }
        equal_maps(actual, expected);
        actual[-1] = expected[-1] = 123;
        assert(actual.load_factor() <= actual.max_load_factor());
        equal_maps(actual, expected);
    }
}

void lookup_boundaries() {
    const std::array<long long, 24> keys{
        485, 1213, 1295, 468, 9217, 5474, 3278, 1628, 1424, 3635, 923, 10177,
        2868, 5203, 1784, 2110, 11330, 10749, 11854, 7782, 12760, 6423, 1857, 10665};
    for (unsigned subset = 0; subset < (1U << 12); ++subset) {
        Map actual(32, 20261020ULL);
        std::unordered_map<long long, long long> expected;
        const auto check = [&] {
            assert(actual.size() == expected.size() && actual.bucket_count() == 32);
            for (const auto key : keys) {
                const auto a = actual.find(key);
                const auto b = expected.find(key);
                assert((a == actual.end()) == (b == expected.end()));
                assert(actual.contains(key) == (b != expected.end()));
                if (b != expected.end()) assert(a->second == b->second);
            }
        };
        for (unsigned i = 0; i < 12; ++i) {
            if ((subset >> i) & 1U) actual[keys[i]] = expected[keys[i]] = i;
        }
        check();
        for (unsigned i = 1; i < 12; i += 2) {
            assert(actual.erase(keys[i]) == expected.erase(keys[i]));
        }
        check();
        for (unsigned i = 12; i < 18; ++i) actual[keys[i]] = expected[keys[i]] = i;
        check();
    }
    for (const unsigned base : {0U, 6U}) {
        Map actual(32, 20261020ULL);
        std::unordered_map<long long, long long> expected;
        for (unsigned i = 0; i < 4; ++i) actual[keys[base + i]] = expected[keys[base + i]] = 10 + i;
        const auto stable_key = keys[base + 3];
        auto* const stable_value = &actual.at(stable_key);
        assert(actual.erase(keys[base]) == expected.erase(keys[base]));
        const auto duplicate = actual.insert({stable_key, -1});
        const auto expected_duplicate = expected.insert({stable_key, -1});
        assert(!duplicate.second && duplicate.second == expected_duplicate.second);
        assert(duplicate.first->second == expected_duplicate.first->second);
        assert(&duplicate.first->second == stable_value);
        equal_maps(actual, expected);
        const auto restored = actual.insert({keys[base], 77});
        const auto expected_restored = expected.insert({keys[base], 77});
        assert(restored.second && restored.second == expected_restored.second);
        assert(restored.first->second == expected_restored.first->second);
        actual[keys[base + 12]] = expected[keys[base + 12]] = 99;
        assert(actual.bucket_count() == 32);
        assert(&actual.at(stable_key) == stable_value && *stable_value == 13);
        equal_maps(actual, expected);
    }
}

void sparse_rebuild() {
    for (const int buckets : {16, 32, 64, 128, 4096}) {
        Map actual(buckets, static_cast<std::uint64_t>(buckets));
        std::unordered_map<long long, long long> expected;
        const std::array<long long, 5> keys{
            std::numeric_limits<long long>::min(), -1, 0, 17,
            std::numeric_limits<long long>::max()};
        for (const auto key : keys) actual[key] = expected[key] = key;
        actual.reserve(static_cast<std::size_t>(buckets) * 4);
        equal_maps(actual, expected);
        for (const auto key : keys) {
            if (key != 17) assert(actual.erase(key) == expected.erase(key));
        }
        actual.rehash(static_cast<std::size_t>(buckets) * 16);
        equal_maps(actual, expected);
        actual.clear();
        expected.clear();
        actual.rehash(buckets);
        equal_maps(actual, expected);
        actual[-7] = expected[-7] = 123;
        equal_maps(actual, expected);
    }
}

struct Tracked {
    static inline int live = 0;
    static inline int copies_before_throw = -1;
    int value;
    Tracked() = delete;
    explicit Tracked(int value) : value(value) { ++live; }
    Tracked(const Tracked& other) : value(other.value) {
        if (copies_before_throw == 0) throw std::runtime_error("copy failure");
        if (copies_before_throw > 0) --copies_before_throw;
        ++live;
    }
    Tracked(Tracked&& other) noexcept(false) : value(other.value) { ++live; }
    ~Tracked() { --live; }
};

void fixed_hash_seed() {
    for (const auto seed : {UINT64_C(0), UINT64_C(123456789), UINT64_MAX}) {
        Map first(0, seed), second(128, seed);
        assert(first.hash_seed() == seed && second.hash_seed() == seed);
        first.reserve(1000);
        second.reserve(1000);
        for (int i = -300; i < 300; ++i) {
            first[i] = second[i] = i;
        }
        for (int i = -300; i < 300; i += 3) {
            first.erase(i);
            second.erase(i);
        }
        for (int i = 1000; i < 1300; ++i) {
            first[i] = second[i] = -i;
        }
        first.rehash(2048);
        second.rehash(2048);
        assert(first.hash_seed() == seed && second.hash_seed() == seed);
        std::ostringstream a, b;
        first.dump(a);
        second.dump(b);
        assert(a.str() == b.str());
        auto copied = first;
        auto moved = std::move(copied);
        assert(moved.hash_seed() == seed);
        a.str("");
        moved.dump(a);
        assert(a.str() == b.str());
        Map assigned(0, seed ^ 1);
        assigned = first;
        assert(assigned.hash_seed() == seed);
        assigned = std::move(moved);
        assert(assigned.hash_seed() == seed);
        Map other(0, seed ^ 1);
        swap(assigned, other);
        assert(other.hash_seed() == seed && assigned.hash_seed() == (seed ^ 1));
        other.clear();
        other.rehash(0);
        other[42] = 1;
        assert(other.hash_seed() == seed && other.at(42) == 1);
    }
    Map random;
    Map replay(0, random.hash_seed());
    for (int i = 0; i < 500; ++i) random[i] = replay[i] = i;
    std::ostringstream a, b;
    a << random;
    b << replay;
    assert(a.str() == b.str());
}

void lifetimes_and_exceptions() {
    {
        cp::hashmap<int, std::string> map;
        map.reserve(1);
        const auto capacity = map.bucket_count();
        const auto n = static_cast<int>(capacity * map.max_load_factor());
        for (int i = 0; i < n; ++i) map.try_emplace(i, 100, 'x');
        map.try_emplace(100, map.at(0));
        assert(map.bucket_count() > capacity);
        assert(map.at(100) == std::string(100, 'x'));
    }
    {
        cp::hashmap<int, std::unique_ptr<int>> map;
        for (int i = 0; i < 1000; ++i) map.try_emplace(i, std::make_unique<int>(i));
        auto pointer = std::make_unique<int>(99);
        assert(!map.try_emplace(0, std::move(pointer)).second);
        assert(pointer && *pointer == 99);
        map.insert({1001, std::make_unique<int>(1001)});
        map[1002] = std::make_unique<int>(1002);
        map.reserve(10000);
        for (int i = 0; i < 1000; ++i) assert(*map.at(i) == i);
        auto moved = std::move(map);
        assert(map.empty());
        map[5] = std::make_unique<int>(6);
        moved.clear();
    }
    {
        cp::hashmap<int, Tracked> map;
        for (int i = 0; i < 100; ++i) map.try_emplace(i, i);
        assert(Tracked::live == 100);
        const auto capacity = map.bucket_count();
        Tracked::copies_before_throw = 7;
        bool threw = false;
        try {
            map.reserve(10000);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw && Tracked::live == 100 && map.bucket_count() == capacity);
        for (int i = 0; i < 100; ++i) assert(map.at(i).value == i);
        Tracked::copies_before_throw = 5;
        threw = false;
        try {
            auto copy = map;
            (void)copy;
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw && Tracked::live == 100);
        Tracked::copies_before_throw = -1;
        auto copy = map;
        assert(Tracked::live == 200);
        map.erase(0);
        assert(Tracked::live == 199);
        copy.clear();
        assert(Tracked::live == 99);
    }
    assert(Tracked::live == 0);
    Map map{{1, 2}};
    const auto impossible = std::numeric_limits<std::size_t>::max();
    bool threw = false;
    try {
        map.reserve(impossible);
    } catch (const std::length_error&) {
        threw = true;
    }
    assert(threw && map.at(1) == 2);
}

int main() {
    basics();
    differential<int>(123456);
    differential<long long>(987654);
    churn_and_growth();
    dense_churn();
    lookup_boundaries();
    sparse_rebuild();
    fixed_hash_seed();
    lifetimes_and_exceptions();
}
