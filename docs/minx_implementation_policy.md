# Minx/Maxx (最小/最大リターン閾値) 実装方針書 - 4 象限対応版

## 1. 概要

### 1.1 目的

**Minx/Maxx (Minimum/Maximum X threshold)** は、ルールがマッチした全てのデータポイントにおける未来リターン値の最小値・最大値に対する閾値です。この閾値を 4 つの象限それぞれに適用することで、象限をまたぐような不安定なルールを排除し、一貫した方向性を持つパターンのみを発見します。

**4 象限すべてに対応**:

- 第 1 象限 (++): 一貫した上昇パターン
- 第 2 象限 (-+): リバウンドパターン（下落 → 上昇）
- 第 3 象限 (--): 一貫した下落パターン
- 第 4 象限 (+-): リバーサルパターン（上昇 → 下落）

### 1.2 従来の問題点

現在のフィルタリングシステム（Support, Minmean, Maxsigx）では：

```
例: あるルールが180回マッチ
  X(t+1) の分布: [+0.8%, +0.5%, +0.3%, ..., -0.2%, +0.1%]
  平均値: +0.12% → Minmean (0.05%) をクリア ✓

  しかし: 一部のポイントが負の値 (-0.2%) を含む
  → 象限をまたいでいる（第1象限から第4象限へ）
  → パターンの一貫性が低い
```

**Minx/Maxx 導入効果**:

- 全てのマッチポイントが特定の象限内に収まることを保証
- 4 つの象限すべてで意味のあるパターンを発見
- 各象限の経済的解釈が明確（トレンド、リバウンド、リバーサル）

---

## 2. 定義と数式

### 2.1 基本的な統計量の定義

ルールが時刻 `{i₁, i₂, ..., iₙ}` でマッチした場合：

```
Min_t1 = min{X[i₁+1], X[i₂+1], ..., X[iₙ+1]}  (t+1 の最小値)
Min_t2 = min{X[i₁+2], X[i₂+2], ..., X[iₙ+2]}  (t+2 の最小値)

Max_t1 = max{X[i₁+1], X[i₂+1], ..., X[iₙ+1]}  (t+1 の最大値)
Max_t2 = max{X[i₁+2], X[i₂+2], ..., X[iₙ+2]}  (t+2 の最大値)
```

### 2.2 4 象限の定義と閾値

#### 第 1 象限 (++): 一貫した上昇

```c
#define Minx_Q1_Positive 0.10

条件: (Min_t1 >= +0.10%) AND (Min_t2 >= +0.10%)
意味: 全てのポイントで t+1 も t+2 も正
解釈: 強い上昇トレンドの継続
```

#### 第 2 象限 (-+): リバウンド（下落 → 上昇）

```c
#define Maxx_Q2_T1 -0.10
#define Minx_Q2_T2 0.10

条件: (Max_t1 <= -0.10%) AND (Min_t2 >= +0.10%)
意味: t+1 は全て負、t+2 は全て正
解釈: 下落後の反発、逆張りエントリーポイント
```

#### 第 3 象限 (--): 一貫した下落

```c
#define Maxx_Q3_Negative -0.10

条件: (Max_t1 <= -0.10%) AND (Max_t2 <= -0.10%)
意味: 全てのポイントで t+1 も t+2 も負
解釈: 強い下落トレンドの継続
```

#### 第 4 象限 (+-): リバーサル（上昇 → 下落）

```c
#define Minx_Q4_T1 0.10
#define Maxx_Q4_T2 -0.10

条件: (Min_t1 >= +0.10%) AND (Max_t2 <= -0.10%)
意味: t+1 は全て正、t+2 は全て負
解釈: 上昇後の反落、利確ポイント
```

### 2.3 統計量の役割

| 指標      | 意味     | 役割                           | 計算方法              |
| --------- | -------- | ------------------------------ | --------------------- |
| **Mean**  | 平均値   | 参考情報（計算のみ、閾値なし） | `Σ X / n`             |
| **Min**   | 最小値   | 象限判定（下限）               | `min(X)`              |
| **Max**   | 最大値   | 象限判定（上限）               | `max(X)`              |
| **Sigma** | 標準偏差 | 分散制御                       | `sqrt(Σ(X-μ)²/(n-1))` |

### 2.4 具体例

**例 1: 第 1 象限パターン（一貫した上昇）**

```
ルールA: 10回マッチ
  X(t+1) = [+1.2%, +0.8%, +0.5%, +0.3%, +0.2%, +0.15%, +0.12%, +0.11%, +0.15%, +0.13%]
  X(t+2) = [+1.5%, +0.9%, +0.6%, +0.4%, +0.25%, +0.18%, +0.14%, +0.13%, +0.17%, +0.15%]

  Min_t1 = +0.11%, Min_t2 = +0.13% → 第1象限の閾値をクリア ✓
  Max_t1 = +1.2%, Max_t2 = +1.5%

  判定: 第1象限 (++) - 一貫した上昇パターン
```

**例 2: 第 2 象限パターン（リバウンド）**

```
ルールB: 8回マッチ
  X(t+1) = [-0.8%, -0.5%, -0.3%, -0.2%, -0.15%, -0.12%, -0.11%, -0.25%]
  X(t+2) = [+1.2%, +0.8%, +0.5%, +0.3%, +0.2%, +0.15%, +0.12%, +0.4%]

  Max_t1 = -0.11% (<= -0.10%) ✓, Min_t2 = +0.12% (>= +0.10%) ✓

  判定: 第2象限 (-+) - リバウンドパターン（逆張りポイント）
```

**例 3: 第 3 象限パターン（一貫した下落）**

```
ルールC: 12回マッチ
  X(t+1) = [-1.2%, -0.8%, -0.5%, -0.3%, -0.2%, -0.15%, -0.12%, -0.11%, -0.15%, -0.13%, -0.18%, -0.22%]
  X(t+2) = [-1.5%, -0.9%, -0.6%, -0.4%, -0.25%, -0.18%, -0.14%, -0.13%, -0.17%, -0.15%, -0.21%, -0.25%]

  Max_t1 = -0.11% (<= -0.10%) ✓, Max_t2 = -0.13% (<= -0.10%) ✓

  判定: 第3象限 (--) - 一貫した下落パターン
```

**例 4: 象限をまたぐパターン（不採用）**

```
ルールD: 10回マッチ
  X(t+1) = [+0.8%, +0.5%, +0.3%, -0.2%, +0.1%, +0.15%, -0.05%, +0.12%, +0.08%, +0.06%]
  X(t+2) = [+0.9%, +0.6%, +0.4%, +0.1%, +0.15%, +0.18%, +0.08%, +0.14%, +0.11%, +0.09%]

  Min_t1 = -0.2% (< +0.10%) ✗ → 第1象限の条件を満たさない
  Max_t1 = +0.8% (> -0.10%) ✗ → 第3象限の条件も満たさない

  判定: どの象限にも該当しない（不採用）
```

---

## 3. 閾値の推奨値

### 3.1 基準データの統計量

**USDJPY 時間足データ (17,482 レコード)**:

- 平均 (μ): 0.0077%
- 標準偏差 (σ): 0.5121%
- 分布: ほぼ正規分布、対称

### 3.2 最終的な閾値設定（ユーザー決定）

```c
// 1. Support閾値（サンプル数の確保）
#define Minsup 0.001                    // 最小支持度: 0.1%

// 2. 象限判定閾値（単一定義で4象限すべてに対応）
#define QUADRANT_THRESHOLD 0.0005       // 象限判定: 0.05% (絶対値)

// 3. Sigma閾値（小集団 = tight cluster）
#define Maxsigx 0.001                   // 最大分散: 0.1%
```

### 3.3 4 象限すべてに同じ閾値を適用

```c
// QUADRANT_THRESHOLD を使用した象限判定

int determine_quadrant(double min_t1, double max_t1, double min_t2, double max_t2)
{
    // 第1象限 (++): 両方とも正
    if (min_t1 >= QUADRANT_THRESHOLD && min_t2 >= QUADRANT_THRESHOLD) {
        return 1;
    }

    // 第3象限 (--): 両方とも負
    if (max_t1 <= -QUADRANT_THRESHOLD && max_t2 <= -QUADRANT_THRESHOLD) {
        return 3;
    }

    // 第2象限 (-+): t+1 が負、t+2 が正
    if (max_t1 <= -QUADRANT_THRESHOLD && min_t2 >= QUADRANT_THRESHOLD) {
        return 2;
    }

    // 第4象限 (+-): t+1 が正、t+2 が負
    if (min_t1 >= QUADRANT_THRESHOLD && max_t2 <= -QUADRANT_THRESHOLD) {
        return 4;
    }

    return 0;  // どの象限にも該当しない
}
```

**設計理念**:

- 単一の閾値定義で 4 象限すべてに対応（コードの簡潔化）
- 正負で対称的な閾値を使用（上昇・下落パターンを公平に評価）

### 3.4 閾値選択の根拠

| 閾値               | 値     | 単位 | 意味                                 |
| ------------------ | ------ | ---- | ------------------------------------ |
| Minsup             | 0.001  | 比率 | 0.1% = 評価可能範囲の 0.1%以上       |
| QUADRANT_THRESHOLD | 0.0005 | 小数 | 0.05% = 象限判定の基準               |
| Maxsigx            | 0.001  | 小数 | 0.1% = 小集団（tight cluster）の基準 |

**想定される発見ルール数** (これらの設定で):

- 50-300 ルール (全象限合計)
- 非常に厳しい設定（高品質なパターンのみ）

---

## 4. 適応度関数の設計（オリジナルコード準拠）

### 4.1 設計方針

**オリジナルコード（backups/既存コードの修正たち/original_code.c）に準拠**し、シンプルな適応度関数を採用する。

### 4.2 オリジナルコードの適応度関数（参考）

```c
// Line 562: 重複ルールの場合
fitness[nk] = fitness[nk] + j2 + supp*10 + 2/(sigmadx+0.1) + 2/(sigmady+0.1);

// Line 623: 新規ルールの場合
fitness[nk] = fitness[nk] + j2 + supp*10 + 2/(sigmadx+0.1) + 2/(sigmady+0.1) + 20;
```

**構成要素**:

1. `j2`: 属性数
2. `supp*10`: Support 率 × 10
3. `2/(sigmadx+0.1)`: X 軸の分散ペナルティ
4. `2/(sigmady+0.1)`: Y 軸の分散ペナルティ
5. `+20`: 新規ルール発見ボーナス

### 4.3 新システムの適応度関数

**時系列予測対応版**（X 軸・Y 軸 → t+1・t+2）

```c
// 新規ルールの場合
fitness_value[individual] +=
    num_attributes +                                           // 属性数
    support * FITNESS_SUPPORT_WEIGHT +                         // Support
    FITNESS_SIGMA_WEIGHT / (sigma_t1 + FITNESS_SIGMA_OFFSET) + // t+1 分散ペナルティ
    FITNESS_SIGMA_WEIGHT / (sigma_t2 + FITNESS_SIGMA_OFFSET) + // t+2 分散ペナルティ
    FITNESS_NEW_RULE_BONUS;                                    // 新規ルールボーナス

// 重複ルールの場合（新規ボーナスなし）
fitness_value[individual] +=
    num_attributes +
    support * FITNESS_SUPPORT_WEIGHT +
    FITNESS_SIGMA_WEIGHT / (sigma_t1 + FITNESS_SIGMA_OFFSET) +
    FITNESS_SIGMA_WEIGHT / (sigma_t2 + FITNESS_SIGMA_OFFSET);
```

### 4.4 パラメータ定義

```c
// 適応度関数のウェイト（オリジナル準拠）
#define FITNESS_SUPPORT_WEIGHT 10         // Support の重み（オリジナル: 10）
#define FITNESS_SIGMA_WEIGHT 2            // Sigma ペナルティの重み（オリジナル: 2）
#define FITNESS_SIGMA_OFFSET 0.001        // 0.1% (Maxsigx と同じ)
#define FITNESS_NEW_RULE_BONUS 20         // 新規ルール発見ボーナス（オリジナル: 20）
```

### 4.5 削除する複雑な要素

**現在のコードから削除するもの**:

```c
// ❌ 削除するマクロ定義
#define FITNESS_ATTRIBUTE_WEIGHT 2
#define HIGH_SUPPORT_BONUS 0.02
#define LOW_VARIANCE_REDUCTION 1.0
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_1 0.3
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_2 0.5
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_3 0.8
#define STATISTICAL_SIGNIFICANCE_BONUS_1 800
#define STATISTICAL_SIGNIFICANCE_BONUS_2 2500
#define STATISTICAL_SIGNIFICANCE_BONUS_3 6000
#define CONCENTRATION_THRESHOLD_1 0.35
#define CONCENTRATION_THRESHOLD_2 0.40
#define CONCENTRATION_THRESHOLD_3 0.45
#define CONCENTRATION_THRESHOLD_4 0.50
#define CONCENTRATION_THRESHOLD_5 0.55
#define CONCENTRATION_BONUS_1 1000
#define CONCENTRATION_BONUS_2 2000
#define CONCENTRATION_BONUS_3 3000
#define CONCENTRATION_BONUS_4 4500
#define CONCENTRATION_BONUS_5 6000
#define SMALL_CLUSTER_THRESHOLD_1 0.0020
#define SMALL_CLUSTER_THRESHOLD_2 0.0015
#define SMALL_CLUSTER_THRESHOLD_3 0.0010
#define SMALL_CLUSTER_BONUS_1 300
#define SMALL_CLUSTER_BONUS_2 600
#define SMALL_CLUSTER_BONUS_3 1000

// ❌ 削除する関数
static double calculate_significance_bonus(double mean_value);
static double calculate_small_cluster_bonus(double support);
static double calculate_concentration_fitness_bonus(double concentration_ratio);
```

**理由**:

- 象限フィルタで方向性・集中度は保証済み
- Mean は参考情報のみ（閾値に使わない）
- オリジナルのシンプルさに回帰

### 4.6 適応度関数の比較

| 要素      | オリジナル      | 現在の複雑版    | 新システム        | 変更理由       |
| --------- | --------------- | --------------- | ----------------- | -------------- |
| 属性数    | j2              | j2 × 2          | num_attributes    | オリジナル準拠 |
| Support   | supp × 10       | support × 50    | support × 10      | オリジナル準拠 |
| 分散(X)   | 2/(sigmadx+0.1) | 800/(sigma+0.1) | -                 | Y 軸削除       |
| 分散(t+1) | -               | -               | 2/(sigma_t1+0.1%) | 時系列対応     |
| 分散(t+2) | -               | -               | 2/(sigma_t2+0.1%) | 時系列対応     |
| 新規      | +20             | +50             | +20               | オリジナル準拠 |
| 集中度    | なし            | 0-6000          | なし              | フィルタで代替 |
| 有意性    | なし            | 0-6000          | なし              | フィルタで代替 |
| 小集団    | なし            | 0-1000          | なし              | フィルタで代替 |

### 4.7 適応度計算の具体例

```
ルールA（新規、4属性、Support=0.15%、sigma_t1=0.08%、sigma_t2=0.09%）

fitness = 4 +                        // 属性数
          0.0015 × 10 +              // Support = 0.015
          2 / (0.0008 + 0.001) +     // t+1分散 = 2 / 0.0018 = 1111
          2 / (0.0009 + 0.001) +     // t+2分散 = 2 / 0.0019 = 1053
          20                         // 新規ボーナス
        = 4 + 0.015 + 1111 + 1053 + 20
        = 2188.015
```

### 4.8 設計思想

**フィルタと適応度の分離**:

- **フィルタ**: 質的な基準（象限、分散閾値）→ 採用/不採用を決定
- **適応度**: 量的な評価（分散の小ささ）→ 進化の方向性を決定

**シンプルさの重視**:

- オリジナルコードの 4 項目 + 新規ボーナス
- 複雑なボーナス計算を排除
- 理解しやすく、調整しやすい

---

## 5. 実装仕様

### 5.1 新規関数の追加

#### 5.1.1 最小値計算関数

**関数名**: `calculate_minimum_return()`

**配置場所**: `main.c` の約 1760 行目（`calculate_stddev()` の後）

**仕様**:

```c
/**
 * @brief マッチした全ポイントにおける未来リターンの最小値を計算
 *
 * @param matched_count マッチ数
 * @param matched_indices マッチした時刻インデックスの配列
 * @param future_offset 未来オフセット (1=t+1, 2=t+2)
 * @return double 最小値（絶対値ではなく符号付き）
 *
 * @note 負の値も返す（象限判定に必要）
 * @note matched_count が 0 の場合は 0.0 を返す
 */
double calculate_minimum_return(int matched_count, int *matched_indices, int future_offset)
{
    if (matched_count == 0) {
        return 0.0;
    }

    double min_value = 999999.0;  // 初期値は十分大きな値

    for (int i = 0; i < matched_count; i++) {
        int time_index = matched_indices[i];
        double future_value = get_future_target_value(time_index, future_offset);

        if (future_value < min_value) {
            min_value = future_value;
        }
    }

    return min_value;
}
```

#### 4.1.2 最大値計算関数

**関数名**: `calculate_maximum_return()`

**配置場所**: `calculate_minimum_return()` の直後

**仕様**:

```c
/**
 * @brief マッチした全ポイントにおける未来リターンの最大値を計算
 *
 * @param matched_count マッチ数
 * @param matched_indices マッチした時刻インデックスの配列
 * @param future_offset 未来オフセット (1=t+1, 2=t+2)
 * @return double 最大値（絶対値ではなく符号付き）
 *
 * @note matched_count が 0 の場合は 0.0 を返す
 */
double calculate_maximum_return(int matched_count, int *matched_indices, int future_offset)
{
    if (matched_count == 0) {
        return 0.0;
    }

    double max_value = -999999.0;  // 初期値は十分小さな値

    for (int i = 0; i < matched_count; i++) {
        int time_index = matched_indices[i];
        double future_value = get_future_target_value(time_index, future_offset);

        if (future_value > max_value) {
            max_value = future_value;
        }
    }

    return max_value;
}
```

#### 4.1.3 象限判定関数

**関数名**: `determine_quadrant()`

**配置場所**: `calculate_maximum_return()` の直後

**仕様**:

```c
/**
 * @brief ルールが属する象限を判定
 *
 * @param min_t1 t+1 の最小値
 * @param max_t1 t+1 の最大値
 * @param min_t2 t+2 の最小値
 * @param max_t2 t+2 の最大値
 * @return int 象限番号 (1, 2, 3, 4) または 0 (該当なし)
 *
 * @note 複数の象限条件を満たす場合、優先順位: Q1 > Q3 > Q2 > Q4
 */
int determine_quadrant(double min_t1, double max_t1, double min_t2, double max_t2)
{
    // 第1象限 (++): 両方とも正
    if (min_t1 >= Minx_Q1_Positive && min_t2 >= Minx_Q1_Positive) {
        return 1;
    }

    // 第3象限 (--): 両方とも負
    if (max_t1 <= Maxx_Q3_Negative && max_t2 <= Maxx_Q3_Negative) {
        return 3;
    }

    // 第2象限 (-+): t+1 が負、t+2 が正
    if (max_t1 <= Maxx_Q2_T1 && min_t2 >= Minx_Q2_T2) {
        return 2;
    }

    // 第4象限 (+-): t+1 が正、t+2 が負
    if (min_t1 >= Minx_Q4_T1 && max_t2 <= Maxx_Q4_T2) {
        return 4;
    }

    // どの象限にも該当しない
    return 0;
}
```

#### 4.1.4 象限名取得関数

**関数名**: `get_quadrant_name()`

**配置場所**: `determine_quadrant()` の直後

**仕様**:

```c
/**
 * @brief 象限番号から象限名を取得
 *
 * @param quadrant 象限番号 (1, 2, 3, 4)
 * @return const char* 象限名
 */
const char* get_quadrant_name(int quadrant)
{
    switch (quadrant) {
        case 1: return "Q1 (Both Up)";
        case 2: return "Q2 (Rebound: Down->Up)";
        case 3: return "Q3 (Both Down)";
        case 4: return "Q4 (Reversal: Up->Down)";
        default: return "N/A (No Quadrant)";
    }
}
```

### 4.2 フィルタリングロジックの変更

**変更箇所**: `check_rule_quality()` 関数（約 1833 行目）

**現在のフィルタ順序**:

```
1. Support フィルタ    (0.1%)
2. Mean フィルタ       (0.05%)
3. Sigma フィルタ      (0.5%)
```

**新しいフィルタ順序**:

```
1. Support フィルタ       (0.1%)   ← 既存
2. 象限判定フィルタ       (0.10%)  ← 新規（4象限対応）
3. Sigma フィルタ         (0.5%)   ← 既存
```

**注意**: Mean（平均値）は計算して出力するが、フィルタリングには使用しない

**実装コード**:

```c
bool check_rule_quality(int individual, int pn_index, int depth)
{
    // ... 既存のコード（マッチ数取得、Support計算）...

    // ===== ステップ1: Support フィルタ（既存）=====
    if (support < Minsup || matched_count < MIN_SUPPORT_COUNT) {
        return false;
    }

    // ===== ステップ2: 象限判定フィルタ（新規、4象限対応）=====
    // 最小値・最大値を計算
    double min_t1 = calculate_minimum_return(matched_count, matched_indices, 1);
    double max_t1 = calculate_maximum_return(matched_count, matched_indices, 1);
    double min_t2 = calculate_minimum_return(matched_count, matched_indices, 2);
    double max_t2 = calculate_maximum_return(matched_count, matched_indices, 2);

    // 象限判定
    int quadrant = determine_quadrant(min_t1, max_t1, min_t2, max_t2);

    if (quadrant == 0) {
        return false;  // どの象限にも該当しない（不採用）
    }

    // ===== ステップ3: Sigma フィルタ（既存）=====
    // Meanは計算するが閾値チェックには使わない（出力用のみ）
    double mean_t1 = calculate_mean(matched_count, matched_indices, 1);
    double mean_t2 = calculate_mean(matched_count, matched_indices, 2);

    double sigma_t1 = calculate_stddev(matched_count, matched_indices, 1, mean_t1);
    double sigma_t2 = calculate_stddev(matched_count, matched_indices, 2, mean_t2);

    if (sigma_t1 > Maxsigx || sigma_t2 > Maxsigx) {
        return false;  // 分散が大きすぎる
    }

    // ===== 象限情報を記録（出力用）=====
    // グローバル変数または構造体に保存
    // rule_quadrant[individual][pn_index] = quadrant;
    // rule_min_t1[individual][pn_index] = min_t1;
    // rule_max_t1[individual][pn_index] = max_t1;
    // rule_min_t2[individual][pn_index] = min_t2;
    // rule_max_t2[individual][pn_index] = max_t2;

    return true;  // 全てのフィルタをクリア
}
```

### 4.3 フィルタ順序の理由

**計算コストの観点**:

| フィルタ | 計算量      | 通過率  | 累積コスト | 用途                   |
| -------- | ----------- | ------- | ---------- | ---------------------- |
| Support  | O(1)        | ~30%    | 低         | サンプル数の確保       |
| 象限判定 | 4×O(n)      | ~20-40% | 中         | 4 象限いずれかに該当   |
| Sigma    | O(n) + sqrt | ~70%    | 高         | 分散の制御             |
| Mean     | O(n)        | 100%    | -          | 出力用（フィルタなし） |

- **Support**: 最も高速、大半のルールを排除
- **象限判定**: Min/Max 計算（4 回）+ 象限判定ロジック
- **Sigma**: 最も高コスト（分散計算+平方根）、分散制御
- **Mean**: 計算のみ（フィルタには使わない）

**最適な順序**: Support → 象限判定 → Sigma → (Mean は参考情報)

**通過率の説明**:

- 4 象限すべてを対象とするため、通過率は従来より上昇
- 第 1 象限のみなら ~10-15%、4 象限対応で ~20-40% に増加

---

## 5. 出力形式の拡張

### 5.1 ルール出力への追加情報

**変更箇所**: `rule_save()` 関数（約 3187 行目）

**現在の出力形式**:

```
=== Rule 42 ===
Support: 0.0234 (2.34%, 180 matches)
Avg[1]= 0.000735 (0.0735%), Sig[1]= 0.003521 (0.3521%)
Avg[2]= 0.000842 (0.0842%), Sig[2]= 0.003158 (0.3158%)
```

**新しい出力形式**:

```
=== Rule 42 ===
Support: 0.0234 (2.34%, 180 matches)
Quadrant: Q1 (Both Up)
Avg[1]= 0.000735 (0.0735%), Sig[1]= 0.003521 (0.3521%)
  Min[1]= 0.001200 (0.1200%), Max[1]= 0.002500 (0.2500%)
Avg[2]= 0.000842 (0.0842%), Sig[2]= 0.003158 (0.3158%)
  Min[2]= 0.001050 (0.1050%), Max[2]= 0.002800 (0.2800%)
```

**実装コード**:

```c
// rule_save() 内で:

// 象限情報の出力
fprintf(fp, "Quadrant: %s\n", get_quadrant_name(quadrant));

// 各 future_offset (1, 2) について統計値を出力
for (int offset = 1; offset <= FUTURE_SPAN; offset++) {
    fprintf(fp, "Avg[%d]= %.6f (%.4f%%), Sig[%d]= %.6f (%.4f%%)\n",
            offset, mean_val, mean_val * 100.0,
            offset, sigma_val, sigma_val * 100.0);

    fprintf(fp, "  Min[%d]= %.6f (%.4f%%), Max[%d]= %.6f (%.4f%%)\n",
            offset, min_val, min_val * 100.0,
            offset, max_val, max_val * 100.0);
}
```

### 5.2 pool/~.txt への Min/Max 列追加

**pool/zrp01a.txt の出力形式**:

**現在**:

```
Rule_ID,Support,Mean_t1,Sigma_t1,Mean_t2,Sigma_t2,Attributes
1,0.0234,0.000735,0.003521,0.000842,0.003158,"BTC_Up USDT_Up"
```

**新規**:

```
Rule_ID,Support,Quadrant,Mean_t1,Sigma_t1,Min_t1,Max_t1,Mean_t2,Sigma_t2,Min_t2,Max_t2,Attributes
1,0.0234,Q1,0.000735,0.003521,0.001200,0.002500,0.000842,0.003158,0.001050,0.002800,"BTC_Up USDT_Up"
```

**追加列**:

- `Quadrant`: 象限番号（Q1, Q2, Q3, Q4）
- `Min_t1`, `Max_t1`: t+1 の最小値・最大値
- `Min_t2`, `Max_t2`: t+2 の最小値・最大値

### 5.3 CSV 検証ファイルへの追加

**検証用 CSV ファイル** (`output/verification/rule_XXXX.csv`) にも統計情報を追加:

```csv
# Rule ID: 42
# Quadrant: Q1 (Both Up)
# Support: 0.0234 (2.34%, 180 matches)
# Mean_t1=0.0735%, Sigma_t1=0.3521%, Min_t1=0.1200%, Max_t1=0.2500%
# Mean_t2=0.0842%, Sigma_t2=0.3158%, Min_t2=0.1050%, Max_t2=0.2800%
time_index,X_t0,X_t1,X_t2,pattern
10,0.15,0.80,0.90,"BTC_Up USDT_Up"
25,0.08,0.30,0.50,"BTC_Up USDT_Up"
...
```

---

## 6. 期待される効果

### 6.1 発見ルール数の変化

**現状** (Minmean=0.05%, 象限フィルタなし):

- 発見ルール数: 6,715 個
- 平均値: 0.077%
- 問題点: 象限をまたぐルールが含まれる

**4 象限対応導入後** (閾値=0.10%):

- 推定ルール数: **500-1,500 個（全象限合計）**
- 象限別内訳:
  - 第 1 象限 (++): 200-600 ルール
  - 第 2 象限 (-+): 100-300 ルール
  - 第 3 象限 (--): 200-600 ルール
  - 第 4 象限 (+-): 100-300 ルール
- 全てのルールが特定の象限内で一貫
- 研究価値: 高（4 種類のパターンを発見）

### 6.2 パターンの質的向上

```
Before (象限フィルタなし):
  ルールA: 平均 +0.12%, 最小 -0.15%, 最大 +0.45%
  → 一部のポイントで下落（象限をまたぐ）

After (第1象限):
  ルールB: 平均 +0.35%, 最小 +0.12%, 最大 +0.68%
  → 全てのポイントで上昇（第1象限内で一貫）

After (第2象限):
  ルールC: 平均 +0.02%, 最小(t+1) -0.25%, 最大(t+1) -0.11%
                         最小(t+2) +0.15%, 最大(t+2) +0.52%
  → リバウンドパターン（下落後の反発）

After (第3象限):
  ルールD: 平均 -0.32%, 最小 -0.58%, 最大 -0.13%
  → 全てのポイントで下落（第3象限内で一貫）
```

### 6.3 統計的解釈の明確化

| 指標              | 意味           | 統計的解釈       | フィルタ使用         |
| ----------------- | -------------- | ---------------- | -------------------- |
| **Mean**          | 平均的な傾向   | 中心傾向のシフト | なし（出力のみ）     |
| **Min/Max**       | 最小値・最大値 | 分布の範囲制約   | **あり（象限判定）** |
| **Sigma <= 0.5%** | ばらつきの制御 | 分散の上限制約   | あり                 |
| **Quadrant**      | 象限分類       | パターンタイプ   | 判定結果を出力       |

**重要**: Min/Max が象限判定の主要な基準となり、Mean は参考情報として計算・出力される。

### 6.4 4 象限パターンの経済的解釈

| 象限    | パターン名           | 意味               | トレーディング戦略           |
| ------- | -------------------- | ------------------ | ---------------------------- |
| Q1 (++) | トレンド継続（上昇） | 強い上昇モメンタム | 順張り買い                   |
| Q2 (-+) | リバウンド           | 下落後の反発       | 逆張り買い（押し目買い）     |
| Q3 (--) | トレンド継続（下落） | 強い下落モメンタム | 順張り売り                   |
| Q4 (+-) | リバーサル           | 上昇後の反落       | 逆張り売り（利確・戻り売り） |

---

## 7. 集中度との関係

### 7.1 集中度 (Concentration Ratio)

**定義**: 最多象限のポイント数 / 全マッチ数

```
例: 180回マッチ
  (++) 象限: 145 ポイント
  (+-) 象限: 25 ポイント
  (-+) 象限: 8 ポイント
  (--) 象限: 2 ポイント

  集中度 = 145 / 180 = 80.6%
```

### 7.2 Minx と Concentration の関係

```
Minx が高い (0.20%)
  → 全ポイントが強い正値
  → (++) 象限に自然と集中
  → 集中度は自動的に高くなる (>90%)

Minx が低い (0.05%)
  → ゼロ付近のポイントも許容
  → (++) と他の象限に分散
  → 集中度は中程度 (60-80%)
```

**設計上の選択**:

- Minx: より直感的（最小値という明確な基準）
- Concentration: 間接的（象限分布から計算）

→ **Minx を採用**（実装がシンプルで解釈しやすい）

---

## 8. 実装の優先度

### 8.1 必須の変更（Phase 1）

| No  | 内容                                             | ファイル | 行数  | 優先度 |
| --- | ------------------------------------------------ | -------- | ----- | ------ |
| 1   | Minx 定数の定義                                  | `main.c` | ~19   | 高     |
| 2   | `calculate_minimum_return()` 追加                | `main.c` | ~1760 | 高     |
| 3   | `check_rule_quality()` 修正（Mean フィルタ削除） | `main.c` | ~1850 | 高     |
| 4   | `rule_save()` 出力拡張（Minx 列追加）            | `main.c` | ~3190 | 高     |

**推定作業時間**: 40 分

### 8.2 推奨の変更（Phase 2）

| No  | 内容                      | ファイル    | 行数  | 優先度 |
| --- | ------------------------- | ----------- | ----- | ------ |
| 5   | CSV 出力への最小値追加    | `main.c`    | ~3300 | 中     |
| 6   | pool/~.txt の出力形式更新 | `main.c`    | ~3100 | 中     |
| 7   | README 更新               | `README.md` | -     | 低     |

**推定作業時間**: 20 分

---

## 9. テスト計画

### 9.1 単体テスト

**テスト 1**: `calculate_minimum_return()` の正確性

```c
// テストケース
int test_indices[] = {10, 25, 47};
// X[11]=+0.8, X[26]=+0.3, X[48]=+0.1
double result = calculate_minimum_return(3, test_indices, 1);
// 期待値: 0.001 (0.1%)
```

**テスト 2**: 負の値を含むケース

```c
// X[11]=+0.5, X[26]=-0.2, X[48]=+0.3
double result = calculate_minimum_return(3, test_indices, 1);
// 期待値: -0.002 (-0.2%)
```

### 9.2 統合テスト

**段階的な閾値テスト**:

```bash
# ステップ1: Minx = 0.05% (緩い)
make clean && make
./main BTC 10
# 期待結果: 1,500-3,000 ルール

# ステップ2: Minx = 0.10% (推奨)
# main.c の Minx を変更
make clean && make
./main BTC 10
# 期待結果: 500-1,500 ルール

# ステップ3: Minx = 0.20% (厳しい)
# main.c の Minx を変更
make clean && make
./main BTC 10
# 期待結果: 100-500 ルール
```

### 9.3 検証手順

**発見されたルールの確認**:

```bash
# 1. ルール数の確認
wc -l output/pool/zrp01a.txt

# 2. 統計値の確認（最小値が閾値以上か）
grep "Min\[" output/pool/zrp01a.txt | head -20

# 3. 散布図での視覚的確認
python3 analysis/crypt/plot_single_rule_2d_future.py --rule-id 1 --symbol BTC

# 4. 象限分布の確認（++ 象限の割合）
# 散布図の統計情報を確認
```

---

## 10. リスクと対策

### 10.1 潜在的なリスク

**リスク 1**: ルール数が少なすぎる

- **原因**: Minx 閾値が高すぎる
- **対策**: 0.05% → 0.10% → 0.20% と段階的にテスト

**リスク 2**: 計算時間の増加

- **原因**: 最小値計算の追加
- **影響度**: 低（線形探索は既存の Mean 計算と同等）
- **推定増加時間**: 5-10%

**リスク 3**: 有効なパターンの見逃し

- **原因**: 閾値が厳しすぎて、僅かな負値を含む良いルールを排除
- **対策**: 最初は緩い閾値（0.05%）から開始し、結果を確認

### 10.2 デバッグ手法

**Minx フィルタで排除されたルールの追跡**:

```c
// check_rule_quality() 内でログ出力
if (min_t1 < Minx || min_t2 < Minx) {
    #ifdef DEBUG_MINX
    printf("Rule rejected by Minx: mean_t1=%.4f%%, min_t1=%.4f%%, Minx=%.4f%%\n",
           mean_t1 * 100.0, min_t1 * 100.0, Minx * 100.0);
    #endif
    return false;
}
```

コンパイル時に有効化:

```bash
gcc -DDEBUG_MINX main.c -o main -lm
```

---

## 11. 理論的背景

### 11.1 条件付き分布の制約

**無条件分布** (全データ):

```
P(X) ~ N(μ=0.0077%, σ=0.5121%)
```

**条件付き分布** (ルール R にマッチ):

```
P(X | R) ~ N(μ_R, σ_R)

新フィルタシステム:
  min(X | R) >= 0.10%  (分布の下限制約) ← 主要閾値
  σ_R <= 0.5%          (分散の制約)
  μ_R: 計算のみ        (参考情報として出力)
```

### 11.2 統計的意義

**Minx の効果**:

```
Minx = 0.10% の場合:
  → min(X | R) >= 0.10%
  → 全てのサンプルが μ + 0.18σ 以上
  → 分布が正の方向に完全にシフト
```

これは、**条件 R によって市場構造が変化した証拠**となる。

### 11.3 金融工学的解釈

**リスク管理の観点**:

| 指標  | リスク指標             | 意味         |
| ----- | ---------------------- | ------------ |
| Mean  | 期待リターン           | 平均的な利益 |
| Sigma | リスク（分散）         | 不確実性     |
| Minx  | **最悪ケースリターン** | 下方リスク   |

**Minx の導入 = Value at Risk (VaR) の概念と類似**

```
VaR (5%): 5%の確率で損失がこの値を超える
Minx: 実績上の最小リターン（0%確率の損失）

→ Minx >= 0.10% は、パターン発現時の最悪ケースでも +0.10% 保証
```

---

## 12. 今後の拡張可能性

### 12.1 双方向対応（Phase 3）

現在は正のパターン（上昇）のみを対象としているが、将来的に負のパターン（下落）にも対応可能:

```c
// 正のパターン用
#define Minx_Positive 0.10   // min(X) >= +0.10%

// 負のパターン用
#define Maxx_Negative -0.10  // max(X) <= -0.10%
```

### 12.2 パーセンタイル閾値（Phase 4）

最小値ではなく、下位 5%点などを使用:

```c
// 下位5%点が閾値以上
double percentile_5 = calculate_percentile(matched_count, matched_indices, 1, 0.05);
if (percentile_5 < Minx) {
    return false;
}
```

**効果**: 外れ値に対してロバスト

### 12.3 動的閾値調整（Phase 5）

ボラティリティに応じて閾値を調整:

```c
double adaptive_minx = baseline_minx * (current_volatility / baseline_volatility);
```

**効果**: 市場環境の変化に対応

---

## 13. まとめ

### 13.1 実装の本質

**Minx = 象限判定の主要閾値（全マッチポイントの最小値）**

```
従来: 平均（Minmean）を主要閾値として使用
新規: 最小値（Minx）を主要閾値として使用
      平均は参考情報として計算・出力のみ

効果: 象限をまたがない、一貫したパターンのみを発見
```

### 13.2 研究価値

**統計的有意性**:

- Mean: 中心傾向の変化
- Minx: 分布の完全なシフト
- **→ より強い統計的証拠**

**実用性**:

- トレーディングシグナルとしての信頼性向上
- リスク管理の明確化（最悪ケースの保証）

### 13.3 実装スケジュール

| Phase   | 内容                     | 所要時間 | 完了基準                            |
| ------- | ------------------------ | -------- | ----------------------------------- |
| Phase 1 | 基本実装（4 箇所の修正） | 40 分    | コンパイル成功、pool 出力に Minx 列 |
| Phase 2 | CSV 出力拡張             | 20 分    | 検証 CSV 統計値確認                 |
| Phase 3 | テストと検証             | 1 時間   | 複数閾値で実行                      |

**合計所要時間**: 約 2 時間

**主要変更点**:

1. Minx 定数の追加
2. `calculate_minimum_return()` 関数の追加
3. `check_rule_quality()` で Mean フィルタを削除、Minx フィルタを追加
4. `rule_save()` で pool 出力に Minx 列を追加

---

## 14. 参考資料

### 14.1 関連ドキュメント

- `/docs/rule_discovery_algorithm.md` - ルール発見アルゴリズム全体
- `/docs/support_definition.md` - Support 定義
- `/docs/rule_scoring_system.md` - ルール評価システム
- `CLAUDE.md` - プロジェクト概要と研究目標

### 14.2 関連関数

| 関数名                      | ファイル | 行数 | 関連性              |
| --------------------------- | -------- | ---- | ------------------- |
| `check_rule_quality()`      | `main.c` | 1833 | Minx フィルタを追加 |
| `calculate_mean()`          | `main.c` | 1730 | 類似の実装パターン  |
| `calculate_stddev()`        | `main.c` | 1745 | 類似の実装パターン  |
| `get_future_target_value()` | `main.c` | 1511 | 未来値の取得        |

### 14.3 データフロー

```
1. GNP進化 (200世代)
   ↓
2. ルール評価 (check_rule_quality)
   ├─ Support フィルタ（既存）
   ├─ Minx フィルタ（新規、主要閾値）
   └─ Sigma フィルタ（既存）
   ↓
3. ルール保存 (rule_save)
   └─ 統計値計算・出力 (Mean, Sigma, Min) ← Min列追加
      ├─ pool/~.txt に Minx列追加
      └─ 通常の出力にも Min値追加
   ↓
4. 検証データ出力 (CSV)
   └─ ヘッダに Min統計情報追加
```

---

## 15. 問い合わせ先

実装中の質問や問題が発生した場合:

1. **アルゴリズムの理論**: `docs/rule_discovery_algorithm.md` を参照
2. **統計計算の詳細**: `main.c` の 1700-1800 行目付近
3. **GNP 構造**: `main.c` の冒頭コメントとマクロ定義

---

## 16. 重要な変更点のまとめ

### ユーザー要望による修正

**変更内容**:

1. **Mean（平均値）はフィルタに使わない**

   - 従来: `Min_Mean >= 0.05%` で閾値判定
   - 新規: 計算して出力のみ、閾値チェックなし

2. **Minx（最小値）が主要な閾値**

   - 象限判定の主要な基準
   - `Minx >= 0.10%` で全マッチポイントが象限内にあることを保証

3. **pool/~.txt に Minx 列を追加**
   - 既存: Support, Mean, Sigma
   - 新規: Support, Mean, Sigma, **Min** ← 追加

**フィルタリングシステム**:

```
Support (0.1%) → Minx (0.10%) → Sigma (0.5%)
                    ↑
                主要閾値
```

**出力形式**:

```
=== Rule 42 ===
Support: 0.0234 (2.34%, 180 matches)
Avg[1]= 0.000735 (0.0735%), Sig[1]= 0.003521 (0.3521%), Min[1]= 0.001200 (0.1200%)
Avg[2]= 0.000842 (0.0842%), Sig[2]= 0.003158 (0.3158%), Min[2]= 0.001050 (0.1050%)
                                                         ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
                                                         新規追加
```

---

**文書バージョン**: 2.0 (4 象限対応版)
**作成日**: 2025-11-11
**最終更新**: 2025-11-11
**対象バージョン**: main.c (f6a6bf5 以降)

---

## 17. 実装サマリー

### 必要な変更箇所

| No  | 箇所       | 内容                                       | コード量  |
| --- | ---------- | ------------------------------------------ | --------- |
| 1   | マクロ定義 | 閾値定数の整理（3 つに簡素化）             | 3 行      |
| 2   | マクロ定義 | 適応度関数パラメータの簡素化               | 4 行      |
| 3   | マクロ削除 | 複雑なボーナス定義を削除                   | -30 行    |
| 4   | 関数追加   | `calculate_minimum_return()`               | 15 行     |
| 5   | 関数追加   | `calculate_maximum_return()`               | 15 行     |
| 6   | 関数追加   | `determine_quadrant()`                     | 20 行     |
| 7   | 関数追加   | `get_quadrant_name()`                      | 10 行     |
| 8   | 関数削除   | ボーナス計算関数 3 つを削除                | -30 行    |
| 9   | 関数修正   | `check_rule_quality()` の象限判定追加      | 10 行修正 |
| 10  | 関数修正   | 適応度計算の簡素化                         | 20 行修正 |
| 11  | 関数修正   | `rule_save()` の Min/Max/Quadrant 出力追加 | 15 行修正 |

**合計**: 約 90 行追加、60 行削除 = **正味 30 行増加**

### 主要な設計判断

1. **4 象限すべてを対象**: 第 1 象限のみでなく、全象限で有用なパターンを発見
2. **Min/Max 両方を使用**: 象限判定には最小値と最大値の両方が必要
3. **単一閾値で統一**: QUADRANT_THRESHOLD = 0.05%（正負対称）
4. **Mean は参考情報**: フィルタには使わず、出力のみ
5. **適応度関数の簡素化**: オリジナルコード準拠（4 要素のみ）
6. **複雑なボーナス削除**: 集中度・有意性・小集団ボーナスを全廃
7. **pool 出力に Quadrant 列追加**: 後処理で象限別の分析が可能

### 閾値システムの最終形

```c
// 3つの閾値のみ（シンプル）
#define Minsup 0.001                    // 0.1% Support
#define QUADRANT_THRESHOLD 0.0005       // 0.05% 象限判定
#define Maxsigx 0.001                   // 0.1% 分散
```

### 適応度関数の最終形

```c
// オリジナル準拠（4要素 + 新規ボーナス）
fitness =
    num_attributes +                    // 属性数
    support × 10 +                      // Support
    2 / (sigma_t1 + 0.1%) +            // t+1 分散
    2 / (sigma_t2 + 0.1%) +            // t+2 分散
    20                                  // 新規ルール
```

### 期待される研究成果

- **パターンの多様性**: 4 種類の異なる市場パターンを発見
- **高品質**: 厳しい閾値（0.05%, 0.1%）により高品質なパターンのみ
- **トレーディング戦略**: 順張り（Q1, Q3）と逆張り（Q2, Q4）の両方に対応
- **統計的有意性**: 象限内での一貫性により、条件付き分布の明確なシフトを証明
- **実用性**: 各象限パターンが具体的な市場戦略に対応
- **コードの保守性**: シンプルな設計で理解・調整が容易
