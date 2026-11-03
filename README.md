# library_ai

生成AIを用いて作成する、AtCoder向けの個人用C++ライブラリ。

リポジトリ: https://github.com/tute7627/library_ai

## コード規約

ライブラリ本体の定義は `namespace cp` に入れる。
コード内に説明用コメントは置かず、関数の機能・使い方・注意点はMarkdownに記載する。
各C++ファイルの先頭には、`blob/main/` 以降のファイルパスまで含めたGitHubの `Source` URLを残す。
ツールが必要とする機械用コメントも保持する。

## テンプレート

`library/template.hpp` には、ライブラリが依存できる部分だけを置く。
内容は `<bits/stdc++.h>`、`using namespace std;`、グローバルな `edge<T>`、`Graph<T>` のみ。
独自の名前空間や `ll`、`rep` などの型別名・マクロ・補助関数は含めない。

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/template.hpp
// Source: https://github.com/tute7627/library_ai/blob/main/library/hashmap.hpp
#include "library/template.hpp"
#include "library/hashmap.hpp"
```

共通ヘッダはライブラリより先に読み込む。テストもこの共通ヘッダだけを先読みして実行する。
`<bits/stdc++.h>` が使えるg++ / libstdc++の環境を前提とする。

共通テンプレートの `edge<T>` と `Graph<T>` は、指定済みテンプレートとの互換性のためグローバルに置く。

| 定義 | 機能 |
|---|---|
| `edge<T = int>` | 行き先 `to`、重み `cost`、辺番号 `id` を持つ |
| `edge()` | `to` と `id` を -1 にする。`cost` はTのデフォルト初期化に従い、組み込み整数型では未初期化 |
| `edge(to, cost = 1, id = -1)` | 各メンバを指定した値で初期化する |
| `operator int() const` | `to` を返す |
| `Graph<T>` | `vector<vector<edge<T>>>` の型別名 |

## ライブラリ

- [hashmap](docs/hashmap.md): 整数キー向けの高速ハッシュマップ。最大使用率50%、64bit整数演算による検索に統一。`[]`、`find`、`insert`、`erase`、範囲for、`count` に対応。

- [wavelet_matrix](docs/wavelet_matrix.md): 整数列の昇順・降順k番目と値域内の個数。重みを後付けして値域・先頭k個の集約、境界値・個数・集約値の探索に対応。複数区間の同時探索、値へのxorも利用可能。座圧版はpairや文字列などの比較可能な値にも対応。

- [indexed_wavelet_matrix](docs/indexed_wavelet_matrix.md): 一般座標をindexに使うWavelet Matrix。重複座標を個別に保持し、座標区間・複数区間で既存の順位選択・重み集約・探索を利用できる。

## 開発環境

Linux / WSL Ubuntu、g++、GNU Make、Python 3.10以上を使用する。
ライブラリ・テストの基準はC++17とし、`-std=c++17` で検証する。
C++20以降が必須となる機能には依存しない。

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

Ubuntuで `venv` の作成時に `ensurepip` がないと表示される場合は、
使用するPythonに対応する `python3-venv` パッケージが必要。

## 検証

[competitive-verifier](https://competitive-verifier.github.io/competitive-verifier/document.ja.html)で自作テストを実行する。
通常の検証では外部ジャッジのテストケースを取得しない。

```bash
source .venv/bin/activate
make test
```

GitHub Actionsでも同じ `make test` を実行する。

### テストの追加

- ライブラリ本体を `library/`、自作テストを `tests/` に置く。
- テストのファイル名を `*.test.cpp` とする。
- `library/template.hpp` を、検証するライブラリより先にincludeする。
- ファイルの先頭に以下のコメントを付け、`main()` でテストを実行する。

```cpp
// Source: https://github.com/tute7627/library_ai/blob/main/tests/hashmap.test.cpp
// competitive-verifier: STANDALONE
```

`Source` URLは、作成するファイルのリポジトリ内パスに合わせて変更する。

正常終了（終了コード0）で成功、それ以外で失敗となる。
`assert` で境界条件や愚直解との一致を確認する。`NDEBUG` は定義しない。
ランダムテストは固定シードを使い、失敗を再現できるようにする。

**competitive-verifierはGitに登録されたファイルを検出するため、
新規ファイルは `git add` してから検証する。commitは不要。**

```bash
git add library/ tests/
make test
```

外部問題を指定する `PROBLEM` 属性は通常のテストには使用しない。
コンパイラ設定は `.competitive-verifier/config.toml` で管理する。
生成ファイルは `.competitive-verifier/cache/` に保存し、Gitの管理対象外とする。
