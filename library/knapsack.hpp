// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#pragma once

namespace cp {

struct knapsack_result {
    long long value;
    std::vector<long long> counts;
};

namespace knapsack_detail {

template <bool Reconstruct = false, class Weight, class Value, class Visit>
bool enumerate_subsets(const std::vector<std::pair<Weight, Value>>& items,
                       std::size_t index, long long total_weight, long long total_value,
                       const Visit& visit, long long* counts = nullptr) {
    static_assert(std::is_integral_v<Weight> && std::is_integral_v<Value>);

    if (index == items.size()) return visit(total_weight, total_value);

    const auto& [weight, value] = items[index];
    const bool skip = enumerate_subsets<Reconstruct>(items, index + 1, total_weight, total_value,
                                                     visit, counts);
    const bool take = enumerate_subsets<Reconstruct>(items, index + 1,
        total_weight + static_cast<long long>(weight), total_value + static_cast<long long>(value),
        visit, counts);
    if constexpr (Reconstruct) {
        if (take || skip) counts[index] = take;
    }
    return take || skip;
}

}

template <class Weight, class Value>
long long knapsack01_bruteforce_at_most(const std::vector<std::pair<Weight, Value>>& items,
                                       long long limit_weight) {
    assert(limit_weight >= 0);
    long long best = 0;
    knapsack_detail::enumerate_subsets(items, 0, 0, 0, [&](long long weight, long long value) {
        if (weight > limit_weight || value <= best) return false;
        best = value;
        return true;
    });
    return best;
}

template <class Weight, class Value>
std::optional<long long> knapsack01_bruteforce_at_least(
    const std::vector<std::pair<Weight, Value>>& items, long long target_weight) {
    std::optional<long long> best;
    knapsack_detail::enumerate_subsets(items, 0, 0, 0, [&](long long weight, long long value) {
        if (weight < target_weight || (best && value >= *best)) return false;
        best = value;
        return true;
    });
    return best;
}

template <class Weight, class Value>
knapsack_result knapsack01_bruteforce_at_most_with_counts(
    const std::vector<std::pair<Weight, Value>>& items, long long limit_weight) {
    assert(limit_weight >= 0);
    knapsack_result result{0, std::vector<long long>(items.size())};
    knapsack_detail::enumerate_subsets<true>(items, 0, 0, 0, [&](long long weight, long long value) {
        if (weight > limit_weight || value <= result.value) return false;
        result.value = value;
        return true;
    }, result.counts.data());
    return result;
}

template <class Weight, class Value>
std::optional<knapsack_result> knapsack01_bruteforce_at_least_with_counts(
    const std::vector<std::pair<Weight, Value>>& items, long long target_weight) {
    knapsack_result result{0, std::vector<long long>(items.size())};
    bool found = false;
    knapsack_detail::enumerate_subsets<true>(items, 0, 0, 0, [&](long long weight, long long value) {
        if (weight < target_weight || (found && value >= result.value)) return false;
        result.value = value;
        found = true;
        return true;
    }, result.counts.data());
    if (!found) return std::nullopt;
    return result;
}

namespace knapsack_detail {

using knapsack_value = unsigned long long;
inline constexpr knapsack_value unreachable = std::numeric_limits<knapsack_value>::max();
inline constexpr knapsack_value overflow = static_cast<knapsack_value>(
    std::numeric_limits<long long>::max()) + 1;

inline std::vector<knapsack_value> prefix_max_values(const std::vector<knapsack_value>& exact) {
    std::vector<knapsack_value> prefix;
    prefix.reserve(exact.size());
    knapsack_value best = 0;
    for (const auto value : exact) {
        if (value != unreachable) best = std::max(best, value);
        prefix.push_back(best);
    }
    return prefix;
}

template <bool Minimize>
struct unbounded_chan_he_table {
    using value_type = knapsack_value;

    long long period_weight = 1;
    long long period_value = 0;
    std::vector<value_type> values;

private:
    using item_type = std::pair<long long, long long>;

    bool reconstruction_enabled_;
    std::size_t item_count_;
    std::size_t period_item_index_ = 0;
    std::vector<std::pair<std::size_t, std::size_t>> trace_items_;
    std::vector<std::size_t> last_items_;

    static bool better_density(const item_type& left, const item_type& right) {
        const auto lhs = static_cast<__int128>(left.second) * right.first;
        const auto rhs = static_cast<__int128>(right.second) * left.first;
        if (lhs != rhs) return Minimize ? lhs < rhs : lhs > rhs;
        return left.first < right.first;
    }

public:
    template <class Weight, class Value>
    explicit unbounded_chan_he_table(const std::vector<std::pair<Weight, Value>>& items,
                                     bool enable_reconstruction)
        : reconstruction_enabled_(enable_reconstruction), item_count_(items.size()) {
        static_assert(std::is_integral_v<Weight> && std::is_integral_v<Value>);
        if (items.empty()) return;

        long long max_weight = 0;
        item_type period_item{0, 0};
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto& [weight, value] = items[i];
            const item_type item{static_cast<long long>(weight), static_cast<long long>(value)};
            assert(item.first > 0 && item.second >= 0);
            max_weight = std::max(max_weight, item.first);
            if (period_item.first == 0 || better_density(item, period_item)) {
                period_item = item;
                period_item_index_ = i;
            }
        }
        std::tie(period_weight, period_value) = period_item;

        const auto cutoff = static_cast<__int128>(period_weight - 1) * max_weight;
        if (cutoff >= values.max_size() || cutoff >= std::numeric_limits<long long>::max()) {
            throw std::length_error("unbounded knapsack table is too large");
        }
        const auto limit = static_cast<std::size_t>(cutoff);
        values.resize(limit + 1);
        if (limit == 0) return;

        std::vector<long long> best_by_weight(static_cast<std::size_t>(max_weight) + 1, -1);
        std::vector<std::size_t> index_by_weight;
        if (enable_reconstruction) index_by_weight.resize(best_by_weight.size());
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto& [weight, value] = items[i];
            const auto item_weight = static_cast<std::size_t>(weight);
            auto& best = best_by_weight[item_weight];
            const auto item_value = static_cast<long long>(value);
            if (best < 0 || (Minimize ? item_value < best : item_value > best)) {
                best = item_value;
                if (enable_reconstruction) index_by_weight[item_weight] = i;
            }
        }
        std::vector<item_type> candidates;
        for (std::size_t weight = 1; weight < best_by_weight.size(); ++weight) {
            if (best_by_weight[weight] >= 0) {
                candidates.emplace_back(static_cast<long long>(weight), best_by_weight[weight]);
            }
        }
        std::sort(candidates.begin(), candidates.end(), better_density);
        if (enable_reconstruction) {
            trace_items_.reserve(candidates.size());
            for (const auto& item : candidates) {
                const auto weight = static_cast<std::size_t>(item.first);
                trace_items_.emplace_back(index_by_weight[weight], weight);
            }
            last_items_.resize(values.size());
        }

        const auto pruning_bound = static_cast<__int128>(2) * max_weight * max_weight;
        std::vector<std::size_t> last_total_weight(candidates.size(), limit);
        for (std::size_t i = 1; i < candidates.size(); ++i) {
            last_total_weight[i] =
                static_cast<std::size_t>(std::min(cutoff, (pruning_bound - 1) / i));
        }
        std::size_t count = candidates.size();
        for (std::size_t total_weight = 1; total_weight <= limit; ++total_weight) {
            while (total_weight > last_total_weight[count - 1]) --count;
            value_type best = Minimize ? overflow : 0;
            auto last_item = candidates.size();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& [weight, value] = candidates[i];
                const auto step = static_cast<std::size_t>(weight);
                if (step > total_weight) continue;
                const auto previous = values[total_weight - step];
                if (previous == unreachable) continue;
                const auto candidate = previous + static_cast<value_type>(value);
                if (last_item == candidates.size() || (Minimize ? candidate < best : candidate > best)) {
                    best = Minimize ? std::min(overflow, candidate) : candidate;
                    last_item = i;
                }
            }
            if constexpr (!Minimize) {
                if (best >= overflow) {
                    throw std::overflow_error("unbounded knapsack value exceeds long long");
                }
            }
            values[total_weight] = last_item == candidates.size() ? unreachable : best;
            if (enable_reconstruction) last_items_[total_weight] = last_item;
        }
    }

    std::pair<std::size_t, long long> reduce(long long weight) const {
        if (weight == 0) return {0, 0};
        const auto cutoff = static_cast<long long>(values.size() - 1);
        long long copies = 0;
        if (weight > cutoff) {
            const long long excess = weight - cutoff;
            copies = excess / period_weight + (excess % period_weight != 0);
            weight -= copies * period_weight;
        }
        return {static_cast<std::size_t>(weight), copies};
    }

    std::optional<long long> query(const std::vector<value_type>& table, long long weight) const {
        assert(weight >= 0);
        if (weight == 0) return 0;
        if (table.empty()) return std::nullopt;
        const auto [index, copies] = reduce(weight);
        const auto value = table[index];
        if (value == unreachable) return std::nullopt;
        const auto result = static_cast<__int128>(copies) * period_value + value;
        if (result > std::numeric_limits<long long>::max()) {
            throw std::overflow_error("unbounded knapsack value exceeds long long");
        }
        return static_cast<long long>(result);
    }

    void require_reconstruction() const {
        if (!reconstruction_enabled_) throw std::logic_error("unbounded knapsack reconstruction is disabled");
    }

    knapsack_result restore(long long value, std::size_t weight, long long copies) const {
        knapsack_result result{value, std::vector<long long>(item_count_)};
        if (copies > 0) result.counts[period_item_index_] = copies;
        while (weight > 0) {
            const auto& [index, step] = trace_items_[last_items_[weight]];
            ++result.counts[index];
            weight -= step;
        }
        return result;
    }
};

struct bounded_trace_stage {
    std::size_t item_index;
    std::size_t step;
    std::vector<std::size_t> counts;
    std::size_t before_target;
};

struct bounded_trace_history {
    bool enabled;
    std::size_t item_count;
    std::vector<bounded_trace_stage> stages;
    std::vector<std::size_t> best_weights;

    void require_enabled() const {
        if (!enabled) throw std::logic_error("bounded knapsack reconstruction is disabled");
    }

    void record_at_most_weights(const std::vector<knapsack_value>& values) {
        if (!enabled) return;
        best_weights.resize(values.size());
        for (std::size_t i = 1; i < values.size(); ++i) {
            best_weights[i] = values[i] > values[i - 1] ? i : best_weights[i - 1];
        }
    }

    void aggregate_at_least_values(std::vector<knapsack_value>& values) {
        auto best = unreachable;
        std::size_t best_weight = values.size() - 1;
        if (enabled) best_weights.resize(values.size());
        for (std::size_t i = values.size(); i-- > 0;) {
            if (values[i] < best) {
                best = values[i];
                best_weight = i;
            }
            values[i] = best;
            if (enabled) best_weights[i] = best_weight;
        }
    }

    template <bool Saturated, class Assign>
    void backtrack(std::size_t weight, const Assign& assign) const {
        for (auto it = stages.rbegin(); it != stages.rend(); ++it) {
            const auto copies = it->counts[weight];
            assign(it->item_index, copies);
            if constexpr (Saturated) {
                if (weight == it->counts.size() - 1) {
                    weight = it->before_target;
                    continue;
                }
            }
            weight -= copies * it->step;
        }
    }

    template <bool Minimize>
    knapsack_result restore(long long value, std::size_t weight) const {
        knapsack_result result{value, std::vector<long long>(item_count)};
        backtrack<Minimize>(weight, [&](std::size_t index, std::size_t copies) {
            result.counts[index] = static_cast<long long>(copies);
        });
        return result;
    }
};

template <bool Minimize, bool Record>
void update_bounded_deque(std::vector<knapsack_value>& dp, std::size_t step,
                          long long value, unsigned long long count, bounded_trace_stage* stage) {
    const std::size_t last_exact_weight = dp.size() - (Minimize ? 2 : 1);
    knapsack_value reached_target = dp.back();
    std::deque<std::pair<std::size_t, __int128>> candidates;
    for (std::size_t residue = 0; residue < step; ++residue) {
        candidates.clear();
        const std::size_t last_position = (last_exact_weight - residue) / step;
        for (std::size_t position = 0; position <= last_position; ++position) {
            const std::size_t weight = residue + position * step;
            const auto previous = dp[weight];
            const auto offset = static_cast<__int128>(position) * value;
            if (!candidates.empty() && position - candidates.front().first > count) {
                candidates.pop_front();
            }
            if (previous != unreachable) {
                if constexpr (Minimize) {
                    const std::size_t needed = last_position - position + 1;
                    if (needed <= count) {
                        const auto cost = static_cast<__int128>(needed) * value + previous;
                        const auto next_target = static_cast<knapsack_value>(
                            std::min<__int128>({reached_target, cost, overflow}));
                        if constexpr (Record) {
                            if (next_target < reached_target) {
                                stage->counts.back() = needed;
                                stage->before_target = weight;
                            }
                        }
                        reached_target = next_target;
                    }
                }
                const auto adjusted = static_cast<__int128>(previous) - offset;
                while (!candidates.empty() && (Minimize ? adjusted <= candidates.back().second
                                                       : adjusted >= candidates.back().second)) {
                    candidates.pop_back();
                }
                candidates.emplace_back(position, adjusted);
            }
            if (candidates.empty()) {
                dp[weight] = unreachable;
                continue;
            }
            const auto result = candidates.front().second + offset;
            if constexpr (Record) stage->counts[weight] = position - candidates.front().first;
            if constexpr (Minimize) {
                dp[weight] = static_cast<knapsack_value>(std::min<__int128>(result, overflow));
            } else {
                if (result >= overflow) {
                    throw std::overflow_error("bounded knapsack value exceeds long long");
                }
                dp[weight] = static_cast<knapsack_value>(result);
            }
        }
    }
    if constexpr (Minimize) dp.back() = reached_target;
}

template <bool Minimize, class Weight, class Value, class Count>
std::vector<knapsack_value> initial_bounded_values(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long maximum_weight) {
    static_assert(std::is_integral_v<Weight> && std::is_integral_v<Value> && std::is_integral_v<Count>);
    assert(maximum_weight >= 0);
    long long table_weight = 0;
    for (const auto& [weight, value, count] : items) {
        const auto item_weight = static_cast<long long>(weight);
        assert(item_weight > 0 && static_cast<long long>(value) >= 0);
        auto copies = static_cast<long long>(count);
        assert(copies >= 0);
        if constexpr (!Minimize) copies = std::min(copies, maximum_weight / item_weight);
        const auto available = static_cast<__int128>(item_weight) * copies + table_weight;
        table_weight = static_cast<long long>(std::min<__int128>(maximum_weight, available));
    }

    std::vector<knapsack_value> dp;
    if (static_cast<unsigned long long>(table_weight) >= dp.max_size()) {
        throw std::length_error("bounded knapsack table is too large");
    }
    dp.assign(static_cast<std::size_t>(table_weight) + 1, unreachable);
    dp[0] = 0;
    return dp;
}

template <bool Minimize, class Weight, class Value, class Count>
std::vector<knapsack_value> build_bounded_deque(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long maximum_weight,
    bounded_trace_history& history) {
    auto dp = initial_bounded_values<Minimize>(items, maximum_weight);
    const auto table_weight = static_cast<long long>(dp.size() - 1);
    if (table_weight == 0) return dp;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const auto& [weight, value, count] = items[index];
        if (count == 0) continue;
        auto item_weight = static_cast<long long>(weight);
        if constexpr (Minimize) {
            item_weight = std::min(item_weight, table_weight);
        } else {
            if (item_weight > table_weight) continue;
        }
        const auto step = static_cast<std::size_t>(item_weight);
        if (history.enabled) {
            history.stages.push_back({index, step, std::vector<std::size_t>(dp.size()), dp.size() - 1});
            update_bounded_deque<Minimize, true>(dp, step, static_cast<long long>(value),
                                                 static_cast<unsigned long long>(count),
                                                 &history.stages.back());
        } else {
            update_bounded_deque<Minimize, false>(dp, step, static_cast<long long>(value),
                                                  static_cast<unsigned long long>(count), nullptr);
        }
    }
    return dp;
}

template <class PreferRight>
void bounded_smawk(std::size_t first_row, std::size_t row_stride, std::size_t row_count,
                   const std::size_t* columns, std::size_t column_count, std::size_t* reduced,
                   std::size_t* winners, const PreferRight& prefer_right) {
    std::size_t reduced_count = 0;
    for (std::size_t i = 0; i < column_count; ++i) {
        const auto column = columns[i];
        while (reduced_count > 0 && prefer_right(first_row + (reduced_count - 1) * row_stride,
                                                 reduced[reduced_count - 1], column)) {
            --reduced_count;
        }
        if (reduced_count < row_count) reduced[reduced_count++] = column;
    }

    if (row_count > 1) {
        bounded_smawk(first_row + row_stride, 2 * row_stride, row_count / 2,
                      reduced, reduced_count, reduced + reduced_count, winners, prefer_right);
    }

    std::size_t column_index = 0;
    for (std::size_t i = 0; i < row_count; i += 2) {
        const auto row = first_row + i * row_stride;
        const auto end = i + 1 < row_count ? winners[row + row_stride] : reduced[reduced_count - 1];
        auto best = reduced[column_index];
        while (reduced[column_index] != end) {
            const auto column = reduced[++column_index];
            if (prefer_right(row, best, column)) best = column;
        }
        winners[row] = best;
    }
}

inline constexpr __int128 proximity_unreachable = std::numeric_limits<__int128>::lowest();

template <bool Minimize, bool Record, class Value>
void update_axiotis_tzamos_group(std::vector<Value>& dp, std::size_t step,
                                const std::vector<__int128>& prefix, std::size_t* choices,
                                std::size_t* before_target) {
    constexpr bool wide = std::is_same_v<Value, __int128>;
    constexpr bool saturate_target = Minimize && !wide;
    constexpr Value missing = wide ? static_cast<Value>(proximity_unreachable)
                                   : static_cast<Value>(unreachable);
    const auto last_exact_weight = dp.size() - (saturate_target ? 2 : 1);
    const auto maximum_rows = last_exact_weight / step + 1;
    auto reached_target = dp.back();
    std::vector<Value> previous(maximum_rows);
    std::vector<std::size_t> columns;
    columns.reserve(maximum_rows);
    std::vector<std::size_t> reduced(2 * maximum_rows);
    std::vector<std::size_t> winners(maximum_rows);

    const auto score = [&](std::size_t row, std::size_t column) {
        return previous[column] + prefix[row - column];
    };
    const auto prefer_right = [&](std::size_t row, std::size_t left, std::size_t right) {
        if (row < right) return false;
        if (row - left >= prefix.size()) return true;
        if constexpr (Minimize) return score(row, right) <= score(row, left);
        return score(row, right) >= score(row, left);
    };

    for (std::size_t residue = 0; residue < step; ++residue) {
        const auto rows = (last_exact_weight - residue) / step + 1;
        columns.clear();
        for (std::size_t position = 0; position < rows; ++position) {
            const auto weight = residue + position * step;
            const auto value = dp[weight];
            if (value == missing) continue;
            previous[position] = value;
            columns.push_back(position);
            if constexpr (saturate_target) {
                const auto needed = rows - position;
                if (needed < prefix.size()) {
                    const auto cost = static_cast<knapsack_value>(
                        std::min<__int128>(value + prefix[needed], overflow));
                    if (cost < reached_target) {
                        reached_target = cost;
                        if constexpr (Record) {
                            choices[dp.size() - 1] = needed;
                            *before_target = weight;
                        }
                    }
                }
            }
        }
        if (columns.empty()) continue;

        bounded_smawk(0, 1, rows, columns.data(), columns.size(), reduced.data(), winners.data(),
                      prefer_right);
        for (std::size_t position = 0; position < rows; ++position) {
            const auto weight = residue + position * step;
            const auto column = winners[position];
            if (column > position || position - column >= prefix.size()) {
                dp[weight] = missing;
                continue;
            }
            const auto value = score(position, column);
            if constexpr (wide) {
                dp[weight] = value;
            } else if constexpr (Minimize) {
                dp[weight] = static_cast<knapsack_value>(std::min<__int128>(value, overflow));
            } else {
                if (value >= overflow) {
                    throw std::overflow_error("bounded knapsack value exceeds long long");
                }
                dp[weight] = static_cast<knapsack_value>(value);
            }
            if constexpr (Record) choices[weight] = position - column;
        }
    }
    if constexpr (saturate_target) dp.back() = reached_target;
}

template <bool Minimize, bool Record>
void update_bounded_axiotis_tzamos(std::vector<knapsack_value>& dp, std::size_t step,
                                  const std::vector<__int128>& prefix, std::size_t* choices,
                                  std::size_t& before_target) {
    update_axiotis_tzamos_group<Minimize, Record>(dp, step, prefix, choices, &before_target);
}

template <bool Minimize, bool Record>
void update_proximity_group(std::vector<__int128>& dp, std::size_t step,
                            const std::vector<__int128>& prefix, std::size_t* choices) {
    update_axiotis_tzamos_group<Minimize, Record>(dp, step, prefix, choices, nullptr);
}

inline unsigned long long bounded_query_weight(long long weight, long long maximum_weight) {
    assert(weight >= 0);
    if (weight > maximum_weight) throw std::out_of_range("bounded knapsack weight exceeds table limit");
    return static_cast<unsigned long long>(weight);
}

inline std::optional<long long> bounded_value_at(const std::vector<knapsack_value>& dp,
                                                  unsigned long long weight) {
    if (weight >= dp.size() || dp[weight] == unreachable) return std::nullopt;
    if (dp[weight] >= overflow) throw std::overflow_error("bounded knapsack value exceeds long long");
    return static_cast<long long>(dp[weight]);
}

struct bounded_weight_group_item {
    std::size_t step;
    long long value;
    unsigned long long count;
    std::size_t original_index;
};

struct bounded_weight_group_history : bounded_trace_history {
    std::vector<bounded_weight_group_item> items;

    bounded_weight_group_history(bool enable_reconstruction, std::size_t item_count)
        : bounded_trace_history{enable_reconstruction, item_count, {}, {}} {}

    template <bool Saturated, class Assign>
    void distribute(std::size_t weight, const Assign& assign) const {
        std::size_t end = items.size();
        backtrack<Saturated>(weight, [&](std::size_t first, std::size_t copies) {
            for (std::size_t i = first; i < end && copies > 0; ++i) {
                const auto taken = std::min<unsigned long long>(copies, items[i].count);
                assign(items[i].original_index, static_cast<long long>(taken));
                copies -= static_cast<std::size_t>(taken);
            }
            end = first;
        });
    }

    template <bool Minimize>
    knapsack_result restore(long long value, std::size_t weight) const {
        knapsack_result result{value, std::vector<long long>(item_count)};
        distribute<Minimize>(weight, [&](std::size_t index, long long copies) {
            result.counts[index] = copies;
        });
        return result;
    }
};

template <bool Minimize, bool Record, class Score>
void update_bounded_weight_group(std::vector<Score>& dp, std::size_t step,
                                 const std::vector<__int128>& prefix, bool uniform,
                                 long long first_value, std::size_t copies, bounded_trace_stage* stage) {
    if constexpr (std::is_same_v<Score, __int128>) {
        update_proximity_group<Minimize, Record>(dp, step, prefix, Record ? stage->counts.data() : nullptr);
    } else if (uniform) {
        update_bounded_deque<Minimize, Record>(dp, step, first_value, copies, stage);
    } else {
        std::size_t unused = 0;
        update_bounded_axiotis_tzamos<Minimize, Record>(dp, step, prefix,
            Record ? stage->counts.data() : nullptr, Record ? stage->before_target : unused);
    }
}

template <bool Minimize, class Score>
void apply_bounded_weight_groups(std::vector<Score>& dp, std::vector<bounded_weight_group_item> ordered,
                                 bounded_weight_group_history& history) {
    constexpr bool wide = std::is_same_v<Score, __int128>;
    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        if (left.step != right.step) return left.step < right.step;
        return Minimize ? left.value < right.value : left.value > right.value;
    });

    std::vector<__int128> prefix;
    for (std::size_t first = 0; first < ordered.size();) {
        const auto step = ordered[first].step;
        std::size_t end = first + 1;
        while (end < ordered.size() && ordered[end].step == step) ++end;
        const auto cap = dp.size() - 1;
        const auto max_copies = cap / step + (!wide && Minimize && cap % step != 0);
        const auto first_value = ordered[first].value;
        const bool uniform = !wide && first_value == ordered[end - 1].value;
        std::size_t copies = 0;
        if (!uniform) prefix.resize(1, 0);
        for (std::size_t i = first; i < end && copies < max_copies; ++i) {
            const auto usable = static_cast<std::size_t>(
                std::min<unsigned long long>(max_copies - copies, ordered[i].count));
            copies += usable;
            if (!uniform) {
                for (std::size_t k = 0; k < usable; ++k) {
                    prefix.push_back(prefix.back() + ordered[i].value);
                }
            }
        }

        if (history.enabled) {
            history.stages.push_back({first, step, std::vector<std::size_t>(dp.size()), cap});
            update_bounded_weight_group<Minimize, true>(dp, step, prefix, uniform, first_value, copies,
                                                         &history.stages.back());
        } else {
            update_bounded_weight_group<Minimize, false>(dp, step, prefix, uniform, first_value, copies,
                                                          nullptr);
        }
        first = end;
    }
    if (history.enabled) history.items = std::move(ordered);
}

template <bool Minimize, class Weight, class Value, class Count>
std::vector<knapsack_value> build_bounded_axiotis_tzamos(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long maximum_weight,
    bounded_weight_group_history& history) {
    auto dp = initial_bounded_values<Minimize>(items, maximum_weight);
    const auto table_weight = static_cast<long long>(dp.size() - 1);
    if (table_weight == 0) return dp;

    std::vector<bounded_weight_group_item> ordered;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& [weight, value, count] = items[i];
        if (count == 0) continue;
        auto step = static_cast<long long>(weight);
        if constexpr (Minimize) step = std::min(step, table_weight);
        if (step > table_weight) continue;
        ordered.push_back({static_cast<std::size_t>(step), static_cast<long long>(value),
                           static_cast<unsigned long long>(count), i});
    }
    apply_bounded_weight_groups<Minimize>(dp, std::move(ordered), history);
    return dp;
}

inline __int128 add_bounded_stock_total(__int128 total, __int128 increment) {
    if (increment > std::numeric_limits<__int128>::max() - total) {
        throw std::overflow_error("bounded knapsack stock total exceeds __int128");
    }
    return total + increment;
}

inline long long checked_bounded_value(__int128 value) {
    if (value > std::numeric_limits<long long>::max()) {
        throw std::overflow_error("bounded knapsack value exceeds long long");
    }
    return static_cast<long long>(value);
}

enum class proximity_query { exact, at_most, at_least };

struct proximity_item {
    long long weight;
    long long value;
    long long count;
    std::size_t index;
};

struct proximity_choice {
    __int128 value = proximity_unreachable;
    std::size_t removed = 0;
    std::size_t added = 0;
};

inline std::vector<__int128> initial_proximity_values(__int128 limit) {
    std::vector<__int128> values;
    if (limit >= values.max_size()) throw std::length_error("bounded knapsack proximity table is too large");
    values.assign(static_cast<std::size_t>(limit) + 1, proximity_unreachable);
    values[0] = 0;
    return values;
}

template <bool Exact>
proximity_choice match_proximity_sides(const std::vector<__int128>& removal,
                                      const std::vector<__int128>& addition,
                                      __int128 greedy_value, __int128 spare) {
    proximity_choice best;
    std::size_t next_added = 0, best_added = 0;
    for (std::size_t removed = 0; removed < removal.size(); ++removed) {
        if (removal[removed] == proximity_unreachable) continue;
        const auto allowance = spare + removed;
        if constexpr (Exact) {
            if (allowance >= addition.size()) break;
            best_added = static_cast<std::size_t>(allowance);
        } else {
            while (next_added < addition.size() && next_added <= allowance) {
                if (addition[next_added] > addition[best_added]) best_added = next_added;
                ++next_added;
            }
        }
        if (addition[best_added] == proximity_unreachable) continue;
        const auto value = greedy_value - removal[removed] + addition[best_added];
        if (value > best.value) best = {value, removed, best_added};
    }
    return best;
}

template <proximity_query Query, bool Reconstruct, class Weight, class Value, class Count>
std::optional<std::conditional_t<Reconstruct, knapsack_result, long long>> solve_bounded_proximity(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long query) {
    static_assert(std::is_integral_v<Weight> && std::is_integral_v<Value> && std::is_integral_v<Count>);
    assert(query >= 0);
    constexpr bool complement = Query == proximity_query::at_least;
    constexpr bool exact = Query == proximity_query::exact;
    using result_type = std::conditional_t<Reconstruct, knapsack_result, long long>;

    __int128 total_weight = 0, total_value = 0;
    std::vector<proximity_item> ordered;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& [weight, value, count] = items[i];
        const proximity_item item{static_cast<long long>(weight), static_cast<long long>(value),
                                  static_cast<long long>(count), i};
        assert(item.weight > 0 && item.value >= 0 && item.count >= 0);
        if (item.count == 0) continue;
        total_weight = add_bounded_stock_total(total_weight, static_cast<__int128>(item.weight) * item.count);
        total_value = add_bounded_stock_total(total_value, static_cast<__int128>(item.value) * item.count);
        ordered.push_back(item);
    }
    if constexpr (complement || exact) {
        if (query > total_weight) return std::nullopt;
    }
    const __int128 capacity = complement ? total_weight - query : query;
    std::vector<long long> counts;
    if constexpr (Reconstruct) counts.resize(items.size());
    const auto finish = [&](const __int128 value) -> result_type {
        const auto answer = checked_bounded_value(complement ? total_value - value : value);
        if constexpr (Reconstruct) return {answer, std::move(counts)};
        else return answer;
    };
    if (capacity >= total_weight) {
        if constexpr (Reconstruct && !complement) {
            for (const auto& item : ordered) counts[item.index] = item.count;
        }
        return finish(total_value);
    }
    if constexpr (Reconstruct && complement) {
        for (const auto& item : ordered) counts[item.index] = item.count;
    }

    ordered.erase(std::remove_if(ordered.begin(), ordered.end(), [&](const proximity_item& item) {
        return item.weight > capacity;
    }), ordered.end());
    std::sort(ordered.begin(), ordered.end(), [](const proximity_item& left, const proximity_item& right) {
        return static_cast<__int128>(left.value) * right.weight >
               static_cast<__int128>(right.value) * left.weight;
    });

    std::size_t split = 0;
    long long split_count = 0;
    __int128 spare = capacity, greedy_value = 0;
    for (; split < ordered.size(); ++split) {
        const auto& item = ordered[split];
        split_count = static_cast<long long>(std::min<__int128>(item.count, spare / item.weight));
        spare -= static_cast<__int128>(item.weight) * split_count;
        greedy_value += static_cast<__int128>(item.value) * split_count;
        if constexpr (Reconstruct) {
            counts[item.index] = complement ? item.count - split_count : split_count;
        }
        if (split_count != item.count) break;
    }
    if (spare == 0) return finish(greedy_value);
    if (split == ordered.size()) {
        if constexpr (exact) return std::nullopt;
        else return finish(greedy_value);
    }

    long long max_weight = 0;
    __int128 remaining_stock_weight = 0;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const auto& item = ordered[i];
        max_weight = std::max(max_weight, item.weight);
        const auto remaining = i == split ? item.count - split_count : item.count;
        if (i >= split) remaining_stock_weight += static_cast<__int128>(item.weight) * remaining;
    }
    const auto radius = static_cast<__int128>(2) * max_weight * max_weight;
    auto removal = initial_proximity_values(std::min(radius, capacity - spare));
    auto addition = initial_proximity_values(std::min({radius, capacity, remaining_stock_weight}));
    std::vector<bounded_weight_group_item> removed_items, added_items;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const auto& item = ordered[i];
        const auto taken = i < split ? item.count : (i == split ? split_count : 0);
        const auto step = static_cast<unsigned long long>(item.weight);
        if (taken > 0 && step < removal.size()) {
            removed_items.push_back({static_cast<std::size_t>(step), item.value,
                                     static_cast<unsigned long long>(taken), item.index});
        }
        if (taken < item.count && step < addition.size()) {
            added_items.push_back({static_cast<std::size_t>(step), item.value,
                                   static_cast<unsigned long long>(item.count - taken), item.index});
        }
    }
    bounded_weight_group_history remove_history(Reconstruct, items.size()), add_history(Reconstruct, items.size());
    apply_bounded_weight_groups<true>(removal, std::move(removed_items), remove_history);
    apply_bounded_weight_groups<false>(addition, std::move(added_items), add_history);

    const auto best = match_proximity_sides<exact>(removal, addition, greedy_value, spare);
    if (best.value == proximity_unreachable) return std::nullopt;
    if constexpr (Reconstruct) {
        constexpr long long sign = complement ? -1 : 1;
        remove_history.distribute<false>(best.removed, [&](std::size_t index, long long copies) {
            counts[index] -= sign * copies;
        });
        add_history.distribute<false>(best.added, [&](std::size_t index, long long copies) {
            counts[index] += sign * copies;
        });
    }
    return finish(best.value);
}

}

template <class Weight, class Value, class Count>
std::optional<long long> bounded_knapsack_proximity_exact(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long weight) {
    return knapsack_detail::solve_bounded_proximity<knapsack_detail::proximity_query::exact, false>(items, weight);
}

template <class Weight, class Value, class Count>
long long bounded_knapsack_proximity_at_most(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long limit_weight) {
    return *knapsack_detail::solve_bounded_proximity<knapsack_detail::proximity_query::at_most, false>(
        items, limit_weight);
}

template <class Weight, class Value, class Count>
std::optional<long long> bounded_knapsack_proximity_at_least(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long target_weight) {
    return knapsack_detail::solve_bounded_proximity<knapsack_detail::proximity_query::at_least, false>(
        items, target_weight);
}

template <class Weight, class Value, class Count>
std::optional<knapsack_result> bounded_knapsack_proximity_exact_with_counts(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long weight) {
    return knapsack_detail::solve_bounded_proximity<knapsack_detail::proximity_query::exact, true>(items, weight);
}

template <class Weight, class Value, class Count>
knapsack_result bounded_knapsack_proximity_at_most_with_counts(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long limit_weight) {
    return *knapsack_detail::solve_bounded_proximity<knapsack_detail::proximity_query::at_most, true>(
        items, limit_weight);
}

template <class Weight, class Value, class Count>
std::optional<knapsack_result> bounded_knapsack_proximity_at_least_with_counts(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long target_weight) {
    return knapsack_detail::solve_bounded_proximity<knapsack_detail::proximity_query::at_least, true>(
        items, target_weight);
}

class unbounded_knapsack_chan_he {
    using table_type = knapsack_detail::unbounded_chan_he_table<false>;
    table_type exact_;
    std::vector<table_type::value_type> at_most_values_;

public:
    template <class Weight, class Value>
    explicit unbounded_knapsack_chan_he(const std::vector<std::pair<Weight, Value>>& items,
                                        bool enable_reconstruction = false)
        : exact_(items, enable_reconstruction),
          at_most_values_(knapsack_detail::prefix_max_values(exact_.values)) {}

    std::optional<long long> exact_value(long long weight) const {
        return exact_.query(exact_.values, weight);
    }

    long long max_value_at_most(long long limit_weight) const {
        return exact_.query(at_most_values_, limit_weight).value_or(0);
    }

    std::optional<knapsack_result> exact_value_with_counts(long long weight) const {
        exact_.require_reconstruction();
        const auto value = exact_value(weight);
        if (!value) return std::nullopt;
        const auto [index, copies] = exact_.reduce(weight);
        return exact_.restore(*value, index, copies);
    }

    knapsack_result max_value_at_most_with_counts(long long limit_weight) const {
        exact_.require_reconstruction();
        const auto value = max_value_at_most(limit_weight);
        if (at_most_values_.empty()) return exact_.restore(value, 0, 0);
        const auto [index, copies] = exact_.reduce(limit_weight);
        const auto first = at_most_values_.begin();
        const auto best = std::lower_bound(first, first + index + 1, at_most_values_[index]);
        return exact_.restore(value, static_cast<std::size_t>(best - first), copies);
    }
};

class unbounded_knapsack_min_chan_he {
    using table_type = knapsack_detail::unbounded_chan_he_table<true>;
    table_type at_least_;
    std::vector<std::size_t> best_weights_;

public:
    template <class Weight, class Value>
    explicit unbounded_knapsack_min_chan_he(const std::vector<std::pair<Weight, Value>>& items,
                                            bool enable_reconstruction = false)
        : at_least_(items, enable_reconstruction) {
        auto& values = at_least_.values;
        if (values.empty()) return;
        const auto last_period = values.end() - at_least_.period_weight;
        const auto period_best = std::min_element(last_period, values.end());
        const auto period_min = std::min(knapsack_detail::overflow, *period_best);
        auto best = std::min(knapsack_detail::overflow,
                             period_min + static_cast<table_type::value_type>(at_least_.period_value));
        auto best_weight = static_cast<std::size_t>(period_best - values.begin()) + at_least_.period_weight;
        if (enable_reconstruction) best_weights_.resize(values.size());
        for (std::size_t weight = values.size(); weight-- > 0;) {
            if (values[weight] <= best) {
                best = values[weight];
                best_weight = weight;
            }
            values[weight] = best;
            if (enable_reconstruction) best_weights_[weight] = best_weight;
        }
    }

    std::optional<long long> min_value_at_least(long long target_weight) const {
        return at_least_.query(at_least_.values, target_weight);
    }

    std::optional<knapsack_result> min_value_at_least_with_counts(long long target_weight) const {
        at_least_.require_reconstruction();
        const auto value = min_value_at_least(target_weight);
        if (!value) return std::nullopt;
        if (target_weight == 0) return at_least_.restore(*value, 0, 0);
        const auto [index, copies] = at_least_.reduce(target_weight);
        const auto best_weight = best_weights_[index];
        if (best_weight >= at_least_.values.size()) {
            return at_least_.restore(*value, best_weight - at_least_.period_weight, copies + 1);
        }
        return at_least_.restore(*value, best_weight, copies);
    }
};

class bounded_knapsack_deque {
    long long max_capacity_;
    knapsack_detail::bounded_trace_history history_;
    std::vector<knapsack_detail::knapsack_value> exact_values_;
    std::vector<knapsack_detail::knapsack_value> at_most_values_;

public:
    template <class Weight, class Value, class Count>
    explicit bounded_knapsack_deque(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                    long long max_capacity, bool enable_reconstruction = false)
        : max_capacity_(max_capacity),
          history_{enable_reconstruction, items.size(), {}, {}},
          exact_values_(knapsack_detail::build_bounded_deque<false>(items, max_capacity, history_)),
          at_most_values_(knapsack_detail::prefix_max_values(exact_values_)) {
        history_.record_at_most_weights(at_most_values_);
    }

    std::optional<long long> exact_value(long long weight) const {
        const auto index = knapsack_detail::bounded_query_weight(weight, max_capacity_);
        return knapsack_detail::bounded_value_at(exact_values_, index);
    }

    long long max_value_at_most(long long limit_weight) const {
        const auto weight = knapsack_detail::bounded_query_weight(limit_weight, max_capacity_);
        if (at_most_values_.empty()) return 0;
        const auto index = std::min<unsigned long long>(weight, at_most_values_.size() - 1);
        return static_cast<long long>(at_most_values_[index]);
    }

    std::optional<knapsack_result> exact_value_with_counts(long long weight) const {
        history_.require_enabled();
        const auto value = exact_value(weight);
        if (!value) return std::nullopt;
        return history_.restore<false>(*value, static_cast<std::size_t>(weight));
    }

    knapsack_result max_value_at_most_with_counts(long long limit_weight) const {
        history_.require_enabled();
        const auto weight = knapsack_detail::bounded_query_weight(limit_weight, max_capacity_);
        if (at_most_values_.empty()) return {0, std::vector<long long>(history_.item_count)};
        const auto index = std::min<unsigned long long>(weight, at_most_values_.size() - 1);
        return history_.restore<false>(static_cast<long long>(at_most_values_[index]),
                                       history_.best_weights[index]);
    }
};

class bounded_knapsack_min_deque {
    long long max_target_weight_;
    knapsack_detail::bounded_trace_history history_;
    std::vector<knapsack_detail::knapsack_value> at_least_values_;

public:
    template <class Weight, class Value, class Count>
    explicit bounded_knapsack_min_deque(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                        long long max_target_weight, bool enable_reconstruction = false)
        : max_target_weight_(max_target_weight),
          history_{enable_reconstruction, items.size(), {}, {}},
          at_least_values_(knapsack_detail::build_bounded_deque<true>(items, max_target_weight, history_)) {
        history_.aggregate_at_least_values(at_least_values_);
    }

    std::optional<long long> min_value_at_least(long long target_weight) const {
        const auto index = knapsack_detail::bounded_query_weight(target_weight, max_target_weight_);
        return knapsack_detail::bounded_value_at(at_least_values_, index);
    }

    std::optional<knapsack_result> min_value_at_least_with_counts(long long target_weight) const {
        history_.require_enabled();
        const auto value = min_value_at_least(target_weight);
        if (!value) return std::nullopt;
        const auto index = static_cast<std::size_t>(target_weight);
        return history_.restore<true>(*value, history_.best_weights[index]);
    }
};

class bounded_knapsack_axiotis_tzamos {
    long long max_capacity_;
    knapsack_detail::bounded_weight_group_history history_;
    std::vector<knapsack_detail::knapsack_value> exact_values_;
    std::vector<knapsack_detail::knapsack_value> at_most_values_;

public:
    template <class Weight, class Value, class Count>
    explicit bounded_knapsack_axiotis_tzamos(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                             long long max_capacity, bool enable_reconstruction = false)
        : max_capacity_(max_capacity),
          history_(enable_reconstruction, items.size()),
          exact_values_(knapsack_detail::build_bounded_axiotis_tzamos<false>(items, max_capacity, history_)),
          at_most_values_(knapsack_detail::prefix_max_values(exact_values_)) {
        history_.record_at_most_weights(at_most_values_);
    }

    std::optional<long long> exact_value(long long weight) const {
        const auto index = knapsack_detail::bounded_query_weight(weight, max_capacity_);
        return knapsack_detail::bounded_value_at(exact_values_, index);
    }

    long long max_value_at_most(long long limit_weight) const {
        const auto weight = knapsack_detail::bounded_query_weight(limit_weight, max_capacity_);
        if (at_most_values_.empty()) return 0;
        const auto index = std::min<unsigned long long>(weight, at_most_values_.size() - 1);
        return static_cast<long long>(at_most_values_[index]);
    }

    std::optional<knapsack_result> exact_value_with_counts(long long weight) const {
        history_.require_enabled();
        const auto value = exact_value(weight);
        if (!value) return std::nullopt;
        return history_.restore<false>(*value, static_cast<std::size_t>(weight));
    }

    knapsack_result max_value_at_most_with_counts(long long limit_weight) const {
        history_.require_enabled();
        const auto weight = knapsack_detail::bounded_query_weight(limit_weight, max_capacity_);
        if (at_most_values_.empty()) return {0, std::vector<long long>(history_.item_count)};
        const auto index = std::min<unsigned long long>(weight, at_most_values_.size() - 1);
        return history_.restore<false>(static_cast<long long>(at_most_values_[index]),
                                       history_.best_weights[index]);
    }
};

class bounded_knapsack_min_axiotis_tzamos {
    long long max_target_weight_;
    knapsack_detail::bounded_weight_group_history history_;
    std::vector<knapsack_detail::knapsack_value> at_least_values_;

public:
    template <class Weight, class Value, class Count>
    explicit bounded_knapsack_min_axiotis_tzamos(
        const std::vector<std::tuple<Weight, Value, Count>>& items, long long max_target_weight,
        bool enable_reconstruction = false)
        : max_target_weight_(max_target_weight),
          history_(enable_reconstruction, items.size()),
          at_least_values_(knapsack_detail::build_bounded_axiotis_tzamos<true>(items, max_target_weight, history_)) {
        history_.aggregate_at_least_values(at_least_values_);
    }

    std::optional<long long> min_value_at_least(long long target_weight) const {
        const auto index = knapsack_detail::bounded_query_weight(target_weight, max_target_weight_);
        return knapsack_detail::bounded_value_at(at_least_values_, index);
    }

    std::optional<knapsack_result> min_value_at_least_with_counts(long long target_weight) const {
        history_.require_enabled();
        const auto value = min_value_at_least(target_weight);
        if (!value) return std::nullopt;
        const auto index = static_cast<std::size_t>(target_weight);
        return history_.restore<true>(*value, history_.best_weights[index]);
    }
};

class bounded_knapsack_compressed {
    using wide = __int128;
    static constexpr wide unreachable = std::numeric_limits<wide>::lowest();

    struct item {
        long long weight;
        long long value;
        long long count;
    };

    struct stock_total {
        wide weight;
        wide value;
    };

    struct answer_pair {
        wide exact;
        wide at_most;
    };

    struct trace_entry {
        std::size_t removed_weight;
        std::size_t at_most_index;
    };

    struct query_choice {
        std::size_t pivot = 0;
        std::optional<std::size_t> index;
        long long periods = 0;
    };

    struct deque_entry {
        wide value;
        std::size_t position;
    };

    struct movement_deque_entry {
        wide value;
        std::size_t position;
        std::size_t movement = 0;
    };

    std::vector<item> items_;
    std::vector<stock_total> prefixes_{{0, 0}};
    std::vector<answer_pair> table_;
    std::vector<std::size_t> original_indices_;
    std::vector<trace_entry> trace_;
    std::vector<std::size_t> removal_choices_;
    std::vector<std::size_t> addition_choices_;
    std::size_t radius_ = 0;
    long long margin_ = 0;
    std::size_t input_size_;
    bool reconstruction_enabled_;

    void require_reconstruction() const {
        if (!reconstruction_enabled_) {
            throw std::logic_error("bounded knapsack reconstruction is disabled");
        }
    }

    template <bool TrackMovement, bool TrackCopies = false>
    void apply_item(std::vector<wide>& dp, const item& type, bool reverse, long long value,
                    std::size_t* movement = nullptr, std::size_t* choices = nullptr) const {
        const auto step = static_cast<std::size_t>(type.weight);
        const auto count = static_cast<std::size_t>(std::min(type.count, 2 * margin_));
        const std::size_t last = dp.size() - 1;
        using entry = std::conditional_t<TrackMovement, movement_deque_entry, deque_entry>;
        std::deque<entry> candidates;
        for (std::size_t residue = 0; residue < step; ++residue) {
            candidates.clear();
            const std::size_t last_position = (last - residue) / step;
            for (std::size_t position = 0; position <= last_position; ++position) {
                const std::size_t offset_weight = residue + position * step;
                const std::size_t index = reverse ? last - offset_weight : offset_weight;
                const auto previous = dp[index];
                const auto offset_value = static_cast<wide>(position) * value;
                if (!candidates.empty() && position - candidates.front().position > count) {
                    candidates.pop_front();
                }
                if (previous != unreachable) {
                    entry candidate{previous - offset_value, position};
                    if constexpr (TrackMovement) candidate.movement = movement[index] + last - offset_weight;
                    while (!candidates.empty()) {
                        const auto& back = candidates.back();
                        if (candidate.value < back.value) break;
                        if constexpr (TrackMovement) {
                            if (candidate.value == back.value && candidate.movement > back.movement) break;
                        }
                        candidates.pop_back();
                    }
                    candidates.push_back(candidate);
                }
                if (candidates.empty()) {
                    dp[index] = unreachable;
                    continue;
                }
                const auto& best = candidates.front();
                dp[index] = best.value + offset_value;
                if constexpr (TrackMovement) {
                    movement[index] = best.movement - (last - offset_weight);
                }
                if constexpr (TrackCopies) choices[index] = position - best.position;
            }
        }
    }

    template <bool TrackMovement>
    void build_rows(std::size_t left, std::size_t right, std::vector<wide> dp,
                    std::vector<std::size_t> movement = {}) {
        if (left == right) {
            const std::size_t row = left * dp.size();
            wide best = unreachable;
            std::size_t best_index = 0;
            for (std::size_t index = 0; index < dp.size(); ++index) {
                const auto value = dp[index];
                if constexpr (TrackMovement) {
                    if (value > best || (value != unreachable && value == best
                                         && movement[index] < movement[best_index])) {
                        best = value;
                        best_index = index;
                    }
                    auto& trace = trace_[row + index];
                    if (value != unreachable) trace.removed_weight = (movement[index] + radius_ - index) / 2;
                    trace.at_most_index = best_index;
                } else {
                    best = std::max(best, value);
                }
                table_[row + index] = {value, best};
            }
            return;
        }

        const std::size_t middle = left + (right - left) / 2;
        auto left_dp = dp;
        auto left_movement = movement;
        for (std::size_t i = middle; i < right; ++i) {
            apply_item<TrackMovement>(left_dp, items_[i], false, items_[i].value, left_movement.data());
        }
        build_rows<TrackMovement>(left, middle, std::move(left_dp), std::move(left_movement));
        for (std::size_t i = left; i <= middle; ++i) {
            apply_item<TrackMovement>(dp, items_[i], true, -items_[i].value, movement.data());
        }
        build_rows<TrackMovement>(middle + 1, right, std::move(dp), std::move(movement));
    }

    void build_choices() {
        const std::size_t row_size = radius_ + 1;
        removal_choices_.resize(items_.size() * row_size);
        addition_choices_.resize(items_.size() * row_size);
        std::vector<wide> dp(row_size, unreachable);
        dp[0] = 0;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            apply_item<false, true>(dp, items_[i], false, -items_[i].value, nullptr,
                                    removal_choices_.data() + i * row_size);
        }
        std::fill(dp.begin(), dp.end(), unreachable);
        dp[0] = 0;
        for (std::size_t i = items_.size(); i-- > 0;) {
            apply_item<false, true>(dp, items_[i], false, items_[i].value, nullptr,
                                    addition_choices_.data() + i * row_size);
        }
    }

    wide solve(wide weight, bool exact, query_choice* choice = nullptr) const {
        if (items_.empty()) return exact && weight != 0 ? unreachable : 0;
        const auto& total = prefixes_.back();
        if (weight >= total.weight) {
            if (choice) choice->pivot = items_.size();
            return exact && weight != total.weight ? unreachable : total.value;
        }

        const auto next = std::upper_bound(prefixes_.begin(), prefixes_.end(), weight,
            [](wide query, const stock_total& prefix) { return query < prefix.weight; });
        std::size_t pivot = static_cast<std::size_t>(next - prefixes_.begin() - 1);
        const auto& type = items_[pivot];
        const auto copies = static_cast<long long>((weight - prefixes_[pivot].weight) / type.weight);
        wide period_value = 0;
        long long periods = 0;
        if (copies > margin_) {
            if (copies <= type.count - margin_) {
                periods = copies - margin_;
                weight -= static_cast<wide>(periods) * type.weight;
                period_value = static_cast<wide>(periods) * type.value;
            } else {
                ++pivot;
            }
        }

        const auto& prefix = prefixes_[pivot];
        const auto index = static_cast<std::size_t>((weight - prefix.weight) + radius_);
        const auto row = pivot * (2 * radius_ + 1);
        const auto& answer = table_[row + index];
        const auto value = exact ? answer.exact : answer.at_most;
        if (value == unreachable) return unreachable;
        if (choice) *choice = {pivot, exact ? index : trace_[row + index].at_most_index, periods};
        return prefix.value + value + period_value;
    }

    std::vector<long long> restore_counts(const query_choice& choice, bool complement = false) const {
        std::vector<long long> counts(input_size_, 0);
        if (!choice.index) {
            if (!complement) {
                for (std::size_t i = 0; i < items_.size(); ++i) counts[original_indices_[i]] = items_[i].count;
            }
            return counts;
        }

        const std::size_t row_size = radius_ + 1;
        auto removed_weight = trace_[choice.pivot * (2 * radius_ + 1) + *choice.index].removed_weight;
        auto added_weight = removed_weight + *choice.index - radius_;
        assert(removed_weight <= radius_ && added_weight <= radius_);
        for (std::size_t i = choice.pivot; i-- > 0;) {
            const auto copies = removal_choices_[i * row_size + removed_weight];
            removed_weight -= copies * static_cast<std::size_t>(items_[i].weight);
            const auto removed = static_cast<long long>(copies);
            counts[original_indices_[i]] = complement ? removed : items_[i].count - removed;
        }
        for (std::size_t i = choice.pivot; i < items_.size(); ++i) {
            const auto copies = addition_choices_[i * row_size + added_weight];
            added_weight -= copies * static_cast<std::size_t>(items_[i].weight);
            const auto added = static_cast<long long>(copies);
            counts[original_indices_[i]] = complement ? items_[i].count - added : added;
        }
        if (choice.periods != 0) {
            counts[original_indices_[choice.pivot]] += complement ? -choice.periods : choice.periods;
        }
        return counts;
    }

public:
    template <class Weight, class Value, class Count>
    explicit bounded_knapsack_compressed(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                         bool enable_reconstruction = false)
        : input_size_(items.size()), reconstruction_enabled_(enable_reconstruction) {
        static_assert(std::is_integral_v<Weight> && std::is_integral_v<Value> && std::is_integral_v<Count>);
        const auto read_item = [&](std::size_t index) {
            const auto& [weight, value, count] = items[index];
            return item{static_cast<long long>(weight), static_cast<long long>(value),
                        static_cast<long long>(count)};
        };
        long long max_weight = 0;
        for (std::size_t i = 0; i < items.size(); ++i) {
            const auto type = read_item(i);
            assert(type.weight > 0 && type.value >= 0 && type.count >= 0);
            if (type.count == 0) continue;
            if (reconstruction_enabled_) original_indices_.push_back(i);
            else items_.push_back(type);
            max_weight = std::max(max_weight, type.weight);
        }
        if (max_weight == 0) return;

        const auto square = static_cast<wide>(max_weight) * max_weight;
        const auto type_count = reconstruction_enabled_ ? original_indices_.size() : items_.size();
        const auto max_row_size = table_.max_size() / (type_count + 1);
        if (max_row_size == 0 || square > (max_row_size - 1) / 8) {
            throw std::length_error("bounded knapsack compressed table is too large");
        }
        radius_ = static_cast<std::size_t>(4 * square);
        margin_ = 2 * max_weight;

        const auto better_density = [](const item& left, const item& right) {
            return static_cast<wide>(left.value) * right.weight >
                   static_cast<wide>(right.value) * left.weight;
        };
        if (reconstruction_enabled_) {
            std::sort(original_indices_.begin(), original_indices_.end(), [&](std::size_t left, std::size_t right) {
                return better_density(read_item(left), read_item(right));
            });
            items_.reserve(type_count);
            for (const auto index : original_indices_) items_.push_back(read_item(index));
        } else {
            std::sort(items_.begin(), items_.end(), better_density);
        }
        prefixes_.reserve(items_.size() + 1);
        for (const auto& type : items_) {
            const auto& prefix = prefixes_.back();
            const auto weight = knapsack_detail::add_bounded_stock_total(
                prefix.weight, static_cast<wide>(type.count) * type.weight);
            const auto value = knapsack_detail::add_bounded_stock_total(
                prefix.value, static_cast<wide>(type.count) * type.value);
            prefixes_.push_back({weight, value});
        }

        const std::size_t row_size = 2 * radius_ + 1;
        table_.resize(prefixes_.size() * row_size);
        std::vector<wide> dp(row_size, unreachable);
        dp[radius_] = 0;
        if (reconstruction_enabled_) {
            trace_.resize(table_.size());
            build_rows<true>(0, items_.size(), std::move(dp), std::vector<std::size_t>(row_size));
            build_choices();
        } else {
            build_rows<false>(0, items_.size(), std::move(dp));
        }
    }

    std::optional<long long> exact_value(long long weight) const {
        assert(weight >= 0);
        const auto value = solve(weight, true);
        if (value == unreachable) return std::nullopt;
        return knapsack_detail::checked_bounded_value(value);
    }

    long long max_value_at_most(long long limit_weight) const {
        assert(limit_weight >= 0);
        return knapsack_detail::checked_bounded_value(solve(limit_weight, false));
    }

    std::optional<long long> min_value_at_least(long long target_weight) const {
        assert(target_weight >= 0);
        if (target_weight == 0) return 0;
        if (items_.empty() || target_weight > prefixes_.back().weight) return std::nullopt;
        const auto& total = prefixes_.back();
        return knapsack_detail::checked_bounded_value(total.value - solve(total.weight - target_weight, false));
    }

    std::optional<knapsack_result> exact_value_with_counts(long long weight) const {
        require_reconstruction();
        assert(weight >= 0);
        query_choice choice;
        const auto value = solve(weight, true, &choice);
        if (value == unreachable) return std::nullopt;
        return knapsack_result{knapsack_detail::checked_bounded_value(value), restore_counts(choice)};
    }

    knapsack_result max_value_at_most_with_counts(long long limit_weight) const {
        require_reconstruction();
        assert(limit_weight >= 0);
        query_choice choice;
        const auto value = solve(limit_weight, false, &choice);
        return {knapsack_detail::checked_bounded_value(value), restore_counts(choice)};
    }

    std::optional<knapsack_result> min_value_at_least_with_counts(long long target_weight) const {
        require_reconstruction();
        assert(target_weight >= 0);
        if (target_weight == 0) return knapsack_result{0, std::vector<long long>(input_size_)};
        const auto& total = prefixes_.back();
        if (target_weight > total.weight) return std::nullopt;
        query_choice choice;
        const auto removed_value = solve(total.weight - target_weight, false, &choice);
        return knapsack_result{knapsack_detail::checked_bounded_value(total.value - removed_value),
                               restore_counts(choice, true)};
    }
};

}
