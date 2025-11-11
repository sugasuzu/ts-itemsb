# 適応度関数の改善提案：より小さな密集集団の発見

## 現状分析

### 現在の適応度関数（main.c Lines 2160-2166）

```c
fitness_value[individual] +=
    j2 * FITNESS_ATTRIBUTE_WEIGHT +            // 属性数: 2
    support * FITNESS_SUPPORT_WEIGHT +         // サポート率: 400倍
    FITNESS_SIGMA_WEIGHT / (sigma + OFFSET) +  // 低分散: 200 / (σ + 0.3)
    concentration_bonus +                      // 象限集中度: 最大10,000
    significance_bonus +                       // 統計的有意性: 最大10,000
    FITNESS_NEW_RULE_BONUS;                    // 新規ルール: 50
```

### 問題点

#### 1. **サポート率が支配的すぎる**

```
例: support=0.003 (0.3%)の場合
  support項 = 0.003 × 400 = 1.2

例: support=0.001 (0.1%)の場合
  support項 = 0.001 × 400 = 0.4

差: 0.8 (2倍の差)
```

**問題:**
- サポート率が高い（=大きな集団）ほど適応度が高くなる
- **小さな密集集団**よりも**大きなゆるい集団**が選ばれやすい

#### 2. **分散項が弱すぎる**

```
例: σ=0.25%（非常に密集）の場合
  sigma項 = 200 / (0.25 + 0.30) = 364

例: σ=0.45%（普通）の場合
  sigma項 = 200 / (0.45 + 0.30) = 267

差: 97（わずか26%の差）
```

**問題:**
- 分散が2倍違っても適応度の差は小さい
- **密集度の違いが評価されにくい**

#### 3. **相対的な重み付けのバランス**

典型的なルールの適応度内訳：
```
属性数項:      4 × 2 = 8
サポート項:    0.002 × 400 = 0.8
分散項:        200 / 0.75 = 267
集中度項:      2,500（中程度）
有意性項:      1,000（中程度）
新規項:        50
──────────────────────────
合計:          約3,825
```

**支配的な要素:**
1. 象限集中度（2,500） - 65%
2. 有意性ボーナス（1,000） - 26%
3. 分散（267） - 7%
4. サポート（0.8） - 0.02%

→ **分散（密集度）の影響が小さすぎる！**

---

## 改善案：小集団・高密集度を優先

### 目標

1. **小さな集団を優遇**：サポート率が低くてもペナルティにしない
2. **密集度を最優先**：σが小さいほど大きなボーナス
3. **バランスの取れた評価**：各要素が適切に影響する

---

### 提案1: サポート率の重みを削減（推奨）

**現在:**
```c
#define FITNESS_SUPPORT_WEIGHT 400
```

**提案:**
```c
#define FITNESS_SUPPORT_WEIGHT 50  // 400 → 50 (1/8に削減)
```

**効果:**
```
サポート率=0.001 → 0.05（小集団でもOK）
サポート率=0.003 → 0.15（差は3倍だが絶対値は小さい）
```

**理由:**
- 小さな密集集団（n=10-15）を発見することが目標
- サポート率の影響を減らし、密集度の影響を相対的に高める

---

### 提案2: 分散項の重みを大幅増加（推奨）

**現在:**
```c
#define FITNESS_SIGMA_WEIGHT 200
#define FITNESS_SIGMA_OFFSET 0.30
```

**提案:**
```c
#define FITNESS_SIGMA_WEIGHT 800   // 200 → 800 (4倍)
#define FITNESS_SIGMA_OFFSET 0.10  // 0.30 → 0.10 (より敏感に)
```

**効果:**
```
σ=0.20% → 800 / 0.30 = 2,667  (非常に密集)
σ=0.30% → 800 / 0.40 = 2,000  (密集)
σ=0.40% → 800 / 0.50 = 1,600  (中程度)
σ=0.50% → 800 / 0.60 = 1,333  (ゆるい)

差: 2,667 - 1,333 = 1,334 (2倍の効果)
```

**理由:**
- σの違いが適応度に大きく反映される
- 密集したクラスターが明確に優遇される

---

### 提案3: 集中度ボーナスの調整

**現在:**
```c
#define CONCENTRATION_BONUS_3 2500   // 50%集中
#define CONCENTRATION_BONUS_4 5000   // 55%集中
#define CONCENTRATION_BONUS_5 10000  // 60%集中
```

**提案:**
```c
#define CONCENTRATION_BONUS_3 1500   // 2500 → 1500 (60%に削減)
#define CONCENTRATION_BONUS_4 3000   // 5000 → 3000 (60%に削減)
#define CONCENTRATION_BONUS_5 6000   // 10000 → 6000 (60%に削減)
```

**理由:**
- 集中度だけが支配的にならないようバランスを取る
- 分散項の影響を相対的に高める

---

### 提案4: 小集団ボーナスの追加（オプション）

**新規追加:**
```c
#define SMALL_CLUSTER_THRESHOLD_1 0.0020  // 0.2%以下
#define SMALL_CLUSTER_THRESHOLD_2 0.0015  // 0.15%以下
#define SMALL_CLUSTER_THRESHOLD_3 0.0010  // 0.10%以下
#define SMALL_CLUSTER_BONUS_1 300
#define SMALL_CLUSTER_BONUS_2 600
#define SMALL_CLUSTER_BONUS_3 1000
```

**適応度計算に追加:**
```c
// 小集団ボーナス（希少だが密集したパターンを評価）
double small_cluster_bonus = 0.0;
if (support <= SMALL_CLUSTER_THRESHOLD_3)
    small_cluster_bonus = SMALL_CLUSTER_BONUS_3;
else if (support <= SMALL_CLUSTER_THRESHOLD_2)
    small_cluster_bonus = SMALL_CLUSTER_BONUS_2;
else if (support <= SMALL_CLUSTER_THRESHOLD_1)
    small_cluster_bonus = SMALL_CLUSTER_BONUS_1;

fitness_value[individual] += small_cluster_bonus;
```

**理由:**
- 希少パターン（サポート率が低い）に明示的なボーナス
- 大きな集団を発見するのではなく、小さな理想的クラスターを発見

---

## 推奨設定：Tight Cluster Optimization

### main.c の修正（Lines 77-106）

```c
/* 品質判定パラメータ（小集団・高密集度優先） */
#define HIGH_SUPPORT_BONUS 0.02
#define LOW_VARIANCE_REDUCTION 1.0
#define FITNESS_SUPPORT_WEIGHT 50   // 400 → 50 (サポート率の影響を削減)
#define FITNESS_SIGMA_OFFSET 0.10   // 0.30 → 0.10 (分散評価を敏感に)
#define FITNESS_NEW_RULE_BONUS 50
#define FITNESS_ATTRIBUTE_WEIGHT 2
#define FITNESS_SIGMA_WEIGHT 800    // 200 → 800 (分散を最重視)

/* 象限集中度ボーナス（バランス調整） */
#define CONCENTRATION_THRESHOLD_1 0.35
#define CONCENTRATION_THRESHOLD_2 0.45
#define CONCENTRATION_THRESHOLD_3 0.50
#define CONCENTRATION_THRESHOLD_4 0.55
#define CONCENTRATION_THRESHOLD_5 0.60

#define CONCENTRATION_BONUS_1 150   // 200 → 150
#define CONCENTRATION_BONUS_2 600   // 800 → 600
#define CONCENTRATION_BONUS_3 1500  // 2500 → 1500
#define CONCENTRATION_BONUS_4 3000  // 5000 → 3000
#define CONCENTRATION_BONUS_5 6000  // 10000 → 6000

/* 統計的有意性ボーナス（バランス調整） */
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_1 0.3
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_2 0.5
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_3 0.8
#define STATISTICAL_SIGNIFICANCE_BONUS_1 800   // 1000 → 800
#define STATISTICAL_SIGNIFICANCE_BONUS_2 2500  // 4000 → 2500
#define STATISTICAL_SIGNIFICANCE_BONUS_3 6000  // 10000 → 6000

/* 小集団ボーナス（新規追加） */
#define SMALL_CLUSTER_THRESHOLD_1 0.0020
#define SMALL_CLUSTER_THRESHOLD_2 0.0015
#define SMALL_CLUSTER_THRESHOLD_3 0.0010
#define SMALL_CLUSTER_BONUS_1 300
#define SMALL_CLUSTER_BONUS_2 600
#define SMALL_CLUSTER_BONUS_3 1000
```

---

## 期待される効果

### 改善前（現在の設定）

典型的なルールの適応度：
```
属性数:     8
サポート:   0.8   ← 大きな集団が有利
分散:       267   ← 密集度の影響小
集中度:     2,500
有意性:     1,000
──────────────
合計:       3,825
```

**選ばれやすいルール:**
- サポート率が高い（大きな集団）
- 象限集中度が高い
- 分散の違いは二次的

### 改善後（提案設定）

典型的なルールの適応度（σ=0.25%, support=0.001）：
```
属性数:     8
サポート:   0.05  ← 小集団でもペナルティなし
分散:       2,667 ← 密集度が支配的！
集中度:     1,500
有意性:     800
小集団:     1,000 ← 希少性ボーナス
──────────────
合計:       5,975
```

**選ばれやすいルール:**
1. **σが小さい（密集度が高い）**
2. 象限集中度が高い
3. サポート率は二次的（小さくてもOK）

---

## 実装手順

### Step 1: main.c の修正

```bash
# Lines 77-106 を上記の「推奨設定」に変更
nano main.c
```

### Step 2: 小集団ボーナス関数の追加（Lines 2156付近）

```c
// 小集団ボーナスを計算
double calculate_small_cluster_bonus(double support)
{
    if (support <= SMALL_CLUSTER_THRESHOLD_3)
        return SMALL_CLUSTER_BONUS_3;
    else if (support <= SMALL_CLUSTER_THRESHOLD_2)
        return SMALL_CLUSTER_BONUS_2;
    else if (support <= SMALL_CLUSTER_THRESHOLD_1)
        return SMALL_CLUSTER_BONUS_1;
    else
        return 0.0;
}
```

### Step 3: 適応度計算に組み込む（Lines 2160-2166）

```c
// 小集団ボーナスを計算
double small_bonus = calculate_small_cluster_bonus(support);

// 適応度を更新
fitness_value[individual] +=
    j2 * FITNESS_ATTRIBUTE_WEIGHT +
    support * FITNESS_SUPPORT_WEIGHT +
    FITNESS_SIGMA_WEIGHT / (future_sigma_ptr[0] + FITNESS_SIGMA_OFFSET) +
    concentration_bonus +
    significance_bonus +
    small_bonus +          // ← 小集団ボーナス追加
    FITNESS_NEW_RULE_BONUS;
```

### Step 4: 再コンパイル & テスト

```bash
gcc -o main main.c -lm -O2
./main USDJPY 1
```

### Step 5: 結果の確認

```bash
# ルール数とσの分布を確認
python3 analysis/fx/analyze_usdjpy_rules.py

# 期待される結果:
# - ルール数: 20-40個（質重視）
# - 平均σ: 0.30-0.35%（より密集）
# - 平均サポート: 0.0015-0.0025（小集団）
# - 平均スコア: 65-75点（高品質）
```

---

## 段階的なアプローチ（推奨）

### Phase 1: 分散重視（まず試す）

```c
#define FITNESS_SIGMA_WEIGHT 600   // 200 → 600 (3倍)
#define FITNESS_SIGMA_OFFSET 0.15  // 0.30 → 0.15
```

→ 実行して結果を確認

### Phase 2: サポート削減（Phase 1で効果が出たら）

```c
#define FITNESS_SUPPORT_WEIGHT 100  // 400 → 100
```

→ 実行して結果を確認

### Phase 3: 完全最適化（Phase 2で良好なら）

```c
全ての推奨設定を適用
```

---

## まとめ

### 現在の問題

- ✗ サポート率が支配的 → **大きな集団**が選ばれる
- ✗ 分散項が弱い → **密集度の違いが評価されない**
- ✗ 結果: 36個のルール、平均σ=0.374%、平均スコア59点

### 改善後の期待

- ✓ 分散項が支配的 → **密集した集団**が選ばれる
- ✓ サポート率は二次的 → **小さな集団**も許容
- ✓ 小集団ボーナス → **希少パターン**を優遇
- ✓ 期待結果: 20-40個のルール、平均σ=0.30-0.35%、平均スコア70-80点

### 次のアクション

```bash
# 1. main.c を修正（推奨設定を適用）
# 2. 再コンパイル
gcc -o main main.c -lm -O2

# 3. テスト実行
./main USDJPY 1

# 4. 結果分析
python3 analysis/fx/analyze_usdjpy_rules.py

# 5. 散布図確認
python3 analysis/fx/plot_usdjpy_clusters.py
```

**これで「より小さな密集した集団」が発見されるはずです！**
