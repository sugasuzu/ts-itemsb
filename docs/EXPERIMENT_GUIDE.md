# 象限集中度ルール発見システム - 実験ガイド

このドキュメントでは、象限集中度に基づくルール発見実験の実施方法を説明します。

---

## 目次

1. [プロジェクトの目的](#プロジェクトの目的)
2. [システムアーキテクチャ](#システムアーキテクチャ)
3. [パラメータ一覧](#パラメータ一覧)
4. [閾値の変更方法](#閾値の変更方法)
5. [適応度関数の修正方法](#適応度関数の修正方法)
6. [コンパイルと実行](#コンパイルと実行)
7. [結果の分析方法](#結果の分析方法)
8. [上位ルールの抽出](#上位ルールの抽出)
9. [可視化方法](#可視化方法)
10. [実験ワークフロー例](#実験ワークフロー例)
11. [トラブルシューティング](#トラブルシューティング)

---

## プロジェクトの目的

### 研究目標

**時系列データから象限集中度の高い（50%以上）興味深いルールを発見する**

- **象限集中度**: X(t+1)とX(t+2)の2次元空間を4象限（Q1:++, Q2:+-, Q3:--, Q4:-+）に分割し、最も多くのデータポイントが集まる象限の割合
- **目標**: 象限集中度50%以上のルールを発見（ランダムベースライン25%の2倍）
- **統計的信頼性**: supportとsigmaで担保
- **実用性**: 発見したルールをユーザーが閾値調整により実用化

### 重要な概念

1. **象限集中度は「方向性の偏り」を示す指標**
   - 高集中度 = 特定の象限にデータが偏って分布
   - 予測精度ではなく、パターンの存在を示す

2. **統計的信頼性**
   - `support_count`: マッチ数（30以上推奨）
   - `sigma`: 標準偏差（0.5%以下推奨）
   - これらで統計的有意性を担保

3. **GNP進化による発見**
   - ランダム生成 → 交叉・突然変異 → 適応度評価 → 選択
   - 201世代、120個体、10 Trials

---

## システムアーキテクチャ

### データフロー

```
入力データ (BTC.txt)
    ↓
GNP進化システム (main.c)
    ↓
ルール品質チェック (check_rule_quality)
    - support_count >= MIN_SUPPORT_COUNT
    - sigma <= Maxsigx
    - concentration >= MIN_CONCENTRATION
    ↓
適応度評価 (fitness function)
    - 基本適応度 = support_count * SUPPORT_WEIGHT
    - 集中度ボーナス（5段階）
    ↓
出力
    - pool/zrp01a.txt: ルール一覧
    - verification/rule_XXXXX.csv: 各ルールのマッチデータ
```

### ファイル構成

```
ts-itemsbs/
├── main.c                          # GNPメインプログラム
├── Makefile                        # ビルド設定
├── 1-deta-enginnering/
│   └── crypto_data_hourly/
│       ├── BTC.txt                 # 入力データ
│       └── output/BTC/
│           ├── pool/
│           │   └── zrp01a.txt      # ルールプール（統計付き）
│           └── verification/
│               └── rule_*.csv      # 各ルールの検証データ
├── analysis/
│   └── crypt/
│       └── plot_single_rule_2d_future.py  # 散布図生成
└── docs/
    └── EXPERIMENT_GUIDE.md         # このドキュメント
```

---

## パラメータ一覧

### main.c の主要パラメータ（Lines 82-88, 222-234）

#### 基本フィルタパラメータ

```c
// Lines 82-88: 基本パラメータ
#define Nrulemax 2002               // 最大ルール数（メモリ制限）
#define Minsup 0.0005               // 最小サポート値（0.05%）
#define Maxsigx 0.5                 // 最大標準偏差（0.5%）
#define Min_Mean 0.05               // [未使用] 平均値閾値
#define MIN_ATTRIBUTES 2            // 最小属性数
#define MIN_SUPPORT_COUNT 30        // 最小マッチ数
#define MIN_CONCENTRATION 0.40      // 最小集中度（40%）
```

**推奨設定範囲:**
- `MIN_CONCENTRATION`: 0.30〜0.45（30%未満は低すぎ、45%超は厳しすぎ）
- `MIN_SUPPORT_COUNT`: 20〜50（統計的信頼性とルール数のトレードオフ）
- `Maxsigx`: 0.4〜0.6（分散の許容範囲）

#### 適応度ボーナスパラメータ

```c
// Lines 222-234: 象限集中度ボーナス（5段階）
#define CONCENTRATION_THRESHOLD_1  0.40   // 40%以上
#define CONCENTRATION_THRESHOLD_2  0.45   // 45%以上
#define CONCENTRATION_THRESHOLD_3  0.50   // 50%以上（目標）
#define CONCENTRATION_THRESHOLD_4  0.55   // 55%以上
#define CONCENTRATION_THRESHOLD_5  0.60   // 60%以上

#define CONCENTRATION_BONUS_1      500    // 40%: ベースライン
#define CONCENTRATION_BONUS_2      2000   // 45%: ×4
#define CONCENTRATION_BONUS_3      8000   // 50%: ×16（目標達成）
#define CONCENTRATION_BONUS_4      15000  // 55%: ×30
#define CONCENTRATION_BONUS_5      25000  // 60%: ×50（最大報酬）
```

**ボーナス設計の考え方:**
- 指数的増加（×4, ×16, ×30, ×50）により高集中度を強く選好
- MIN_CONCENTRATIONと最初のボーナス閾値は一致させる

---

## 閾値の変更方法

### 1. 最小集中度フィルタの変更

**目的**: ルール登録の最低基準を変更

```bash
# main.c の Line 88 を編集
vim main.c +88
```

```c
// 変更前
#define MIN_CONCENTRATION 0.40      // 40%以上

// 変更例1: より緩い基準（多くのルールを発見）
#define MIN_CONCENTRATION 0.30      // 30%以上

// 変更例2: より厳しい基準（高品質ルールのみ）
#define MIN_CONCENTRATION 0.45      // 45%以上
```

**影響:**
- 低く設定 → ルール数増加、進化速度向上、平均品質低下
- 高く設定 → ルール数減少、進化速度低下、平均品質向上

**推奨実験:**
- 30%, 35%, 40%, 45% で比較実験

### 2. サポート閾値の変更

**目的**: 統計的信頼性の基準を変更

```c
// Line 87
#define MIN_SUPPORT_COUNT 30        // 最小マッチ数

// 変更例1: より多くのルールを許容
#define MIN_SUPPORT_COUNT 20        // 20マッチ以上

// 変更例2: より高い信頼性を要求
#define MIN_SUPPORT_COUNT 50        // 50マッチ以上
```

### 3. 標準偏差閾値の変更

**目的**: 分散の許容範囲を変更

```c
// Line 84
#define Maxsigx 0.5                 // 最大標準偏差0.5%

// 変更例1: より安定したルールを要求
#define Maxsigx 0.4                 // 最大標準偏差0.4%

// 変更例2: より広い分散を許容
#define Maxsigx 0.6                 // 最大標準偏差0.6%
```

---

## 適応度関数の修正方法

### 現在の適応度関数（Lines 2661-2740）

```c
// Line 2661: fitness_score_per_rule 関数

// 基本適応度 = サポート数 × 重み
double fitness = (double)rule_support_count * SUPPORT_WEIGHT;

// 集中度ボーナス（5段階）
int concentration_bonus = 0;
if (concentration_ratio >= CONCENTRATION_THRESHOLD_5) {
    concentration_bonus = CONCENTRATION_BONUS_5;  // 60%+: 25000点
} else if (concentration_ratio >= CONCENTRATION_THRESHOLD_4) {
    concentration_bonus = CONCENTRATION_BONUS_4;  // 55%+: 15000点
} else if (concentration_ratio >= CONCENTRATION_THRESHOLD_3) {
    concentration_bonus = CONCENTRATION_BONUS_3;  // 50%+: 8000点
} else if (concentration_ratio >= CONCENTRATION_THRESHOLD_2) {
    concentration_bonus = CONCENTRATION_BONUS_2;  // 45%+: 2000点
} else if (concentration_ratio >= CONCENTRATION_THRESHOLD_1) {
    concentration_bonus = CONCENTRATION_BONUS_1;  // 40%+: 500点
}

fitness += concentration_bonus;
```

### 修正例1: ボーナス階層を増やす（7段階）

```c
// main.c の Lines 222-234 に追加定義
#define CONCENTRATION_THRESHOLD_6  0.65   // 65%以上
#define CONCENTRATION_THRESHOLD_7  0.70   // 70%以上
#define CONCENTRATION_BONUS_6      40000  // 65%: ×80
#define CONCENTRATION_BONUS_7      60000  // 70%: ×120

// Lines 2711-2726 の if-else を拡張
if (concentration_ratio >= CONCENTRATION_THRESHOLD_7) {
    concentration_bonus = CONCENTRATION_BONUS_7;
} else if (concentration_ratio >= CONCENTRATION_THRESHOLD_6) {
    concentration_bonus = CONCENTRATION_BONUS_6;
} else if (concentration_ratio >= CONCENTRATION_THRESHOLD_5) {
    concentration_bonus = CONCENTRATION_BONUS_5;
}
// ... (以下同様)
```

### 修正例2: サポート重みを変更

```c
// Line 213
#define SUPPORT_WEIGHT 2000         // サポート1マッチあたりの重み

// 変更例1: サポートを重視
#define SUPPORT_WEIGHT 5000         // より多くのマッチを選好

// 変更例2: 集中度を相対的に重視
#define SUPPORT_WEIGHT 1000         // サポートの影響を減らす
```

### 修正例3: 絶対平均値ボーナスを追加（オプション）

もし mean値も考慮したい場合:

```c
// Lines 2726以降に追加
// 絶対平均値ボーナス（オプション）
double abs_mean_t1 = fabs(future_mean_array[0]);
if (abs_mean_t1 >= 1.0) {
    fitness += 30000;  // |mean| >= 1.0%
} else if (abs_mean_t1 >= 0.5) {
    fitness += 15000;  // |mean| >= 0.5%
} else if (abs_mean_t1 >= 0.3) {
    fitness += 5000;   // |mean| >= 0.3%
}
```

---

## コンパイルと実行

### Makefile の使用方法

プロジェクトには`Makefile`が含まれています。

#### 基本コマンド

```bash
# コンパイル
make

# コンパイル + デフォルト実行（BTC, 10 Trials）
make test

# クリーン（生成ファイル削除）
make clean

# 実行ファイルの確認
ls -lh main
```

#### 直接実行

```bash
# BTC, 10 Trials
./main BTC 10

# ログファイルに保存
./main BTC 10 > run_experiment.log 2>&1

# バックグラウンド実行
nohup ./main BTC 10 > run_experiment.log 2>&1 &

# 進行状況の監視
tail -f run_experiment.log
```

#### 実行時間の目安

- **MIN_CONCENTRATION = 0.30**: 約50-60秒（10 Trials）
- **MIN_CONCENTRATION = 0.40**: 約630秒（10 Trials）
- **MIN_CONCENTRATION = 0.45**: 約15-20分（推定）

### コンパイルオプション

`Makefile`の設定を変更する場合:

```makefile
# 最適化レベルの変更
CFLAGS = -O3 -Wall -Wno-unused-variable -Wno-unused-but-set-variable

# デバッグビルド
CFLAGS = -g -Wall -O0
```

---

## 結果の分析方法

### 1. 基本統計の確認

実行ログから基本情報を抽出:

```bash
# 実行完了の確認
grep "Processing Complete" run_experiment.log

# Trial別ルール数
grep "Trial rules:" run_experiment.log

# 総ルール数
grep "Total integrated rules:" run_experiment.log

# 実行時間
grep "Total Time:" run_experiment.log
```

### 2. ルールプールファイルの確認

```bash
# ルール数（ヘッダー含む）
wc -l 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt

# 上位10ルールの表示
head -11 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt | column -t

# カラム名の確認
head -1 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt
```

**zrp01a.txt のカラム構成:**
```
Attr1-8:        ルールの属性（時系列パターン）
X(t+1)_mean:    t+1時点の平均収益率
X(t+1)_sigma:   t+1時点の標準偏差
X(t+2)_mean:    t+2時点の平均収益率
X(t+2)_sigma:   t+2時点の標準偏差
support_count:  マッチ数
support_rate:   サポート率
Negative:       ルールID
HighSup:        高サポートフラグ
LowVar:         低分散フラグ
NumAttr:        属性数
```

### 3. 集中度分析スクリプト

集中度の統計を計算:

```bash
python3 /tmp/analyze_concentration.py
```

このスクリプトは以下を出力:
- 平均集中度、最大集中度
- 集中度分布（60%+, 55-59%, 50-54%, 45-49%, <45%）
- トップ20ルールの詳細（集中度、サポート、象限分布）

---

## 上位ルールの抽出

### 1. 集中度別の抽出スクリプト

```bash
# 上位ルール抽出スクリプトの作成
cat << 'EOF' > extract_top_rules.py
import os
import pandas as pd

# 全検証ファイルを読み込んで集中度を計算
verification_dir = "1-deta-enginnering/crypto_data_hourly/output/BTC/verification/"
results = []

for filename in os.listdir(verification_dir):
    if filename.startswith('rule_') and filename.endswith('.csv'):
        rule_id = int(filename.replace('rule_', '').replace('.csv', ''))
        csv_path = os.path.join(verification_dir, filename)

        df_csv = pd.read_csv(csv_path)
        if len(df_csv) >= 30:
            df_csv['X(t+1)'] = pd.to_numeric(df_csv['X(t+1)'], errors='coerce')
            df_csv['X(t+2)'] = pd.to_numeric(df_csv['X(t+2)'], errors='coerce')

            q1 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] > 0)).sum()
            q2 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] <= 0)).sum()
            q3 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] <= 0)).sum()
            q4 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] > 0)).sum()

            conc = max(q1, q2, q3, q4) / len(df_csv) * 100
            mean_t1 = df_csv['X(t+1)'].mean()
            mean_t2 = df_csv['X(t+2)'].mean()

            results.append({
                'rule_id': rule_id,
                'conc': conc,
                'support': len(df_csv),
                'mean_t1': mean_t1,
                'mean_t2': mean_t2,
                'q1': q1, 'q2': q2, 'q3': q3, 'q4': q4
            })

df_results = pd.DataFrame(results)

# 集中度でソート
df_sorted = df_results.sort_values('conc', ascending=False)

# 上位N件を抽出
N = 50
print(f"=== 集中度上位{N}件のルール ===\n")
print(f"{'Rank':<6} {'RuleID':<8} {'Conc%':<8} {'Support':<8} {'Mean(t+1)':<12} {'Mean(t+2)':<12} {'Dominant':<12}")
print("-" * 80)

for idx, (_, row) in enumerate(df_sorted.head(N).iterrows(), 1):
    max_q = max(row['q1'], row['q2'], row['q3'], row['q4'])
    if max_q == row['q1']:
        dom = "Q1(++)"
    elif max_q == row['q2']:
        dom = "Q2(+-)"
    elif max_q == row['q3']:
        dom = "Q3(--)"
    else:
        dom = "Q4(-+)"

    print(f"{idx:<6} {int(row['rule_id']):<8} {row['conc']:>5.1f}%   "
          f"{int(row['support']):<8} {row['mean_t1']:>+10.3f}%  "
          f"{row['mean_t2']:>+10.3f}%  {dom:<12}")

# CSVで保存
df_sorted.head(N).to_csv('top_rules.csv', index=False)
print(f"\n保存: top_rules.csv")
EOF

# 実行
python3 extract_top_rules.py
```

### 2. 特定条件でのフィルタリング

```python
# 例: 集中度55%以上のルールのみ抽出
df_high_conc = df_results[df_results['conc'] >= 55.0]
print(f"集中度55%以上: {len(df_high_conc)}件")

# 例: Q1象限（両方向上昇）に集中するルールのみ
df_q1_rules = df_results[df_results['q1'] == df_results[['q1','q2','q3','q4']].max(axis=1)]
df_q1_rules = df_q1_rules[df_q1_rules['conc'] >= 50.0]
print(f"Q1集中（50%以上）: {len(df_q1_rules)}件")
```

### 3. ルール属性の確認

```bash
# 特定ルールIDの属性を確認（例: Rule 1231）
awk 'NR==1 || NR==1232' 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt | column -t
```

---

## 可視化方法

### 1. 個別ルールの散布図

```bash
# Rule ID 1231 の X(t+1) vs X(t+2) 散布図
python3 analysis/crypt/plot_single_rule_2d_future.py --rule-id 1231 --symbol BTC
```

**出力:**
- 散布図PNG画像（象限分布、平均線、統計情報付き）
- ファイル名: `rule_1231_2d_scatter.png`（推定）

### 2. 集中度 vs Mean の散布図

```python
# scatter_conc_vs_mean.py として保存
import pandas as pd
import matplotlib.pyplot as plt

# 既に計算済みの results から
df_results = pd.DataFrame(results)

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

# X(t+1)
ax1.scatter(df_results['conc'], df_results['mean_t1'].abs(), alpha=0.5)
ax1.axhline(y=0.5, color='r', linestyle='--', label='|mean| = 0.5%')
ax1.axvline(x=50, color='g', linestyle='--', label='Conc = 50%')
ax1.set_xlabel('Concentration (%)')
ax1.set_ylabel('|X(t+1)_mean| (%)')
ax1.set_title('Concentration vs |Mean| for X(t+1)')
ax1.legend()
ax1.grid(True, alpha=0.3)

# X(t+2)
ax2.scatter(df_results['conc'], df_results['mean_t2'].abs(), alpha=0.5, color='orange')
ax2.axhline(y=0.5, color='r', linestyle='--', label='|mean| = 0.5%')
ax2.axvline(x=50, color='g', linestyle='--', label='Conc = 50%')
ax2.set_xlabel('Concentration (%)')
ax2.set_ylabel('|X(t+2)_mean| (%)')
ax2.set_title('Concentration vs |Mean| for X(t+2)')
ax2.legend()
ax2.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('concentration_vs_mean.png', dpi=150)
print("Saved: concentration_vs_mean.png")
```

### 3. 集中度分布のヒストグラム

```python
import matplotlib.pyplot as plt

plt.figure(figsize=(10, 6))
plt.hist(df_results['conc'], bins=50, edgecolor='black', alpha=0.7)
plt.axvline(x=50, color='r', linestyle='--', linewidth=2, label='Target: 50%')
plt.axvline(x=df_results['conc'].mean(), color='g', linestyle='--', linewidth=2,
            label=f'Mean: {df_results["conc"].mean():.1f}%')
plt.xlabel('Concentration (%)', fontsize=12)
plt.ylabel('Number of Rules', fontsize=12)
plt.title('Distribution of Quadrant Concentration', fontsize=14, fontweight='bold')
plt.legend(fontsize=11)
plt.grid(True, alpha=0.3)
plt.savefig('concentration_distribution.png', dpi=150)
print("Saved: concentration_distribution.png")
```

---

## 実験ワークフロー例

### 実験1: 最小集中度30% vs 40% の比較

**目的**: フィルタ閾値が結果に与える影響を評価

#### ステップ1: 30%で実験

```bash
# 1. パラメータ設定
vim main.c +88
# MIN_CONCENTRATION を 0.30 に設定

vim main.c +222
# CONCENTRATION_THRESHOLD_1 を 0.30 に設定
# CONCENTRATION_BONUS_1〜7 を30%起点に調整

# 2. コンパイル
make clean && make

# 3. 実行
./main BTC 10 > run_min30.log 2>&1

# 4. 実行時間の確認
grep "Total Time:" run_min30.log

# 5. 集中度分析
python3 /tmp/analyze_concentration.py > results_min30.txt
```

#### ステップ2: 40%で実験

```bash
# 1. パラメータ設定
vim main.c +88
# MIN_CONCENTRATION を 0.40 に設定

vim main.c +222
# CONCENTRATION_THRESHOLD_1 を 0.40 に設定
# CONCENTRATION_BONUS_1〜5 を40%起点に調整

# 2. コンパイル
make clean && make

# 3. 実行
./main BTC 10 > run_min40.log 2>&1

# 4. 実行時間の確認
grep "Total Time:" run_min40.log

# 5. 集中度分析
python3 /tmp/analyze_concentration.py > results_min40.txt
```

#### ステップ3: 結果比較

```bash
# 比較レポート作成
cat << 'EOF' > compare_results.sh
#!/bin/bash

echo "=== 30% vs 40% Comparison ==="
echo ""
echo "【30%フィルタ】"
grep -A5 "平均集中度" results_min30.txt
echo ""
echo "【40%フィルタ】"
grep -A5 "平均集中度" results_min40.txt
EOF

bash compare_results.sh
```

### 実験2: サポート閾値の影響調査

**目的**: MIN_SUPPORT_COUNT = 20, 30, 50 で比較

```bash
# サポート20
vim main.c +87  # MIN_SUPPORT_COUNT = 20
make clean && make
./main BTC 10 > run_sup20.log 2>&1

# サポート30
vim main.c +87  # MIN_SUPPORT_COUNT = 30
make clean && make
./main BTC 10 > run_sup30.log 2>&1

# サポート50
vim main.c +87  # MIN_SUPPORT_COUNT = 50
make clean && make
./main BTC 10 > run_sup50.log 2>&1

# 各結果のルール数比較
grep "Total integrated rules:" run_sup*.log
```

### 実験3: ボーナス体系の最適化

**目的**: ボーナスポイントの指数関数的増加率を調整

#### パターンA: 緩やかな増加（多様性重視）

```c
#define CONCENTRATION_BONUS_1      1000   // 40%
#define CONCENTRATION_BONUS_2      2500   // 45%: ×2.5
#define CONCENTRATION_BONUS_3      5000   // 50%: ×5
#define CONCENTRATION_BONUS_4      10000  // 55%: ×10
#define CONCENTRATION_BONUS_5      20000  // 60%: ×20
```

#### パターンB: 急激な増加（高集中度特化）

```c
#define CONCENTRATION_BONUS_1      500    // 40%
#define CONCENTRATION_BONUS_2      3000   // 45%: ×6
#define CONCENTRATION_BONUS_3      12000  // 50%: ×24
#define CONCENTRATION_BONUS_4      30000  // 55%: ×60
#define CONCENTRATION_BONUS_5      60000  // 60%: ×120
```

```bash
# パターンAで実行
vim main.c +227  # ボーナス値を編集
make clean && make
./main BTC 10 > run_bonus_gradual.log 2>&1
python3 /tmp/analyze_concentration.py > results_bonus_gradual.txt

# パターンBで実行
vim main.c +227  # ボーナス値を編集
make clean && make
./main BTC 10 > run_bonus_steep.log 2>&1
python3 /tmp/analyze_concentration.py > results_bonus_steep.txt

# 最大集中度の比較
grep "最大集中度" results_bonus_*.txt
```

---

## トラブルシューティング

### 1. コンパイルエラー

#### エラー: `undefined reference to 'sqrt'`

**原因**: 数学ライブラリがリンクされていない

**解決方法**:
```bash
# Makefileを確認
grep LDFLAGS Makefile

# -lm が含まれていることを確認
# 含まれていない場合は追加
LDFLAGS = -lm
```

#### エラー: `warning: unused variable`

**対処**: 警告のみなので実行には影響なし。警告を抑制する場合:

```makefile
CFLAGS = -O3 -Wall -Wno-unused-variable -Wno-unused-but-set-variable
```

### 2. 実行時エラー

#### エラー: ルールが全く生成されない（0 rules）

**原因**: フィルタが厳しすぎる

**解決方法**:
1. `MIN_CONCENTRATION` を下げる（例: 0.40 → 0.35 → 0.30）
2. `MIN_SUPPORT_COUNT` を下げる（例: 50 → 30 → 20）
3. `Maxsigx` を上げる（例: 0.4 → 0.5 → 0.6）

#### エラー: 実行時間が異常に長い（1時間以上）

**原因**: フィルタが厳しすぎて進化が停滞

**解決方法**:
1. Ctrl+C で中断
2. `MIN_CONCENTRATION` を下げる
3. 再実行

**確認コマンド**:
```bash
# Gen 0 の初期ルール数を確認（24個以上が目安）
grep "Gen.=    0:" run_experiment.log
```

### 3. 分析スクリプトエラー

#### エラー: `ModuleNotFoundError: No module named 'pandas'`

**解決方法**:
```bash
pip3 install pandas numpy matplotlib
```

#### エラー: CSVファイルが見つからない

**確認**:
```bash
# verification ディレクトリの存在確認
ls -ld 1-deta-enginnering/crypto_data_hourly/output/BTC/verification/

# CSVファイルの存在確認
ls 1-deta-enginnering/crypto_data_hourly/output/BTC/verification/ | wc -l
```

### 4. 結果の解釈

#### 問題: 平均集中度が30%台で停滞

**原因**: ランダムベースライン（25%）からの改善が限定的

**対応**:
1. ボーナスポイントを増やす（指数的増加率を上げる）
2. 世代数を増やす（201 → 500など）
3. Trials数を増やす（10 → 20など）

#### 問題: 高集中度ルール（50%以上）が少ない

**対応**:
1. `CONCENTRATION_BONUS_3`以降を大幅に増やす
2. `SUPPORT_WEIGHT` を下げて集中度の相対的重要性を上げる
3. MIN_CONCENTRATIONをやや下げて初期多様性を確保（例: 40% → 35%）

---

## クイックリファレンス

### よく使うコマンド集

```bash
# === コンパイルと実行 ===
make clean && make                    # 再コンパイル
./main BTC 10 > run.log 2>&1         # 実行してログ保存
tail -f run.log                       # 実行監視

# === 結果確認 ===
grep "Total Time:" run.log            # 実行時間
grep "Total integrated rules:" run.log  # 総ルール数
head -11 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt | column -t  # 上位10ルール

# === 分析 ===
python3 /tmp/analyze_concentration.py  # 集中度分析
python3 extract_top_rules.py           # 上位ルール抽出
python3 analysis/crypt/plot_single_rule_2d_future.py --rule-id 1231 --symbol BTC  # 散布図

# === パラメータ編集 ===
vim main.c +88   # MIN_CONCENTRATION
vim main.c +87   # MIN_SUPPORT_COUNT
vim main.c +222  # CONCENTRATION_THRESHOLD/BONUS
vim main.c +213  # SUPPORT_WEIGHT
```

### 推奨実験パラメータセット

| 実験目的 | MIN_CONC | MIN_SUP | Maxsigx | BONUS_1 | BONUS_3 | 期待結果 |
|---------|----------|---------|---------|---------|---------|----------|
| 多様性重視 | 0.30 | 20 | 0.6 | 500 | 5000 | 多数のルール、平均集中度低 |
| バランス型（推奨）| 0.35 | 30 | 0.5 | 1000 | 8000 | 適度なルール数、平均35% |
| 高品質特化 | 0.40 | 40 | 0.4 | 500 | 12000 | 少数ルール、高集中度 |
| 極端（研究用）| 0.45 | 50 | 0.35 | 1000 | 20000 | 非常に少数、最高品質 |

---

## まとめ

このガイドに従えば、以下のことが可能です:

1. ✅ パラメータを自由に調整して実験
2. ✅ 実験結果の分析と可視化
3. ✅ 上位ルールの抽出と評価
4. ✅ 異なる設定での比較実験

**推奨実験フロー:**
1. まず `MIN_CONCENTRATION=0.35`, `MIN_SUPPORT_COUNT=30` でベースライン確立
2. 集中度分布を確認し、50%以上のルール数を評価
3. ボーナス体系を調整して高集中度ルールを増やす
4. 最終的に50%以上のルールが50-100件程度になる設定を探す

質問や問題がある場合は、このドキュメントを参照してください。
