# サポート率計算式の修正 (2025-11-11)

## 修正概要

`negative_count`の計算式を論理的に正しい式に修正しました。

---

## 修正内容

### 修正前（間違った式）

```c
void calculate_negative_counts()
{
    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < Nmx; i++)
            {
                // ❌ 論理的に不正確な式（オリジナルコードから継承）
                negative_count[individual][k][i] =
                    match_count[individual][k][0] - evaluation_count[individual][k][i] +
                    match_count[individual][k][i];
            }
        }
    }
}
```

**問題点:**
- 式の意味が不明確：「全データ数 - 評価数 + マッチ数」は何を表すのか？
- 深さが変わっても分母がほぼ一定（`match_count[0]`が支配的）
- 標準的なサポート率の定義から逸脱

### 修正後（正しい式）

```c
void calculate_negative_counts()
{
    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < Nmx; i++)
            {
                // ✅ 論理的に正確な式
                negative_count[individual][k][i] = evaluation_count[individual][k][i];
            }
        }
    }
}
```

**改善点:**
- 明確な定義：`evaluation_count` = ルール評価が行われた回数
- サポート率の標準的な定義：`support_rate = match_count / evaluation_count`
- 深さごとに正確な分母を使用

---

## サポート率の定義

### 標準的な定義

```
support_rate = マッチした回数 / 評価可能だった回数
             = match_count / evaluation_count
```

**意味:**
- 分子（`match_count`）: ルールパターンにマッチした時点の数
- 分母（`evaluation_count`）: ルール評価が可能だった時点の数（マッチ + 不一致）

### 例

100個のデータポイントがあり、ルール「A=1 AND B=1」を評価する場合：

```
- 80時点: A=1, B=1 → マッチ（10回）
- 80時点: A=1, B=0 → 不一致（50回）
- 80時点: A=0, B=* → 不一致（20回）
- 20時点: データ不足（過去の時点が範囲外）→ 評価不可

match_count = 10
evaluation_count = 80 (マッチ10 + 不一致70)

support_rate = 10 / 80 = 0.125 (12.5%)
```

**修正前の計算（間違い）:**
```
negative_count = 100 - 80 + 10 = 30  ← 意味不明
support_rate = 10 / 30 = 0.333 (33.3%) ← 過大評価
```

**修正後の計算（正しい）:**
```
negative_count = 80
support_rate = 10 / 80 = 0.125 (12.5%) ← 正確
```

---

## 時系列の扱い

### 過去の属性（ルール条件部）

```
time_index (現在時点)
  ├─ t-4: 4時点前の属性
  ├─ t-3: 3時点前の属性
  ├─ t-2: 2時点前の属性
  ├─ t-1: 1時点前の属性
  └─ t-0: 現在時点の属性
```

**evaluation_countのカウント:**
- 過去の属性（t-4からt-0）が全て揃っている → evaluation_count++
- どれか1つでも範囲外 → 評価不可（カウントしない）

### 未来値（予測対象）

```
time_index (現在時点)
  ├─ t+1: 1時点後のX値（リターン）
  ├─ t+2: 2時点後のX値
  ├─ t+3: 3時点後のX値
  └─ ...
```

**未来値の統計:**
- ルールがマッチした場合のみ、未来値を統計に累積
- `future_sum`, `future_sigma_array` に加算
- データ終端を超えた場合は `NAN` として無視

**重要:** 未来値の有無は `evaluation_count` に影響しない
- 分母（evaluation_count）: 過去の属性が揃っているか
- 分子（match_count）: そのうちルールにマッチしたか
- 統計値（future_sum等）: マッチした時点の未来値を記録

---

## 修正の影響

### 1. サポート率の値が変わる

**修正前:**
- サポート率が過大評価されていた
- 特に深いルール（属性数が多い）で顕著

**修正後:**
- 正確なサポート率が計算される
- より厳密なルール評価が可能

### 2. 発見されるルールが変わる

**フィルタ条件（`Minsup = 0.001`など）:**
- 同じ閾値でも、通過するルールが変わる
- 修正後は低いサポート率になるため、発見ルール数が減る可能性

### 3. 既存データとの互換性

**⚠️ 注意:**
- 修正前のデータ（`zrp01a.txt`など）とは互換性なし
- 全ての通貨ペアで再実行が必要
- サポート率の値が異なるため、直接比較できない

---

## 検証方法

### 1. 簡単なテストケース

```c
// 例: 深さ1のルール
match_count[k][1] = 10      // 10回マッチ
evaluation_count[k][1] = 80  // 80回評価可能

// 修正前
negative_count = 17482 - 80 + 10 = 17412  ← 全データ数に依存
support_rate = 10 / 17412 = 0.00057 (0.057%)

// 修正後
negative_count = 80
support_rate = 10 / 80 = 0.125 (12.5%)  ← 正確
```

### 2. 実データでの確認

```bash
# 小規模テスト（EURUSD, 3試行）
./main EURUSD 3

# 出力ファイルを確認
head 1-deta-enginnering/forex_data_daily/output/EURUSD/pool/zrp01a.txt
```

**確認ポイント:**
- `support_rate`列の値が妥当か（0.1%～10%程度）
- `Negative`列（evaluation_count）が`support_count`より大きいか
- `support_rate = support_count / Negative` が成立するか

---

## 理論的根拠

### アソシエーションルールマイニングの標準定義

```
Support(X → Y) = P(X ∧ Y)
               = |X ∧ Y| / |D|

Confidence(X → Y) = P(Y | X)
                  = |X ∧ Y| / |X|
```

本システムでは：
```
X: ルール条件部（過去の属性パターン）
Y: 未来値の統計的性質（mean, sigma, concentration）

support_rate ≈ Confidence(X → "pattern")
             = match_count / evaluation_count
             = P(パターンマッチ | 評価可能)
```

**修正後の式は、この標準定義に準拠しています。**

---

## まとめ

### 修正内容

```c
// 修正前（❌ 間違い）
negative_count = match_count[0] - evaluation_count + match_count

// 修正後（✅ 正しい）
negative_count = evaluation_count
```

### 効果

1. ✅ サポート率が論理的に正確になる
2. ✅ 標準的な定義に準拠
3. ✅ ルール評価の信頼性向上
4. ⚠️ 既存データとの互換性なし（再実行必要）

### 次のステップ

1. 全通貨ペアで再実行
2. 新しいサポート率でルール品質を再評価
3. 必要に応じて閾値（`Minsup`など）を調整
4. 結果を比較分析

---

**修正日:** 2025-11-11
**修正者:** Claude Code
**影響範囲:** `calculate_negative_counts()`, `evaluate_single_instance()` のコメント
**後方互換性:** なし（全データ再生成が必要）
