# GNP時系列ルール発見アルゴリズム

## 目次

1. [概要](#概要)
2. [システムアーキテクチャ](#システムアーキテクチャ)
3. [データ構造](#データ構造)
4. [評価範囲の決定](#評価範囲の決定)
5. [GNPネットワーク構造](#gnpネットワーク構造)
6. [ルール評価プロセス](#ルール評価プロセス)
7. [Support（支持度）の計算](#support支持度の計算)
8. [統計量の計算](#統計量の計算)
9. [ルール品質フィルタ](#ルール品質フィルタ)
10. [進化的アルゴリズム](#進化的アルゴリズム)
11. [適応度関数](#適応度関数)
12. [出力とVerification](#出力とverification)

---

## 概要

### 目的

時系列金融データから**統計的に有意なパターン**を自動発見するシステム。

**発見対象:**
```
IF (過去の属性パターン X)
THEN (未来値の統計的性質 Y)

例:
  IF (BTC_Up=1 at t-1) AND (ETH_Up=1 at t-2)
  THEN X(t+1) の平均 = +0.8%, 標準偏差 = 0.3%
```

### 主要技術

- **GNP (Genetic Network Programming)**: グラフ構造の進化的最適化
- **時系列パターンマイニング**: 過去→未来の因果関係発見
- **Association Rule Mining**: Support（支持度）に基づくルール抽出

### 対象データ

- **為替データ**: 20通貨ペア（日次データ）
- **暗号通貨データ**: 20銘柄（時間足データ）
- **属性次元**: 60次元（20資産 × 3状態）

---

## システムアーキテクチャ

### 全体フロー

```
[1] データ読み込み・前処理
      ↓
[2] GNPネットワークの初期化（120個体）
      ↓
[3] 進化ループ（201世代）
      ├─ [3.1] ルール評価（全時点をスキャン）
      ├─ [3.2] 統計計算（mean, sigma, concentration）
      ├─ [3.3] ルール品質フィルタ
      ├─ [3.4] ルール登録（重複除去）
      ├─ [3.5] 適応度計算
      └─ [3.6] 選択・交叉・突然変異
      ↓
[4] ルールプール出力
      ↓
[5] Verification CSV生成
```

### 主要コンポーネント

| コンポーネント | 説明 |
|-------------|------|
| **GNP Engine** | グラフベースのルール生成エンジン |
| **Evaluator** | 各時点でルールマッチを評価 |
| **Statistics** | 未来値の統計量を計算 |
| **Filter** | ルール品質チェック |
| **Evolution** | 選択・交叉・突然変異 |
| **Output** | ルールプール・Verification出力 |

---

## データ構造

### 入力データ形式

**例: EURUSD.txt**
```
BTC_Up  BTC_Stay  BTC_Down  ETH_Up  ETH_Stay  ...  X       T
1       0         0         0       0         ...  0.52    2023-01-01
0       0         1         1       0         ...  -0.31   2023-01-02
1       0         0         1       0         ...  0.85    2023-01-03
```

### データ構成

```c
Nrd = 4,132             // レコード数（時系列の長さ）
Nzk = 60                // 属性数（20通貨ペア × 3状態）

data_buffer[Nrd][Nzk]   // 属性値（0 or 1）
x_buffer[Nrd]           // リターン値（%）
t_buffer[Nrd]           // タイムスタンプ
```

### 3値化の意味

| 状態 | 意味 | 条件 |
|-----|------|------|
| **Up** | 上昇 | リターン > 閾値 |
| **Stay** | 横ばい | 閾値内 |
| **Down** | 下降 | リターン < -閾値 |

**例:**
```
EURUSD のリターン = +0.52%
→ EURUSD_Up = 1, EURUSD_Stay = 0, EURUSD_Down = 0
```

---

## 評価範囲の決定

### 時系列制約

**過去参照の制約:**
- ルール条件部は t-2, t-1, t-0 の属性を参照
- 最初の MAX_TIME_DELAY = 2 時点は過去が不足

**未来予測の制約:**
- 統計計算には t+1, t+2 の未来値が必要
- 最後の FUTURE_SPAN = 2 時点は未来が不足

### 評価可能範囲

```c
start_index = MAX_TIME_DELAY          // = 2
end_index = Nrd - FUTURE_SPAN         // = Nrd - 2

評価範囲: [2, Nrd-2)
評価時点数 = (Nrd - 2) - 2 = Nrd - 4
```

**具体例:**
```
Nrd = 4,132
MAX_TIME_DELAY = 2
FUTURE_SPAN = 2

評価範囲: [2, 4,130)
評価時点数 = 4,128
```

### コード実装

```c
void get_safe_data_range(int *start_index, int *end_index)
{
    *start_index = MAX_TIME_DELAY;
    *end_index = Nrd - FUTURE_SPAN;
}

void evaluate_all_individuals()
{
    get_safe_data_range(&safe_start, &safe_end);

    for (i = safe_start; i < safe_end; i++)  // i = 2 ~ 4,129
    {
        evaluate_single_instance(i);
    }
}
```

---

## GNPネットワーク構造

### ノード構成

```c
Nkotai = 120          // 個体数（並列探索）
Npn = 10              // 処理ノード数（開始点）
Njg = 100             // 判定ノード数
```

**合計ノード数 = 10 + 100 = 110 ノード/個体**

### ノードタイプ

#### 1. 処理ノード (Processing Node)

- **役割**: ルール探索の開始点
- **数**: 10個
- **接続**: 最初の判定ノードへのポインタ

#### 2. 判定ノード (Judgment Node)

- **役割**: 属性条件の判定
- **数**: 100個
- **構造**:
  ```c
  node_structure[individual][node_id][0] = 属性ID（0-59）
  node_structure[individual][node_id][1] = 次ノードID（Yes側）
  node_structure[individual][node_id][2] = 時間遅延（0-2）
  ```

### ネットワーク探索

```
処理ノード k
    ↓
判定ノード: BTC_Up (t-1) = 1?
    ↓ Yes
判定ノード: ETH_Up (t-2) = 1?
    ↓ Yes
判定ノード: XRP_Up (t-0) = 1?
    ↓ Yes
→ ルールマッチ
→ 未来値（t+1, t+2）を統計に記録
```

### 深さ (Depth)

```c
#define Nmx 7         // 最大深さ（最大属性数 - 1）

depth = 0: 開始点（全時点をカウント）
depth = 1: 1つ目の属性判定後
depth = 2: 2つ目の属性判定後
...
depth = 7: 7つ目の属性判定後
```

---

## ルール評価プロセス

### 評価アルゴリズム

```c
void evaluate_single_instance(int time_index)
{
    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)  // 各処理ノードから開始
        {
            current_node_id = k;
            depth = 0;
            match_flag = 1;

            // 深さ0: 全時点でカウント
            match_count[individual][k][0]++;

            // 最初の判定ノードへ
            current_node_id = node_structure[individual][k][1];

            // 判定ノードを辿る
            while (current_node_id > (Npn - 1) && depth < Nmx)
            {
                depth++;

                // 属性ID取得
                attr_id = node_structure[individual][current_node_id][0];
                time_delay = node_structure[individual][current_node_id][2];

                // 過去の時点の属性値を取得
                data_index = time_index - time_delay;
                if (data_index < 0) break;  // 範囲外

                attr_value = data_buffer[data_index][attr_id];

                if (attr_value == 1)
                {
                    // マッチ継続
                    if (match_flag == 1)
                    {
                        match_count[individual][k][depth]++;

                        // 未来値統計を記録
                        accumulate_future_statistics(...);
                        update_quadrant_statistics(...);
                    }
                    evaluation_count[individual][k][depth]++;
                    current_node_id = node_structure[...][1];  // 次へ
                }
                else if (attr_value == 0)
                {
                    // 不一致: 処理ノードに戻る
                    evaluation_count[individual][k][depth]++;
                    break;
                }
                else
                {
                    // データなし: マッチフラグをオフにして継続
                    evaluation_count[individual][k][depth]++;
                    match_flag = 0;
                    current_node_id = node_structure[...][1];
                }
            }
        }
    }
}
```

### マッチカウントの意味

```c
match_count[individual][k][depth] = そのルールパターンにマッチした回数
evaluation_count[individual][k][depth] = ルール評価が行われた回数
```

**例:**
```
depth=0: match_count[0] = 4,128（全時点）
depth=1: match_count[1] = 2,100（BTC_Up=1）
depth=2: match_count[2] = 800（BTC_Up=1 AND ETH_Up=1）
depth=3: match_count[3] = 180（BTC_Up=1 AND ETH_Up=1 AND XRP_Up=1）
```

---

## Support（支持度）の計算

### 標準定義

Association Rule Mining における標準的な Support の定義:

```
Support(X) = |{X が成立するトランザクション}| / |{全トランザクション}|
```

### 本システムでの実装

```c
support_rate = match_count[depth] / match_count[0]
```

**意味:**
```
分子: match_count[depth] = ルールにマッチした時点数
分母: match_count[0] = 評価可能な全時点数
```

### 計算手順

#### ステップ1: match_count[0] のカウント

```c
// 深さ0で全時点を無条件カウント
for (i = safe_start; i < safe_end; i++)  // i = 2 ~ 4,129
{
    evaluate_single_instance(i);
    // → match_count[0]++ が実行される
}

match_count[0] = Nrd - FUTURE_SPAN - MAX_TIME_DELAY
               = 4,132 - 2 - 2
               = 4,128
```

#### ステップ2: negative_count の設定

```c
void calculate_negative_counts()
{
    for (i = 0; i < Nmx; i++)
    {
        // 全深さで match_count[0] を使用
        negative_count[individual][k][i] = match_count[individual][k][0];
    }
}
```

#### ステップ3: サポート率の計算

```c
double calculate_support_value(int matched_count, int negative_count_val)
{
    return (double)matched_count / (double)negative_count_val;
}

// 使用例
support = calculate_support_value(match_count[depth], negative_count[depth]);
```

### 具体例

```
ルール: BTC_Up=1 AND ETH_Up=1 AND XRP_Up=1

全データ: Nrd = 4,132
評価可能時点: 4,128

match_count[3] = 180（ルールマッチ回数）
match_count[0] = 4,128（全時点）

support_rate = 180 / 4,128 = 0.0436 = 4.36%
```

### 重要な性質

1. **標準定義に準拠**: 標準的な Support の定義と完全に一致
2. **深さに依存しない分母**: 全深さで同じ分母（match_count[0]）を使用
3. **時系列制約を考慮**: 過去参照と未来予測の両方の制約を反映

---

## 統計量の計算

### 未来値の統計

各ルールマッチ時に、t+1 と t+2 の未来値を記録:

```c
void accumulate_future_statistics(int individual, int k, int depth, int time_index)
{
    for (offset = 0; offset < FUTURE_SPAN; offset++)  // offset = 0, 1
    {
        future_val = get_future_value(time_index, offset + 1);  // t+1, t+2
        if (isnan(future_val)) continue;

        // 統計を累積
        future_sum[individual][k][depth][offset] += future_val;
        future_sigma_array[individual][k][depth][offset] += future_val * future_val;
    }
}
```

### 平均値と標準偏差の計算

```c
void calculate_rule_statistics()
{
    for (offset = 0; offset < FUTURE_SPAN; offset++)
    {
        int n = match_count[individual][k][j];

        // 平均: E[X]
        double mean = future_sum[individual][k][j][offset] / (double)n;

        // 標本分散: E[X²] - E[X]²
        double variance = future_sigma_array[individual][k][j][offset] / (double)n
                        - mean * mean;

        // 不偏分散に変換: s² = n/(n-1) × σ²（Bessel補正）
        variance = variance * n / (n - 1);

        // 標準偏差
        double sigma = sqrt(variance);

        future_sum[individual][k][j][offset] = mean;
        future_sigma_array[individual][k][j][offset] = sigma;
    }
}
```

### 象限統計（Quadrant Statistics）

t+1 と t+2 の符号の組み合わせで4象限に分類:

```c
void update_quadrant_statistics(int individual, int k, int depth, int time_index)
{
    double future_t1 = get_future_value(time_index, 1);
    double future_t2 = get_future_value(time_index, 2);

    int quadrant = determine_quadrant(future_t1, future_t2);
    // Q1: (t+1 > 0, t+2 > 0)
    // Q2: (t+1 < 0, t+2 > 0)
    // Q3: (t+1 < 0, t+2 < 0)
    // Q4: (t+1 > 0, t+2 < 0)

    quadrant_count[individual][k][depth][quadrant]++;
}
```

### 集中度（Concentration Ratio）

最も集中している象限の割合:

```c
double calculate_concentration_ratio(int *quadrant_counts)
{
    int total = q1 + q2 + q3 + q4;
    int max_count = max(q1, q2, q3, q4);

    return (double)max_count / total;
}
```

**意味:**
- Concentration = 1.0: 全て1つの象限に集中（完全な方向性）
- Concentration = 0.25: 4象限に均等分散（ランダム）

---

## ルール品質フィルタ

### フィルタ条件

```c
#define Minsup 0.001              // 最小サポート率: 0.1%
#define Maxsigx 0.25              // 最大標準偏差: 0.25%
#define Min_Mean 0.5              // 最小平均: 0.5%
#define MIN_ATTRIBUTES 2          // 最小属性数: 2
#define MIN_SUPPORT_COUNT 8       // 最小マッチ回数: 8
```

### チェックアルゴリズム

```c
int check_rule_quality(double *future_sigma, double *future_mean,
                       double support, int num_attrs,
                       int *quadrant_counts, int matched_count)
{
    // 1. 基本条件
    if (support < Minsup) return 0;
    if (num_attrs < MIN_ATTRIBUTES) return 0;

    // 2. 分散の安定性（全時点で低分散）
    for (i = 0; i < FUTURE_SPAN; i++)
    {
        if (future_sigma[i] > Maxsigx) return 0;
    }

    // 3. 平均の閾値（t+1 AND t+2 両方）
    for (i = 0; i < FUTURE_SPAN; i++)
    {
        if (fabs(future_mean[i]) < Min_Mean) return 0;
    }

    // 4. 最小マッチ回数
    if (matched_count < MIN_SUPPORT_COUNT) return 0;

    return 1;  // 全条件を満たす
}
```

### フィルタの意図

| 条件 | 意図 |
|-----|------|
| **Minsup** | 希少すぎるパターンを除外 |
| **Maxsigx** | 予測値が安定している（低ボラティリティ） |
| **Min_Mean** | 統計的に有意な変化（ノイズレベルを超える） |
| **MIN_ATTRIBUTES** | 単純すぎるルールを除外 |
| **MIN_SUPPORT_COUNT** | 統計的信頼性の確保 |

---

## 進化的アルゴリズム

### 世代ループ

```c
for (ng = 0; ng < Generation; ng++)  // Generation = 201
{
    // 1. ルール評価
    evaluate_all_individuals();

    // 2. 統計計算
    calculate_rule_statistics();

    // 3. ルール登録
    register_new_rules();

    // 4. 適応度計算
    calculate_fitness();

    // 5. ランキング
    rank_individuals();

    // 6. 選択
    select_elite();

    // 7. 交叉
    crossover();

    // 8. 突然変異
    mutation();
}
```

### 選択 (Selection)

**エリート戦略:**
```c
// 適応度上位40個体を3回複製
for (i = 0; i < 40; i++)
{
    elite[i] → population[i]        // 1回目
    elite[i] → population[i+40]     // 2回目
    elite[i] → population[i+80]     // 3回目
}
```

**効果:**
- 優良な遺伝子を保存
- 多様性を維持（3コピー）

### 交叉 (Crossover)

```c
#define Nkousa 20    // 交叉回数/個体

for (i = 0; i < 20; i++)
{
    for (j = 0; j < Nkousa; j++)
    {
        // ランダムに判定ノードを選択
        node_id = rand() % Njg + Npn;

        // 個体 i と i+20 の間で遺伝子を交換
        swap(gene[i][node_id], gene[i+20][node_id]);
    }
}
```

### 突然変異 (Mutation)

#### 1. 処理ノードの突然変異

```c
#define Muratep 1    // 突然変異率: 1/1 = 100%

for (i = 0; i < 120; i++)
{
    for (j = 0; j < Npn; j++)
    {
        if (rand() % Muratep == 0)
        {
            // 接続先をランダムに変更
            genecnct[i][j] = rand() % Njg + Npn;
        }
    }
}
```

#### 2. 判定ノードの接続変異

```c
#define Muratej 6    // 突然変異率: 1/6 ≈ 16.7%

for (i = 40; i < 80; i++)
{
    for (j = Npn; j < (Npn + Njg); j++)
    {
        if (rand() % Muratej == 0)
        {
            genecnct[i][j] = rand() % Njg + Npn;
        }
    }
}
```

#### 3. 判定ノードの属性変異（ルーレット選択）

```c
#define Muratea 6    // 突然変異率: 1/6 ≈ 16.7%

for (i = 80; i < 120; i++)
{
    for (j = Npn; j < (Npn + Njg); j++)
    {
        if (rand() % Muratea == 0)
        {
            // 頻出属性を優先的に選択（ルーレット選択）
            new_attr = roulette_selection(attribute_frequency);
            geneatt[i][j] = new_attr;
        }
    }
}
```

---

## 適応度関数

### 基本適応度

```c
double fitness = 0.0;

// 1. 新規ルール発見ボーナス
if (is_new_rule)
{
    fitness += FITNESS_NEW_RULE_BONUS;          // +50
    fitness += num_attributes * FITNESS_ATTRIBUTE_WEIGHT;  // +2 × 属性数
    fitness += support * FITNESS_SUPPORT_WEIGHT;  // +50 × サポート率
    fitness += 2.0 / (sigma + FITNESS_SIGMA_OFFSET);  // +分散の逆数
}
else
{
    // 既存ルール再発見（小ボーナス）
    fitness += num_attributes * FITNESS_ATTRIBUTE_WEIGHT;
    fitness += support * FITNESS_SUPPORT_WEIGHT;
    fitness += 2.0 / (sigma + FITNESS_SIGMA_OFFSET);
}
```

### ボーナス: 象限集中度

```c
if (concentration >= 0.60) fitness += 6000;
else if (concentration >= 0.55) fitness += 3000;
else if (concentration >= 0.50) fitness += 1500;
else if (concentration >= 0.45) fitness += 600;
else if (concentration >= 0.35) fitness += 150;
```

### ボーナス: 統計的有意性

```c
double abs_mean = fabs(mean);

if (abs_mean >= 0.8) fitness += 6000;
else if (abs_mean >= 0.5) fitness += 2500;
else if (abs_mean >= 0.3) fitness += 800;
```

### ボーナス: 小集団（希少パターン）

```c
if (support_rate <= 0.0010) fitness += 1000;
else if (support_rate <= 0.0015) fitness += 600;
else if (support_rate <= 0.0020) fitness += 300;
```

### 適応度の意図

- **新規発見を優遇**: 多様なルールを収集
- **集中度を重視**: 方向性が明確なパターン
- **有意性を重視**: ノイズを超える強いシグナル
- **希少性を評価**: 珍しいが重要なパターン

---

## 出力とVerification

### ルールプール (zrp01a.txt)

**フォーマット:**
```
support_count  support_rate  Negative  HighSup  LowVar  NumAttr  X(t+1)_mean  X(t+1)_sigma  X(t+2)_mean  X(t+2)_sigma  ...
15             0.0036        4128      1        1       3        0.823        0.245         0.651        0.223         ...
8              0.0019        4128      0        1       2        -0.712       0.198         -0.543       0.201         ...
```

**列の説明:**

| 列名 | 説明 |
|-----|------|
| **support_count** | ルールマッチ回数（match_count） |
| **support_rate** | サポート率（match_count / 4,128） |
| **Negative** | 全時点数（match_count[0] = 4,128） |
| **HighSup** | 高サポートフラグ（1 or 0） |
| **LowVar** | 低分散フラグ（1 or 0） |
| **NumAttr** | 属性数 |
| **X(t+1)_mean** | t+1の平均リターン（%） |
| **X(t+1)_sigma** | t+1の標準偏差（%） |
| **X(t+2)_mean** | t+2の平均リターン（%） |
| **X(t+2)_sigma** | t+2の標準偏差（%） |
| **Attr1, Attr2, ...** | 属性名（例: BTC_Up） |
| **Delay1, Delay2, ...** | 時間遅延（0-2） |

### Verification CSV

**例: rule_001.csv**
```
Row,T,X(t+1),X(t+2)
123,2023-01-15,0.85,0.72
456,2023-02-20,0.91,0.68
789,2023-03-10,0.76,0.55
...
```

**用途:**
- ルールがマッチした全時点を記録
- 散布図生成（X(t+1) vs X(t+2)）
- 象限分布の可視化
- 外れ値の検出

---

## アルゴリズムの特徴

### 1. 時系列を考慮した設計

- **過去参照**: t-2, t-1, t-0 の属性を組み合わせ
- **未来予測**: t+1, t+2 の統計的性質を分析
- **時間遅延の適応**: GNPが最適な時間遅延を学習

### 2. 標準的なSupportの実装

```
support_rate = match_count / (Nrd - FUTURE_SPAN - MAX_TIME_DELAY)
```

- Association Rule Miningの標準定義に準拠
- 時系列制約を正確に反映
- 解釈が明確

### 3. 多目的最適化

適応度関数が複数の目標を同時に最適化:
- **サポート率**: 頻度
- **集中度**: 方向性
- **有意性**: 強度
- **希少性**: 新規性

### 4. 統計的厳密性

- **不偏推定**: Bessel補正による不偏分散
- **多重チェック**: 複数の品質フィルタ
- **Verification**: 全マッチポイントを記録

### 5. スケーラビリティ

- **並列探索**: 120個体 × 10開始点 = 1,200パターン/世代
- **メモリ効率**: 動的配列による柔軟性
- **高速評価**: 1パスで全統計を計算

---

## まとめ

### システムの強み

1. ✅ **GNPによる柔軟なルール生成**: グラフ構造で複雑なパターンを表現
2. ✅ **標準的なSupport定義**: 学術的に正確な指標
3. ✅ **時系列制約の正確な処理**: 過去参照と未来予測の両方を考慮
4. ✅ **統計的厳密性**: 不偏推定、多重フィルタ
5. ✅ **完全なVerification**: 全マッチポイントを記録

### 適用分野

- **為替取引**: 通貨ペアの相関パターン発見
- **暗号通貨**: ボラティリティパターン抽出
- **リスク管理**: 異常パターンの早期検出
- **ポートフォリオ最適化**: 相関崩壊の予測

### 今後の拡張可能性

- **Confidence指標の追加**: `match_count[depth] / match_count[depth-1]`
- **Lift指標の追加**: `Support(X∧Y) / (Support(X) × Support(Y))`
- **多変量統計**: t+1とt+2の相関分析
- **動的閾値**: データ分布に応じた適応的フィルタ

---

**作成日**: 2025-11-11
**バージョン**: 1.0
**関連ファイル**: `main.c`, `support_definition.md`, `support_rate_fix_20251111.md`
