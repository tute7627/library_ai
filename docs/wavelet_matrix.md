# cp::wavelet_matrix / cp::compressed_wavelet_matrix

固定した値列について、区間のk番目の値、値域内の個数を求める。
必要な場合だけ `build_weights` で重み列を追加し、値域内の重みの集約と、
集約値が条件を満たす値の昇順・降順探索も利用できる。
通常版は整数を扱い、問い合わせごとに値へのxorを指定できる。
座圧版は大小関係を定義できる型を扱え、大きく疎な整数のほか `pair` や文字列なども使える。
一般座標をindexとして使い、同一座標に複数要素を置く場合は
[indexed_wavelet_matrix](indexed_wavelet_matrix.md) を利用できる。

共通ヘッダ `library/template.hpp` を先に読み込む。
値列・重み列の要素更新には対応しない。重み列全体の作り直しは可能。
順位選択・先頭k個の集約・境界探索は、複数の添字区間をまとめて対象にするオーバーロードも持つ。

## 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

int main() {
    std::vector<int> a{-5, 1, 4, 1, 9};
    cp::wavelet_matrix<int> wm(a);
    assert(wm.kth_smallest(0, 5, 3) == 4);
    assert(wm.kth_largest(0, 5, 1) == 4);
    assert(wm.range_freq(0, 5, 1, 5) == 3);

    wm.build_weights({10, 2, 3, 5, 7});
    assert(wm.range_fold(0, 5, 1, 5) == 10);
    assert(wm.fold_smallest(0, 5, 3) == 17);
    assert(wm.fold_largest(0, 5, 2) == 10);
    assert(wm.range_freq(0, 5, 3, 7, 2U) == 3);
    assert(wm.range_fold(0, 5, 3, 7, 2U) == 10);
    auto value = wm.first_value(0, 5, 1, [](long long sum) { return sum >= 8; });
    assert(value);
    const auto [boundary, count, folded] = *value;
    assert(boundary == 4 && count == 3 && folded == 10);
    auto reverse = wm.last_value(0, 5, 5, [](long long sum) { return sum >= 8; });
    assert(reverse && reverse->value == 1 && reverse->count == 3 && reverse->fold == 10);

    cp::compressed_wavelet_matrix<long long> compressed({-1000000000000LL, 7, 7});
    compressed.build_weights({2, 3, 4});
    assert(compressed.range_fold(0, 3, 0, 8) == 7);
    cp::compressed_wavelet_matrix<int> ties({10, 10, 10});
    ties.build_weights({10, 10, 10});
    const auto stop = ties.first_value(0, 3, 0,
        [](long long sum) { return sum > 15; }, false);
    assert(stop && stop->value == 10 && stop->count == 2 && stop->fold == 20);
    const auto all = ties.first_value(0, 3, 0,
        [](long long sum) { return sum > 15; });
    assert(all && all->value == 10 && all->count == 3 && all->fold == 30);
}
```

昇順の探索は、値1の重みが合計7、値4まで含めると10になるため `{4,3,10}` を返す。
降順の探索は、上限5未満の値4の重みが3、値1まで含めると10になるため `{1,3,10}` を返す。
結果構造体の `value` は境界値、`count` は取り込んだ要素数、`fold` は重みの集約値。
`take_all_equal` は既定でtrueで、境界と同じ値の要素をすべて含める。
falseを指定すると、同値の途中でも条件を初めて満たした要素で止める。
xor引数は値だけに適用され、添字や重みは変化しない。

結果を `result` に受け取ったとき、`if (result)` の中で
`result->value`・`result->count`・`result->fold` で参照できる。
`auto [value, count, folded] = *result;` と分解することもできる。
集約値は、既定の群では総和、独自の群ではその群の演算結果を表す。
false指定時の同値内の順序は `fold_smallest`・`fold_largest` と同じで、最初に条件を満たす要素数を返す。
通常版では `wm.first_value(l,r,lower,predicate,false,xor_mask)` の順に指定する。
フラグには `true`・`false` またはbool変数を渡す。整数引数は従来のxorマスク指定として扱う。

座圧版は、値に `pair` を使う場合も同じAPIで問い合わせる。
次の値域 `[{1,2},{2,0})` は辞書順で、`{1,4}` も含む。

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

int main() {
    using Key = std::pair<int, int>;
    cp::compressed_wavelet_matrix<Key> wm({{2, 0}, {1, 4}, {1, 2}, {1, 2}});
    const Key lower{1, 2}, upper{2, 0}, middle{1, 4};
    assert(wm.kth_smallest(0, 4, 2) == middle);
    assert(wm.range_freq(0, 4, lower, upper) == 3);
    wm.build_weights({3, 1, 2, 4});
    assert(wm.range_fold(0, 4, lower, upper) == 7);
    const auto result = wm.first_value(0, 4, lower,
        [](long long sum) { return sum >= 7; });
    assert(result && result->value == middle && result->count == 3 && result->fold == 7);
}
```

## 型と前提条件

書式:

```text
cp::wavelet_matrix<Key, Weight = long long, Group = cp::wavelet_sum_group<Weight>>
cp::compressed_wavelet_matrix<Key, Weight = long long, Group = cp::wavelet_sum_group<Weight>>
```

- 通常版の `Key` はboolを除く64bit以下の整数型。符号付き・符号なしのどちらも使える。
- 座圧版の `Key` はコピー構築・コピー代入可能で、`operator<` が厳密弱順序を満たす型。
  デフォルト構築は不要。`std::pair`、`std::tuple`、`std::string`、独自型も使える。
  `==`・`!=`・`<=`・`>`・`>=` は不要。比較結果は構築後も変化しないこと。
  浮動小数点を使う場合、NaNを含めないこと。
  `pair` と `tuple` の標準の比較は辞書順で、値域もその順序に従う。
- 要素数は `INT_MAX` 以下。添字と単一区間の順位・指定個数は `int`。
  複数区間の順位・指定個数と、境界探索が返す個数は `std::int64_t`。
- C++17、`<bits/stdc++.h>`、GCC互換の `__builtin_popcountll` が使える環境を前提とする。
- `Weight` はコピー構築・ムーブ構築・コピー代入・ムーブ代入可能な型。
  デフォルト構築は不要。重みを使わない場合、重みの演算は実体化されない。
- `Group` は `static Weight identity()`、
  `static Weight op(const Weight&, const Weight&)`、
  `static Weight inverse(const Weight&)` を提供する。
  `op` は結合的・可換で、`identity()` は単位元、`inverse(x)` は逆元であること。
  演算は決定的で、引数を変更しないこと。
- 既定の `wavelet_sum_group<Weight>` は `Weight{0}`、`a + b`、`-a` を使用する。
  min/maxやOR/ANDのように逆元のない演算はこの実装の対象外。
- すべての重みの中間演算が型の範囲に収まること。
  符号付き加算のオーバーフローだけでなく、最小値の符号反転にも注意する。
  符号なし整数の既定演算は、その型のビット幅を法とする加算になる。
- 添字区間は常に `0 <= l <= r <= size()` の半開区間。
  `kth_smallest` と `kth_largest` 以外は空区間を受け付ける。
- 重みの問い合わせは `build_weights` 完了後に使用する。
  添字・k・重み列の長さ・構築済み条件の違反は `assert` の対象で、例外ではない。

公開型は `key_type = Key`、`weight_type = Weight`、`size_type = int`、
`range_type = std::pair<int, int>`、
`search_result_type = cp::wavelet_search_result<Key, Weight>`。
通常版だけ `mask_type = std::make_unsigned_t<Key>` も公開する。
探索の個数は、複数区間では重複込みの総要素数が `INT_MAX` を超え得るため `std::int64_t`。
単一区間でも同じ戻り値型を使う。

通常版のxorマスクは `mask_type`。
値を同じ幅の符号なし整数に変換してxorしたビット列を、再び `Key` の値として解釈する。
符号付き型では、最上位ビットが1の結果を負数として扱い、通常の符号付きの大小関係で比較する。
最上位ビットを切り替えるマスクも使用できる。
`kth_smallest`・`kth_largest` と、`first_value`・`last_value` の `value` フィールドはxor後の値を返す。

座圧版の引数と戻り値は常に元の型の値。
同値は `!(a < b) && !(b < a)` で判定し、比較上同値な値は同じ値として扱う。
順位選択・境界探索の戻り値は、その同値類から構築時に保存した代表値。
代表値は入力列のいずれかの値で、問い合わせ区間内の実体と一致する保証はない。
同値でも各要素と重みは個別に保持し、`fold_smallest` などの同値内の選択順は変わらない。
座圧版の値域の端点 `lower`・`upper` は `const Key&` で受け取る。
元の値へのxorは座圧によって大小関係を保てないため、座圧版にはxor引数がない。

## 計算量と領域

`N = size()`、`σ` は異なる値の数とする。座圧版では比較上の同値類の数を数える。
通常版では、値を符号順に並ぶ符号なし整数へ対応させた最小値・最大値の、
異なる最上位ビットまでの幅を `B` とする。全要素が等しい場合と空列では `B = 0`。
共通する上位ビットは段数に含めず、大きな共通部分がある値列でも小さな段数になる。

座圧版では `B = ceil(log2 σ)` とし、`σ <= 1` では0とする。
以下で `H = B + 1`、`L = 1 + ceil(log2(σ + 1))`、`D = 1 + ceil(log2(N + 1))` とする。
`W` は通常版では `Key` のビット幅、座圧版では32とする。いずれも最大64。
複数区間版では `R = ranges.size()`、`M = Σ(r-l)` とする。
`M` は区間の重複分も含む総要素数で、`INT64_MAX` 以下であること。
型の比較・演算・コピー・ムーブ・破棄、重み演算、述語評価は1回 `O(1)` とする。
文字列などでこれらのコストが要素の長さに依存する場合は、その実際のコストも加味する。

通常版の保持領域は、重みなしで `O(1 + B ceil(N/64))` 個の固定幅ワードと `O(B)` の管理情報。
重み構築後は追加で `(N + 1)H` 個の重みを保持する。
座圧版は、さらに `O(N)` 個の `Key` 要素の領域を保持する。
文字列などの `Key` 要素が別途確保する領域は、その要素の保持コストとして加算する。
重みを構築しない限り、重み配列は確保しない。

単一区間の問い合わせの追加領域は通常 `O(1)`。
単一区間の `first_value` と `last_value` は `O(W)` の固定作業領域を使う。
複数区間版の順位選択・先頭k個の集約・境界探索は `O(R)` の作業領域を使う。
値渡し引数に移動したvector自身の既存の確保容量は、この追加領域に含めない。
構築・重み構築の作業領域は `O(N + H)` で、構築される保持領域は別に必要。
重みの再構築中は、旧重みデータと新重みデータを同時に保持する。

領域確保・キーのコピーや比較・重みの操作・述語で生じた例外は呼び出し元へ伝わる。
公開APIは内部要素への参照・イテレータを返さない。戻り値は独立した値である。
コピーは保持データを複製し、その領域に比例した時間・領域を使う。
ムーブ後の移動元は破棄・代入だけを行うこと。

## API概略

| API | 内容 | 通常版の時間 | 座圧版の時間 |
|---|---|---|---|
| デフォルト構築 | 空列を構築 | `O(1)` | `O(1)` |
| 値列から構築 | 値の問い合わせを準備 | `O(1 + NH)` | `O(1 + N log(N+1))` |
| `size / empty / has_weights` | 状態取得 | `O(1)` | `O(1)` |
| `kth_smallest / kth_largest` | 昇順・降順のk番目の値 | `O(H)` | `O(H)` |
| `range_freq` | 値域内の個数 | `O(H)` | `O(L + H)` |
| `build_weights` | 重みデータを構築・交換 | `O(1 + (N+1)H)` | `O(1 + (N+1)H)` |
| `range_fold(l, r)` | 添字区間全体の重み集約 | `O(1)` | `O(1)` |
| `range_fold(l, r, lower, upper)` | 値域内の重み集約 | `O(H)` | `O(L + H)` |
| `fold_smallest / fold_largest` | 昇順・降順の先頭k個の重み集約 | `O(H)` | `O(H)` |
| `first_value` | 昇順の境界値・取り込んだ個数・集約値 | `O(H)` | `O(L + H)` |
| `last_value` | 降順の境界値・取り込んだ個数・集約値 | `O(H)` | `O(L + H)` |
| `kth_smallest / kth_largest(ranges, k)` | 複数区間をまとめたk番目の値 | `O(1 + RH)` | `O(1 + RH)` |
| `fold_smallest / fold_largest(ranges, k)` | 複数区間をまとめた先頭k個の重み集約 | `O(1 + RH)` | `O(1 + RH)` |
| `first_value(ranges, lower, predicate)` | 複数区間をまとめた昇順探索 | `O(1 + RH)` | `O(L + RH)` |
| `last_value(ranges, upper, predicate)` | 複数区間をまとめた降順探索 | `O(1 + RH)` | `O(L + RH)` |

表の探索系は `take_all_equal=true` の計算量。false指定時はAPI詳述を参照。
いずれも最悪計算量であり、期待・償却計算量ではない。
`range_freq` と値域付き `range_fold` は空区間または `!(lower < upper)` の場合、
通常版で `O(1)`。座圧版は `!(lower < upper)` なら `O(1)`。

## 複数区間の共通仕様と使用例

`vector<range_type>` で半開区間の組 `{l,r}` を渡す。
すべての要素をまとめた多重集合を対象とする。
`fold_smallest`・`fold_largest` と、false指定の `first_value`・`last_value` が同値の途中で止まる場合には、
区間の順序が選ばれる重み・個数・集約値に影響する。
区間をソート・結合する必要はなく、空区間も指定できる。
重複・包含する区間では、同じ添字の要素と重みを出現回数だけ数える。
和集合として一度ずつ扱いたい場合は、呼び出し前に区間を結合すること。

区間配列は値渡しで受け取るため、lvalueを渡した元の配列は変更されない。
`std::move(ranges)` を渡せば、引数受け取り時のコピーを省ける。
区間の各端点は `0 <= l <= r <= size()` を満たすこと。
`kth_smallest`・`kth_largest` の順位は `std::int64_t` で、`0 <= k < M` が必要。
`fold_smallest`・`fold_largest` の指定個数も `std::int64_t` で、`0 <= k <= M` が必要。

複数区間版の重み探索でも `build_weights` が必要。
全区間の要素をまとめて値順に取り込む。既定のtrue指定では同じ値を全区間分まとめて取り込み、
値単位で条件が初めて成立する境界を求める。この値単位での条件が単調であること。
false指定では同値内も区間の出現順、各区間内の添字順で1個ずつ取り込み、
初めて条件が成立する時点で止める。こちらは要素単位で条件が単調であること。
どちらも単位元でfalseが必要。区間ごとの探索結果を集める操作ではない。
重みの中間演算には区間の重複分も含め、型の範囲に収まること。

区間配列が空、またはすべて空区間なら、`first_value` と `last_value` は
`nullopt` を返す。重み構築済みと単位元でfalseという条件は、この場合も必要。
`kth_smallest`・`kth_largest` は総要素数0の場合には使用できない。
`fold_smallest`・`fold_largest` は `k = 0` で単位元を返す。
通常版は全区間に共通のxorマスクを指定でき、座圧版はxor引数なし。

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

int main() {
    std::vector<int> a{5, 1, 4, 1, 9};
    std::vector<long long> w{10, 2, 3, 5, 7};
    std::vector<std::pair<int, int>> ranges{{0, 2}, {3, 5}};
    cp::wavelet_matrix<int> wm(a);
    assert(wm.kth_smallest(ranges, 2) == 5);
    assert(wm.kth_smallest(ranges, 2, 2U) == 7);
    wm.build_weights(w);
    const auto predicate = [](long long sum) { return sum >= 8; };
    const auto first = wm.first_value(ranges, 1, predicate);
    assert(first && first->value == 5 && first->count == 3 && first->fold == 17);
    const auto last = wm.last_value(ranges, 10, predicate);
    assert(last && last->value == 5 && last->count == 2 && last->fold == 17);
    const auto first_xor = wm.first_value(ranges, 3, predicate, 2U);
    assert(first_xor && first_xor->value == 7 && first_xor->count == 3 && first_xor->fold == 17);
    const auto last_xor = wm.last_value(ranges, 12, predicate, 2U);
    assert(last_xor && last_xor->value == 7 && last_xor->count == 2 && last_xor->fold == 17);

    cp::compressed_wavelet_matrix<int> compressed(a);
    compressed.build_weights(w);
    assert(compressed.kth_smallest(ranges, 2) == 5);
    const auto compressed_first = compressed.first_value(ranges, 1, predicate);
    assert(compressed_first && compressed_first->value == 5 &&
           compressed_first->count == 3 && compressed_first->fold == 17);
    const auto compressed_last = compressed.last_value(ranges, 10, predicate);
    assert(compressed_last && compressed_last->value == 5 &&
           compressed_last->count == 2 && compressed_last->fold == 17);
    assert(compressed.fold_smallest(ranges, 3) == 17);
    assert(compressed.fold_largest(ranges, 2) == 17);
}
```

対象となる値は `[5,1,1,9]`、対応する重みは `[10,2,5,7]`。
昇順では値1の重み7に値5の重み10を加えると条件が成立する。
降順では値9の重み7に値5の重み10を加えると成立する。

## API詳述

### wavelet_search_result / search_result_type

書式: `cp::wavelet_search_result<Key, Weight>`。
各Matrixの `search_result_type` はこの型を表す。

| フィールド | 型 | 内容 |
|---|---|---|
| `value` | `Key` | 探索で見つかった境界値。通常版ではxor後の値、座圧版では元の型の代表値 |
| `count` | `std::int64_t` | 取り込んだ要素数。true指定では境界と同値の全要素を含み、false指定では最初の成立時点で止める |
| `fold` | `Weight` | 同じ要素の重みを `Group::op` で集約した値 |

3フィールドはpublicで、`{value, count, fold}` の順に集成体初期化できる。
構造化束縛にも同じ順で対応する。`Key` や `Weight` のデフォルト構築は不要。
構築・コピー・ムーブのコストは各フィールドの対応する操作の合計。
結果は独立した値で、変更してもMatrixには影響しない。

### コンストラクタ

書式: `wavelet_matrix()`、`explicit wavelet_matrix(const vector<Key>& values)`。
座圧版はクラス名を `compressed_wavelet_matrix` に読み替える。

- **引数**: `values` は元の添字順の値列。空列も使用可能。
- **戻り値**: なし。
- **内容**: 値の問い合わせを準備する。重みは未構築で、`has_weights() == false`。
  引数の変更・破棄は構築後のオブジェクトに影響しない。
- **時間計算量**: デフォルト構築は `O(1)`。
  値列からの構築は通常版 `O(1 + NH)`、座圧版 `O(1 + N log(N+1))`。
- **条件・例外**: `values.size() > INT_MAX` は `std::length_error` の対象。

### size / empty / has_weights

書式: `int size() const`、`bool empty() const`、`bool has_weights() const`。

- **引数**: なし。
- **戻り値**: それぞれ要素数、空列かどうか、重み構築済みかどうか。
- **内容**: 現在の状態を取得する。空列でも `build_weights({})` 後は重み構築済みになる。
- **時間計算量**: すべて `O(1)`。

### kth_smallest / kth_largest

書式:

```text
Key kth_smallest(int l, int r, int k, mask_type xor_mask = 0) const
Key kth_smallest(vector<range_type> ranges, int64_t k, mask_type xor_mask = 0) const
Key kth_largest(int l, int r, int k, mask_type xor_mask = 0) const
Key kth_largest(vector<range_type> ranges, int64_t k, mask_type xor_mask = 0) const
```

座圧版はxor引数なし。

- **引数**: 添字区間 `[l, r)`、0-indexedの順位 `k`、通常版では値に適用するxorマスク。
- **戻り値**: `kth_smallest` は小さい順、`kth_largest` は大きい順に並べたときの `k` 番目。
  同じ値もそれぞれ1要素として数える。
- **内容**: 通常版はxor後の値の大小関係で選び、xor後の値を返す。重み構築は不要。
- **複数区間版**: 引数 `ranges` の全要素を重複分込みでまとめ、0-indexedの順位を選ぶ。
  戻り値は通常版ではxor後の値、座圧版では元の値。重み構築は不要。
- **時間計算量**: 単一区間版は両版とも `O(H)`。複数区間版は両版とも `O(1+RH)`。
- **領域**: 単一区間版は追加 `O(1)`、複数区間版は作業領域 `O(R)`。
- **条件**: 単一区間版は `0 <= k < r-l`、複数区間版は `0 <= k < M`。
  総要素数0では使用できない。複数区間の共通仕様も参照。

### range_freq

書式: `int range_freq(int l, int r, Key lower, Key upper, mask_type xor_mask = 0) const`。
座圧版はxor引数なしで、`lower`・`upper` は `const Key&`。

- **引数**: 添字区間 `[l, r)`、値域 `[lower, upper)`、通常版ではxorマスク。
- **戻り値**: `!(対象値 < lower) && 対象値 < upper` を満たす要素数。
- **内容**: 通常版の対象値は `a[i] ^ xor_mask`、座圧版では元の値。
  空区間または `!(lower < upper)` では0。重み構築は不要。
- **時間計算量**: 通常版 `O(H)`、座圧版 `O(L+H)`。
- **端点**: `upper` と比較上同値の値は含まない。整数型では最大値に1を加えて上端を作らないこと。
  全要素の個数だけが必要なら `r-l` を使う。

### build_weights

書式: `void build_weights(const vector<Weight>& weights)`。

- **引数**: 値列と同じ長さで、元の添字に対応した重み列。
- **戻り値**: なし。
- **内容**: 重みの問い合わせに必要な累積データを構築する。
  既存の重みがある場合は置き換える。値・個数・k番目の問い合わせ結果は変化しない。
  引数の変更・破棄は構築後のオブジェクトに影響しない。
- **時間計算量**: 両版とも `O(1 + (N+1)H)`。旧データの破棄も含む。
- **領域**: 追加の保持領域は `(N+1)H` 個の重み。作業領域は `O(N+H)`。
  再構築中は旧保持領域も必要。
- **条件・例外**: `weights.size() == size()`。
  確保や重み演算が例外を投げた場合、元の重みデータは維持される。

### range_fold

書式:

```text
Weight range_fold(int l, int r) const
Weight range_fold(int l, int r, Key lower, Key upper, mask_type xor_mask = 0) const
```

座圧版の値域付きオーバーロードはxor引数なしで、`lower`・`upper` は `const Key&`。

- **引数**: 添字区間 `[l,r)`、値域付きの場合は `[lower,upper)` とxorマスク。
- **戻り値**: 条件に合う要素の重みを `Group::op` で集約した値。
  該当要素がない場合は単位元。
- **内容**: 値域なしでは添字区間全体を集約する。値域付きでは `range_freq` と同じ値の条件で絞る。
  xorは値の選別だけに適用し、重み自身には適用しない。
  `!(lower < upper)` も単位元を返す。
- **時間計算量**: 値域なしは両版 `O(1)`。
  値域付きは通常版 `O(H)`、座圧版 `O(L+H)`。
- **条件**: 重み構築済みであること。空区間にもこの条件が必要。
  可換演算を前提とし、元の添字順に演算する保証はない。
- **端点**: 値域付きでは上端と比較上同値の値を含まない。
  整数型の最大値を上端としても最大値自身を含まない。
  値で絞らず全体を集約する場合は値域なしを使う。

### fold_smallest / fold_largest

書式:

```text
Weight fold_smallest(int l, int r, int k, mask_type xor_mask = 0) const
Weight fold_smallest(vector<range_type> ranges, int64_t k, mask_type xor_mask = 0) const
Weight fold_largest(int l, int r, int k, mask_type xor_mask = 0) const
Weight fold_largest(vector<range_type> ranges, int64_t k, mask_type xor_mask = 0) const
```

座圧版はxor引数なし。

- **引数**: 添字区間 `[l,r)` または区間配列 `ranges`、取り込む要素数 `k`、
  通常版では値に適用するxorマスク。`k` は順位ではなく個数。
- **戻り値**: `fold_smallest` は対象値の小さい順、`fold_largest` は大きい順に
  ちょうど `k` 個選んだ要素の重みを `Group::op` で集約した値。
  `k = 0` では単位元。`k` が総要素数と等しければ全重みの集約。
- **内容**: 通常版はxor後の値の順序で選ぶ。重み自身にxorは適用しない。
  同値でも1要素ずつ数え、境界値の要素が複数あっても指定個数で打ち切る。
  同値内では、単一区間版は元の添字の昇順、複数区間版は指定された区間を
  連結したときの出現順で選ぶ。降順の `fold_largest` でも同値内の順序は同じ。
  可換群の集約なので、重み演算そのものの評価順は保証しない。
- **複数区間版**: 全区間の要素をまとめて選び、重複する添字も出現回数だけ数える。
  区間配列は値渡しで受け取り、呼び出し元のlvalueは変更しない。
- **時間計算量**: 両版とも単一区間 `O(H)`、複数区間 `O(1+RH)`。
  `k = 0` と全要素の選択は、単一区間 `O(1)`、複数区間 `O(1+R)`。
- **領域**: 単一区間は追加 `O(1)`、複数区間は作業領域 `O(R)`。
- **条件**: 重み構築済みで、単一区間は `0 <= k <= r-l`、複数区間は `0 <= k <= M`。
  空区間・空の区間配列も `k = 0` なら使用できる。重み構築済み条件はこの場合も必要。

### first_value

書式（オーバーロードをまとめた表記）:

```text
template <class Predicate>
optional<search_result_type> first_value(int l, int r, Key lower, Predicate predicate,
                                        bool take_all_equal = true, mask_type xor_mask = 0) const
template <class Predicate>
optional<search_result_type> first_value(vector<range_type> ranges, Key lower,
                                        Predicate predicate, bool take_all_equal = true,
                                        mask_type xor_mask = 0) const
```

座圧版はxor引数なしで、`lower` は `const Key&`。
通常版ではフラグを省略して `predicate, xor_mask` と渡す従来の書式も利用できる。
フラグはbool型だけを受け付け、整数の0や1はフラグとして解釈しない。

- **引数**: 添字区間 `[l,r)` または区間配列、値の下限 `lower`、重みの集約値を受け取る述語、
  同値をすべて含めるかを指定する `take_all_equal`、通常版ではxorマスク。述語は値渡しする。
- **戻り値**: true指定では、値域 `[lower,v]` の重みの集約で述語がtrueになる最小の実在値vを
  `value` として返す。`count` と `fold` はこの値域の全要素の個数と重みの集約値で、同値をすべて含む。
  false指定では、lower以上の要素を昇順に1個ずつ取り込み、述語が初めてtrueになる時点の結果を返す。
  この場合の `value` は最後に取り込んだ値、`count` は取り込んだ最小の正の個数、`fold` はその集約値。
  どちらも条件を満たす時点がなければ `std::nullopt`。
- **内容**: `count` は重み0の要素も1個として数え、`predicate(result.fold) == true` を満たす。
  false指定時は同値の途中でも止める。同値内では単一区間版は元の添字順、
  複数区間版は区間の出現順、各区間内の添字順に選ぶ。true指定時はこの順序に結果は依存しない。
  通常版はxor後の値を探索し、`value` フィールドとして返す。
- **複数区間版**: 全区間の要素をまとめて選び、重複する要素・重みも出現回数だけ数える。
  空の区間配列や空区間、下限以上の要素がない場合は `nullopt`。
- **時間計算量**: true指定時は単一区間で通常版 `O(H)`、座圧版 `O(L+H)`、
  複数区間で通常版 `O(1+RH)`、座圧版 `O(L+RH)`。述語評価は `O(H)` 回。
  false指定時は単一区間で通常版 `O(H+D)`、座圧版 `O(L+H+D)`、
  複数区間で通常版 `O(1+RH+D)`、座圧版 `O(L+RH+D)`。
  述語評価は単一区間で `O(H+D)` 回、複数区間で `O(H+R+D)` 回。
- **領域**: 単一区間版は固定作業領域 `O(W)`、複数区間版は作業領域 `O(R)`。
- **条件**: 重み構築済みで、`predicate(Group::identity()) == false`。
  true指定時は、同値をまとめて取り込む値単位で、述語がfalseからtrueへ高々1回だけ変化すること。
  false指定時は、同値内も含めて1個ずつ取り込む要素単位で、この単調性を満たすこと。
  述語は同じ入力に同じ結果を返し、重みやオブジェクトを変更しないこと。
  例えば非負重みの和が正の目標以上になる条件は両モードで使える。
  負の重みの和やxor結果の大小比較には、一般にこの単調性がない。
  空の場合も重み構築済みと単位元でfalseという条件が必要。
- **上端との関係**: 戻る `value` は含む側の境界値。
  整数の半開区間の上端として `value+1` が必要な場合は、十分広い型などで計算すること。
  一般の型では直後の値が存在するとは限らない。

### last_value

書式（オーバーロードをまとめた表記）:

```text
template <class Predicate>
optional<search_result_type> last_value(int l, int r, Key upper, Predicate predicate,
                                       bool take_all_equal = true, mask_type xor_mask = 0) const
template <class Predicate>
optional<search_result_type> last_value(vector<range_type> ranges, Key upper,
                                       Predicate predicate, bool take_all_equal = true,
                                       mask_type xor_mask = 0) const
```

座圧版はxor引数なしで、`upper` は `const Key&`。
通常版では `predicate, xor_mask` と渡す従来の書式も利用できる。フラグはbool型だけを受け付ける。

- **引数**: 添字区間 `[l,r)` または区間配列、含まない側の上限 `upper`、述語、
  `take_all_equal`、通常版ではxorマスク。述語は値渡しする。
- **戻り値**: true指定では、値域 `[v,upper)` の重みの集約で述語がtrueになる最大の実在値vを
  `value` として返す。`count` と `fold` はこの値域の全要素の個数と重みの集約値で、同値をすべて含む。
  false指定では、upper未満の要素を降順に1個ずつ取り込み、述語が初めてtrueになる時点の結果を返す。
  `value` は最後に取り込んだ値、`count` は取り込んだ最小の正の個数、`fold` はその集約値。
  どちらも条件を満たす時点がなければ `std::nullopt`。
- **内容**: true指定時は同値をまとめて取り込む。false指定時は同値の途中でも止める。
  同値内の順序は `first_value` と同じで、値の降順に伴って同値内の順序は反転しない。
  `count` は重み0の要素も数え、`predicate(result.fold) == true` を満たす。
  通常版はxor後の値を探索し、`value` フィールドとして返す。
- **複数区間版**: 全区間の要素をまとめて選び、重複する要素・重みも出現回数だけ数える。
  空の区間配列や空区間、上限未満の要素がない場合は `nullopt`。
- **時間計算量**: true指定時は単一区間で通常版 `O(H)`、座圧版 `O(L+H)`、
  複数区間で通常版 `O(1+RH)`、座圧版 `O(L+RH)`。述語評価は `O(H)` 回。
  false指定時は単一区間で通常版 `O(H+D)`、座圧版 `O(L+H+D)`、
  複数区間で通常版 `O(1+RH+D)`、座圧版 `O(L+RH+D)`。
  述語評価は単一区間で `O(H+D)` 回、複数区間で `O(H+R+D)` 回。
- **領域**: 単一区間版は固定作業領域 `O(W)`、複数区間版は作業領域 `O(R)`。
- **条件**: 重み構築済みで、`predicate(Group::identity()) == false`。
  true指定時は値単位、false指定時は同値内を含む要素単位で、述語がfalseからtrueへ高々1回だけ変化すること。
  述語の決定性と副作用に関する条件は `first_value` と同じ。
  空の場合も重み構築済みと単位元でfalseという条件が必要。
- **端点**: `upper` と比較上同値の値は含まない。返す `value` は含む側の下端。
  整数型の最大値を上限にしても最大値自身は対象外。最大値に1を加えて上端を作らないこと。

### wavelet_sum_group

書式: `cp::wavelet_sum_group<Weight>`。

- **引数・戻り値**: `identity()` は `Weight{0}` を返す。
  `op(a,b)` は `a+b`、`inverse(a)` は `-a` を返す。
- **内容**: 既定の加算演算を提供する。状態は持たない。
- **時間計算量**: それぞれ対応する `Weight` の演算と同じ。
- **条件**: 共通の型条件とオーバーフロー条件を満たすこと。
  浮動小数点演算は厳密な結合則を満たさないため、可換群としての正確な結果は保証しない。

## xor集約への差し替え

次の例では、値へのxorと重みのxor集約を独立に指定する。

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

struct xor_group {
    static unsigned identity() { return 0; }
    static unsigned op(const unsigned& a, const unsigned& b) { return a ^ b; }
    static unsigned inverse(const unsigned& a) { return a; }
};

int main() {
    cp::wavelet_matrix<unsigned, unsigned, xor_group> wm({1, 2, 3});
    wm.build_weights({4, 8, 16});
    assert(wm.range_fold(0, 3, 1, 3) == (4U ^ 8U));
    assert(wm.range_fold(0, 3, 1, 4, 1U) == (8U ^ 16U));
}
```

## verify候補

- [CODE FESTIVAL 2014 上海 J - XORAND](https://atcoder.jp/contests/code-festival-2014-china-open/tasks/code_festival_china_j)
  値へのxor、`kth_smallest`。累積xor列を構築し、左側のANDが一定となる分割位置の区間ごとに、右端の累積xorをマスクとしてxor後の最小値を求める。順位は0を指定する。
- [JOI 2022/2023 春季トレーニング A - ふたつの通貨 (Two Currencies)](https://atcoder.jp/contests/joisp2023/tasks/joisp2023_a)
  複数区間、重みの総和、境界探索、index座標の圧縮。HLDでパスを複数区間に分け、料金を値と重みにして `first_value(..., false)` で銀貨の予算を初めて超える個数を求める。同じ辺の複数関所は `indexed_wavelet_matrix` の同一座標に置ける。
- [Codeforces 1771F - Hossam and Range Minimum Query](https://codeforces.com/problemset/problem/1771/F)
  xorによる重みの集約、値単位の境界探索。値ごとに同じ乱数を重みとして割り当て、xor群と `first_value(..., true)` で集約値が0でなくなる最小の値を探す。奇数回出現する値の集合が空でないのにハッシュが0となる衝突では誤判定し得る。
- [yukicoder No.2065 - Sum of Min](https://yukicoder.me/problems/no/2065)
  値域内の総和と個数。値 `A_i` を重みにも使い、`X` 未満の要素の総和と、`X` 以上の要素の個数に `X` を掛けた値を足す。
