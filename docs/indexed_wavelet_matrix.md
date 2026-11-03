# cp::indexed_wavelet_matrix

大小関係を定義できる座標をindexとして持つ静的な要素列に対して、座標区間内の順位選択、
値域内の個数、重みの集約と境界探索を行う。
同じindex座標に複数要素を置ける。存在しない座標を区間端点に指定することもできる。

`cp::wavelet_matrix` または `cp::compressed_wavelet_matrix` を包む。
index座標の圧縮と値の座圧は独立で、通常版を包んだ場合は値へのxorも使える。
値・index座標・要素数は構築後に変更できない。重み列全体の再構築には対応する。

共通ヘッダ `library/template.hpp` を先に読み込む。
`library/wavelet_matrix.hpp` に通常版・値座圧版とともに定義される。

## 使用例

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

int main() {
    using Matrix = cp::indexed_wavelet_matrix<long long, cp::wavelet_matrix<int>>;
    const std::vector<long long> index{5, 0, 3, 0, 5};
    const std::vector<int> value{1, 7, 9, 2, 4};
    const std::vector<long long> weight{10, 20, 30, 40, 50};
    Matrix wm(index, value);
    assert(wm.size() == 5);
    assert(wm.range_size(0, 5) == 3);
    assert(wm.range_size(1, 3) == 0);
    assert(wm.kth_smallest(0, 5, 1) == 7);
    assert(wm.kth_largest(0, 5, 0) == 9);
    assert(wm.kth_smallest(0, 5, 1, 2U) == 5);
    assert(wm.range_freq(0, 5, 0, 6, 2U) == 2);

    wm.build_weights(weight);
    assert(wm.range_fold(0, 5) == 90);
    assert(wm.range_fold(0, 5, 2, 8) == 60);
    assert(wm.fold_smallest(0, 5, 2) == 60);
    assert(wm.fold_largest(0, 5, 2) == 50);
    const auto predicate = [](long long sum) { return sum >= 50; };
    const auto first = wm.first_value(0, 5, 2, predicate);
    assert(first && first->value == 7 && first->count == 2 && first->fold == 60);
    const auto last = wm.last_value(0, 5, 10, predicate);
    assert(last && last->value == 7 && last->count == 2 && last->fold == 50);

    const std::vector<Matrix::range_type> ranges{{0, 1}, {5, 6}, {0, 1}};
    assert(wm.range_size(ranges) == 6);
    assert(wm.kth_smallest(ranges, 2) == 2);
    assert(wm.fold_smallest(ranges, 3) == 90);
    const auto boundary = wm.first_value(ranges, 1,
        [](long long sum) { return sum >= 70; });
    assert(boundary && boundary->value == 2 && boundary->count == 3 && boundary->fold == 90);

    using Compressed = cp::indexed_wavelet_matrix<long long,
                                                  cp::compressed_wavelet_matrix<int>>;
    Compressed compressed(index, value);
    compressed.build_weights(weight);
    assert(compressed.fold_smallest(ranges, 3) == 90);
    const auto compressed_boundary = compressed.first_value(ranges, 1,
        [](long long sum) { return sum >= 70; });
    assert(compressed_boundary && compressed_boundary->value == 2 &&
           compressed_boundary->count == 3 && compressed_boundary->fold == 90);
}
```

index座標 `[0,5)` にある値は `7,2,9`、対応する重みは `20,40,30`。
`build_weights` に渡す重みは、常にコンストラクタへ渡したときの入力順でよい。
複数区間の例では `[0,1)` を2回指定しているため、その座標にある各要素も2回ずつ数える。

`pair` をindexにする場合も、同じAPIを使える。
次の `[ {1,2}, {2,0} )` は辞書順の区間で、座標 `{1,4}` も含む。

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/wavelet_matrix.hpp
#include "library/template.hpp"
#include "library/wavelet_matrix.hpp"

int main() {
    using Index = std::pair<int, int>;
    using Matrix = cp::indexed_wavelet_matrix<Index, cp::wavelet_matrix<int>>;
    Matrix wm({{2, 0}, {1, 4}, {1, 2}, {1, 2}}, {30, 10, 20, 40});
    assert(wm.range_size({1, 2}, {2, 0}) == 3);
    assert(wm.kth_smallest({1, 2}, {2, 0}, 1) == 20);
    wm.build_weights({3, 1, 2, 4});
    assert(wm.range_fold({1, 2}, {1, 4}) == 6);
    const std::vector<Matrix::range_type> ranges{
        {{1, 2}, {1, 4}}, {{2, 0}, {3, 0}}
    };
    assert(wm.fold_smallest(ranges, 2) == 5);
    const auto result = wm.first_value(ranges, 0,
        [](long long sum) { return sum >= 4; });
    assert(result && result->value == 30 && result->count == 2 && result->fold == 5);
}
```

## 型と前提条件

書式:

```text
cp::indexed_wavelet_matrix<Index, Matrix>
```

- `Index` はコピー構築・コピー代入可能で、`operator<` が厳密弱順序を満たす型。
  デフォルト構築は不要。整数のほか、`std::pair`、`std::tuple`、`std::string`、独自型を使える。
  `==`・`!=`・`<=`・`>`・`>=` は不要。
- index座標a,bが同じかどうかは、`!(a < b) && !(b < a)` で判定する。
  以降の「同じ座標」はこの比較上の同値を意味する。
  比較結果は構築後も変化しないこと。浮動小数点を使う場合、NaNを含めないこと。
  `pair` と `tuple` の標準の比較は辞書順で、座標区間もその順序に従う。
- `Matrix` は `cp::wavelet_matrix<Key, Weight, Group>` または
  `cp::compressed_wavelet_matrix<Key, Weight, Group>`。
  `Key`・`Weight`・`Group` の条件は [wavelet_matrix](wavelet_matrix.md) と同じ。
- 要素数は `INT_MAX` 以下。同じindex座標でも、要素を併合せず個別に保持する。
- C++17、`<bits/stdc++.h>`、GCC互換の `__builtin_popcountll` が使える環境を前提とする。
- 座標区間は半開区間 `[l,r)` で、`!(r < l)` が必要。
  対象は入力要素iのうち `!(positions[i] < l) && positions[i] < r` を満たすもの。
  端点が既存の座標である必要はなく、保存された座標の範囲外でもよい。
- 区間端点も `Index` 型で、右端と比較上同値の座標は含まない。
  必要な境界を表せる `Index` を選ぶこと。整数型の最大値を右端にしても最大値自身は含まない。
  最大値に1を加えて右端を作らないこと。
- 個数・順位はindex座標の種類数ではなく要素数に対するもの。
  重み0の要素も1個として数える。
- 重みの問い合わせは `build_weights` 完了後に使う。
  長さの不一致、区間・順位・構築済み条件の違反は `assert` の対象で、例外ではない。
- すべての重みの中間演算が型の範囲に収まること。複数区間の重複分も含む。
  可換群の条件、探索述語の単調性条件も [wavelet_matrix](wavelet_matrix.md) と同じ。

公開型は `index_type = Index`、`matrix_type = Matrix`、
`key_type = Matrix::key_type`、`weight_type = Matrix::weight_type`、
`size_type = int`、`range_type = std::pair<Index, Index>`、
`search_result_type = Matrix::search_result_type`。
結果型は `value`・`count`・`fold` を持つ構造体で、詳細は [wavelet_search_result](wavelet_matrix.md#wavelet_search_result--search_result_type) を参照。
通常版を包む場合だけ、`mask_type = Matrix::mask_type` も公開する。

xor引数の有無と意味は包んだ型に従う。通常版では、値にだけxorを適用し、
index座標と重みは変化しない。順位・探索の返す値はxor後の値。
値座圧版を包む場合はxor引数がなく、元の値をそのまま扱う。

単一区間の端点は `const Index&`、値域の端点は `const key_type&` で受け取る。
ラッパーへの引数渡しではこれらをコピーしない。
複数区間は `const vector<range_type>&` で受け取り、区間配列を変更しない。
区間の重複は要素の重複として数える。和集合として扱う場合は利用側で区間を結合する。
空の区間配列や、要素を含まない座標区間も使用できる。
順位選択だけは、対象要素が1個以上必要。

`fold_smallest`・`fold_largest` と、false指定の `first_value`・`last_value` が同値の途中で止まる場合は、
単一区間ではindex座標の昇順、同座標では入力順に選ぶ。
複数区間では区間配列順を優先し、各区間内では同じ規則を使う。
降順の `fold_largest`・`last_value` でも同値内の選び方は同じ。

## 計算量と領域

`N = size()`、`R = ranges.size()`、`M = Σ range_size(l,r)` とする。
`M` は区間の重複を含む総要素数で、`INT64_MAX` 以下であること。
`D = 1 + ceil(log2(N+1))` とする。
値の異なる種類数を `σ`、包んだ型のビット段数を `B` とし、
値座圧版では比較上の同値類の数を `σ` とする。
`H = B+1`、`L = 1 + ceil(log2(σ+1))` とする。
`B` の定義は [wavelet_matrix](wavelet_matrix.md) と同じ。
`W` は通常版では `Key` のビット幅、値座圧版では32。
型の比較・コピー・ムーブ・破棄、重み演算、述語評価は1回 `O(1)` とする。
文字列などでこれらのコストが要素の長さに依存する場合は、その実際のコストも加味する。

包んだ型の保持領域に加え、追加で `O(N)` 個の `Index` 要素と整数を保持する。
文字列などの `Index` 要素が別途確保する領域は、その要素の保持コストとして加算する。
構築時の作業領域は `O(N+H)`、重み構築時は `O(N+H)`。
重みの再構築中は旧重みデータと新重みデータを同時に保持する。

単一区間の問い合わせの作業領域は通常 `O(1)`。
`first_value` と `last_value` は追加で `O(W)` の固定作業領域を使う。
複数区間版は `O(R)` の作業領域を使う。ただし `range_size(ranges)` は `O(1)`。
const参照で受け取る区間配列自体の確保容量は、この作業領域に含めない。

領域確保・座標やキーのコピーや比較・重みの操作・述語で生じた例外は呼び出し元へ伝わる。
公開APIは内部への参照・イテレータを返さない。引数の変更・破棄は構築済みデータに影響しない。
コピーは座標・並び順・包んだ型の保持データを複製し、その要素数に比例した時間・領域を使う。
ムーブ後の移動元は破棄・代入だけを行うこと。

## API概略

| API | 内容 | 通常版を包む時間 | 値座圧版を包む時間 |
|---|---|---|---|
| デフォルト構築 | 空列を構築 | `O(1)` | `O(1)` |
| 座標列・値列から構築 | 問い合わせを準備 | `O(1+N log(N+1)+NH)` | `O(1+N log(N+1))` |
| `size / empty / has_weights` | 状態取得 | `O(1)` | `O(1)` |
| `range_size(l,r)` | 座標区間内の要素数 | `O(D)` | `O(D)` |
| `range_size(ranges)` | 複数区間の総要素数 | `O(1+RD)` | `O(1+RD)` |
| `build_weights` | 入力順の重みを構築・交換 | `O(1+(N+1)H)` | `O(1+(N+1)H)` |
| `kth_smallest / kth_largest` | 昇順・降順のk番目 | `O(D+H)` | `O(D+H)` |
| `range_freq` | 座標区間・値域内の個数 | `O(D+H)` | `O(D+L+H)` |
| `range_fold(l,r)` | 座標区間内の全重み集約 | `O(D)` | `O(D)` |
| 値域付き `range_fold` | 座標区間・値域内の重み集約 | `O(D+H)` | `O(D+L+H)` |
| `fold_smallest / fold_largest` | 昇順・降順の先頭k個を集約 | `O(D+H)` | `O(D+H)` |
| `first_value / last_value` | 境界値・取り込んだ個数・集約値 | `O(D+H)` | `O(D+L+H)` |
| 複数区間の順位選択・先頭k個集約 | 全区間をまとめて選ぶ | `O(1+R(D+H))` | `O(1+R(D+H))` |
| 複数区間の境界探索 | 全区間をまとめて探索 | `O(1+R(D+H))` | `O(L+R(D+H))` |

いずれも最悪計算量で、期待・償却計算量ではない。

## API詳述

以下のxor付き書式は通常版を包む場合のもの。
値座圧版を包む場合はxor引数を省く。

### コンストラクタ

書式:

```text
indexed_wavelet_matrix()
indexed_wavelet_matrix(const vector<Index>& positions, const vector<key_type>& values)
```

- **引数**: `positions[i]` は要素iのindex座標、`values[i]` はその値。
  座標は未ソートでよく、同一座標に複数要素を置ける。空入力も使用可能。
- **戻り値**: なし。
- **内容**: index座標で絞り込む問い合わせを準備する。重みは未構築。
  `values` の要素数は座標が同じでも減らない。
- **時間計算量**: デフォルト構築は `O(1)`。
  通常版を包む場合は `O(1+N log(N+1)+NH)`、値座圧版は `O(1+N log(N+1))`。
  座標列が初めから広義単調増加なら、通常版では `O(1+NH)`。
- **条件・例外**: `positions.size() == values.size()`。
  要素数が `INT_MAX` を超える場合は `std::length_error`。

### size / empty / has_weights

書式: `int size() const`、`bool empty() const`、`bool has_weights() const`。

- **引数**: なし。
- **戻り値**: 全要素数、要素がないかどうか、重み構築済みかどうか。
  `size()` はindex座標の種類数や最大座標ではない。
- **内容**: 状態を取得する。空入力でも `build_weights({})` 後は重み構築済み。
- **時間計算量**: すべて `O(1)`。

### range_size

書式:

```text
int range_size(const Index& l, const Index& r) const
int64_t range_size(const vector<range_type>& ranges) const
```

- **引数**: 座標区間 `[l,r)`、または座標区間の配列。
- **戻り値**: 対象要素数。複数区間版は重複を出現回数だけ数えた総要素数。
  対象要素がない場合は0。
- **内容**: 値の条件を付けずに要素を数える。重み構築は不要。
- **時間計算量**: 単一区間 `O(D)`、複数区間 `O(1+RD)`。
- **領域**: 両方とも追加 `O(1)`。
- **条件**: 各区間で `!(r < l)`。複数区間の総要素数は `INT64_MAX` 以下。

### build_weights

書式: `void build_weights(const vector<weight_type>& weights)`。

- **引数**: コンストラクタの入力順に対応した重み列。`weights[i]` は入力要素iの重み。
- **戻り値**: なし。
- **内容**: 重み問い合わせを準備する。既存の重みがある場合は全体を置き換える。
  index座標・値・要素数と、重みを使わない問い合わせの結果は変化しない。
  同じ座標の要素にも個別の重みを指定できる。
- **時間計算量**: `O(1+(N+1)H)`。旧重みデータの破棄も含む。
- **領域**: 包んだ型の保持する重みに加え、作業領域 `O(N+H)`。
- **条件・例外**: `weights.size() == size()`。
  重みのコピーや演算、領域確保が失敗した場合、元の重みデータを維持する。

### kth_smallest / kth_largest

書式:

```text
key_type kth_smallest(const Index& l, const Index& r, int k, mask_type xor_mask = 0) const
key_type kth_smallest(const vector<range_type>& ranges, int64_t k, mask_type xor_mask = 0) const
key_type kth_largest(const Index& l, const Index& r, int k, mask_type xor_mask = 0) const
key_type kth_largest(const vector<range_type>& ranges, int64_t k, mask_type xor_mask = 0) const
```

- **引数**: 座標区間または区間配列、0-indexedの順位k、通常版ではxorマスク。
- **戻り値**: `kth_smallest` は小さい順、`kth_largest` は大きい順でk番目の対象値。
  同じ値・同じ座標の要素もそれぞれ数える。
- **内容**: 全対象要素をまとめて順位を選ぶ。重み構築は不要。
- **時間計算量**: 両版とも単一区間 `O(D+H)`、複数区間 `O(1+R(D+H))`。
- **条件**: 単一区間は `0 <= k < range_size(l,r)`、複数区間は `0 <= k < M`。
  空の対象には使用できない。

### range_freq

書式:

```text
int range_freq(const Index& l, const Index& r, const key_type& lower, const key_type& upper, mask_type xor_mask = 0) const
```

- **引数**: 座標区間 `[l,r)`、値域 `[lower,upper)`、通常版ではxorマスク。
- **戻り値**: 座標条件と値の条件を両方満たす要素数。
- **内容**: 対象値が `lower` 以上 `upper` 未満のものを数える。
  `!(lower < upper)` や対象要素がない場合は0。重み構築は不要。
- **時間計算量**: 通常版 `O(D+H)`、値座圧版 `O(D+L+H)`。
- **条件**: `!(r < l)`。値の上端も半開区間で、上端と比較上同値の値を含まない。

### range_fold

書式:

```text
weight_type range_fold(const Index& l, const Index& r) const
weight_type range_fold(const Index& l, const Index& r, const key_type& lower, const key_type& upper, mask_type xor_mask = 0) const
```

- **引数**: 座標区間 `[l,r)`、値域付きの場合は `[lower,upper)` とxorマスク。
- **戻り値**: 対象要素の重みを `Group::op` で集約した値。対象がなければ単位元。
- **内容**: 値域なしでは座標区間内の全要素を、値域付きでは値の条件も満たす要素を集約する。
  `!(lower < upper)` は単位元。
- **時間計算量**: 値域なしは両版とも `O(D)`。
  値域付きは通常版 `O(D+H)`、値座圧版 `O(D+L+H)`。
- **条件**: 重み構築済み。空の対象や空の値域でもこの条件が必要。
  可換群として集約し、重み演算の評価順は保証しない。

### fold_smallest / fold_largest

書式:

```text
weight_type fold_smallest(const Index& l, const Index& r, int k, mask_type xor_mask = 0) const
weight_type fold_smallest(const vector<range_type>& ranges, int64_t k, mask_type xor_mask = 0) const
weight_type fold_largest(const Index& l, const Index& r, int k, mask_type xor_mask = 0) const
weight_type fold_largest(const vector<range_type>& ranges, int64_t k, mask_type xor_mask = 0) const
```

- **引数**: 座標区間または区間配列、取り込む個数k、通常版ではxorマスク。
- **戻り値**: 小さい順または大きい順にちょうどk個選んだ要素の重みの集約。
- **内容**: kは順位ではなく個数。0では単位元、総要素数と等しい場合は全重みの集約。
  同値の途中で打ち切る場合は、共通の同値内の選択順に従う。
- **時間計算量**: 両版とも単一区間 `O(D+H)`、複数区間 `O(1+R(D+H))`。
  0個または全要素を選ぶ場合は、単一区間 `O(D)`、複数区間 `O(1+RD)`。
- **条件**: 重み構築済みで、単一区間は `0 <= k <= range_size(l,r)`、複数区間は `0 <= k <= M`。
  空の対象でも `k = 0` なら使用できる。

### first_value / last_value

書式（オーバーロードをまとめた表記）:

```text
template <class Predicate>
optional<search_result_type> first_value(const Index& l, const Index& r, const key_type& lower,
                                             Predicate predicate, bool take_all_equal = true,
                                             mask_type xor_mask = 0) const
template <class Predicate>
optional<search_result_type> first_value(const vector<range_type>& ranges, const key_type& lower,
                                             Predicate predicate, bool take_all_equal = true,
                                             mask_type xor_mask = 0) const
template <class Predicate>
optional<search_result_type> last_value(const Index& l, const Index& r, const key_type& upper,
                                            Predicate predicate, bool take_all_equal = true,
                                             mask_type xor_mask = 0) const
template <class Predicate>
optional<search_result_type> last_value(const vector<range_type>& ranges, const key_type& upper,
                                            Predicate predicate, bool take_all_equal = true,
                                             mask_type xor_mask = 0) const
```

通常版はフラグを省略して `predicate, xor_mask` と渡す従来の書式も利用できる。
フラグはbool型だけを受け付ける。省略時はtrue。

- **引数**: 座標区間または区間配列、値の下限lowerまたは含まない上限upper、述語、
  同値をすべて含めるかを指定する `take_all_equal`、通常版ではxorマスク。述語は値渡しする。
- **戻り値**: true指定の `first_value` は値域 `[lower,v]`、`last_value` は値域 `[v,upper)` の
  集約で初めて条件を満たす境界値vを `value` として返す。`count` と `fold` はこの値域の全要素の
  個数と重みの集約値で、同値をすべて含む。
  false指定では同値も1個ずつ取り込み、述語が初めてtrueになる時点の結果を返す。
  `value` は最後に取り込んだ値、`count` は取り込んだ最小の正の個数、`fold` はその集約値。
  個数は `std::int64_t`、集約値は `weight_type`。存在しなければ `std::nullopt`。
- **内容**: false指定時は同値の途中でも条件成立時点で止める。同値内の順序は先頭k個の集約と同じ。
  単一区間はindex座標順、同座標は入力順。複数区間は区間の出現順を優先する。
  降順探索でも同値内の順序は反転しない。返す境界は値側の値であり、index座標ではない。
  true指定時はこの同値内の順序に結果は依存しない。
  両モードで `count` は重み0の要素も数え、`predicate(result.fold) == true` を満たす。
- **時間計算量**: 両モードとも単一区間は通常版 `O(D+H)`、値座圧版 `O(D+L+H)`。
  複数区間は通常版 `O(1+R(D+H))`、値座圧版 `O(L+R(D+H))`。
  述語評価はtrue指定で `O(H)` 回、false指定で単一区間 `O(H+D)` 回、複数区間 `O(H+R+D)` 回。
- **条件**: 重み構築済みで、`predicate(Group::identity()) == false`。
  true指定時は同値をまとめる値単位、false指定時は同値内を含む要素単位で、
  述語がfalseからtrueへ高々1回だけ変化すること。
  同じ入力に同じ結果を返し、重みやオブジェクトを変更しないこと。
  対象が空の場合も構築済み条件と単位元でfalseの条件が必要。
- **端点**: `first_value` が返すvは含む上端。整数値について半開区間の上端としてv+1を作る場合は、
  十分広い型などで計算すること。一般の型では直後の値が存在するとは限らない。
  `last_value` のupperと比較上同値の値は対象に含まない。
