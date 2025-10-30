# Stock Analysis Tools

株価データ用の分析・可視化ツール

## ディレクトリ構造

```
analysis/stock/
├── rule_statistics_analysis.py             # ルール統計分析スクリプト（NEW!）
├── batch_analyze_all_stocks.sh             # 全銘柄統計分析バッチ
├── xt_scatter_actual_data_visualization.py # X-T散布図作成スクリプト
├── batch_visualize_all_stocks.sh           # 全銘柄可視化バッチ
├── visualizations_xt_actual/               # 可視化結果（gitignore対象）
│   ├── 7203/                               # 銘柄ごとのディレクトリ
│   │   ├── rule_0001_xt_scatter.png
│   │   ├── rule_0002_xt_scatter.png
│   │   └── ...
│   └── ...
├── significant_rules_*.csv                 # 有意なルール一覧
└── README.md
```

## 使用方法

### 1. ルール統計分析（NEW!）

**単一銘柄の分析:**
```bash
python3 analysis/stock/rule_statistics_analysis.py 7203
```

**全銘柄の分析:**
```bash
python3 analysis/stock/rule_statistics_analysis.py
# または
bash analysis/stock/batch_analyze_all_stocks.sh
```

**分析内容:**
- 基本統計量（X_mean, X_sigma, t統計量の分布）
- 統計的有意性分析（|t| >= 2.0のルール数）
- X_meanの分布とパーセンタイル
- サポート数と有意性の関係
- シグナル対ノイズ比（SNR）分析
- 上位10件の最も有意なルール
- 有意なルールのCSVエクスポート

### 2. 単一銘柄の可視化

```bash
python3 analysis/stock/xt_scatter_actual_data_visualization.py
```

このスクリプトは`output/`ディレクトリ内の全銘柄を自動検出し、
各銘柄の上位10ルールを可視化します。

### 3. バッチ処理

```bash
# 統計分析
bash analysis/stock/batch_analyze_all_stocks.sh

# 可視化
bash analysis/stock/batch_visualize_all_stocks.sh
```

全銘柄を一括で処理します。

### 4. プログラムから個別実行

**統計分析:**
```python
from analysis.stock.rule_statistics_analysis import RuleStatisticsAnalyzer

analyzer = RuleStatisticsAnalyzer('7203')
analyzer.run_full_analysis()
```

**可視化:**
```python
from analysis.stock.xt_scatter_actual_data_visualization import ActualDataScatterPlotterXT

plotter = ActualDataScatterPlotterXT('7203')
plotter.process_all_rules(max_rules=10, min_samples=1, sort_by='support')
```

## 出力

### X-T散布図

- **全体データ**: 灰色の点（全期間の変化率）
- **ルール適用データ**: 赤色の点（ルール条件に合致した時点の翌日の変化率）
- **統計情報**: 平均値（破線）、±1σ範囲（塗りつぶし）

### ファイル名形式

```
rule_XXXX_xt_scatter.png
```

- `XXXX`: ルールインデックス
- ルールはsupport_count（サポート数）の降順でソートされます

## 前提条件

### 必要なファイル

1. **ルールファイル**: `output/{stock_code}/pool/zrp01a.txt`
   - GNMinerで発見されたルール

2. **データファイル**: `nikkei225_data/gnminer_individual/{stock_code}.txt`
   - 株価データ（バイナリ形式）

### Pythonパッケージ

```bash
pip install pandas matplotlib
```

## 設定

### 可視化するルール数を変更

```python
# 上位5ルールのみ可視化
plotter.process_all_rules(max_rules=5, min_samples=1, sort_by='support')
```

### Y軸の範囲を変更

`xt_scatter_actual_data_visualization.py`の244行目を編集:

```python
# デフォルト: -10% ~ +10%
ax.set_ylim(-10, 10)

# 例: -20% ~ +20%に変更
ax.set_ylim(-20, 20)
```

## 可視化の見方

### 局所化効果

ルール適用によって、全体データから特定パターンが抽出されます:

- **狭い分散**: ルールが特定の状況を捉えている
- **明確な平均**: ルールが方向性のある変化を予測している
- **時間的集中**: 特定の期間に有効なルール

### 統計情報ボックス

右上のボックスに表示される情報:

```
Localization Effect:
━━━━━━━━━━━━━━━━━━━━
All data:      5268 records
Rule matched:  2410 records (45.7%)

Matched Statistics:
━━━━━━━━━━━━━━━━━━━━
Mean:    0.123
Std:     1.456
Min:    -3.234
Max:     4.567
Range:   7.801
```

## 統計分析結果の解釈

### t統計量について

**t = X_mean / (X_sigma / √n)**

- **|t| >= 2.0**: 95%信頼水準で有意（偶然ではない）
- **|t| >= 2.58**: 99%信頼水準で有意
- **|t| >= 3.0**: 99.7%信頼水準で有意（極めて高い信頼性）

### 重要な指標

1. **Significance Rate（有意率）**
   - 全ルール中、統計的に有意（|t|>=2.0）なルールの割合
   - 10%前後が典型的
   - 高いほど予測可能性が高い

2. **Signal-to-Noise Ratio (SNR)**
   - SNR = |X_mean| / X_sigma
   - 0.1以上: 強いシグナル
   - 0.05-0.1: 中程度のシグナル
   - <0.05: 弱いシグナル（ノイズが支配的）

3. **X_meanの大きさ**
   - 金融データでは0.05%（0.0005）でも価値がある
   - 重要なのは絶対値ではなく、t統計量による有意性

### 典型的な結果例

```
Total Significant Rules (|t|>=2.0): 217 (10.8%)
Mean SNR: 0.037516
High SNR Rules (SNR > 0.1): 45 (2.2%)
```

**解釈:**
- 約10%のルールが統計的に有意
- SNRは低いが、これは金融データでは正常
- SNR>0.1のルールは強い予測力を持つ可能性

## 注意事項

- 可視化画像は`.gitignore`に含まれており、Gitで追跡されません
- 大量の銘柄を処理する場合、完了まで時間がかかる場合があります
- メモリ使用量が多い場合は、一度に処理する銘柄数を減らしてください
- X_meanが小さくても、t統計量が大きければ価値のあるルール
- 統計的有意性は過去データに基づくため、将来の保証ではない
