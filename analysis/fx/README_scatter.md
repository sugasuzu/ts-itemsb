# Rule-Based Scatter Plot Generator

発見されたルールに対して、条件に合致するデータポイントのX値（変化率）の局所分布を可視化するツールです。

## 概要

このスクリプトは以下を行います：

1. **ルール解析**: `output/{PAIR}/{direction}/pool/zrp01a.txt` からルールを読み込み
2. **データマッチング**: 元データ `forex_data/extreme_gnminer/{PAIR}_{direction}.txt` から条件に合致するレコードを特定
3. **X値抽出**: マッチしたレコードの t+1 時点のX値を取得
4. **可視化**: 1次元散布図、ヒストグラム、Box plotで分布を可視化

## 時間軸の理解

```
ルール: NZDJPY_Up(t-1) AND EURAUD_Up(t-1)

時間軸:
  t-4    t-3    t-2    t-1    t-0     t+1
  ↓      ↓      ↓      ↓      ↓       ↓
[過去データ............現在]   [予測対象]
        属性条件チェック       取得するX値
```

### マッチング例

```
データ:
行idx  NZDJPY_Up  EURAUD_Up  X
0         0          1      1.2
1         1          0      1.5
2         1          1      1.8  ← 現在時点t
3         0          1      1.4  ← t+1 (取得)
4         1          1      2.1
```

**行2を現在時点tとして評価**:
- 条件: `NZDJPY_Up(t-1) AND EURAUD_Up(t-1)`
- t-1 = 行1: NZDJPY_Up=1, EURAUD_Up=0 → **不一致**

**行3を現在時点tとして評価**:
- 条件: `NZDJPY_Up(t-1) AND EURAUD_Up(t-1)`
- t-1 = 行2: NZDJPY_Up=1, EURAUD_Up=1 → **一致！**
- **取得するX値**: t+1 = 行4のX = **2.1**

## 使用方法

### 基本使用

```bash
# デフォルト: サポート数の多い順で上位50ルール
python analysis/fx/rule_scatter_plot.py GBPAUD positive

# サポート数の多い順で上位10ルール
python analysis/fx/rule_scatter_plot.py GBPAUD positive --top 10

# ExtremeScoreの高い順で上位20ルール
python analysis/fx/rule_scatter_plot.py GBPAUD positive --top 20 --sort-by extreme_score

# SNRの高い順（シグナル品質）
python analysis/fx/rule_scatter_plot.py USDJPY negative --top 15 --sort-by snr

# 発見順（ファイルの元の順序）
python analysis/fx/rule_scatter_plot.py EURJPY positive --top 10 --sort-by discovery

# 最小サンプル数を10に設定
python analysis/fx/rule_scatter_plot.py GBPAUD positive --min-samples 10
```

### オプション

```
positional arguments:
  pair                  Currency pair (e.g., GBPAUD)
  direction            Direction (positive/negative)

optional arguments:
  --top N              Number of top rules to process (default: 50)
  --min-samples N      Minimum samples required for plotting (default: 5)
  --sort-by CRITERION  Sort criterion (default: support)
                       Choices: support, extreme_score, snr, extremeness, discovery
```

### ソート基準の説明

| 基準 | 説明 | 推奨用途 |
|------|------|----------|
| **support** (デフォルト) | サポート数（マッチング数）の多い順 | 頑健性を重視 |
| **extreme_score** | ExtremeScoreの高い順 | 総合的な極値シグナル品質 |
| **snr** | SNR（Signal-to-Noise Ratio）の高い順 | クリアなシグナル |
| **extremeness** | 極値の強さの順 | 強い変動パターン |
| **discovery** | 発見順（ファイルの元の順序） | 時系列的な分析 |

## 出力構造

```
analysis/fx/visualizations/
├── GBPAUD_positive/
│   ├── rule_0000_scatter.png
│   ├── rule_0001_scatter.png
│   ├── rule_0002_scatter.png
│   └── ...
├── GBPAUD_negative/
│   └── ...
└── USDJPY_positive/
    └── ...
```

## 可視化内容

各散布図には以下が含まれます：

### 1. 上段: 1次元散布図
- **X軸**: X値（変化率 %）
- **Y軸**: ランダムジッター（重なり防止用）
- **赤破線**: 平均値
- **オレンジ点線**: 中央値
- **赤色帯**: ±1σ範囲

### 2. 中段左: ヒストグラム
- X値の度数分布
- 平均値・中央値のライン表示

### 3. 中段右: 統計情報
```
Statistics:
━━━━━━━━━━━━━━━━━━━━
Count:     34
Mean:      1.302
Median:    1.285
Std Dev:   0.242
Min:       1.003
Max:       2.197
Range:     1.194

Rule Metrics:
━━━━━━━━━━━━━━━━━━━━
Support:   32 (18.39%)
SNR:       5.975
Extremeness: 3.296
Signal:    1.261
```

### 4. 下段: Box Plot
- 四分位範囲、外れ値の可視化

## 実装詳細

### ルール解析 (`parse_rule_file`)

zrp01a.txtから属性条件を解析：

```python
Input: "NZDJPY_Up(t-1)	EURAUD_Up(t-1)	0	..."
Output: [
    {'attr': 'NZDJPY_Up', 'delay': 1},
    {'attr': 'EURAUD_Up', 'delay': 1}
]
```

### データマッチング (`match_rule_to_data`)

```python
# 最大遅延を計算
max_delay = max([1, 4, 2]) = 4

# 評価可能範囲
start_idx = 4  # t-4まで参照するため
end_idx = len(data) - 1  # t+1を取得するため

# 各時点tを評価
for t in range(start_idx, end_idx):
    # 全条件をチェック
    for condition in conditions:
        check_idx = t - condition['delay']
        if data[check_idx][condition['attr']] != 1:
            break
    else:
        # 全条件一致 → t+1のXを取得
        X_values.append(data[t + 1]['X'])
```

## 使用例

### Example 1: GBPAUD positive サポート数Top 10

```bash
python analysis/fx/rule_scatter_plot.py GBPAUD positive --top 10 --sort-by support
```

**出力**:
```
✓ Sorted by support_count (descending)
✓ Parsed 10 rules

Processing 10 rules...
  ✓ Rule 2121:   82 records matched (47.13% support)
  ✓ Rule 15411:  76 records matched (43.68% support)
  ✓ Rule 10218:  76 records matched (43.68% support)
  ...

Total rules:       10
Plots created:     10
Skipped (< 5):     0
```

**Top 3ルール**:
1. Rule 2121: `AUDJPY_Up(t-3) AND NZDJPY_Up(t-3)` - 82 records (47.13%)
2. Rule 15411: `AUDUSD_Up(t-1) AND NZDUSD_Up(t-1)` - 76 records (43.68%)
3. Rule 10218: `EURUSD_Up(t-1) AND USDCHF_Down(t-1)` - 76 records (43.68%)

### Example 2: USDJPY negative ExtremeScore Top 5

```bash
python analysis/fx/rule_scatter_plot.py USDJPY negative --top 5 --sort-by extreme_score
```

**出力**:
```
✓ Sorted by ExtremeScore (descending)
✓ Parsed 5 rules

Processing 5 rules...
  ✓ Rule 13257:  18 records matched
  ✓ Rule 3905:   18 records matched
  ...
```

### Example 3: SNRでソート（シグナル品質重視）

```bash
python analysis/fx/rule_scatter_plot.py EURJPY positive --top 15 --sort-by snr
```

## 注意事項

1. **データファイルの存在確認**
   - `output/{PAIR}/{direction}/pool/zrp01a.txt`
   - `forex_data/extreme_gnminer/{PAIR}_{direction}.txt`

2. **属性名の一致**
   - ルールの属性名とデータの列名が一致している必要があります

3. **評価可能範囲**
   - 最大遅延に応じて、最初の数行はスキップされます
   - t+1を取得するため、最後の1行もスキップされます

4. **最小サンプル数**
   - デフォルトで5サンプル未満のルールはスキップされます
   - `--min-samples` オプションで調整可能

## トラブルシューティング

### エラー: "Attribute not found in data"

**原因**: ルールの属性名がデータに存在しない

**解決策**:
- データファイルの列名を確認
- ルールファイルの属性名を確認

### 警告: "No matched records"

**原因**: 条件に合致するレコードが存在しない

**解決策**:
- データの期間を確認
- 条件が厳しすぎる可能性を検討

## 依存パッケージ

```bash
pip install pandas numpy matplotlib
```

## 更新履歴

- **2025-10-30**: 初版リリース
  - ルール解析機能
  - 時間遅延対応マッチング
  - 1次元散布図可視化
