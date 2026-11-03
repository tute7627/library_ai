// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#pragma once

#include <cassert>

namespace cp {

template <class Weight>
struct wavelet_sum_group {
    static Weight identity() { return Weight{0}; }
    static Weight op(const Weight& a, const Weight& b) { return a + b; }
    static Weight inverse(const Weight& a) { return -a; }
};

template <class Key, class Weight>
struct wavelet_search_result {
    Key value;
    std::int64_t count;
    Weight fold;
};

namespace detail {

class wavelet_bit_vector {
    std::vector<std::uint64_t> words_;
    std::vector<int> counts_;

public:
    explicit wavelet_bit_vector(int n)
        : words_((static_cast<std::size_t>(n) + 63) / 64, 0),
          counts_(words_.size() + 1, 0) {}
    void set(int i) { words_[i >> 6] |= UINT64_C(1) << (i & 63); }
    bool get(int i) const { return (words_[i >> 6] >> (i & 63)) & 1; }
    void build() {
        for (std::size_t i = 0; i < words_.size(); ++i)
            counts_[i + 1] = counts_[i] + __builtin_popcountll(words_[i]);
    }
    int rank1(int r) const {
        const int block = r >> 6, offset = r & 63;
        return counts_[block] + (offset == 0 ? 0 : __builtin_popcountll(
            words_[block] & ((UINT64_C(1) << offset) - 1)));
    }
};

}

template <class Key, class Weight = long long,
          class Group = wavelet_sum_group<Weight>>
class wavelet_matrix {
    static_assert(std::is_integral_v<Key> && !std::is_same_v<Key, bool> &&
                      sizeof(Key) <= sizeof(std::uint64_t),
                  "wavelet_matrix requires an integer key of at most 64 bits");

public:
    using key_type = Key;
    using weight_type = Weight;
    using mask_type = std::make_unsigned_t<Key>;
    using size_type = int;
    using range_type = std::pair<int, int>;
    using search_result_type = wavelet_search_result<Key, Weight>;

private:
    using U = mask_type;
    static constexpr int width = std::numeric_limits<U>::digits;
    static constexpr U sign_bit = std::is_signed_v<Key> ? U{1} << (width - 1) : U{0};
    int n_ = 0;
    int bits_ = 0;
    U base_ = 0;
    std::vector<detail::wavelet_bit_vector> levels_;
    std::vector<int> zeros_;
    std::vector<std::vector<Weight>> prefix_;

    static U low_mask(int bits) {
        return bits == width ? std::numeric_limits<U>::max()
                             : static_cast<U>((U{1} << bits) - 1);
    }
    static U encode(Key value) { return static_cast<U>(value) ^ sign_bit; }
    static Key decode(U value) {
        if constexpr (std::is_signed_v<Key>) {
            if (value < sign_bit)
                return static_cast<Key>(std::numeric_limits<Key>::min() +
                                        static_cast<Key>(value));
            return static_cast<Key>(value - sign_bit);
        } else {
            return value;
        }
    }
    void check_range(int l, int r) const { assert(0 <= l && l <= r && r <= n_); }
    std::int64_t check_ranges(const std::vector<range_type>& ranges) const {
        std::int64_t total = 0;
        for (const auto& interval : ranges) {
            check_range(interval.first, interval.second);
            const int length = interval.second - interval.first;
            assert(total <= std::numeric_limits<std::int64_t>::max() - length);
            total += length;
        }
        return total;
    }
    U root_value(U mask) const {
        return static_cast<U>((base_ ^ mask) & static_cast<U>(~low_mask(bits_)));
    }
    Weight fold_node(int depth, int l, int r) const {
        return Group::op(Group::inverse(prefix_[depth][l]), prefix_[depth][r]);
    }
    template <bool Weighted, bool Counted = false>
    auto less(int l, int r, U upper, U mask) const {
        using Result = std::conditional_t<Counted, std::pair<Weight, int>,
                                          std::conditional_t<Weighted, Weight, int>>;
        Result answer = []() -> Result {
            if constexpr (Counted) return {Group::identity(), 0};
            else if constexpr (Weighted) return Group::identity();
            else return 0;
        }();
        const U root = root_value(mask);
        if (upper <= root) return answer;
        if (upper > static_cast<U>(root | low_mask(bits_))) {
            if constexpr (Counted) return Result{fold_node(0, l, r), r - l};
            else if constexpr (Weighted) return fold_node(0, l, r);
            else return r - l;
        }
        for (int depth = 0; depth < bits_ && l < r; ++depth) {
            const int bit = bits_ - 1 - depth;
            const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
            const bool flipped = (mask >> bit) & 1;
            const bool take_one = (upper >> bit) & 1;
            if (take_one) {
                const int a = flipped ? zeros_[depth] + lo : l - lo;
                const int b = flipped ? zeros_[depth] + hi : r - hi;
                if constexpr (Counted) {
                    answer.first = Group::op(answer.first, fold_node(depth + 1, a, b));
                    answer.second += b - a;
                } else if constexpr (Weighted)
                    answer = Group::op(answer, fold_node(depth + 1, a, b));
                else answer += b - a;
            }
            if (take_one != flipped) {
                l = zeros_[depth] + lo;
                r = zeros_[depth] + hi;
            } else {
                l -= lo;
                r -= hi;
            }
        }
        return answer;
    }
    template <class Predicate>
    wavelet_search_result<U, Weight> finish_value(int l, int r, U value,
            std::int64_t taken, const Weight& accumulated, Weight answer,
            Predicate& predicate) const {
        int low = 0, high = r - l;
        while (high - low > 1) {
            const int middle = low + (high - low) / 2;
            Weight next = Group::op(accumulated, fold_node(bits_, l, l + middle));
            if (predicate(next)) {
                high = middle;
                answer = std::move(next);
            } else {
                low = middle;
            }
        }
        return {value, taken + high, std::move(answer)};
    }
    template <bool TakeAllEqual, class Predicate>
    std::optional<wavelet_search_result<U, Weight>> find_value(int l, int r, U lower, Predicate& predicate, U mask) const {
        U value = root_value(mask);
        if (l == r || static_cast<U>(value | low_mask(bits_)) < lower)
            return std::nullopt;
        struct search_node {
            int depth, l, r;
            U value;
        };
        std::array<search_node, width> pending;
        int count = 0, depth = 0;
        std::int64_t taken = 0;
        Weight accumulated = Group::identity();
        Weight answer = accumulated;
        for (;;) {
            if (l < r) {
                if (value >= lower) {
                    Weight next = Group::op(accumulated, fold_node(depth, l, r));
                    if (predicate(next)) {
                        answer = std::move(next);
                        break;
                    }
                    accumulated = std::move(next);
                    taken += r - l;
                } else {
                    const int bit = bits_ - 1 - depth;
                    const bool flipped = (mask >> bit) & 1;
                    const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
                    const int a = flipped ? zeros_[depth] + lo : l - lo;
                    const int b = flipped ? zeros_[depth] + hi : r - hi;
                    const int c = flipped ? l - lo : zeros_[depth] + lo;
                    const int d = flipped ? r - hi : zeros_[depth] + hi;
                    if ((lower >> bit) & 1) {
                        value |= U{1} << bit;
                        l = c;
                        r = d;
                    } else {
                        if (c < d)
                            pending[count++] = {depth + 1, c, d,
                                                static_cast<U>(value | (U{1} << bit))};
                        l = a;
                        r = b;
                    }
                    ++depth;
                    continue;
                }
            }
            if (count == 0) return std::nullopt;
            const auto node = pending[--count];
            depth = node.depth;
            l = node.l;
            r = node.r;
            value = node.value;
        }
        for (; depth < bits_; ++depth) {
            const int bit = bits_ - 1 - depth;
            const bool flipped = (mask >> bit) & 1;
            const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
            const int a = flipped ? zeros_[depth] + lo : l - lo;
            const int b = flipped ? zeros_[depth] + hi : r - hi;
            const int c = flipped ? l - lo : zeros_[depth] + lo;
            const int d = flipped ? r - hi : zeros_[depth] + hi;
            if (a == b) {
                value |= U{1} << bit;
                l = c;
                r = d;
            } else if (c == d) {
                l = a;
                r = b;
            } else {
                Weight next = Group::op(accumulated, fold_node(depth + 1, a, b));
                if (predicate(next)) {
                    answer = std::move(next);
                    l = a;
                    r = b;
                } else {
                    accumulated = std::move(next);
                    taken += b - a;
                    value |= U{1} << bit;
                    l = c;
                    r = d;
                }
            }
        }
        if constexpr (TakeAllEqual)
            return wavelet_search_result<U, Weight>{value, taken + r - l, std::move(answer)};
        else
            return finish_value(l, r, value, taken, accumulated, std::move(answer), predicate);
    }

    template <bool TakeAllEqual, class Predicate>
    std::optional<wavelet_search_result<U, Weight>> find_value(std::vector<range_type> ranges, U lower,
                               Predicate& predicate, U mask) const {
        Weight below = Group::identity();
        Weight total = Group::identity();
        std::int64_t taken = 0;
        for (const auto& interval : ranges) {
            const int l = interval.first, r = interval.second;
            const auto excluded = less<true, true>(l, r, lower, mask);
            below = Group::op(below, excluded.first);
            taken -= excluded.second;
            total = Group::op(total, fold_node(0, l, r));
        }
        Weight accumulated = Group::inverse(below);
        Weight answer = Group::op(accumulated, total);
        if (!predicate(answer)) return std::nullopt;
        U value = root_value(mask);
        std::vector<range_type> small(ranges.size());
        for (int depth = 0; depth < bits_; ++depth) {
            const int bit = bits_ - 1 - depth;
            const bool flipped = (mask >> bit) & 1;
            Weight small_sum = Group::identity();
            std::int64_t small_count = 0;
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                const int l = ranges[i].first, r = ranges[i].second;
                const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
                if (flipped) {
                    small[i] = {zeros_[depth] + lo, zeros_[depth] + hi};
                    ranges[i] = {l - lo, r - hi};
                } else {
                    small[i] = {l - lo, r - hi};
                    ranges[i] = {zeros_[depth] + lo, zeros_[depth] + hi};
                }
                small_count += small[i].second - small[i].first;
                small_sum = Group::op(small_sum,
                    fold_node(depth + 1, small[i].first, small[i].second));
            }
            Weight next = Group::op(accumulated, small_sum);
            if (static_cast<U>(value | low_mask(bit)) >= lower && predicate(next)) {
                if constexpr (TakeAllEqual) answer = std::move(next);
                ranges.swap(small);
            } else {
                accumulated = std::move(next);
                taken += small_count;
                value |= U{1} << bit;
            }
        }
        if constexpr (TakeAllEqual) {
            for (const auto& interval : ranges) taken += interval.second - interval.first;
            return wavelet_search_result<U, Weight>{value, taken, std::move(answer)};
        } else {
            for (const auto& interval : ranges) {
                const int l = interval.first, r = interval.second;
                if (l == r) continue;
                Weight next = Group::op(accumulated, fold_node(bits_, l, r));
                if (predicate(next))
                    return finish_value(l, r, value, taken, accumulated, std::move(next), predicate);
                accumulated = std::move(next);
                taken += r - l;
            }
            return std::nullopt;
        }
    }

public:
    wavelet_matrix() = default;
    explicit wavelet_matrix(const std::vector<Key>& values) {
        if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("cp::wavelet_matrix size too large");
        n_ = static_cast<int>(values.size());
        if (n_ == 0) return;
        std::vector<U> current(n_);
        U minimum = std::numeric_limits<U>::max(), maximum = 0;
        for (int i = 0; i < n_; ++i) {
            current[i] = encode(values[i]);
            minimum = std::min(minimum, current[i]);
            maximum = std::max(maximum, current[i]);
        }
        for (U varying = minimum ^ maximum; varying != 0; varying >>= 1) ++bits_;
        base_ = static_cast<U>(minimum & static_cast<U>(~low_mask(bits_)));
        if (bits_ == 0) return;
        std::vector<U> next(n_), right(n_);
        levels_.reserve(bits_);
        zeros_.reserve(bits_);
        for (int bit = bits_ - 1; bit >= 0; --bit) {
            levels_.emplace_back(n_);
            auto& level = levels_.back();
            int zero = 0, one = 0;
            for (int i = 0; i < n_; ++i) {
                if ((current[i] >> bit) & 1) {
                    level.set(i);
                    right[one++] = current[i];
                } else {
                    next[zero++] = current[i];
                }
            }
            level.build();
            zeros_.push_back(zero);
            std::copy_n(right.begin(), one, next.begin() + zero);
            current.swap(next);
        }
    }

    int size() const { return n_; }
    bool empty() const { return n_ == 0; }
    bool has_weights() const { return !prefix_.empty(); }

    Key kth_smallest(int l, int r, int k, U xor_mask = 0) const {
        check_range(l, r);
        assert(0 <= k && k < r - l);
        U answer = root_value(xor_mask);
        for (int depth = 0; depth < bits_; ++depth) {
            const int bit = bits_ - 1 - depth;
            const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
            const bool flipped = (xor_mask >> bit) & 1;
            const int small = flipped ? hi - lo : (r - l) - (hi - lo);
            const bool take_one = k >= small;
            if (take_one) {
                k -= small;
                answer |= U{1} << bit;
            }
            if (take_one != flipped) {
                l = zeros_[depth] + lo;
                r = zeros_[depth] + hi;
            } else {
                l -= lo;
                r -= hi;
            }
        }
        return decode(answer);
    }

    Key kth_smallest(std::vector<range_type> ranges, std::int64_t k,
                     U xor_mask = 0) const {
        const std::int64_t total = check_ranges(ranges);
        assert(0 <= k && k < total);
        U answer = root_value(xor_mask);
        std::vector<range_type> small(ranges.size());
        for (int depth = 0; depth < bits_; ++depth) {
            const int bit = bits_ - 1 - depth;
            const bool flipped = (xor_mask >> bit) & 1;
            std::int64_t count = 0;
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                const int l = ranges[i].first, r = ranges[i].second;
                const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
                if (flipped) {
                    small[i] = {zeros_[depth] + lo, zeros_[depth] + hi};
                    ranges[i] = {l - lo, r - hi};
                } else {
                    small[i] = {l - lo, r - hi};
                    ranges[i] = {zeros_[depth] + lo, zeros_[depth] + hi};
                }
                count += small[i].second - small[i].first;
            }
            if (k < count) {
                ranges.swap(small);
            } else {
                k -= count;
                answer |= U{1} << bit;
            }
        }
        return decode(answer);
    }

    Key kth_largest(int l, int r, int k, U xor_mask = 0) const {
        check_range(l, r);
        assert(0 <= k && k < r - l);
        return kth_smallest(l, r, r - l - 1 - k, xor_mask);
    }
    Key kth_largest(std::vector<range_type> ranges, std::int64_t k,
                    U xor_mask = 0) const {
        const std::int64_t total = check_ranges(ranges);
        assert(0 <= k && k < total);
        return kth_smallest(std::move(ranges), total - 1 - k, xor_mask);
    }

    int range_freq(int l, int r, Key lower, Key upper, U xor_mask = 0) const {
        check_range(l, r);
        if (lower >= upper || l == r) return 0;
        return less<false>(l, r, encode(upper), xor_mask) -
               less<false>(l, r, encode(lower), xor_mask);
    }
    void build_weights(const std::vector<Weight>& weights) {
        assert(weights.size() == static_cast<std::size_t>(n_));
        std::vector<std::vector<Weight>> prefix;
        prefix.reserve(bits_ + 1);
        std::vector<Weight> current(weights), next(n_, Group::identity());
        for (int depth = 0; depth <= bits_; ++depth) {
            prefix.emplace_back(static_cast<std::size_t>(n_) + 1, Group::identity());
            auto& sums = prefix.back();
            for (int i = 0; i < n_; ++i) sums[i + 1] = Group::op(sums[i], current[i]);
            if (depth == bits_) break;
            int zero = 0, one = zeros_[depth];
            for (int i = 0; i < n_; ++i) {
                if (levels_[depth].get(i)) next[one++] = std::move(current[i]);
                else next[zero++] = std::move(current[i]);
            }
            current.swap(next);
        }
        prefix_.swap(prefix);
    }
    Weight range_fold(int l, int r) const {
        check_range(l, r);
        assert(has_weights());
        return fold_node(0, l, r);
    }
    Weight range_fold(int l, int r, Key lower, Key upper, U xor_mask = 0) const {
        check_range(l, r);
        assert(has_weights());
        if (lower >= upper || l == r) return Group::identity();
        return Group::op(Group::inverse(less<true>(l, r, encode(lower), xor_mask)),
                         less<true>(l, r, encode(upper), xor_mask));
    }
    Weight fold_smallest(int l, int r, int k, U xor_mask = 0) const {
        check_range(l, r);
        assert(has_weights());
        assert(0 <= k && k <= r - l);
        Weight answer = Group::identity();
        if (k == 0) return answer;
        for (int depth = 0; depth < bits_; ++depth) {
            if (k == r - l)
                return Group::op(answer, fold_node(depth, l, r));
            const int bit = bits_ - 1 - depth;
            const bool flipped = (xor_mask >> bit) & 1;
            const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
            const int a = flipped ? zeros_[depth] + lo : l - lo;
            const int b = flipped ? zeros_[depth] + hi : r - hi;
            const int c = flipped ? l - lo : zeros_[depth] + lo;
            const int d = flipped ? r - hi : zeros_[depth] + hi;
            if (k <= b - a) {
                if (k == b - a)
                    return Group::op(answer, fold_node(depth + 1, a, b));
                l = a;
                r = b;
            } else {
                if (a < b) answer = Group::op(answer, fold_node(depth + 1, a, b));
                k -= b - a;
                l = c;
                r = d;
            }
        }
        return Group::op(answer, fold_node(bits_, l, l + k));
    }
    Weight fold_smallest(std::vector<range_type> ranges, std::int64_t k,
                         U xor_mask = 0) const {
        const std::int64_t total = check_ranges(ranges);
        assert(has_weights());
        assert(0 <= k && k <= total);
        Weight answer = Group::identity();
        if (k == 0) return answer;
        if (k == total) {
            for (const auto& interval : ranges)
                answer = Group::op(answer, fold_node(0, interval.first, interval.second));
            return answer;
        }
        std::vector<range_type> small(ranges.size());
        for (int depth = 0; depth < bits_; ++depth) {
            const int bit = bits_ - 1 - depth;
            const bool flipped = (xor_mask >> bit) & 1;
            std::int64_t count = 0;
            for (std::size_t i = 0; i < ranges.size(); ++i) {
                const int l = ranges[i].first, r = ranges[i].second;
                const int lo = levels_[depth].rank1(l), hi = levels_[depth].rank1(r);
                if (flipped) {
                    small[i] = {zeros_[depth] + lo, zeros_[depth] + hi};
                    ranges[i] = {l - lo, r - hi};
                } else {
                    small[i] = {l - lo, r - hi};
                    ranges[i] = {zeros_[depth] + lo, zeros_[depth] + hi};
                }
                count += small[i].second - small[i].first;
            }
            if (k <= count) {
                if (k == count) {
                    for (const auto& interval : small)
                        if (interval.first < interval.second)
                            answer = Group::op(answer,
                                fold_node(depth + 1, interval.first, interval.second));
                    return answer;
                }
                ranges.swap(small);
            } else {
                for (const auto& interval : small)
                    if (interval.first < interval.second)
                        answer = Group::op(answer,
                            fold_node(depth + 1, interval.first, interval.second));
                k -= count;
            }
        }
        for (const auto& interval : ranges) {
            const int take = static_cast<int>(std::min<std::int64_t>(
                k, interval.second - interval.first));
            if (take != 0)
                answer = Group::op(answer, fold_node(bits_, interval.first, interval.first + take));
            k -= take;
            if (k == 0) break;
        }
        return answer;
    }
    Weight fold_largest(int l, int r, int k, U xor_mask = 0) const {
        return fold_smallest(l, r, k, xor_mask ^ std::numeric_limits<U>::max());
    }
    Weight fold_largest(std::vector<range_type> ranges, std::int64_t k,
                        U xor_mask = 0) const {
        return fold_smallest(std::move(ranges), k,
                             xor_mask ^ std::numeric_limits<U>::max());
    }
    template <class Predicate>
    std::optional<search_result_type> first_value(int l, int r, Key lower,
            Predicate predicate, U xor_mask = 0) const {
        return first_value(l, r, lower, std::move(predicate), true, xor_mask);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> first_value(int l, int r, Key lower,
            Predicate predicate, Bool take_all_equal, U xor_mask = 0) const {
        check_range(l, r);
        assert(has_weights());
        assert(!predicate(Group::identity()));
        auto result = take_all_equal ? find_value<true>(l, r, encode(lower), predicate, xor_mask)
                                     : find_value<false>(l, r, encode(lower), predicate, xor_mask);
        if (!result) return std::nullopt;
        return search_result_type{decode(result->value), result->count,
                                  std::move(result->fold)};
    }
    template <class Predicate>
    std::optional<search_result_type> last_value(int l, int r, Key upper,
            Predicate predicate, U xor_mask = 0) const {
        return last_value(l, r, upper, std::move(predicate), true, xor_mask);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> last_value(int l, int r, Key upper,
            Predicate predicate, Bool take_all_equal, U xor_mask = 0) const {
        check_range(l, r);
        assert(has_weights());
        assert(!predicate(Group::identity()));
        const U bound = encode(upper);
        if (bound == 0) return std::nullopt;
        const U reversed_mask = xor_mask ^ std::numeric_limits<U>::max();
        const U lower = static_cast<U>(~static_cast<U>(bound - 1));
        auto result = take_all_equal ? find_value<true>(l, r, lower, predicate, reversed_mask)
                                     : find_value<false>(l, r, lower, predicate, reversed_mask);
        if (!result) return std::nullopt;
        return search_result_type{decode(static_cast<U>(~result->value)), result->count,
                                  std::move(result->fold)};
    }
    template <class Predicate>
    std::optional<search_result_type> first_value(std::vector<range_type> ranges, Key lower,
            Predicate predicate, U xor_mask = 0) const {
        return first_value(std::move(ranges), lower, std::move(predicate), true, xor_mask);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> first_value(std::vector<range_type> ranges, Key lower,
            Predicate predicate, Bool take_all_equal, U xor_mask = 0) const {
        check_ranges(ranges);
        assert(has_weights());
        assert(!predicate(Group::identity()));
        auto result = take_all_equal ? find_value<true>(std::move(ranges), encode(lower), predicate, xor_mask)
                                     : find_value<false>(std::move(ranges), encode(lower), predicate, xor_mask);
        if (!result) return std::nullopt;
        return search_result_type{decode(result->value), result->count,
                                  std::move(result->fold)};
    }
    template <class Predicate>
    std::optional<search_result_type> last_value(std::vector<range_type> ranges, Key upper,
            Predicate predicate, U xor_mask = 0) const {
        return last_value(std::move(ranges), upper, std::move(predicate), true, xor_mask);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> last_value(std::vector<range_type> ranges, Key upper,
            Predicate predicate, Bool take_all_equal, U xor_mask = 0) const {
        check_ranges(ranges);
        assert(has_weights());
        assert(!predicate(Group::identity()));
        const U bound = encode(upper);
        if (bound == 0) return std::nullopt;
        const U reversed_mask = xor_mask ^ std::numeric_limits<U>::max();
        const U lower = static_cast<U>(~static_cast<U>(bound - 1));
        auto result = take_all_equal ? find_value<true>(std::move(ranges), lower, predicate, reversed_mask)
                                     : find_value<false>(std::move(ranges), lower, predicate, reversed_mask);
        if (!result) return std::nullopt;
        return search_result_type{decode(static_cast<U>(~result->value)), result->count,
                                  std::move(result->fold)};
    }
};

template <class Key, class Weight = long long,
          class Group = wavelet_sum_group<Weight>>
class compressed_wavelet_matrix {
    std::vector<Key> values_;
    wavelet_matrix<std::uint32_t, Weight, Group> matrix_;

    std::uint32_t rank(const Key& value) const {
        return static_cast<std::uint32_t>(
            std::lower_bound(values_.begin(), values_.end(), value) - values_.begin());
    }

public:
    using key_type = Key;
    using weight_type = Weight;
    using size_type = int;
    using range_type = std::pair<int, int>;
    using search_result_type = wavelet_search_result<Key, Weight>;

    compressed_wavelet_matrix() = default;
    explicit compressed_wavelet_matrix(const std::vector<Key>& values) {
        if (values.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("cp::compressed_wavelet_matrix size too large");
        const int n = static_cast<int>(values.size());
        values_.reserve(n);
        std::vector<std::uint32_t> ranks(n);
        {
            std::vector<std::pair<Key, int>> ordered;
            ordered.reserve(n);
            for (int i = 0; i < n; ++i) ordered.emplace_back(values[i], i);
            std::sort(ordered.begin(), ordered.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            for (const auto& item : ordered) {
                if (values_.empty() || values_.back() < item.first)
                    values_.push_back(item.first);
                ranks[item.second] = static_cast<std::uint32_t>(values_.size() - 1);
            }
        }
        matrix_ = wavelet_matrix<std::uint32_t, Weight, Group>(ranks);
    }

    int size() const { return matrix_.size(); }
    bool empty() const { return matrix_.empty(); }
    bool has_weights() const { return matrix_.has_weights(); }
    void build_weights(const std::vector<Weight>& weights) { matrix_.build_weights(weights); }
    Key kth_smallest(int l, int r, int k) const { return values_[matrix_.kth_smallest(l, r, k)]; }
    Key kth_smallest(std::vector<range_type> ranges, std::int64_t k) const {
        return values_[matrix_.kth_smallest(std::move(ranges), k)];
    }
    Key kth_largest(int l, int r, int k) const { return values_[matrix_.kth_largest(l, r, k)]; }
    Key kth_largest(std::vector<range_type> ranges, std::int64_t k) const {
        return values_[matrix_.kth_largest(std::move(ranges), k)];
    }
    Weight fold_smallest(int l, int r, int k) const { return matrix_.fold_smallest(l, r, k); }
    Weight fold_smallest(std::vector<range_type> ranges, std::int64_t k) const {
        return matrix_.fold_smallest(std::move(ranges), k);
    }
    Weight fold_largest(int l, int r, int k) const { return matrix_.fold_largest(l, r, k); }
    Weight fold_largest(std::vector<range_type> ranges, std::int64_t k) const {
        return matrix_.fold_largest(std::move(ranges), k);
    }
    int range_freq(int l, int r, const Key& lower, const Key& upper) const {
        if (!(lower < upper)) return matrix_.range_freq(l, r, 0, 0);
        return matrix_.range_freq(l, r, rank(lower), rank(upper));
    }
    Weight range_fold(int l, int r) const { return matrix_.range_fold(l, r); }
    Weight range_fold(int l, int r, const Key& lower, const Key& upper) const {
        if (!(lower < upper)) return matrix_.range_fold(l, r, 0, 0);
        return matrix_.range_fold(l, r, rank(lower), rank(upper));
    }
    template <class Predicate>
    std::optional<search_result_type> first_value(int l, int r, const Key& lower,
            Predicate predicate) const {
        return first_value(l, r, lower, std::move(predicate), true);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> first_value(int l, int r, const Key& lower,
            Predicate predicate, Bool take_all_equal) const {
        auto result = matrix_.first_value(l, r, rank(lower), std::move(predicate), take_all_equal);
        if (!result) return std::nullopt;
        return search_result_type{values_[result->value], result->count,
                                  std::move(result->fold)};
    }
    template <class Predicate>
    std::optional<search_result_type> last_value(int l, int r, const Key& upper,
            Predicate predicate) const {
        return last_value(l, r, upper, std::move(predicate), true);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> last_value(int l, int r, const Key& upper,
            Predicate predicate, Bool take_all_equal) const {
        auto result = matrix_.last_value(l, r, rank(upper), std::move(predicate), take_all_equal);
        if (!result) return std::nullopt;
        return search_result_type{values_[result->value], result->count,
                                  std::move(result->fold)};
    }
    template <class Predicate>
    std::optional<search_result_type> first_value(std::vector<range_type> ranges, const Key& lower,
            Predicate predicate) const {
        return first_value(std::move(ranges), lower, std::move(predicate), true);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> first_value(std::vector<range_type> ranges, const Key& lower,
            Predicate predicate, Bool take_all_equal) const {
        auto result = matrix_.first_value(std::move(ranges), rank(lower), std::move(predicate), take_all_equal);
        if (!result) return std::nullopt;
        return search_result_type{values_[result->value], result->count,
                                  std::move(result->fold)};
    }
    template <class Predicate>
    std::optional<search_result_type> last_value(std::vector<range_type> ranges, const Key& upper,
            Predicate predicate) const {
        return last_value(std::move(ranges), upper, std::move(predicate), true);
    }
    template <class Predicate, class Bool,
              std::enable_if_t<std::is_same_v<std::decay_t<Bool>, bool>, int> = 0>
    std::optional<search_result_type> last_value(std::vector<range_type> ranges, const Key& upper,
            Predicate predicate, Bool take_all_equal) const {
        auto result = matrix_.last_value(std::move(ranges), rank(upper), std::move(predicate), take_all_equal);
        if (!result) return std::nullopt;
        return search_result_type{values_[result->value], result->count,
                                  std::move(result->fold)};
    }
};

namespace detail {

template <class Matrix, class = void>
struct indexed_wavelet_matrix_mask {};

template <class Matrix>
struct indexed_wavelet_matrix_mask<Matrix, std::void_t<typename Matrix::mask_type>> {
    using mask_type = typename Matrix::mask_type;
};

}

template <class Index, class Matrix>
class indexed_wavelet_matrix : public detail::indexed_wavelet_matrix_mask<Matrix> {
public:
    using index_type = Index;
    using matrix_type = Matrix;
    using key_type = typename Matrix::key_type;
    using weight_type = typename Matrix::weight_type;
    using size_type = int;
    using range_type = std::pair<Index, Index>;
    using search_result_type = typename Matrix::search_result_type;

private:
    using core_range_type = typename Matrix::range_type;
    std::vector<Index> positions_;
    std::vector<int> order_;
    Matrix matrix_;

    core_range_type index_range(const Index& l, const Index& r) const {
        assert(!(r < l));
        const auto first = std::lower_bound(positions_.begin(), positions_.end(), l);
        const auto last = std::lower_bound(first, positions_.end(), r);
        return {static_cast<int>(first - positions_.begin()),
                static_cast<int>(last - positions_.begin())};
    }
    std::vector<core_range_type> index_ranges(const std::vector<range_type>& ranges) const {
        std::vector<core_range_type> result;
        result.reserve(ranges.size());
        for (const auto& interval : ranges)
            result.push_back(index_range(interval.first, interval.second));
        return result;
    }

public:
    indexed_wavelet_matrix() = default;
    indexed_wavelet_matrix(const std::vector<Index>& positions,
                           const std::vector<key_type>& values) {
        assert(positions.size() == values.size());
        if (positions.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::length_error("cp::indexed_wavelet_matrix size too large");
        positions_ = positions;
        if (std::is_sorted(positions.begin(), positions.end())) {
            matrix_ = Matrix(values);
            return;
        }
        order_.resize(positions.size());
        std::iota(order_.begin(), order_.end(), 0);
        std::sort(order_.begin(), order_.end(), [&](int a, int b) {
            if (positions[a] < positions[b]) return true;
            if (positions[b] < positions[a]) return false;
            return a < b;
        });
        std::vector<key_type> ordered_values;
        ordered_values.reserve(values.size());
        for (std::size_t i = 0; i < order_.size(); ++i) {
            positions_[i] = positions[order_[i]];
            ordered_values.push_back(values[order_[i]]);
        }
        matrix_ = Matrix(ordered_values);
    }

    int size() const { return matrix_.size(); }
    bool empty() const { return matrix_.empty(); }
    bool has_weights() const { return matrix_.has_weights(); }
    int range_size(const Index& l, const Index& r) const {
        const auto interval = index_range(l, r);
        return interval.second - interval.first;
    }
    std::int64_t range_size(const std::vector<range_type>& ranges) const {
        std::int64_t total = 0;
        for (const auto& interval : ranges) {
            const int count = range_size(interval.first, interval.second);
            assert(total <= std::numeric_limits<std::int64_t>::max() - count);
            total += count;
        }
        return total;
    }
    void build_weights(const std::vector<weight_type>& weights) {
        assert(weights.size() == positions_.size());
        if (order_.empty()) {
            matrix_.build_weights(weights);
            return;
        }
        std::vector<weight_type> ordered_weights;
        ordered_weights.reserve(weights.size());
        for (const int i : order_) ordered_weights.push_back(weights[i]);
        matrix_.build_weights(ordered_weights);
    }

    template <class... Mask>
    auto kth_smallest(const Index& l, const Index& r, int k, Mask... xor_mask) const
        -> decltype(matrix_.kth_smallest(0, 0, k, xor_mask...)) {
        const auto interval = index_range(l, r);
        return matrix_.kth_smallest(interval.first, interval.second, k, xor_mask...);
    }
    template <class... Mask>
    auto kth_smallest(const std::vector<range_type>& ranges, std::int64_t k,
                      Mask... xor_mask) const
        -> decltype(matrix_.kth_smallest(std::declval<std::vector<core_range_type>>(),
                                         k, xor_mask...)) {
        return matrix_.kth_smallest(index_ranges(ranges), k, xor_mask...);
    }
    template <class... Mask>
    auto kth_largest(const Index& l, const Index& r, int k, Mask... xor_mask) const
        -> decltype(matrix_.kth_largest(0, 0, k, xor_mask...)) {
        const auto interval = index_range(l, r);
        return matrix_.kth_largest(interval.first, interval.second, k, xor_mask...);
    }
    template <class... Mask>
    auto kth_largest(const std::vector<range_type>& ranges, std::int64_t k,
                     Mask... xor_mask) const
        -> decltype(matrix_.kth_largest(std::declval<std::vector<core_range_type>>(),
                                        k, xor_mask...)) {
        return matrix_.kth_largest(index_ranges(ranges), k, xor_mask...);
    }
    template <class... Mask>
    auto range_freq(const Index& l, const Index& r, const key_type& lower, const key_type& upper,
                    Mask... xor_mask) const
        -> decltype(matrix_.range_freq(0, 0, lower, upper, xor_mask...)) {
        const auto interval = index_range(l, r);
        return matrix_.range_freq(interval.first, interval.second, lower, upper, xor_mask...);
    }
    weight_type range_fold(const Index& l, const Index& r) const {
        const auto interval = index_range(l, r);
        return matrix_.range_fold(interval.first, interval.second);
    }
    template <class... Mask>
    auto range_fold(const Index& l, const Index& r, const key_type& lower, const key_type& upper,
                    Mask... xor_mask) const
        -> decltype(matrix_.range_fold(0, 0, lower, upper, xor_mask...)) {
        const auto interval = index_range(l, r);
        return matrix_.range_fold(interval.first, interval.second, lower, upper, xor_mask...);
    }
    template <class... Mask>
    auto fold_smallest(const Index& l, const Index& r, int k, Mask... xor_mask) const
        -> decltype(matrix_.fold_smallest(0, 0, k, xor_mask...)) {
        const auto interval = index_range(l, r);
        return matrix_.fold_smallest(interval.first, interval.second, k, xor_mask...);
    }
    template <class... Mask>
    auto fold_smallest(const std::vector<range_type>& ranges, std::int64_t k,
                       Mask... xor_mask) const
        -> decltype(matrix_.fold_smallest(std::declval<std::vector<core_range_type>>(),
                                          k, xor_mask...)) {
        return matrix_.fold_smallest(index_ranges(ranges), k, xor_mask...);
    }
    template <class... Mask>
    auto fold_largest(const Index& l, const Index& r, int k, Mask... xor_mask) const
        -> decltype(matrix_.fold_largest(0, 0, k, xor_mask...)) {
        const auto interval = index_range(l, r);
        return matrix_.fold_largest(interval.first, interval.second, k, xor_mask...);
    }
    template <class... Mask>
    auto fold_largest(const std::vector<range_type>& ranges, std::int64_t k,
                      Mask... xor_mask) const
        -> decltype(matrix_.fold_largest(std::declval<std::vector<core_range_type>>(),
                                         k, xor_mask...)) {
        return matrix_.fold_largest(index_ranges(ranges), k, xor_mask...);
    }
    template <class Predicate, class... Options>
    auto first_value(const Index& l, const Index& r, const key_type& lower, Predicate predicate,
                     Options... options) const
        -> decltype(matrix_.first_value(0, 0, lower, std::move(predicate), options...)) {
        const auto interval = index_range(l, r);
        return matrix_.first_value(interval.first, interval.second, lower,
                                   std::move(predicate), options...);
    }
    template <class Predicate, class... Options>
    auto first_value(const std::vector<range_type>& ranges, const key_type& lower,
                     Predicate predicate, Options... options) const
        -> decltype(matrix_.first_value(std::declval<std::vector<core_range_type>>(),
                                        lower, std::move(predicate), options...)) {
        return matrix_.first_value(index_ranges(ranges), lower, std::move(predicate), options...);
    }
    template <class Predicate, class... Options>
    auto last_value(const Index& l, const Index& r, const key_type& upper, Predicate predicate,
                    Options... options) const
        -> decltype(matrix_.last_value(0, 0, upper, std::move(predicate), options...)) {
        const auto interval = index_range(l, r);
        return matrix_.last_value(interval.first, interval.second, upper,
                                  std::move(predicate), options...);
    }
    template <class Predicate, class... Options>
    auto last_value(const std::vector<range_type>& ranges, const key_type& upper,
                    Predicate predicate, Options... options) const
        -> decltype(matrix_.last_value(std::declval<std::vector<core_range_type>>(),
                                       upper, std::move(predicate), options...)) {
        return matrix_.last_value(index_ranges(ranges), upper, std::move(predicate), options...);
    }
};

}
