# ナップサック

共通ヘッダ `library/template.hpp` を先に読み込む。

## 使用数の復元

`*_with_counts` は最適解を一つ返す。結果型 `cp::knapsack_result` の `long long value` は最適価値和、`std::vector<long long> counts` は入力順の使用数で、長さは `items.size()`。未使用の品物は 0 になり、同価値の解が複数ある場合はそのうち一つが返る。

前計算クラスで復元するには、コンストラクタの最後の引数 `enable_reconstruction` に `true` を渡す。既定値は `false`。復元が無効な状態で `*_with_counts` を呼ぶと、重量の検査に先立って `std::logic_error` を送出する。値だけを返すメソッドはどちらの設定でも使える。
復元結果の配列の確保に失敗した場合は `std::bad_alloc` が伝わる。0/1 全列挙と単発差分 DP では、復元付きの関数を直接呼べる。

## 0/1 ナップサック

各品物を高々一度選び、合計重量の条件を満たす部分集合のうち、価値の合計が最適なものを求める。
品物の数が小さければ、重量や価値の絶対値が大きくても全列挙を使える。

### 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#include "library/template.hpp"
#include "library/knapsack.hpp"

int main() {
    const std::vector<std::pair<int, long long>> items{{4, 10}, {5, 3}, {2, 2}};
    assert(cp::knapsack01_bruteforce_at_most(items, 4) == 10);
    assert(cp::knapsack01_bruteforce_at_least(items, 4) == 3);
    assert(!cp::knapsack01_bruteforce_at_least(items, 12));
    const auto result = cp::knapsack01_bruteforce_at_least_with_counts(items, 4);
    assert(result && result->value == 3);
    assert((result->counts == std::vector<long long>{0, 1, 0}));
}
```

重量を 4 以下に抑える場合は重量 4 の品物が最適。
重量を 4 以上にする場合は重量 5 の品物が最適で、目標を超えてもよい。

### 前提と計算量

- `items` の各要素は `(重量, 価値)` の順の `std::pair`。同じ組が複数あれば、それぞれを一度ずつ選べる。
- `Weight` と `Value` は整数型。負数と 0 を使える。各値と、任意の部分集合の重量和・価値和は `long long` に収まること。
- 空集合の重量和・価値和はともに 0。
- `N` は `items.size()`。追加領域は再帰スタックと、復元付きの場合の結果配列を含めて `O(N + 1)`。

### API概略

| API | 条件・戻り値 | 時間計算量 |
|---|---|---|
| `knapsack01_bruteforce_at_most` | 重量和が上限以下になる最大価値和 | `O(2^N)` |
| `knapsack01_bruteforce_at_least` | 重量和が目標以上になる最小価値和。解なしなら `std::nullopt` | `O(2^N)` |
| `knapsack01_bruteforce_at_most_with_counts` | 上限以下での最大価値和と使用数 | `O(2^N)` |
| `knapsack01_bruteforce_at_least_with_counts` | 目標以上での最小価値和と使用数。解なしなら `std::nullopt` | `O(2^N)` |

### API詳述

#### `knapsack01_bruteforce_at_most`

```text
template <class Weight, class Value>
long long cp::knapsack01_bruteforce_at_most(
    const std::vector<std::pair<Weight, Value>>& items, long long limit_weight);
```

`limit_weight` は 0 以上の重量上限。`items` の部分集合のうち、重量和が `limit_weight` 以下となるものの最大価値和を返す。
空集合は常に条件を満たすため、戻り値は 0 以上になる。時間計算量は `O(2^N)`。

#### `knapsack01_bruteforce_at_least`

```text
template <class Weight, class Value>
std::optional<long long> cp::knapsack01_bruteforce_at_least(
    const std::vector<std::pair<Weight, Value>>& items, long long target_weight);
```

`target_weight` は重量の下限。負数と 0 も指定できる。
`items` の部分集合のうち、重量和が `target_weight` 以上となるものの最小価値和を返す。
条件を満たす部分集合がなければ `std::nullopt` を返す。時間計算量は `O(2^N)`。

#### `knapsack01_bruteforce_at_most_with_counts`・`knapsack01_bruteforce_at_least_with_counts`

```text
template <class Weight, class Value>
cp::knapsack_result cp::knapsack01_bruteforce_at_most_with_counts(
    const std::vector<std::pair<Weight, Value>>& items, long long limit_weight);

template <class Weight, class Value>
std::optional<cp::knapsack_result> cp::knapsack01_bruteforce_at_least_with_counts(
    const std::vector<std::pair<Weight, Value>>& items, long long target_weight);
```

`items` と重量条件の引数は、それぞれ値だけを返す関数と同じ。最適価値和と、各品物の使用数を 0 または 1 で返す。下限版で条件を満たす部分集合がなければ `std::nullopt` を返す。時間計算量はどちらも `O(2^N)`。

## 個数無制限ナップサック

各種類の品物を何個でも選べる場合の価値を求める。
`cp::unbounded_knapsack_chan_he` は、合計重量が指定値ちょうどまたは容量以下の最大価値を求める。
`cp::unbounded_knapsack_min_chan_he` は、合計重量が目標以上の最小価値を求める。
どちらも同じ品物集合を一度前計算し、異なる重量を繰り返し問い合わせられる。最大単品重量が小さく、問い合わせる重量が大きい場合に使える。 [Chan–He §4–5](https://arxiv.org/html/2110.02503#S5)

### 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#include "library/template.hpp"
#include "library/knapsack.hpp"

int main() {
    const std::vector<std::pair<int, long long>> items{{4, 10}, {6, 13}};
    const cp::unbounded_knapsack_chan_he knapsack(items, true);
    assert(knapsack.exact_value(10) == 23);
    assert(!knapsack.exact_value(9));
    assert(knapsack.max_value_at_most(9) == 20);
    assert(knapsack.exact_value(4000000000LL) == 10000000000LL);
    const auto result = knapsack.exact_value_with_counts(10);
    assert(result && result->value == 23);
    assert((result->counts == std::vector<long long>{1, 1}));

    const cp::unbounded_knapsack_min_chan_he min_knapsack(items);
    assert(min_knapsack.min_value_at_least(5) == 13);
}
```

合計重量が 10 ちょうどなら重さ 4 と 6 の品物を一つずつ選ぶ。容量が 9 なら重さ 4 の品物を二つ選べる。
重量を 5 以上にする最小価値は、重さ 6 の品物を一つ選んだときの 13。

### 前提と計算量

- `items` の各要素は `(重量, 価値)` の順の `std::pair`。`Weight` と `Value` は整数型で、各値が `long long` に収まること。
- 重量は正、価値は 0 以上。同じ重さの品物を複数指定できる。
- 問い合わせる重量と容量は 0 以上。空集合の重量和・価値和はともに 0。
- `N` は `items.size()`、`Δ` は最大単品重量とする。空入力では `Δ = 0` とする。
- `a` は価値密度 `価値 / 重量` が、最大化版では最大、最小化版では最小となる品物のうち、最も軽いものの重量とする。
- 前計算の重量上限を `B = (a - 1)Δ` とする。空入力では `B = 0`。保持領域は `O(B + 1)`、構築時のピーク追加領域は `O(B + Δ + 1)`。
- 復元を有効にすると `O(B + 1)` の領域が追加で必要になる。復元時間の `L` は周期分を除いて辿る品物の個数で、`L ≤ B`。結果配列を含む追加領域は `O(N + 1)`。
- `__int128` をサポートする g++ の C++17 環境を使用する。

### API概略

| API | 処理・戻り値 | 時間計算量 |
|---|---|---|
| `unbounded_knapsack_chan_he(items, enable_reconstruction = false)` | 品物集合を前計算する | `O(N + Δ² log(Δ + 1) + 1)` |
| `exact_value(weight)` | 指定重量ちょうどの最大価値。到達不能なら `std::nullopt` | `O(1)` |
| `max_value_at_most(limit_weight)` | 指定容量以下の最大価値 | `O(1)` |
| `exact_value_with_counts(weight)` | 指定重量ちょうどの最大価値と使用数。到達不能なら `std::nullopt` | `O(N + L + 1)` |
| `max_value_at_most_with_counts(limit_weight)` | 指定容量以下の最大価値と使用数 | `O(N + L + log(B + 1) + 1)` |
| `unbounded_knapsack_min_chan_he(items, enable_reconstruction = false)` | 最小化用に品物集合を前計算する | `O(N + Δ² log(Δ + 1) + 1)` |
| `min_value_at_least(target_weight)` | 指定重量以上の最小価値。到達不能なら `std::nullopt` | `O(1)` |
| `min_value_at_least_with_counts(target_weight)` | 指定重量以上の最小価値と使用数。到達不能なら `std::nullopt` | `O(N + L + 1)` |

### API詳述

#### `unbounded_knapsack_chan_he`

```text
template <class Weight, class Value>
explicit unbounded_knapsack_chan_he(
    const std::vector<std::pair<Weight, Value>>& items, bool enable_reconstruction = false);
```

`items` は品物の種類の一覧。品物集合に対する問い合わせを前計算する。空入力も受け付ける。
`enable_reconstruction` は使用数の復元を有効にする指定。
時間計算量は最悪 `O(N + Δ² log(Δ + 1) + 1)`。
必要な表の大きさが表現できない場合は `std::length_error`、前計算で必要な最適価値が `long long` に収まらない場合は `std::overflow_error` を送出する。
領域の確保に失敗すると `std::bad_alloc` が伝わる。

#### `exact_value`

```text
std::optional<long long> exact_value(long long weight) const;
```

`weight` は 0 以上の目標重量。合計重量が `weight` と一致する選び方の最大価値を返す。一致する選び方がなければ `std::nullopt` を返す。
重量 0 の最大価値は 0。空入力で正の重量を問い合わせると `std::nullopt` を返す。
結果が `long long` に収まらない場合は `std::overflow_error` を送出する。時間計算量と追加領域は `O(1)`。

#### `max_value_at_most`

```text
long long max_value_at_most(long long limit_weight) const;
```

`limit_weight` は 0 以上の容量。合計重量が `limit_weight` 以下になる選び方の最大価値を返す。
空集合を選べるため、戻り値は 0 以上。空入力または容量 0 なら 0 を返す。
結果が `long long` に収まらない場合は `std::overflow_error` を送出する。時間計算量と追加領域は `O(1)`。

#### `unbounded_knapsack_min_chan_he`

```text
template <class Weight, class Value>
explicit unbounded_knapsack_min_chan_he(
    const std::vector<std::pair<Weight, Value>>& items, bool enable_reconstruction = false);
```

`items` は品物の種類の一覧。合計重量の下限ごとに最小価値を問い合わせられるように前計算する。空入力も受け付ける。
`enable_reconstruction` は使用数の復元を有効にする指定。
時間計算量は最悪 `O(N + Δ² log(Δ + 1) + 1)`。
必要な表の大きさが表現できない場合は `std::length_error` を送出する。領域の確保に失敗すると `std::bad_alloc` が伝わる。

#### `min_value_at_least`

```text
std::optional<long long> min_value_at_least(long long target_weight) const;
```

`target_weight` は 0 以上の重量下限。合計重量が `target_weight` 以上になる選び方の最小価値を返す。目標重量を超える選び方も含む。
目標重量 0 の最小価値は 0。空入力で正の重量を問い合わせると `std::nullopt` を返す。
最小価値が `long long` に収まらない場合は `std::overflow_error` を送出する。時間計算量と追加領域は `O(1)`。

#### `exact_value_with_counts`・`max_value_at_most_with_counts`・`min_value_at_least_with_counts`

```text
std::optional<cp::knapsack_result> exact_value_with_counts(long long weight) const;
cp::knapsack_result max_value_at_most_with_counts(long long limit_weight) const;
std::optional<cp::knapsack_result> min_value_at_least_with_counts(long long target_weight) const;
```

最初の 2 つは最大化クラス、最後は最小化クラスのメソッド。引数の意味はそれぞれ値だけを返すメソッドと同じ。最適価値和と使用数を返し、完全一致・下限版で解がなければ `std::nullopt` を返す。復元が無効なら `std::logic_error`、最適価値が `long long` に収まらなければ `std::overflow_error` を送出する。
復元時間は完全一致・下限版が `O(N + L + 1)`、上限版が `O(N + L + log(B + 1) + 1)`。

## 個数上限付きナップサック

各種類の品物を指定個数まで選ぶ。`cp::bounded_knapsack_deque` は、合計重量が指定値ちょうどまたは容量以下の最大価値を求める。
`cp::bounded_knapsack_min_deque` は、合計重量が目標以上の最小価値を求める。
どちらも問い合わせの上限が分かっているときに前計算し、その範囲の問い合わせを繰り返せる。
異なる重量の数が品物の種類数より少ない場合は、[重量別にまとめる Axiotis–Tzamos 型 DP](#個数上限付きナップサックの重量別-dp) も使える。

### 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#include "library/template.hpp"
#include "library/knapsack.hpp"

int main() {
    const std::vector<std::tuple<int, long long, int>> items{{4, 10, 1}, {6, 13, 1}};
    const cp::bounded_knapsack_deque knapsack(items, 10, true);
    assert(knapsack.exact_value(10) == 23);
    assert(!knapsack.exact_value(8));
    assert(knapsack.max_value_at_most(9) == 13);
    const auto result = knapsack.max_value_at_most_with_counts(9);
    assert(result.value == 13);
    assert((result.counts == std::vector<long long>{0, 1}));

    const cp::bounded_knapsack_min_deque min_knapsack(items, 5);
    assert(min_knapsack.min_value_at_least(5) == 13);
}
```

重さ 4 の品物は 1 個までなので、重さ 8 ちょうどの問い合わせは `std::nullopt` を返す。
最小化版の構築時に指定した 5 は問い合わせの上限。重量を 5 以上にする最小価値は、重さ 6 の品物を 1 個選んだときの 13。

### 前提と計算量

- `items` の各要素は `(重量, 価値, 個数上限)` の順の `std::tuple`。`Weight`、`Value`、`Count` は整数型で、各値が `long long` に収まること。
- 重量は正、価値と個数上限は 0 以上。空入力を受け付け、空集合の重量和と価値和は 0。
- 構築時の上限と問い合わせる重量は 0 以上。構築時の上限を超える問い合わせでは `std::out_of_range` を送出する。
- `N` は `items.size()`、`W` は構築時の上限とする。保持領域と構築時のピーク追加領域は、ともに最悪 `O(W + 1)`。
- 復元を有効にすると、保持領域と構築時のピーク追加領域は最悪 `O(NW + W + 1)`。復元結果を含む問い合わせ時の追加領域は `O(N + 1)`。
- `__int128` をサポートする g++ の C++17 環境を使用する。

### API概略

| API | 処理・戻り値 | 時間計算量 |
|---|---|---|
| `bounded_knapsack_deque(items, max_capacity, enable_reconstruction = false)` | 最大化用に品物集合を前計算する | `O(N + NW + W + 1)` |
| `exact_value(weight)` | 指定重量ちょうどの最大価値。到達不能なら `std::nullopt` | `O(1)` |
| `max_value_at_most(limit_weight)` | 指定容量以下の最大価値 | `O(1)` |
| `exact_value_with_counts(weight)` | 指定重量ちょうどの最大価値と使用数。到達不能なら `std::nullopt` | `O(N + 1)` |
| `max_value_at_most_with_counts(limit_weight)` | 指定容量以下の最大価値と使用数 | `O(N + 1)` |
| `bounded_knapsack_min_deque(items, max_target_weight, enable_reconstruction = false)` | 最小化用に品物集合を前計算する | `O(N + NW + W + 1)` |
| `min_value_at_least(target_weight)` | 指定重量以上の最小価値。到達不能なら `std::nullopt` | `O(1)` |
| `min_value_at_least_with_counts(target_weight)` | 指定重量以上の最小価値と使用数。到達不能なら `std::nullopt` | `O(N + 1)` |

### API詳述

#### `bounded_knapsack_deque`

```text
template <class Weight, class Value, class Count>
explicit bounded_knapsack_deque(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                long long max_capacity, bool enable_reconstruction = false);
```

`items` は品物の種類の一覧。`max_capacity` は構築時に指定する容量上限で、以降は 0 から `max_capacity` までの重量を問い合わせられる。
`enable_reconstruction` は使用数の復元を有効にする指定。
前計算の時間計算量は最悪 `O(N + NW + W + 1)`。
必要な表の大きさが表現できない場合は `std::length_error`、前計算する重量ちょうどの最大価値が `long long` に収まらない場合は `std::overflow_error` を送出する。
領域の確保に失敗すると `std::bad_alloc` が伝わる。

#### `exact_value`

```text
std::optional<long long> exact_value(long long weight) const;
```

`weight` は 0 以上の目標重量。合計重量が `weight` ちょうどになる選び方の最大価値を返し、到達不能なら `std::nullopt` を返す。重量 0 では 0 を返す。
`weight` が `max_capacity` を超えると `std::out_of_range` を送出する。時間計算量と追加領域は `O(1)`。

#### `max_value_at_most`

```text
long long max_value_at_most(long long limit_weight) const;
```

`limit_weight` は 0 以上の容量。合計重量が `limit_weight` 以下になる選び方の最大価値を返す。空入力または容量 0 では 0 を返す。
`limit_weight` が `max_capacity` を超えると `std::out_of_range` を送出する。時間計算量と追加領域は `O(1)`。

#### `bounded_knapsack_min_deque`

```text
template <class Weight, class Value, class Count>
explicit bounded_knapsack_min_deque(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                    long long max_target_weight, bool enable_reconstruction = false);
```

`items` は品物の種類の一覧。`max_target_weight` は問い合わせる目標重量の上限で、以降は 0 から `max_target_weight` までの目標を問い合わせられる。
`enable_reconstruction` は使用数の復元を有効にする指定。
前計算の時間計算量は最悪 `O(N + NW + W + 1)`。
必要な表の大きさが表現できない場合は `std::length_error` を送出する。領域の確保に失敗すると `std::bad_alloc` が伝わる。

#### `min_value_at_least`

```text
std::optional<long long> min_value_at_least(long long target_weight) const;
```

`target_weight` は 0 以上の重量下限。合計重量が `target_weight` 以上になる選び方の最小価値を返し、条件を満たす選び方がなければ `std::nullopt` を返す。目標重量 0 では 0 を返す。
`target_weight` が `max_target_weight` を超えると `std::out_of_range`、最小価値が `long long` に収まらないと `std::overflow_error` を送出する。時間計算量と追加領域は `O(1)`。

#### `exact_value_with_counts`・`max_value_at_most_with_counts`・`min_value_at_least_with_counts`

```text
std::optional<cp::knapsack_result> exact_value_with_counts(long long weight) const;
cp::knapsack_result max_value_at_most_with_counts(long long limit_weight) const;
std::optional<cp::knapsack_result> min_value_at_least_with_counts(long long target_weight) const;
```

最初の 2 つは最大化クラス、最後は最小化クラスのメソッド。引数の意味と範囲はそれぞれ値だけを返すメソッドと同じ。最適価値和と使用数を返し、完全一致・下限版で解がなければ `std::nullopt` を返す。復元が無効なら `std::logic_error`、構築時の上限を超えると `std::out_of_range`、最小価値が `long long` に収まらないと `std::overflow_error` を送出する。
復元時間は `O(N + 1)`。

## 個数上限付きナップサックの重量別 DP

`cp::bounded_knapsack_axiotis_tzamos` は、合計重量が指定値ちょうどまたは容量以下の最大価値を求める。
`cp::bounded_knapsack_min_axiotis_tzamos` は、合計重量が目標以上の最小価値を求める。
同じ重さの品物を一つのグループにまとめるので、重さの種類数が少なく、問い合わせの重量範囲が表に収まるときに使える。
問い合わせる重量が大きく、最大単品重量が小さい場合は、[単発差分 DP](#個数上限付きナップサックの単発差分-dp)や[圧縮表](#個数上限付きナップサックの圧縮表)も使える。
重量別のグループを凹畳み込みで統合する手法に基づく。 [Axiotis–Tzamos 論文の第 3.1 節](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ICALP.2019.19) · [noshi91 の 0/1 実装](https://noshi91.github.io/Library/algorithm/axiotis_tzamos_knapsack.cpp) · [個数上限付きへの適用（Polak–Rohwedder–Węgrzycki、補題 2.2）](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ICALP.2021.106)

### 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#include "library/template.hpp"
#include "library/knapsack.hpp"

int main() {
    const std::vector<std::tuple<int, long long, int>> items{{3, 8, 1}, {3, 5, 2}, {4, 7, 1}};
    const cp::bounded_knapsack_axiotis_tzamos maximum(items, 7, true);
    assert(maximum.exact_value(6) == 13);
    const auto best = maximum.max_value_at_most_with_counts(7);
    assert(best.value == 15);
    assert((best.counts == std::vector<long long>{1, 0, 1}));

    const cp::bounded_knapsack_min_axiotis_tzamos minimum(items, 6, true);
    const auto cheapest = minimum.min_value_at_least_with_counts(6);
    assert(cheapest && cheapest->value == 10);
    assert((cheapest->counts == std::vector<long long>{0, 2, 0}));
}
```

重量 6 ちょうどで最大化するには、重さ 3 の品物を各種類から 1 個ずつ選ぶ。重量 6 以上で最小化するには、価値 5 の品物を 2 個選ぶ。

### 前提と計算量

- `items` の各要素は `(重量, 価値, 個数上限)` の順の `std::tuple`。各型は整数型で、値は `long long` に収まること。重量は正、価値と個数上限は 0 以上。
- 空入力を受け付ける。空集合の重量和と価値和は 0。構築時の上限と問い合わせ重量は 0 以上。構築時の上限を超える問い合わせでは `std::out_of_range` を送出する。
- `N` は `items.size()`、`T` は構築時に指定した重量上限、`W` は実際の重量表の上限とする。品物 `i` の重量を `w_i`、個数上限を `c_i` とする。最大化では `c'_i = min(c_i, floor(T / w_i))` として `W = min(T, Σ w_i c'_i)`。最小化では `W = min(T, Σ w_i c_i)`。
- `D` は処理する重量グループの数。最大化では `c'_i > 0` の品物の異なる重量数、最小化では `c_i > 0` の品物の `min(w_i, T)` の異なる値の数とする。`W = 0` では `D = 0`。重量 `w` のグループでは、重量表に必要な個数までの累積価値だけを使う。最大化では高々 `floor(W / w)` 個、最小化では高々 `ceil(W / w)` 個で、どちらも在庫数が上限。
- 復元を無効にした場合、保持領域は `O(W + 1)`、構築時のピーク追加領域は `O(N + W + 1)`。復元を有効にすると、`O(N + D(W + 1) + 1)` の領域を追加で保持する。復元結果を含む問い合わせ時の追加領域は `O(N + 1)`。
- `__int128` をサポートする g++ の C++17 環境を使用する。

### API概略

| API | 処理・戻り値 | 時間計算量 |
|---|---|---|
| `bounded_knapsack_axiotis_tzamos(items, max_capacity, enable_reconstruction = false)` | 最大化用に品物集合を前計算する | `O(N + N log(N + 1) + (D + 1)(W + 1))` |
| `exact_value(weight)` | 指定重量ちょうどの最大価値。到達不能なら `std::nullopt` | `O(1)` |
| `max_value_at_most(limit_weight)` | 指定容量以下の最大価値 | `O(1)` |
| `exact_value_with_counts(weight)` | 指定重量ちょうどの最大価値と使用数。到達不能なら `std::nullopt` | `O(N + 1)` |
| `max_value_at_most_with_counts(limit_weight)` | 指定容量以下の最大価値と使用数 | `O(N + 1)` |
| `bounded_knapsack_min_axiotis_tzamos(items, max_target_weight, enable_reconstruction = false)` | 最小化用に品物集合を前計算する | `O(N + N log(N + 1) + (D + 1)(W + 1))` |
| `min_value_at_least(target_weight)` | 指定重量以上の最小価値。到達不能なら `std::nullopt` | `O(1)` |
| `min_value_at_least_with_counts(target_weight)` | 指定重量以上の最小価値と使用数。到達不能なら `std::nullopt` | `O(N + 1)` |

### API詳述

#### `bounded_knapsack_axiotis_tzamos`・`bounded_knapsack_min_axiotis_tzamos`

```text
template <class Weight, class Value, class Count>
explicit bounded_knapsack_axiotis_tzamos(
    const std::vector<std::tuple<Weight, Value, Count>>& items,
    long long max_capacity, bool enable_reconstruction = false);

template <class Weight, class Value, class Count>
explicit bounded_knapsack_min_axiotis_tzamos(
    const std::vector<std::tuple<Weight, Value, Count>>& items,
    long long max_target_weight, bool enable_reconstruction = false);
```

`items` は品物の種類の一覧。最大化版は `max_capacity` 以下の容量と完全一致重量、最小化版は `max_target_weight` 以下の目標重量について、最適価値を前計算する。`enable_reconstruction` は使用数の復元を有効にする指定。
構築時間はどちらも最悪 `O(N + N log(N + 1) + (D + 1)(W + 1))`。
必要な表の大きさが表現できない場合は `std::length_error`、領域の確保に失敗すると `std::bad_alloc` が伝わる。最大化版は前計算する重量ちょうどの最大価値が `long long` に収まらない場合に `std::overflow_error` を送出する。

#### `exact_value`・`max_value_at_most`・`min_value_at_least`

```text
std::optional<long long> exact_value(long long weight) const;
long long max_value_at_most(long long limit_weight) const;
std::optional<long long> min_value_at_least(long long target_weight) const;
```

最初の 2 つは最大化クラスのメソッド。`exact_value` は `weight` ちょうどの最大価値を返し、`max_value_at_most` は `limit_weight` 以下の最大価値を返す。最後は最小化クラスのメソッド。`min_value_at_least` は `target_weight` 以上の最小価値を返し、目標重量を超える選び方も含む。
引数は 0 以上で、0 なら 0 を返す。完全一致・下限版で解がなければ `std::nullopt` を返す。容量以下の最大化では常に空集合を選べる。
構築時の上限を超えると `std::out_of_range`、最小価値が `long long` に収まらないと `std::overflow_error` を送出する。時間計算量と追加領域は `O(1)`。

#### `exact_value_with_counts`・`max_value_at_most_with_counts`・`min_value_at_least_with_counts`

```text
std::optional<cp::knapsack_result> exact_value_with_counts(long long weight) const;
cp::knapsack_result max_value_at_most_with_counts(long long limit_weight) const;
std::optional<cp::knapsack_result> min_value_at_least_with_counts(long long target_weight) const;
```

引数の意味と所属クラスはそれぞれ値だけを返すメソッドと同じ。最適価値と入力順の使用数を返す。完全一致・下限版で解がなければ `std::nullopt` を返す。
復元が無効なら `std::logic_error` を送出する。復元が有効な場合の重量範囲と価値の例外は、値だけを返すメソッドと同じ。時間計算量と追加領域は `O(N + 1)`。

## 個数上限付きナップサックの単発差分 DP

呼び出しごとに、指定された容量や目標重量の最適値を求める。価値密度順の貪欲解を基準に、重量が `O(Δ²)` の範囲で品物の追加と除去を計算する。最大単品重量 `Δ` が小さく、実効容量が `Δ²` より大きいときに、容量まで重量表を作る [重量別 DP](#個数上限付きナップサックの重量別-dp) と使い分けられる。同じ品物について大きく離れた重量を繰り返し問い合わせる場合は、[圧縮表](#個数上限付きナップサックの圧縮表)を使える。 [Polak–Rohwedder–Węgrzycki 論文の第 2–3 節](https://drops.dagstuhl.de/entities/document/10.4230/LIPIcs.ICALP.2021.106)

### 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#include "library/template.hpp"
#include "library/knapsack.hpp"

int main() {
    const std::vector<std::tuple<int, long long, int>> items{{3, 8, 1}, {4, 10, 2}, {5, 11, 1}};
    assert(cp::bounded_knapsack_proximity_exact(items, 8) == 20);
    assert(!cp::bounded_knapsack_proximity_exact(items, 1));
    assert(cp::bounded_knapsack_proximity_at_most(items, 8) == 20);
    const auto result = cp::bounded_knapsack_proximity_at_least_with_counts(items, 8);
    assert(result && result->value == 19);
    assert((result->counts == std::vector<long long>{1, 0, 1}));
}
```

容量 8 の最大価値は、重さ 4 の品物を 2 個選んだときの 20。重量 8 以上で最小化するには、重さ 3 と 5 の品物を 1 個ずつ選ぶ。

### 前提と計算量

- `items` の各要素は `(重量, 価値, 個数上限)` の順の `std::tuple`。`Weight`、`Value`、`Count` は整数型で、各値は `long long` に収まること。重量は正、価値と個数上限は 0 以上。
- 問い合わせ重量は 0 以上。個数上限 0 と空入力を受け付ける。空集合の重量和と価値和はともに 0。全在庫の重量和と価値和は、それぞれ符号付き `__int128` に収まること。
- `N` は `items.size()`、`n` は個数上限が正の品物の種類数、`D` はその品物の異なる重量の数、`Δ` はその中の最大単品重量。`n = 0` の場合は `D = Δ = 0`。
- `F = N + n log(n + 1) + (D + 1)(Δ² + 1)` とすると、1 回の呼び出しの最悪時間は `O(F)`。値だけを返す場合の作業領域は `O(n + Δ² + 1)`。使用数を返す場合は結果配列を含め `O((D + 1)(Δ² + 1) + N + 1)` の領域を追加で使う。
- `__int128` をサポートする g++ の C++17 環境を使用する。

### API概略

| API | 処理・戻り値 | 時間計算量 |
|---|---|---|
| `bounded_knapsack_proximity_exact` | 指定重量ちょうどの最大価値。到達不能なら `std::nullopt` | `O(F)` |
| `bounded_knapsack_proximity_at_most` | 指定容量以下の最大価値 | `O(F)` |
| `bounded_knapsack_proximity_at_least` | 指定重量以上の最小価値。到達不能なら `std::nullopt` | `O(F)` |
| `bounded_knapsack_proximity_exact_with_counts` | 指定重量ちょうどの最大価値と使用数。到達不能なら `std::nullopt` | `O(F)` |
| `bounded_knapsack_proximity_at_most_with_counts` | 指定容量以下の最大価値と使用数 | `O(F)` |
| `bounded_knapsack_proximity_at_least_with_counts` | 指定重量以上の最小価値と使用数。到達不能なら `std::nullopt` | `O(F)` |

### API詳述

#### `bounded_knapsack_proximity_exact`・`bounded_knapsack_proximity_at_most`・`bounded_knapsack_proximity_at_least`

```text
template <class Weight, class Value, class Count>
std::optional<long long> cp::bounded_knapsack_proximity_exact(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long weight);

template <class Weight, class Value, class Count>
long long cp::bounded_knapsack_proximity_at_most(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long limit_weight);

template <class Weight, class Value, class Count>
std::optional<long long> cp::bounded_knapsack_proximity_at_least(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long target_weight);
```

`items` は品物の種類の一覧。完全一致版は `weight` ちょうどの最大価値、上限版は `limit_weight` 以下の最大価値、下限版は `target_weight` 以上の最小価値を返す。下限版では全在庫から除く品物を選び、残す品物の価値を求める。
重量の引数が 0 の場合はどれも 0 を返す。完全一致・下限版で解がなければ `std::nullopt` を返す。上限版では常に空集合を選べる。
全在庫の重量和・価値和が符号付き `__int128` に収まらない場合、または返す最適価値が `long long` に収まらない場合は `std::overflow_error` を送出する。表の大きさが表現できない場合は `std::length_error`、領域の確保に失敗すると `std::bad_alloc` が伝わる。時間計算量はどれも `O(F)`。

#### `bounded_knapsack_proximity_exact_with_counts`・`bounded_knapsack_proximity_at_most_with_counts`・`bounded_knapsack_proximity_at_least_with_counts`

```text
template <class Weight, class Value, class Count>
std::optional<cp::knapsack_result> cp::bounded_knapsack_proximity_exact_with_counts(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long weight);

template <class Weight, class Value, class Count>
cp::knapsack_result cp::bounded_knapsack_proximity_at_most_with_counts(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long limit_weight);

template <class Weight, class Value, class Count>
std::optional<cp::knapsack_result> cp::bounded_knapsack_proximity_at_least_with_counts(
    const std::vector<std::tuple<Weight, Value, Count>>& items, long long target_weight);
```

引数の意味、解がない場合の戻り値、例外は値だけを返す関数と同じ。最適価値和と、入力順の使用数を返す。時間計算量はどれも `O(F)`。

## 個数上限付きナップサックの圧縮表

`cp::bounded_knapsack_compressed` は、各種類の品物を指定個数まで選べる場合の価値を求める。
同じ品物集合について、合計重量が指定値ちょうどまたは容量以下の最大価値と、目標以上の最小価値を問い合わせられる。
最大単品重量が小さく、個数上限や問い合わせる重量が大きい場合に使える。構築時に問い合わせ上限を指定する必要はない。

### 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/knapsack.hpp
#include "library/template.hpp"
#include "library/knapsack.hpp"

int main() {
    const std::vector<std::tuple<int, int, long long>> items{{4, 7, 1000000000LL}};
    const cp::bounded_knapsack_compressed knapsack(items, true);
    assert(knapsack.exact_value(4000000000LL) == 7000000000LL);
    assert(!knapsack.exact_value(3999999999LL));
    assert(knapsack.max_value_at_most(3999999999LL) == 6999999993LL);
    assert(knapsack.min_value_at_least(3999999999LL) == 7000000000LL);
    assert(!knapsack.min_value_at_least(4000000001LL));
    const auto result = knapsack.min_value_at_least_with_counts(3999999999LL);
    assert(result && result->value == 7000000000LL);
    assert((result->counts == std::vector<long long>{1000000000LL}));
}
```

重さ 4 の品物が 10 億個ある。合計重量を 3,999,999,999 以下にするには 1 個残す必要があり、同じ重量以上にするには全品物を選ぶ。

### 前提と計算量

- `items` の各要素は `(重量, 価値, 個数上限)` の順の `std::tuple`。`Weight`、`Value`、`Count` は整数型で、各値が `long long` に収まること。
- 重量は正、価値と個数上限は 0 以上。個数上限 0 と空入力を受け付ける。空集合の重量和・価値和はともに 0。
- 問い合わせる重量は 0 以上。全在庫を選んだときの重量和・価値和は、それぞれ符号付き `__int128` に収まること。
- `N` は `items.size()`、`n` は個数上限が正の種類数、`Δ` はその中の最大単品重量とする。`n = 0` のとき `Δ = 0`。
- 保持領域と構築時のピーク追加領域は、ともに最悪 `O(nΔ² + n + 1)`。
- 復元を有効にすると、同じオーダーの領域が追加で必要になる。復元結果を含む問い合わせ時の追加領域は `O(N + 1)`。
- `__int128` をサポートする g++ の C++17 環境を使用する。

### API概略

| API | 処理・戻り値 | 時間計算量 |
|---|---|---|
| `bounded_knapsack_compressed(items, enable_reconstruction = false)` | 品物集合を前計算する | `O(N + nΔ² log(n + 1) + 1)` |
| `exact_value(weight)` | 指定重量ちょうどの最大価値。到達不能なら `std::nullopt` | `O(log(n + 1) + 1)` |
| `max_value_at_most(limit_weight)` | 指定容量以下の最大価値 | `O(log(n + 1) + 1)` |
| `min_value_at_least(target_weight)` | 指定重量以上の最小価値。到達不能なら `std::nullopt` | `O(log(n + 1) + 1)` |
| `exact_value_with_counts(weight)` | 指定重量ちょうどの最大価値と使用数。到達不能なら `std::nullopt` | `O(N + 1)` |
| `max_value_at_most_with_counts(limit_weight)` | 指定容量以下の最大価値と使用数 | `O(N + 1)` |
| `min_value_at_least_with_counts(target_weight)` | 指定重量以上の最小価値と使用数。到達不能なら `std::nullopt` | `O(N + 1)` |

### API詳述

#### `bounded_knapsack_compressed`

```text
template <class Weight, class Value, class Count>
explicit bounded_knapsack_compressed(const std::vector<std::tuple<Weight, Value, Count>>& items,
                                     bool enable_reconstruction = false);
```

`items` は品物の種類の一覧。各重量条件を問い合わせられるように前計算する。時間計算量は最悪 `O(N + nΔ² log(n + 1) + 1)`。
`enable_reconstruction` は使用数の復元を有効にする指定。
全在庫の重量和または価値和が符号付き `__int128` に収まらない場合は `std::overflow_error`、必要な表の大きさが表現できない場合は `std::length_error` を送出する。
領域の確保に失敗すると `std::bad_alloc` が伝わる。

#### `exact_value`

```text
std::optional<long long> exact_value(long long weight) const;
```

`weight` は 0 以上の目標重量。合計重量が `weight` ちょうどになる選び方の最大価値を返す。条件を満たす選び方がなければ `std::nullopt` を返す。重量 0 では 0 を返す。
最大価値が `long long` に収まらない場合は `std::overflow_error` を送出する。時間計算量は `O(log(n + 1) + 1)`、追加領域は `O(1)`。

#### `max_value_at_most`

```text
long long max_value_at_most(long long limit_weight) const;
```

`limit_weight` は 0 以上の容量。合計重量が `limit_weight` 以下になる選び方の最大価値を返す。空入力または容量 0 では 0 を返す。
最大価値が `long long` に収まらない場合は `std::overflow_error` を送出する。時間計算量は `O(log(n + 1) + 1)`、追加領域は `O(1)`。

#### `min_value_at_least`

```text
std::optional<long long> min_value_at_least(long long target_weight) const;
```

`target_weight` は 0 以上の重量下限。合計重量が `target_weight` 以上になる選び方の最小価値を返し、条件を満たす選び方がなければ `std::nullopt` を返す。目標重量 0 では 0 を返す。
最小価値が `long long` に収まらない場合は `std::overflow_error` を送出する。時間計算量は `O(log(n + 1) + 1)`、追加領域は `O(1)`。

#### `exact_value_with_counts`・`max_value_at_most_with_counts`・`min_value_at_least_with_counts`

```text
std::optional<cp::knapsack_result> exact_value_with_counts(long long weight) const;
cp::knapsack_result max_value_at_most_with_counts(long long limit_weight) const;
std::optional<cp::knapsack_result> min_value_at_least_with_counts(long long target_weight) const;
```

引数の意味はそれぞれ値だけを返すメソッドと同じ。最適価値和と使用数を返し、完全一致・下限版で解がなければ `std::nullopt` を返す。復元が無効なら `std::logic_error`、最適価値が `long long` に収まらなければ `std::overflow_error` を送出する。
復元時間は `O(N + 1)`。
