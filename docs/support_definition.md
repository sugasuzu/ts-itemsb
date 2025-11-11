# サポート（支持度）の定義

## 1. データマイニングにおける標準的な定義

### Support（サポート、支持度）

アソシエーションルールマイニングにおける基本的な指標の一つ。

```
Support(X → Y) = P(X ∧ Y)
               = |{X ∧ Y}| / |D|
               = (X と Y が同時に出現する回数) / (全データ数)
```

**意味**: ルール全体（条件部 X と結果部 Y）がデータ全体の中でどれくらいの頻度で出現するか

**例**: スーパーマーケットの購買データ
```
ルール: {パン} → {牛乳}
全取引: 1000件
パンと牛乳を両方購入: 50件

Support = 50 / 1000 = 0.05 = 5%
```

---

## 2. Confidence（信頼度、確信度）

```
Confidence(X → Y) = P(Y | X)
                  = |{X ∧ Y}| / |{X}|
                  = (X と Y が同時に出現する回数) / (X が出現する回数)
```

**意味**: 条件部 X が成立した場合、結果部 Y がどれくらいの確率で成立するか

**例**:
```
ルール: {パン} → {牛乳}
パンを購入: 200件
パンと牛乳を両方購入: 50件

Confidence = 50 / 200 = 0.25 = 25%
```

---

## 3. 本システムでの定義

### このシステムの構造

```
ルール: IF (過去の属性パターン) THEN (未来値の統計的性質)

例:
  IF (BTC_Up=1 AND ETH_Up=1 at t-1)
  THEN (X(t+1) の平均 = +0.8%, 標準偏差 = 0.3%)
```

### 本システムでの「サポート」の意味

このシステムでは、**Confidence（信頼度）に相当するものを "support_rate" として計算しています**。

```c
// Line 1824-1831: calculate_support_value()
double calculate_support_value(int matched_count, int negative_count_val)
{
    if (negative_count_val == 0)
        return 0.0;

    return (double)matched_count / (double)negative_count_val;
}
```

```c
// Line 2092-2094: 使用箇所
int matched_count = match_count[individual][k][loop_j];
double support = calculate_support_value(matched_count,
                                         negative_count[individual][k][loop_j]);
```

### 修正後の計算式

```
support_rate = match_count / negative_count
             = match_count / evaluation_count
             = (ルールにマッチした回数) / (ルール評価が可能だった回数)
```

**これは標準的な Confidence の定義に相当します。**

---

## 4. 用語の対応関係

| 標準的な用語 | 本システムでの用語 | 説明 |
|------------|----------------|------|
| **Support** | （未実装） | データ全体に対する頻度 |
| **Confidence** | `support_rate` | 条件成立時の結果出現率 |
| Support Count | `support_count` | マッチした回数（分子） |
| - | `negative_count` = `evaluation_count` | 評価可能だった回数（分母） |

---

## 5. 具体例で理解する

### データ構造

```
全データ: 4,132時点（EURUSD日次データ）

各時点で以下を記録:
  - 過去の属性: BTC_Up, ETH_Up, ... (60次元)
  - 未来値: X(t+1), X(t+2) のリターン
```

### ルール例

```
ルール: IF (BTC_Up=1 at t-1) AND (ETH_Up=1 at t-2)
        THEN ...
```

### 評価プロセス

| 時点 | BTC_Up(t-1) | ETH_Up(t-2) | 評価可能？ | マッチ？ | 未来値 X(t+1) |
|------|-------------|-------------|----------|---------|--------------|
| 0    | -1 (範囲外) | -1 (範囲外) | ❌ No    | -       | -            |
| 1    | -1 (範囲外) | データなし   | ❌ No    | -       | -            |
| 2    | 1           | 0           | ✅ Yes   | ❌ No   | +0.5%        |
| 3    | 1           | 1           | ✅ Yes   | ✅ Yes  | +0.8%        |
| 4    | 0           | 1           | ✅ Yes   | ❌ No   | -0.3%        |
| 5    | 1           | 1           | ✅ Yes   | ✅ Yes  | +1.2%        |
| ...  | ...         | ...         | ...      | ...     | ...          |

### 集計結果

```
evaluation_count = 3,500 時点  （評価可能だった回数）
match_count = 180 時点         （ルールにマッチした回数）

support_rate = 180 / 3,500 = 0.0514 = 5.14%
```

**意味**:
- 過去の属性が揃った 3,500 時点のうち
- このルールパターンにマッチしたのは 180 時点（5.14%）
- マッチした 180 時点の未来値（X(t+1), X(t+2)）を統計解析

---

## 6. 本システムの特徴

### (1) 時系列を考慮

```
過去（条件部）: t-4, t-3, t-2, t-1, t-0 の属性
未来（結果部）: t+1, t+2, t+3 の X 値（リターン）
```

**evaluation_count の判定**:
- 過去の時点が全てデータ範囲内にある → evaluation_count++
- どれか1つでも範囲外 → 評価不可（カウントしない）

### (2) 2段階のフィルタリング

**第1段階: サポート率（Confidence）**
```c
#define Minsup 0.001  // 最小サポート率 = 0.1%
```
- `support_rate >= Minsup` のルールのみ残す
- 例: 0.1% 未満のルールは希少すぎて除外

**第2段階: サポート数（Support Count）**
```c
#define MIN_SUPPORT_COUNT 8  // 最小マッチ回数
```
- `match_count >= MIN_SUPPORT_COUNT` のルールのみ残す
- 例: 8回未満しかマッチしないルールは統計的に不安定

### (3) 修正による影響

**修正前（間違った式）:**
```
negative_count = match_count[0] - evaluation_count + match_count
               ≈ 全データ数（ほぼ定数）

support_rate = match_count / (全データ数)
             ≈ 標準的な Support に近い（但し不正確）
```

**修正後（正しい式）:**
```
negative_count = evaluation_count

support_rate = match_count / evaluation_count
             = 標準的な Confidence（信頼度）
```

---

## 7. 出力ファイルでの確認方法

### zrp01a.txt の例

```
support_count  support_rate  Negative  HighSup  LowVar  NumAttr  ...
15             0.0234        641       1        1       3        ...
8              0.0125        640       0        1       2        ...
```

**解釈**:
- 1行目: 641時点で評価可能、うち15時点でマッチ → 2.34%
- 2行目: 640時点で評価可能、うち8時点でマッチ → 1.25%

**検証**:
```
support_rate = support_count / Negative

1行目: 15 / 641 = 0.0234 ✅
2行目: 8 / 640 = 0.0125 ✅
```

---

## 8. まとめ

### 本システムでの「サポート」は Confidence（信頼度）

```
support_rate = P(ルールマッチ | 評価可能)
             = match_count / evaluation_count
```

**意味**:
- 過去の属性が揃った時点のうち
- このルールパターンが出現する確率

### 標準的な Support（支持度）との違い

| 指標 | 定義 | 本システム |
|-----|------|-----------|
| Support | データ全体に対する頻度 | 未実装 |
| Confidence | 条件成立時の結果出現率 | `support_rate` として実装 |

### 用語の注意点

- 本システムの `support_rate` = 標準用語の **Confidence**
- 本システムの `support_count` = マッチ回数（絶対数）
- 標準用語の **Support** = `match_count / 全データ数` に相当（未計算）

### 修正の効果

修正により、`support_rate` が正しく Confidence として計算されるようになりました：

```
修正前: support_rate ≈ match_count / 全データ数（不正確）
修正後: support_rate = match_count / evaluation_count（正確な Confidence）
```

---

**作成日**: 2025-11-11
**関連ファイル**: `main.c` (Line 1824-1831, 1712-1737, 2092-2094)
