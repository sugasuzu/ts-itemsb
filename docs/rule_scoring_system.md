# ルール総合スコアリングシステム

発見されたルールの品質を定量的に評価するための総合スコアリングシステムの設計と実装方法。

---

## 概要

GNPで発見されたルールは以下の観点で評価される:

1. **象限集中度** (Quadrant Concentration): X(t+1) vs X(t+2) 散布図での偏り
2. **方向一貫性** (Direction Consistency): X(t+1) vs T 散布図での符号の偏り
3. **平均値の大きさ** (Mean Magnitude): シグナルの強度
4. **分散の小ささ** (Tightness): クラスターの密集度
5. **サポート率** (Support Rate): データ全体に対する出現頻度

---

## スコア式の定義

### 1. X(t+1) vs X(t+2) 散布図用スコア

**用途:** 2次元散布図での象限集中度を評価

```
Score_2D = (support_rate × 1000) × |mean_avg| × quadrant_concentration / sigma_avg

where:
  support_rate         = マッチ数 / 全データ数
  mean_avg             = (|mean(X(t+1))| + |mean(X(t+2))|) / 2
  quadrant_concentration = max(Q1, Q2, Q3, Q4) / total_matches
  sigma_avg            = (sigma(X(t+1)) + sigma(X(t+2))) / 2

  Q1, Q2, Q3, Q4 = 各象限のマッチ数 (++, +-, -+, --)
```

**物理的意味:**
```
Score_2D = (出現頻度 × シグナル強度 × 集中度) / ノイズ
```

**スケーリング:**
- `support_rate × 1000`: 0.0036-0.0086 → 3.6-8.6 (計算しやすい範囲に調整)
- スコア範囲: 0.75 - 3.49 (USDJPY日足の場合)

**例: Rule #010**
```
CADJPY_Up(t-8) AND NZDUSD_Stay(t-1)

support_rate = 0.0074
mean_avg = (|-0.329| + |+0.263|) / 2 = 0.296%
quadrant_concentration = 10/15 = 0.667  (15回中10回が-+象限)
sigma_avg = (0.498 + 0.340) / 2 = 0.419%

Score_2D = (0.0074 × 1000) × 0.296 × 0.667 / 0.419
         = 7.4 × 0.296 × 0.667 / 0.419
         = 3.49
```

---

### 2. X(t+1) vs T 散布図用スコア

**用途:** 時系列散布図での方向一貫性を評価

```
Score_Temporal = (support_rate × 1000) × |mean(X(t+1))| × direction_concentration / sigma(X(t+1))

where:
  support_rate            = マッチ数 / 全データ数
  mean(X(t+1))           = X(t+1)の平均値
  direction_concentration = max(positive_count, negative_count) / total_matches
  sigma(X(t+1))          = X(t+1)の標準偏差
```

**物理的意味:**
```
Score_Temporal = (出現頻度 × シグナル強度 × 方向一貫性) / ノイズ
```

**方向一貫性の定義:**
- X(t+1) > 0 の回数と X(t+1) < 0 の回数を比較
- 多数派の割合を計算
- ランダムなら50%、完全に一方向なら100%

**例: Rule #001**
```
NZDUSD_Stay(t-1) AND GBPCAD_Up(t-3)

support_rate = 0.0073
mean(X(t+1)) = -0.358%
direction_concentration = 12/15 = 0.800  (15回中12回が負)
sigma(X(t+1)) = 0.438%

Score_Temporal = (0.0073 × 1000) × 0.358 × 0.800 / 0.438
               = 7.3 × 0.358 × 0.800 / 0.438
               = 4.78
```

---

## スコア式の選択基準

### いつ Score_2D を使うか

✅ **X(t+1) と X(t+2) の関係性を評価したい場合**
- 「t+1で下落、t+2で反発」のような2時点パターン
- 散布図での象限集中を重視
- トレード戦略: 2ステップの予測 (エントリー→決済のタイミング)

### いつ Score_Temporal を使うか

✅ **X(t+1) の方向予測のみを評価したい場合**
- 「このパターンが出たら上昇/下降」の一方向予測
- 符号の一貫性を重視
- トレード戦略: 単純なロング/ショート判断

---

## 実装例

### Python実装

```python
import math

def calculate_score_2d(rule_data, verification_file):
    """
    X(t+1) vs X(t+2) 散布図用スコア

    Args:
        rule_data: dict with keys [mean_t1, mean_t2, sigma_t1, sigma_t2, support_rate]
        verification_file: CSV file path

    Returns:
        (score, quadrant_concentration): tuple
    """
    # 象限集中度を計算
    quadrants = [0, 0, 0, 0]  # ++, +-, -+, --

    with open(verification_file, 'r') as f:
        next(f)  # ヘッダースキップ
        for line in f:
            parts = line.strip().split(',')
            x_t1 = float(parts[-2])
            x_t2 = float(parts[-1])

            if x_t1 > 0 and x_t2 > 0:
                quadrants[0] += 1
            elif x_t1 > 0 and x_t2 < 0:
                quadrants[1] += 1
            elif x_t1 < 0 and x_t2 > 0:
                quadrants[2] += 1
            else:
                quadrants[3] += 1

    total = sum(quadrants)
    quadrant_concentration = max(quadrants) / total

    # スコア計算
    mean_avg = (abs(rule_data['mean_t1']) + abs(rule_data['mean_t2'])) / 2
    sigma_avg = (rule_data['sigma_t1'] + rule_data['sigma_t2']) / 2

    score = (rule_data['support_rate'] * 1000 * mean_avg * quadrant_concentration) / sigma_avg

    return score, quadrant_concentration


def calculate_score_temporal(rule_data, verification_file):
    """
    X(t+1) vs T 散布図用スコア

    Args:
        rule_data: dict with keys [mean_t1, sigma_t1, support_rate]
        verification_file: CSV file path

    Returns:
        (score, direction_concentration): tuple
    """
    # 方向一貫性を計算
    x_t1_values = []

    with open(verification_file, 'r') as f:
        next(f)  # ヘッダースキップ
        for line in f:
            parts = line.strip().split(',')
            x_t1 = float(parts[-2])
            x_t1_values.append(x_t1)

    positive_count = sum(1 for x in x_t1_values if x > 0)
    negative_count = sum(1 for x in x_t1_values if x < 0)

    direction_concentration = max(positive_count, negative_count) / len(x_t1_values)

    # スコア計算
    score = (rule_data['support_rate'] * 1000 * abs(rule_data['mean_t1']) *
             direction_concentration) / rule_data['sigma_t1']

    return score, direction_concentration
```

---

### C言語実装 (main.cへの統合用)

```c
/**
 * X(t+1) vs X(t+2) 散布図用スコア計算
 */
double calculate_score_2d(double mean_t1, double mean_t2,
                          double sigma_t1, double sigma_t2,
                          double support_rate,
                          int *quadrant_counts)
{
    // 象限集中度
    int total = quadrant_counts[0] + quadrant_counts[1] +
                quadrant_counts[2] + quadrant_counts[3];

    int max_quadrant = quadrant_counts[0];
    for (int i = 1; i < 4; i++) {
        if (quadrant_counts[i] > max_quadrant)
            max_quadrant = quadrant_counts[i];
    }

    double quadrant_concentration = (double)max_quadrant / total;

    // 平均値とσ
    double mean_avg = (fabs(mean_t1) + fabs(mean_t2)) / 2.0;
    double sigma_avg = (sigma_t1 + sigma_t2) / 2.0;

    // スコア計算
    double score = (support_rate * 1000.0 * mean_avg * quadrant_concentration) / sigma_avg;

    return score;
}

/**
 * X(t+1) vs T 散布図用スコア計算
 */
double calculate_score_temporal(double mean_t1, double sigma_t1,
                                double support_rate,
                                double *x_t1_values, int count)
{
    // 方向一貫性
    int positive_count = 0;
    int negative_count = 0;

    for (int i = 0; i < count; i++) {
        if (x_t1_values[i] > 0)
            positive_count++;
        else if (x_t1_values[i] < 0)
            negative_count++;
    }

    int max_direction = (positive_count > negative_count) ? positive_count : negative_count;
    double direction_concentration = (double)max_direction / count;

    // スコア計算
    double score = (support_rate * 1000.0 * fabs(mean_t1) * direction_concentration) / sigma_t1;

    return score;
}
```

---

## スコアの解釈

### Score_2D の範囲と意味 (USDJPY日足データの場合)

| スコア範囲 | 評価 | 象限集中度 | 意味 |
|-----------|------|-----------|------|
| **3.0 - 3.5** | 優秀 | 60-70% | 明確な2次元パターン |
| **2.0 - 3.0** | 良好 | 50-60% | 実用的なパターン |
| **1.0 - 2.0** | 普通 | 40-50% | やや弱いパターン |
| **< 1.0** | 弱い | < 40% | ランダムに近い |

### Score_Temporal の範囲と意味

| スコア範囲 | 評価 | 方向一貫性 | 意味 |
|-----------|------|-----------|------|
| **> 4.0** | 優秀 | > 75% | 明確な方向予測 |
| **2.0 - 4.0** | 良好 | 60-75% | 実用的な予測 |
| **1.0 - 2.0** | 普通 | 50-60% | やや弱い予測 |
| **< 1.0** | 弱い | < 50% | ランダム |

---

## スコアの各要素の寄与分析

### 相関係数 (Score_2D との関係)

```
Concentration:  +0.738  (最重要)
Sigma:          -0.560  (低分散が重要)
|Mean|:         +0.485  (適度に重要)
Support_rate:   +0.092  (補助的)
```

**解釈:**
- **集中度が最重要**: 象限に偏っているかどうかがスコアを決定
- **低分散も重要**: tight clusterであることが必要
- **平均値は補助的**: 極端に大きい必要はない
- **サポート率は補正項**: 基準を満たしていれば影響小

---

## support_rate vs support_count の違い

### なぜ support_rate を使うか

**support_rate の定義:**
```
support_rate = マッチ数 / 全データ数
```

**利点:**
1. **データセットに依存しない**: 異なるデータ期間でも比較可能
2. **希少性を反映**: 全体の何%で発生するかを示す
3. **統計的意味が明確**: サンプリング確率として解釈可能

**例:**
```
USDJPY日足データ (約4,100営業日)

ルールA: 15回マッチ → support_rate = 15/4100 = 0.0037 (0.37%)
ルールB: 30回マッチ → support_rate = 30/4100 = 0.0073 (0.73%)

→ ルールBはルールAの2倍の頻度で発生 (絶対値ではなく相対比較が重要)
```

### スケーリング係数 (×1000) の理由

support_rate は 0.003-0.008 程度の小さな値なので、そのまま使うとスコアが極端に小さくなる。
1000倍することで 3-8 の範囲に調整し、他の要素とバランスを取る。

```
調整前: score = 0.0074 × 0.296 × 0.667 / 0.419 = 0.00349
調整後: score = 7.4 × 0.296 × 0.667 / 0.419 = 3.49
```

---

## 実践例: Top 3ルールの比較

### Rank 1: Rule #010 (Score_2D = 3.49)

```
パターン: CADJPY_Up(t-8) AND NZDUSD_Stay(t-1)

データ:
  Support rate:  0.0074 (0.74%)
  Mean(t+1):    -0.329%
  Mean(t+2):    +0.263%
  Sigma(t+1):    0.498%
  Sigma(t+2):    0.340%

象限分布 (15回):
  ++:  2回 (13%)
  +-:  1回 ( 7%)
  -+: 10回 (67%) ← 最大
  --:  2回 (13%)

スコア計算:
  mean_avg = (0.329 + 0.263) / 2 = 0.296%
  sigma_avg = (0.498 + 0.340) / 2 = 0.419%
  quadrant_concentration = 10/15 = 0.667

  Score_2D = 7.4 × 0.296 × 0.667 / 0.419 = 3.49

解釈:
  このパターンが出現 → 67%の確率で「t+1下落、t+2反発」
```

### Rank 2: Rule #001 (Score_2D = 3.13)

```
パターン: NZDUSD_Stay(t-1) AND GBPCAD_Up(t-3)

データ:
  Support rate:  0.0073 (0.73%)
  Mean(t+1):    -0.358%
  Mean(t+2):    +0.229%
  Sigma(t+1):    0.438%
  Sigma(t+2):    0.383%

象限分布 (15回):
  ++:  2回 (13%)
  +-:  1回 ( 7%)
  -+:  9回 (60%) ← 最大
  --:  3回 (20%)

スコア計算:
  mean_avg = (0.358 + 0.229) / 2 = 0.294%
  sigma_avg = (0.438 + 0.383) / 2 = 0.410%
  quadrant_concentration = 9/15 = 0.600

  Score_2D = 7.3 × 0.294 × 0.600 / 0.410 = 3.13

方向一貫性 (X(t+1)のみ):
  正: 3回 (20%)
  負: 12回 (80%) ← 強い下落傾向

  Score_Temporal = 7.3 × 0.358 × 0.800 / 0.438 = 4.78
```

---

## まとめ

### スコア式の選択

```
X(t+1) vs X(t+2) 散布図 → Score_2D
X(t+1) vs T 散布図      → Score_Temporal
```

### 推奨閾値

**高品質ルール:**
- Score_2D > 2.5 または Score_Temporal > 3.5

**実用可能ルール:**
- Score_2D > 1.5 または Score_Temporal > 2.0

**除外すべきルール:**
- Score_2D < 1.0 または Score_Temporal < 1.0

### 次のステップ

1. ルールプールにスコア列を追加
2. スコア順にソート・ランキング
3. 可視化レポートの自動生成
4. GNP進化のフィットネス関数への統合
