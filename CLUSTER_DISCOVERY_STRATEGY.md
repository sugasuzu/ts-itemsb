# 散布図上の密集した小集団（クラスター）発見戦略

## 目標の明確化

### 理想的なクラスターとは？

```
散布図: X(t+1) vs X(t+2)

     X(t+2)
      |
    ⚫⚫|⚫⚫     ← 密集した円形/正方形の小集団
    ⚫⚫|⚫⚫        (σが小さい、明確な方向性)
      |
------+------ X(t+1)
      |
      |  ・・・  ← 全データ（分散が大きい、ランダム）
```

### 数学的定義

**理想的なクラスター = 以下の3条件を満たす**

1. **低分散（Tight Cluster）**
   ```
   σ_X(t+1) <= 0.3%  かつ  σ_X(t+2) <= 0.3%
   → 点が密集している（円形/正方形）
   ```

2. **明確な方向性（Clear Direction）**
   ```
   |mean_X(t+1)| >= 0.3%  または  |mean_X(t+2)| >= 0.3%
   → ゼロから離れている（予測価値がある）
   ```

3. **十分な観測数（Statistical Reliability）**
   ```
   n >= 10  (最低10回の観測)
   → 統計的に信頼できる
   ```

---

## 現在のGNMiner設定の問題点

### 現在の閾値（main.c）

```c
#define Minsup 0.001              // サポート率: 0.1%
#define Maxsigx 1.0               // 最大σ: 1.0%  ← 緩すぎる！
#define Minmean 0.1               // 最小平均: 0.1%  ← 低すぎる！
#define MIN_CONCENTRATION 0.4     // 象限集中度: 40%
#define MIN_SUPPORT_COUNT 2       // 最小観測数: 2回  ← 少なすぎる！
```

### 問題点の分析

| パラメータ | 現在値 | 問題 | 理想値 |
|-----------|--------|------|--------|
| **Maxsigx** | 1.0% | 分散が大きすぎる → 広く散らばる | **0.35%** |
| **Minmean** | 0.1% | ゼロに近すぎる → 予測価値なし | **0.3%** |
| **MIN_SUPPORT_COUNT** | 2回 | 観測数が少なすぎる → 偶然の可能性 | **10回** |

### 現在の結果（USDJPY 1試行）

```
発見ルール数: 2,000個
問題:
  - σ = 0.4-0.7% → 分散が大きい（広く散らばる）
  - mean ≈ 0.1-0.3% → ゼロに近い（予測価値低い）
  - n = 4-12回 → 観測数が少ない

→ 密集した小集団ではない！
```

---

## 解決策: 閾値の厳格化

### 提案1: Tight Cluster設定（推奨）

```c
// main.c の修正

/* ルールマイニング制約 - Tight Cluster 設定 */
#define Minsup 0.0005             // 0.001 → 0.0005 (希少パターンOK)
#define Maxsigx 0.35              // 1.0 → 0.35 (低分散を要求)
#define Minmean 0.3               // 0.1 → 0.3 (明確な方向性を要求)
#define MIN_CONCENTRATION 0.5     // 0.4 → 0.5 (象限集中度を強化)
#define MIN_SUPPORT_COUNT 10      // 2 → 10 (統計的信頼性)
```

**期待される結果:**
- ルール数: 2,000個 → **50-200個**（厳選）
- σ: 0.4-0.7% → **0.2-0.35%**（密集）
- mean: 0.1-0.3% → **0.3-0.8%**（明確）
- 散布図: **円形/正方形に密集したクラスター**

---

### 提案2: Very Tight Cluster設定（研究用）

```c
// さらに厳格な設定

#define Minsup 0.0003             // 非常に希少でもOK
#define Maxsigx 0.25              // 非常に低分散
#define Minmean 0.5               // 強い方向性
#define MIN_CONCENTRATION 0.6     // 強い象限集中
#define MIN_SUPPORT_COUNT 8       // 希少なので8回でOK
```

**期待される結果:**
- ルール数: **10-50個**（非常に厳選）
- σ: **0.15-0.25%**（非常に密集）
- mean: **0.5-1.0%**（強い方向性）
- 散布図: **ほぼ完全な円形クラスター**

---

## 実装手順

### Step 1: main.c の修正

```bash
# main.c を編集
nano main.c

# 以下の行を修正（35-39行目）
#define Minsup 0.0005             // 変更
#define Maxsigx 0.35              // 変更
#define Minmean 0.3               // 変更
#define MIN_SUPPORT_COUNT 10      // 追加または変更
```

### Step 2: MIN_SUPPORT_COUNT の実装確認

```bash
# main.c でMIN_SUPPORT_COUNTが使われているか確認
grep -n "MIN_SUPPORT_COUNT" main.c
```

もし使われていない場合、`check_rule_quality()` 関数に追加が必要：

```c
int check_rule_quality(...) {
    // 既存のチェック
    if (support_rate < Minsup) return 0;

    // サポート数のチェックを追加
    if (support_count < MIN_SUPPORT_COUNT) return 0;  // ← 追加

    // 以下省略...
}
```

### Step 3: 再コンパイル

```bash
gcc -o main main.c -lm -O2
```

### Step 4: テスト実行

```bash
./main USDJPY 1
```

### Step 5: 結果の確認

```bash
# 発見ルール数を確認
wc -l 1-deta-enginnering/forex_data_daily/output/USDJPY/pool/zrp01a.txt

# ルールの統計を確認
head -20 1-deta-enginnering/forex_data_daily/output/USDJPY/pool/zrp01a.txt
```

---

## 評価指標

### 良いクラスターの判定基準

#### 1. 低分散スコア（Tightness Score）

```python
def calculate_tightness_score(sigma_x1, sigma_x2):
    """
    分散が小さいほど高スコア

    σ <= 0.25%  → スコア 100 (Perfect)
    σ <= 0.35%  → スコア 80  (Good)
    σ <= 0.50%  → スコア 50  (Acceptable)
    σ >  0.50%  → スコア 0   (Poor)
    """
    avg_sigma = (sigma_x1 + sigma_x2) / 2

    if avg_sigma <= 0.25:
        return 100
    elif avg_sigma <= 0.35:
        return 80
    elif avg_sigma <= 0.50:
        return 50
    else:
        return 0
```

#### 2. 方向性スコア（Directional Score）

```python
def calculate_directional_score(mean_x1, mean_x2):
    """
    ゼロから離れているほど高スコア

    |mean| >= 0.5%  → スコア 100
    |mean| >= 0.3%  → スコア 80
    |mean| >= 0.2%  → スコア 50
    |mean| <  0.2%  → スコア 0
    """
    avg_mean = abs((mean_x1 + mean_x2) / 2)

    if avg_mean >= 0.5:
        return 100
    elif avg_mean >= 0.3:
        return 80
    elif avg_mean >= 0.2:
        return 50
    else:
        return 0
```

#### 3. クラスター品質スコア（総合評価）

```python
def calculate_cluster_quality(sigma_x1, sigma_x2, mean_x1, mean_x2, support_count):
    """
    総合品質スコア

    100点満点:
      - Tightness: 40点
      - Directionality: 40点
      - Statistical reliability: 20点
    """
    tightness = calculate_tightness_score(sigma_x1, sigma_x2) * 0.4
    direction = calculate_directional_score(mean_x1, mean_x2) * 0.4
    reliability = min(support_count / 20, 1.0) * 20  # 20回以上で満点

    total_score = tightness + direction + reliability

    # 評価
    if total_score >= 80:
        grade = "A - Excellent Cluster"
    elif total_score >= 60:
        grade = "B - Good Cluster"
    elif total_score >= 40:
        grade = "C - Acceptable"
    else:
        grade = "D - Poor"

    return total_score, grade
```

---

## 可視化による検証

### 発見されたルールの散布図を生成

```python
# analysis/fx/plot_fx_cluster.py を作成

import pandas as pd
import matplotlib.pyplot as plt

def plot_cluster(rule_id):
    """
    特定のルールの散布図を生成
    """
    # ルールのverificationファイルを読み込み
    df = pd.read_csv(f'1-deta-enginnering/forex_data_daily/output/USDJPY/verification/rule_{rule_id}.csv')

    # マッチした点のみ抽出
    matched = df[df['matched'] == 1]

    # 散布図
    plt.figure(figsize=(10, 10))

    # 全データ（グレー、薄く）
    plt.scatter(df['X_t+1'], df['X_t+2'],
                alpha=0.1, s=10, c='gray', label='All data')

    # マッチした点（赤、濃く）
    plt.scatter(matched['X_t+1'], matched['X_t+2'],
                alpha=0.8, s=50, c='red', label=f'Rule {rule_id} matches')

    # 平均を示す十字線
    mean_x1 = matched['X_t+1'].mean()
    mean_x2 = matched['X_t+2'].mean()
    plt.axvline(mean_x1, color='red', linestyle='--', alpha=0.5)
    plt.axhline(mean_x2, color='red', linestyle='--', alpha=0.5)

    # 統計情報
    sigma_x1 = matched['X_t+1'].std()
    sigma_x2 = matched['X_t+2'].std()
    n = len(matched)

    stats_text = f'Rule {rule_id}\n'
    stats_text += f'n = {n}\n'
    stats_text += f'mean(X_t+1) = {mean_x1:.3f}%\n'
    stats_text += f'mean(X_t+2) = {mean_x2:.3f}%\n'
    stats_text += f'σ(X_t+1) = {sigma_x1:.3f}%\n'
    stats_text += f'σ(X_t+2) = {sigma_x2:.3f}%'

    plt.text(0.02, 0.98, stats_text,
             transform=plt.gca().transAxes,
             verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    plt.xlabel('X(t+1)')
    plt.ylabel('X(t+2)')
    plt.title(f'Cluster Visualization - Rule {rule_id}')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.axis('equal')
    plt.tight_layout()
    plt.savefig(f'cluster_rule_{rule_id}.png', dpi=300)
    plt.close()
```

---

## 期待される結果の例

### 良いクラスター（Tight Cluster）

```
Rule #42:
  IF GBPJPY_Down(t-0) AND EURAUD_Stay(t-2) AND GBPCAD_Up(t-0)

  散布図:
       X(t+2)
        |
      ⚫⚫|⚫⚫    ← n=15, σ_x1=0.24%, σ_x2=0.28%
      ⚫⚫|⚫⚫       mean_x1=+0.33%, mean_x2=-0.61%
        |
    ----+---- X(t+1)

  評価:
    - Tightness Score: 100 (σ < 0.25%)
    - Directional Score: 80 (|mean| ≈ 0.47%)
    - Reliability Score: 15 (n=15)
    - Total: 88点 → Grade A "Excellent Cluster"
```

### 悪いクラスター（Scattered）

```
Rule #123:
  IF USDJPY_Up(t-1) AND EURUSD_Down(t-2)

  散布図:
       X(t+2)
        |
      ・・|・・   ← n=5, σ_x1=0.72%, σ_x2=0.68%
    ・・  |  ・      mean_x1=+0.15%, mean_x2=+0.12%
        |
    ----+---- X(t+1)

  評価:
    - Tightness Score: 0 (σ > 0.50%)
    - Directional Score: 0 (|mean| < 0.2%)
    - Reliability Score: 5 (n=5)
    - Total: 5点 → Grade D "Poor"
```

---

## 実装の優先順位

### Phase 1: 閾値の厳格化（今すぐ）✅

```c
// main.c の修正
#define Maxsigx 0.35              // 1.0 → 0.35
#define Minmean 0.3               // 0.1 → 0.3
#define MIN_SUPPORT_COUNT 10      // 2 → 10
```

### Phase 2: 品質スコアの実装（今週）

```c
// main.c にクラスター品質評価関数を追加
double calculate_cluster_quality_score(
    double mean_x1, double sigma_x1,
    double mean_x2, double sigma_x2,
    int support_count
) {
    // Tightness
    double avg_sigma = (sigma_x1 + sigma_x2) / 2;
    double tightness = (avg_sigma <= 0.35) ? 1.0 : 0.0;

    // Directionality
    double avg_mean = fabs((mean_x1 + mean_x2) / 2);
    double direction = (avg_mean >= 0.3) ? 1.0 : 0.0;

    // Reliability
    double reliability = (support_count >= 10) ? 1.0 : 0.0;

    return (tightness + direction + reliability) / 3.0;
}
```

### Phase 3: 可視化による検証（来週）

```bash
# 全ルールの散布図を生成
for i in {1..50}; do
    python3 plot_fx_cluster.py --rule-id $i
done
```

---

## まとめ

### 密集した小集団を見つける3ステップ

1. **閾値を厳格化**
   - Maxsigx: 1.0% → **0.35%**（密集度）
   - Minmean: 0.1% → **0.3%**（方向性）
   - MIN_SUPPORT_COUNT: 2 → **10**（信頼性）

2. **品質スコアで評価**
   - Tightness（密集度）
   - Directionality（方向性）
   - Reliability（信頼性）

3. **散布図で視覚的確認**
   - 赤い点が円形/正方形に密集
   - σが小さい（0.2-0.35%）
   - meanがゼロから離れている（0.3%以上）

### 次のアクション

```bash
# 1. main.c を修正（3つのパラメータ）
# 2. 再コンパイル
gcc -o main main.c -lm -O2

# 3. テスト実行
./main USDJPY 1

# 4. 結果確認
head -20 1-deta-enginnering/forex_data_daily/output/USDJPY/pool/zrp01a.txt

# 期待: ルール数が2,000個 → 50-200個に減少
#      σが0.2-0.35%、meanが0.3%以上の高品質ルール
```

**これで密集した小集団（理想的なクラスター）が発見できます！**
