// Source: https://github.com/tute7627/library_ai/blob/main/tests/knapsack.test.cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
// competitive-verifier: STANDALONE
#include "library/template.hpp"
#include "library/knapsack.hpp"

namespace {

template <class Item>
__int128 selected_weight(const std::vector<Item>& items, const cp::knapsack_result& result) {
    assert(result.counts.size() == items.size());
    __int128 weight = 0, value = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto copies = result.counts[i];
        assert(copies >= 0);
        if constexpr (std::tuple_size_v<Item> == 3) assert(copies <= std::get<2>(items[i]));
        weight += static_cast<__int128>(std::get<0>(items[i])) * copies;
        value += static_cast<__int128>(std::get<1>(items[i])) * copies;
    }
    assert(value == result.value);
    return weight;
}

void verify_unbounded_knapsack(const std::vector<std::pair<int, int>>& items, int max_weight) {
    const cp::unbounded_knapsack_chan_he solver(items, true);
    std::vector<long long> dp(max_weight + 1, -1);
    dp[0] = 0;
    long long best = 0;
    for (int weight = 0; weight <= max_weight; ++weight) {
        for (const auto& [item_weight, value] : items) {
            if (item_weight <= weight && dp[weight - item_weight] >= 0) {
                dp[weight] = std::max(dp[weight], dp[weight - item_weight] + value);
            }
        }
        const auto exact = solver.exact_value_with_counts(weight);
        if (dp[weight] < 0) {
            assert(!solver.exact_value(weight));
            assert(!exact);
        } else {
            assert(solver.exact_value(weight) == dp[weight]);
            assert(exact && exact->value == dp[weight] && selected_weight(items, *exact) == weight);
        }
        best = std::max(best, dp[weight]);
        assert(solver.max_value_at_most(weight) == best);
        const auto at_most = solver.max_value_at_most_with_counts(weight);
        assert(at_most.value == best && selected_weight(items, at_most) <= weight);
    }
}

void test_unbounded_knapsack() {
    const auto max_weight = std::numeric_limits<long long>::max();
    const std::vector<std::pair<int, int>> empty;
    const cp::unbounded_knapsack_chan_he empty_solver(empty);
    assert(empty_solver.exact_value(0) == 0);
    assert(!empty_solver.exact_value(max_weight));
    assert(empty_solver.max_value_at_most(max_weight) == 0);

    const std::vector<std::pair<int, long long>> tied_items{{4, 4}, {2, 1}, {2, 2}, {6, 4}};
    const cp::unbounded_knapsack_chan_he tied_solver(tied_items);
    assert(tied_solver.exact_value(max_weight - 1) == max_weight - 1);
    assert(!tied_solver.exact_value(max_weight));
    assert(tied_solver.max_value_at_most(max_weight) == max_weight - 1);

    const std::vector<std::pair<int, long long>> large_value{{2, max_weight}};
    const cp::unbounded_knapsack_chan_he large_value_solver(large_value);
    assert(large_value_solver.exact_value(2) == max_weight);
    assert(large_value_solver.max_value_at_most(3) == max_weight);
    bool overflow_detected = false;
    try {
        (void)large_value_solver.exact_value(4);
    } catch (const std::overflow_error&) {
        overflow_detected = true;
    }
    assert(overflow_detected);

    verify_unbounded_knapsack({{4, 0}, {6, 0}}, 180);
    verify_unbounded_knapsack({{1, 2}, {2, 1}, {3, 7}, {4, 4}}, 200);

    std::vector<std::pair<int, int>> dense;
    for (int weight = 1; weight <= 40; ++weight) {
        dense.emplace_back(weight, weight * weight);
    }
    verify_unbounded_knapsack(dense, 3400);

    std::mt19937 rng(20261109);
    std::uniform_int_distribution<int> size_dist(0, 24), weight_dist(1, 24), value_dist(0, 200);
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<std::pair<int, int>> items(size_dist(rng));
        for (auto& [weight, value] : items) {
            weight = weight_dist(rng);
            value = value_dist(rng);
        }
        verify_unbounded_knapsack(items, 1200);
    }
}

void verify_unbounded_min_knapsack(const std::vector<std::pair<int, int>>& items, int max_target) {
    const cp::unbounded_knapsack_min_chan_he solver(items, true);
    int max_item_weight = 1;
    for (const auto& item : items) max_item_weight = std::max(max_item_weight, item.first);

    constexpr auto unreachable = std::numeric_limits<long long>::max();
    const int limit = max_target + max_item_weight - 1;
    std::vector<long long> dp(limit + 1, unreachable);
    dp[0] = 0;
    for (int weight = 1; weight <= limit; ++weight) {
        for (const auto& [item_weight, cost] : items) {
            if (item_weight <= weight && dp[weight - item_weight] != unreachable) {
                dp[weight] = std::min(dp[weight], dp[weight - item_weight] + cost);
            }
        }
    }
    long long best = unreachable;
    for (int weight = limit; weight >= 0; --weight) {
        best = std::min(best, dp[weight]);
        if (weight > max_target) continue;
        const auto answer = solver.min_value_at_least(weight);
        const auto restored = solver.min_value_at_least_with_counts(weight);
        if (best == unreachable) {
            assert(!answer);
            assert(!restored);
        } else {
            assert(answer == best);
            assert(restored && restored->value == best && selected_weight(items, *restored) >= weight);
        }
    }
}

void test_unbounded_min_knapsack() {
    constexpr auto maximum = std::numeric_limits<long long>::max();
    const std::vector<std::pair<int, int>> empty;
    const cp::unbounded_knapsack_min_chan_he empty_solver(empty);
    assert(empty_solver.min_value_at_least(0) == 0);
    assert(!empty_solver.min_value_at_least(maximum));

    const std::vector<std::pair<int, int>> overshoot_items{{3, 1}, {4, 8}};
    const cp::unbounded_knapsack_min_chan_he overshoot_solver(overshoot_items);
    assert(overshoot_solver.min_value_at_least(8) == 3);
    assert(overshoot_solver.min_value_at_least(9) == 3);
    assert(overshoot_solver.min_value_at_least(10) == 4);

    const std::vector<std::pair<int, long long>> expensive_exact{{3, 0}, {1, maximum}};
    const cp::unbounded_knapsack_min_chan_he free_solver(expensive_exact);
    assert(free_solver.min_value_at_least(2) == 0);
    assert(free_solver.min_value_at_least(maximum) == 0);

    const std::vector<std::pair<int, int>> even_items{{2, 1}};
    const cp::unbounded_knapsack_min_chan_he even_solver(even_items);
    assert(even_solver.min_value_at_least(maximum) == maximum / 2 + 1);

    const std::vector<std::pair<int, long long>> expensive_items{{3, maximum}, {1, maximum}};
    const cp::unbounded_knapsack_min_chan_he expensive_solver(expensive_items);
    assert(expensive_solver.min_value_at_least(3) == maximum);
    bool overflow_detected = false;
    try {
        (void)expensive_solver.min_value_at_least(4);
    } catch (const std::overflow_error&) {
        overflow_detected = true;
    }
    assert(overflow_detected);

    verify_unbounded_min_knapsack({{4, 4}, {2, 9}, {2, 2}, {6, 7}}, 180);
    verify_unbounded_min_knapsack({{4, 0}, {6, 0}}, 180);

    std::vector<std::pair<int, int>> dense;
    for (int weight = 1; weight <= 40; ++weight) {
        dense.emplace_back(weight, 80 - weight);
    }
    verify_unbounded_min_knapsack(dense, 3400);

    std::mt19937 rng(20261110);
    std::uniform_int_distribution<int> size_dist(0, 24), weight_dist(1, 24), cost_dist(0, 200);
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<std::pair<int, int>> items(size_dist(rng));
        for (auto& [weight, cost] : items) {
            weight = weight_dist(rng);
            cost = cost_dist(rng);
        }
        verify_unbounded_min_knapsack(items, 1200);
    }
}

template <class Exception, class Function>
void expect_exception(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return;
    }
    assert(false);
}

int total_bounded_weight(const std::vector<std::tuple<int, int, int>>& items) {
    int total_weight = 0;
    for (const auto& [weight, value, count] : items) total_weight += weight * count;
    return total_weight;
}

std::pair<std::vector<long long>, std::vector<long long>> brute_bounded_knapsack_values(
    const std::vector<std::tuple<int, int, int>>& items) {
    const int total_weight = total_bounded_weight(items);
    constexpr auto unreachable = std::numeric_limits<long long>::max();
    std::vector<long long> maximum(total_weight + 1, -1), minimum(total_weight + 1, unreachable);
    maximum[0] = minimum[0] = 0;
    for (const auto& [weight, value, count] : items) {
        auto next_maximum = maximum;
        auto next_minimum = minimum;
        for (int before = 0; before <= total_weight; ++before) {
            if (maximum[before] < 0) continue;
            for (int copies = 1; copies <= count; ++copies) {
                const int after = before + weight * copies;
                next_maximum[after] = std::max(next_maximum[after], maximum[before] + value * copies);
                next_minimum[after] = std::min(next_minimum[after], minimum[before] + value * copies);
            }
        }
        maximum = std::move(next_maximum);
        minimum = std::move(next_minimum);
    }
    return {std::move(maximum), std::move(minimum)};
}

template <class MaximumSolver, class MinimumSolver>
void verify_bounded_knapsack_queries(const std::vector<std::tuple<int, int, int>>& items,
                                     const MaximumSolver& maximum_solver,
                                     const MinimumSolver& minimum_solver,
                                     int max_capacity, int max_target) {
    auto [maximum, minimum] = brute_bounded_knapsack_values(items);
    const int total_weight = static_cast<int>(maximum.size()) - 1;
    constexpr auto unreachable = std::numeric_limits<long long>::max();
    long long best = 0;
    for (int weight = 0; weight <= max_capacity; ++weight) {
        const auto exact = maximum_solver.exact_value_with_counts(weight);
        if (weight <= total_weight && maximum[weight] >= 0) {
            assert(maximum_solver.exact_value(weight) == maximum[weight]);
            assert(exact && exact->value == maximum[weight] && selected_weight(items, *exact) == weight);
            best = std::max(best, maximum[weight]);
        } else {
            assert(!maximum_solver.exact_value(weight));
            assert(!exact);
        }
        assert(maximum_solver.max_value_at_most(weight) == best);
        const auto at_most = maximum_solver.max_value_at_most_with_counts(weight);
        assert(at_most.value == best && selected_weight(items, at_most) <= weight);
    }
    for (int weight = total_weight - 1; weight >= 0; --weight) {
        minimum[weight] = std::min(minimum[weight], minimum[weight + 1]);
    }
    for (int weight = 0; weight <= max_target; ++weight) {
        const auto at_least = minimum_solver.min_value_at_least_with_counts(weight);
        if (weight <= total_weight && minimum[weight] != unreachable) {
            assert(minimum_solver.min_value_at_least(weight) == minimum[weight]);
            assert(at_least && at_least->value == minimum[weight] &&
                   selected_weight(items, *at_least) >= weight);
        } else {
            assert(!minimum_solver.min_value_at_least(weight));
            assert(!at_least);
        }
    }
}

void verify_bounded_knapsack(const std::vector<std::tuple<int, int, int>>& items,
                             int max_capacity, int max_target) {
    const cp::bounded_knapsack_deque maximum_solver(items, max_capacity, true);
    const cp::bounded_knapsack_min_deque minimum_solver(items, max_target, true);
    verify_bounded_knapsack_queries(items, maximum_solver, minimum_solver, max_capacity, max_target);
}

void test_bounded_knapsack() {
    const std::vector<std::tuple<int, int, int>> few{{3, 2, 1}, {5, 8, 1}, {1, 100, 0}};
    const cp::bounded_knapsack_deque maximum_solver(few, 12);
    const cp::bounded_knapsack_min_deque minimum_solver(few, 12);
    assert(!maximum_solver.exact_value(6));
    assert(maximum_solver.max_value_at_most(6) == 8);
    assert(maximum_solver.exact_value(8) == 10);
    assert(maximum_solver.max_value_at_most(12) == 10);
    assert(minimum_solver.min_value_at_least(4) == 8);
    assert(minimum_solver.min_value_at_least(6) == 10);
    assert(!minimum_solver.min_value_at_least(9));
    expect_exception<std::out_of_range>([&] { (void)maximum_solver.exact_value(13); });
    expect_exception<std::out_of_range>([&] { (void)maximum_solver.max_value_at_most(13); });
    expect_exception<std::out_of_range>([&] { (void)minimum_solver.min_value_at_least(13); });
    verify_bounded_knapsack({}, 10, 10);
    verify_bounded_knapsack({{1, 7, 0}}, 10, 10);
    verify_bounded_knapsack(few, 0, 0);

    constexpr auto largest = std::numeric_limits<long long>::max();
    const std::vector<std::tuple<long long, int, long long>> heavy{{largest, 1, largest}, {3, 9, 2}};
    const cp::bounded_knapsack_deque heavy_maximum(heavy, 7);
    const cp::bounded_knapsack_min_deque heavy_minimum(heavy, 7);
    assert(!heavy_maximum.exact_value(7));
    assert(heavy_maximum.max_value_at_most(7) == 18);
    assert(heavy_minimum.min_value_at_least(7) == 1);

    const std::vector<std::tuple<int, int, long long>> abundant{{2, 3, largest}};
    const cp::bounded_knapsack_deque abundant_maximum(abundant, 7);
    const cp::bounded_knapsack_min_deque abundant_minimum(abundant, 7);
    assert(abundant_maximum.max_value_at_most(7) == 9);
    assert(abundant_minimum.min_value_at_least(7) == 12);

    const std::vector<std::tuple<int, long long, int>> expensive{{1, largest, 2}};
    const cp::bounded_knapsack_deque expensive_maximum(expensive, 1);
    const cp::bounded_knapsack_min_deque expensive_minimum(expensive, 3);
    assert(expensive_maximum.exact_value(1) == largest);
    assert(expensive_minimum.min_value_at_least(1) == largest);
    assert(!expensive_minimum.min_value_at_least(3));
    expect_exception<std::overflow_error>([&] { (void)cp::bounded_knapsack_deque(expensive, 2); });
    expect_exception<std::overflow_error>([&] { (void)expensive_minimum.min_value_at_least(2); });

    const std::vector<std::tuple<int, long long, int>> free_overshoot{{1, largest, 2}, {3, 0, 1}};
    const cp::bounded_knapsack_min_deque free_solver(free_overshoot, 6);
    assert(free_solver.min_value_at_least(2) == 0);
    assert(free_solver.min_value_at_least(4) == largest);
    assert(!free_solver.min_value_at_least(6));

    std::mt19937 rng(20261111);
    std::uniform_int_distribution<int> size_dist(0, 8), weight_dist(1, 15), value_dist(0, 50);
    std::uniform_int_distribution<int> count_dist(0, 5), limit_dist(0, 125);
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<std::tuple<int, int, int>> items(size_dist(rng));
        for (auto& [weight, value, count] : items) {
            weight = weight_dist(rng);
            value = value_dist(rng);
            count = count_dist(rng);
        }
        const int max_capacity = limit_dist(rng), max_target = limit_dist(rng);
        verify_bounded_knapsack(items, max_capacity, max_target);
    }
}

void verify_axiotis_tzamos_bounded_knapsack(const std::vector<std::tuple<int, int, int>>& items,
                                            int max_capacity, int max_target) {
    const cp::bounded_knapsack_axiotis_tzamos maximum_solver(items, max_capacity, true);
    const cp::bounded_knapsack_min_axiotis_tzamos minimum_solver(items, max_target, true);
    verify_bounded_knapsack_queries(items, maximum_solver, minimum_solver, max_capacity, max_target);
}

void test_axiotis_tzamos_bounded_knapsack() {
    verify_axiotis_tzamos_bounded_knapsack({}, 10, 10);
    verify_axiotis_tzamos_bounded_knapsack({{1, 7, 0}}, 10, 10);
    verify_axiotis_tzamos_bounded_knapsack({{2, 1, 3}}, 0, 0);
    verify_axiotis_tzamos_bounded_knapsack({{18, 40, 1}, {6, 4, 2}, {12, 7, 1},
                                           {18, 1, 1}, {6, 10, 1}, {12, 13, 2}}, 107, 110);
    verify_axiotis_tzamos_bounded_knapsack({{5, 12, 1}, {8, 20, 1}, {8, 4, 1}, {13, 7, 1}}, 23, 27);

    constexpr auto largest = std::numeric_limits<long long>::max();
    const std::vector<std::tuple<int, int, long long>> same_weight{
        {4, 7, largest}, {4, 12, 2}, {4, 99, 0}, {4, 3, 1}, {4, 5, largest}};
    const cp::bounded_knapsack_axiotis_tzamos same_weight_max(same_weight, 19, true);
    const cp::bounded_knapsack_min_axiotis_tzamos same_weight_min(same_weight, 19, true);
    assert(!same_weight_max.exact_value_with_counts(19));
    const auto maximum = same_weight_max.max_value_at_most_with_counts(19);
    assert(maximum.value == 38 && selected_weight(same_weight, maximum) == 16);
    assert((maximum.counts == std::vector<long long>{2, 2, 0, 0, 0}));
    const auto minimum = same_weight_min.min_value_at_least_with_counts(19);
    assert(minimum && minimum->value == 23 && selected_weight(same_weight, *minimum) == 20);
    assert((minimum->counts == std::vector<long long>{0, 0, 0, 1, 4}));

    const std::vector<std::tuple<long long, int, long long>> heavy{
        {largest, 8, largest}, {3, 5, 1}, {largest - 1, 2, largest}, {6, 3, 1}};
    const cp::bounded_knapsack_axiotis_tzamos heavy_max(heavy, 7);
    const cp::bounded_knapsack_min_axiotis_tzamos heavy_min(heavy, 7, true);
    assert(heavy_max.max_value_at_most(7) == 5);
    const auto overshoot = heavy_min.min_value_at_least_with_counts(7);
    assert(overshoot && overshoot->value == 2 && selected_weight(heavy, *overshoot) == largest - 1);

    const std::vector<std::tuple<int, long long, int>> expensive{{1, largest, 1}, {2, largest, 1}};
    const cp::bounded_knapsack_axiotis_tzamos expensive_max(expensive, 2, true);
    const cp::bounded_knapsack_min_axiotis_tzamos expensive_min(expensive, 4, true);
    const auto exact = expensive_max.exact_value_with_counts(2);
    assert(exact && exact->value == largest && selected_weight(expensive, *exact) == 2);
    const auto finite = expensive_min.min_value_at_least_with_counts(2);
    assert(finite && finite->value == largest && selected_weight(expensive, *finite) == 2);
    assert(!expensive_min.min_value_at_least_with_counts(4));
    expect_exception<std::overflow_error>([&] { (void)cp::bounded_knapsack_axiotis_tzamos(expensive, 3); });
    expect_exception<std::overflow_error>([&] { (void)expensive_min.min_value_at_least_with_counts(3); });

    const std::vector<std::tuple<int, long long, int>> expensive_group{{1, largest, 2}, {1, 1, 1}, {3, 0, 1}};
    const cp::bounded_knapsack_min_axiotis_tzamos expensive_group_min(expensive_group, 6, true);
    const auto cheap = expensive_group_min.min_value_at_least_with_counts(4);
    assert(cheap && cheap->value == 1 && selected_weight(expensive_group, *cheap) == 4);
    expect_exception<std::overflow_error>([&] { (void)expensive_group_min.min_value_at_least_with_counts(5); });

    std::mt19937 rng(20261113);
    std::uniform_int_distribution<int> size_dist(0, 14), weight_dist(1, 7), value_dist(0, 30);
    std::uniform_int_distribution<int> count_dist(0, 4), limit_dist(0, 125);
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<std::tuple<int, int, int>> items(size_dist(rng));
        for (auto& [weight, value, count] : items) {
            weight = (trial % 2 + 1) * weight_dist(rng);
            value = value_dist(rng);
            count = count_dist(rng);
        }
        const int max_capacity = limit_dist(rng), max_target = limit_dist(rng);
        verify_axiotis_tzamos_bounded_knapsack(items, max_capacity, max_target);
    }
}

void verify_bounded_knapsack_proximity(const std::vector<std::tuple<int, int, int>>& items,
                                        int weight, int capacity, int target) {
    const auto [maximum, minimum] = brute_bounded_knapsack_values(items);
    const int total_weight = static_cast<int>(maximum.size()) - 1;
    constexpr auto unreachable = std::numeric_limits<long long>::max();
    std::optional<long long> expected_exact, expected_at_least;
    if (weight <= total_weight && maximum[weight] >= 0) expected_exact = maximum[weight];
    const auto expected_at_most = *std::max_element(maximum.begin(),
                                                     maximum.begin() + std::min(capacity, total_weight) + 1);
    if (target <= total_weight) {
        const auto best = *std::min_element(minimum.begin() + target, minimum.end());
        if (best != unreachable) expected_at_least = best;
    }

    assert(cp::bounded_knapsack_proximity_exact(items, weight) == expected_exact);
    assert(cp::bounded_knapsack_proximity_at_most(items, capacity) == expected_at_most);
    assert(cp::bounded_knapsack_proximity_at_least(items, target) == expected_at_least);

    const auto exact = cp::bounded_knapsack_proximity_exact_with_counts(items, weight);
    assert(exact.has_value() == expected_exact.has_value());
    if (exact) assert(exact->value == expected_exact && selected_weight(items, *exact) == weight);
    const auto at_most = cp::bounded_knapsack_proximity_at_most_with_counts(items, capacity);
    assert(at_most.value == expected_at_most && selected_weight(items, at_most) <= capacity);
    const auto at_least = cp::bounded_knapsack_proximity_at_least_with_counts(items, target);
    assert(at_least.has_value() == expected_at_least.has_value());
    if (at_least) assert(at_least->value == expected_at_least && selected_weight(items, *at_least) >= target);
}

void test_bounded_knapsack_proximity() {
    verify_bounded_knapsack_proximity({}, 0, 0, 0);
    verify_bounded_knapsack_proximity({{2, 5, 0}}, 1, 10, 1);
    for (int weight = 0; weight <= 25; ++weight) {
        verify_bounded_knapsack_proximity({{6, 11, 2}, {4, 9, 2}, {3, 6, 1}}, weight, weight, weight);
        verify_bounded_knapsack_proximity({{6, 0, 2}, {4, 0, 2}, {2, 0, 1}}, weight, weight, weight);
    }

    constexpr long long stock = 1000000000000LL;
    const std::vector<std::tuple<int, int, long long>> abundant{{2, 5, stock}, {3, 4, stock}};
    const auto exact = cp::bounded_knapsack_proximity_exact_with_counts(abundant, 5 * stock - 2);
    assert(exact && exact->value == 9 * stock - 5 && selected_weight(abundant, *exact) == 5 * stock - 2);
    const auto at_most = cp::bounded_knapsack_proximity_at_most_with_counts(abundant, 5 * stock - 2);
    assert(at_most.value == 9 * stock - 4 && selected_weight(abundant, at_most) == 5 * stock - 3);
    const auto at_least = cp::bounded_knapsack_proximity_at_least_with_counts(abundant, 3 * stock + 1);
    assert(at_least && at_least->value == 4 * stock + 5 && selected_weight(abundant, *at_least) == 3 * stock + 2);

    constexpr auto largest = std::numeric_limits<long long>::max();
    const std::vector<std::tuple<long long, long long, long long>> wide_complement{
        {2, largest, largest}, {largest, largest, 0}, {3, 2, 1}};
    assert(cp::bounded_knapsack_proximity_at_least(wide_complement, 3) == 2);
    const auto finite_cover = cp::bounded_knapsack_proximity_at_least_with_counts(wide_complement, 3);
    assert(finite_cover && finite_cover->value == 2 && selected_weight(wide_complement, *finite_cover) == 3);
    assert((finite_cover->counts == std::vector<long long>{0, 0, 1}));

    const std::vector<std::tuple<int, long long, int>> wide_greedy{{2, largest, 2}, {5, 7, 1}};
    assert(cp::bounded_knapsack_proximity_exact(wide_greedy, 5) == 7);
    const auto finite_exact = cp::bounded_knapsack_proximity_exact_with_counts(wide_greedy, 5);
    assert(finite_exact && finite_exact->value == 7 && selected_weight(wide_greedy, *finite_exact) == 5);
    expect_exception<std::overflow_error>([&] { (void)cp::bounded_knapsack_proximity_exact(wide_greedy, 4); });
    expect_exception<std::overflow_error>([&] { (void)cp::bounded_knapsack_proximity_at_most(wide_greedy, 5); });
    expect_exception<std::overflow_error>([&] { (void)cp::bounded_knapsack_proximity_at_least(wide_greedy, 7); });
    expect_exception<std::overflow_error>([&] {
        (void)cp::bounded_knapsack_proximity_exact_with_counts(wide_greedy, 4);
    });
    expect_exception<std::overflow_error>([&] {
        (void)cp::bounded_knapsack_proximity_at_most_with_counts(wide_greedy, 5);
    });
    expect_exception<std::overflow_error>([&] {
        (void)cp::bounded_knapsack_proximity_at_least_with_counts(wide_greedy, 7);
    });

    const std::vector<std::tuple<int, long long, long long>> overflowing_value(3, {1, largest, largest});
    const std::vector<std::tuple<long long, int, long long>> overflowing_weight(3, {largest, 0, largest});
    expect_exception<std::overflow_error>([&] {
        (void)cp::bounded_knapsack_proximity_at_most(overflowing_value, 1);
    });
    expect_exception<std::overflow_error>([&] {
        (void)cp::bounded_knapsack_proximity_at_least(overflowing_weight, 1);
    });

    std::mt19937 rng(20261114);
    std::uniform_int_distribution<int> size_dist(0, 12), weight_dist(1, 7), value_dist(0, 30);
    std::uniform_int_distribution<int> count_dist(0, 8);
    for (int trial = 0; trial < 400; ++trial) {
        std::vector<std::tuple<int, int, int>> items(size_dist(rng));
        for (auto& [weight, value, count] : items) {
            weight = (trial % 2 + 1) * weight_dist(rng);
            value = value_dist(rng);
            count = count_dist(rng);
        }
        std::uniform_int_distribution<int> query_dist(0, total_bounded_weight(items) + 3);
        const int weight = query_dist(rng), capacity = query_dist(rng), target = query_dist(rng);
        verify_bounded_knapsack_proximity(items, weight, capacity, target);
    }
}

void verify_compressed_bounded_knapsack(const std::vector<std::tuple<int, int, int>>& items) {
    const cp::bounded_knapsack_compressed solver(items, true);
    const int last_query = total_bounded_weight(items) + 1;
    verify_bounded_knapsack_queries(items, solver, solver, last_query, last_query);
}

void test_compressed_bounded_knapsack() {
    verify_compressed_bounded_knapsack({});
    verify_compressed_bounded_knapsack({{7, 15, 0}});
    verify_compressed_bounded_knapsack(
        {{5, 10, 55}, {4, 8, 63}, {6, 13, 53}, {3, 6, 58}, {7, 8, 4}, {2, 5, 57}});
    verify_compressed_bounded_knapsack({{2, 2, 60}, {4, 5, 80}, {6, 0, 52}});

    constexpr long long stock = 1000000000000LL;
    const std::vector<std::tuple<int, int, long long>> abundant{{2, 5, stock}, {3, 4, stock}};
    const cp::bounded_knapsack_compressed abundant_solver(abundant);
    assert(abundant_solver.exact_value(5 * stock - 2) == 9 * stock - 5);
    assert(abundant_solver.max_value_at_most(5 * stock - 2) == 9 * stock - 4);
    assert(abundant_solver.min_value_at_least(3 * stock + 1) == 4 * stock + 5);
    assert(!abundant_solver.exact_value(5 * stock + 1));
    assert(!abundant_solver.min_value_at_least(5 * stock + 1));
    assert(abundant_solver.max_value_at_most(5 * stock + 1) == 9 * stock);

    constexpr auto largest = std::numeric_limits<long long>::max();
    const std::vector<std::tuple<int, long long, long long>> wide{{1, largest, largest}, {1, 1, largest}};
    const cp::bounded_knapsack_compressed wide_solver(wide);
    assert(wide_solver.exact_value(1) == largest);
    assert(wide_solver.max_value_at_most(1) == largest);
    assert(wide_solver.min_value_at_least(largest) == largest);
    assert(wide_solver.min_value_at_least(largest - 1) == largest - 1);
    expect_exception<std::overflow_error>([&] { (void)wide_solver.exact_value(2); });
    expect_exception<std::overflow_error>([&] { (void)wide_solver.max_value_at_most(2); });

    const std::vector<std::tuple<int, long long, int>> expensive{{2, largest, 2}};
    const cp::bounded_knapsack_compressed expensive_solver(expensive);
    assert(!expensive_solver.exact_value(1));
    assert(expensive_solver.min_value_at_least(1) == largest);
    expect_exception<std::overflow_error>([&] { (void)expensive_solver.min_value_at_least(3); });

    std::mt19937 rng(20261112);
    std::uniform_int_distribution<int> size_dist(0, 7), weight_dist(1, 8), value_dist(0, 30);
    std::uniform_int_distribution<int> count_dist(0, 45);
    for (int trial = 0; trial < 300; ++trial) {
        std::vector<std::tuple<int, int, int>> items(size_dist(rng));
        for (auto& [weight, value, count] : items) {
            weight = weight_dist(rng);
            value = value_dist(rng);
            count = count_dist(rng);
        }
        verify_compressed_bounded_knapsack(items);
    }
}

void test_compressed_sweet_alchemy() {
    constexpr int budget = 1000000000, limit = 1000000;
    const std::vector<std::tuple<int, int, int>> items{
        {5, 675, budget / 675}, {1, 159, limit}, {3, 393, limit}, {1, 135, limit}, {1, 147, limit}};
    const cp::bounded_knapsack_compressed solver(items);
    long long low = 0, high = budget + 1LL;
    while (high - low > 1) {
        const long long middle = low + (high - low) / 2;
        const auto value = solver.min_value_at_least(middle);
        if (value && *value <= budget) {
            low = middle;
        } else {
            high = middle;
        }
    }
    assert(low == 7496296);
}

void test_reconstruction_options() {
    const std::vector<std::pair<int, int>> unbounded;
    const cp::unbounded_knapsack_chan_he unbounded_max(unbounded);
    const cp::unbounded_knapsack_min_chan_he unbounded_min(unbounded, false);
    expect_exception<std::logic_error>([&] { (void)unbounded_max.exact_value_with_counts(0); });
    expect_exception<std::logic_error>([&] { (void)unbounded_max.max_value_at_most_with_counts(0); });
    expect_exception<std::logic_error>([&] { (void)unbounded_min.min_value_at_least_with_counts(1); });

    const std::vector<std::tuple<int, int, int>> bounded;
    const cp::bounded_knapsack_deque deque_max(bounded, 0);
    const cp::bounded_knapsack_min_deque deque_min(bounded, 0);
    const cp::bounded_knapsack_compressed compressed(bounded, false);
    expect_exception<std::logic_error>([&] { (void)deque_max.exact_value_with_counts(0); });
    expect_exception<std::logic_error>([&] { (void)deque_max.max_value_at_most_with_counts(0); });
    expect_exception<std::logic_error>([&] { (void)deque_min.min_value_at_least_with_counts(1); });
    expect_exception<std::logic_error>([&] { (void)compressed.exact_value_with_counts(0); });
    expect_exception<std::logic_error>([&] { (void)compressed.max_value_at_most_with_counts(0); });
    expect_exception<std::logic_error>([&] { (void)compressed.min_value_at_least_with_counts(1); });

    const cp::bounded_knapsack_deque enabled_max(bounded, 0, true);
    const cp::bounded_knapsack_min_deque enabled_min(bounded, 0, true);
    expect_exception<std::out_of_range>([&] { (void)enabled_max.exact_value_with_counts(1); });
    expect_exception<std::out_of_range>([&] { (void)enabled_max.max_value_at_most_with_counts(1); });
    expect_exception<std::out_of_range>([&] { (void)enabled_min.min_value_at_least_with_counts(1); });

    const std::vector<std::tuple<int, int, int>> grouped{{2, 3, 1}, {2, 5, 2}};
    const cp::bounded_knapsack_axiotis_tzamos grouped_max(grouped, 4);
    const cp::bounded_knapsack_min_axiotis_tzamos grouped_min(grouped, 4, false);
    assert(grouped_max.exact_value(4) == 10);
    assert(grouped_max.max_value_at_most(3) == 5);
    assert(grouped_min.min_value_at_least(4) == 8);
    expect_exception<std::logic_error>([&] { (void)grouped_max.exact_value_with_counts(4); });
    expect_exception<std::logic_error>([&] { (void)grouped_max.max_value_at_most_with_counts(3); });
    expect_exception<std::logic_error>([&] { (void)grouped_min.min_value_at_least_with_counts(4); });

    const cp::bounded_knapsack_axiotis_tzamos enabled_grouped_max(grouped, 4, true);
    const cp::bounded_knapsack_min_axiotis_tzamos enabled_grouped_min(grouped, 4, true);
    expect_exception<std::out_of_range>([&] { (void)enabled_grouped_max.exact_value_with_counts(5); });
    expect_exception<std::out_of_range>([&] { (void)enabled_grouped_max.max_value_at_most_with_counts(5); });
    expect_exception<std::out_of_range>([&] { (void)enabled_grouped_min.min_value_at_least_with_counts(5); });
}

void test_large_reconstruction() {
    constexpr auto largest = std::numeric_limits<long long>::max();
    const std::vector<std::pair<int, int>> even{{2, 1}};
    const cp::unbounded_knapsack_min_chan_he even_solver(even, true);
    const auto overshoot = even_solver.min_value_at_least_with_counts(largest);
    assert(overshoot && overshoot->value == largest / 2 + 1);
    assert(selected_weight(even, *overshoot) > largest);

    const std::vector<std::pair<int, int>> free{{2, 0}, {1, 0}};
    const cp::unbounded_knapsack_min_chan_he free_solver(free, true);
    const auto zero_cost = free_solver.min_value_at_least_with_counts(largest);
    assert(zero_cost && zero_cost->value == 0 && selected_weight(free, *zero_cost) >= largest);

    const std::vector<std::pair<int, long long>> expensive{{2, largest}};
    const cp::unbounded_knapsack_chan_he expensive_solver(expensive, true);
    expect_exception<std::overflow_error>([&] { (void)expensive_solver.exact_value_with_counts(4); });
    expect_exception<std::overflow_error>([&] { (void)expensive_solver.max_value_at_most_with_counts(4); });

    const std::vector<std::tuple<long long, int, long long>> heavy{{3, 9, 2}, {largest, 1, largest}};
    const cp::bounded_knapsack_min_deque heavy_solver(heavy, 7, true);
    const auto heavy_result = heavy_solver.min_value_at_least_with_counts(7);
    assert(heavy_result && heavy_result->value == 1 && selected_weight(heavy, *heavy_result) == largest);

    const std::vector<std::tuple<int, long long, long long>> wide{
        {1, 1, largest}, {2, 0, 0}, {1, largest, largest}};
    const cp::bounded_knapsack_compressed wide_solver(wide, true);
    const auto cheap = wide_solver.min_value_at_least_with_counts(largest);
    assert(cheap && cheap->value == largest && selected_weight(wide, *cheap) == largest);
    assert((cheap->counts == std::vector<long long>{largest, 0, 0}));
    expect_exception<std::overflow_error>([&] { (void)wide_solver.exact_value_with_counts(2); });
    expect_exception<std::overflow_error>([&] { (void)wide_solver.max_value_at_most_with_counts(2); });
}

}

int main() {
    test_unbounded_knapsack();
    test_unbounded_min_knapsack();
    test_bounded_knapsack();
    test_axiotis_tzamos_bounded_knapsack();
    test_bounded_knapsack_proximity();
    test_compressed_bounded_knapsack();
    test_compressed_sweet_alchemy();
    test_reconstruction_options();
    test_large_reconstruction();

    const std::vector<std::pair<int, int>> empty;
    assert(cp::knapsack01_bruteforce_at_most(empty, 0) == 0);
    assert(cp::knapsack01_bruteforce_at_least(empty, 0) == 0);
    assert(!cp::knapsack01_bruteforce_at_least(empty, 1));

    const std::vector<std::pair<int, int>> items{{4, 10}, {5, 3}, {2, 2}, {2, 2}};
    assert(cp::knapsack01_bruteforce_at_most(items, 4) == 10);
    assert(cp::knapsack01_bruteforce_at_most(items, 8) == 14);
    assert(cp::knapsack01_bruteforce_at_least(items, 4) == 3);
    assert(cp::knapsack01_bruteforce_at_least(items, 5) == 3);
    assert(!cp::knapsack01_bruteforce_at_least(items, 14));

    const std::vector<std::pair<int, long long>> signed_items{{3, 7}, {-3, 2}, {0, -5}};
    assert(cp::knapsack01_bruteforce_at_most(signed_items, 0) == 9);
    assert(cp::knapsack01_bruteforce_at_least(signed_items, 0) == -5);
    assert(cp::knapsack01_bruteforce_at_least(signed_items, 3) == 2);

    const auto max_value = std::numeric_limits<long long>::max();
    const auto min_value = std::numeric_limits<long long>::min();
    const std::vector<std::pair<long long, long long>> extremes{{max_value, max_value},
                                                                {0, min_value}};
    assert(cp::knapsack01_bruteforce_at_most(extremes, max_value) == max_value);
    assert(cp::knapsack01_bruteforce_at_least(extremes, 0) == min_value);
    assert(cp::knapsack01_bruteforce_at_least(extremes, max_value) == -1);
    const std::vector<std::pair<long long, long long>> single{{max_value, max_value}};
    assert(cp::knapsack01_bruteforce_at_least(single, max_value) == max_value);

    std::mt19937 rng(20261108);
    std::uniform_int_distribution<int> size_dist(0, 10), item_dist(-8, 12);
    std::uniform_int_distribution<int> limit_dist(0, 30), target_dist(-10, 40);
    for (int trial = 0; trial < 300; ++trial) {
        const int n = size_dist(rng);
        std::vector<std::pair<int, int>> random_items(n);
        for (auto& [weight, value] : random_items) {
            weight = item_dist(rng);
            value = item_dist(rng);
        }
        const int limit = limit_dist(rng), target = target_dist(rng);
        std::vector<long long> at_most, at_least;
        for (unsigned mask = 0; mask < (1U << n); ++mask) {
            long long weight = 0, value = 0;
            for (int i = 0; i < n; ++i) {
                if ((mask >> i) & 1U) {
                    weight += random_items[i].first;
                    value += random_items[i].second;
                }
            }
            if (weight <= limit) at_most.push_back(value);
            if (weight >= target) at_least.push_back(value);
        }
        assert(cp::knapsack01_bruteforce_at_most(random_items, limit) ==
               *std::max_element(at_most.begin(), at_most.end()));
        const auto maximum = cp::knapsack01_bruteforce_at_most_with_counts(random_items, limit);
        assert(maximum.value == *std::max_element(at_most.begin(), at_most.end()));
        assert(selected_weight(random_items, maximum) <= limit);
        for (const auto count : maximum.counts) assert(count <= 1);
        const auto answer = cp::knapsack01_bruteforce_at_least(random_items, target);
        const auto minimum = cp::knapsack01_bruteforce_at_least_with_counts(random_items, target);
        if (at_least.empty()) {
            assert(!answer);
            assert(!minimum);
        } else {
            assert(answer == *std::min_element(at_least.begin(), at_least.end()));
            assert(minimum && minimum->value == answer);
            assert(selected_weight(random_items, *minimum) >= target);
            for (const auto count : minimum->counts) assert(count <= 1);
        }
    }
}
