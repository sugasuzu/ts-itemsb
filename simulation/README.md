# Rule-Based Trade Simulator

発見されたルールを使って実際のトレードをシミュレートし、パフォーマンスを評価するシステムです。

## 概要

このシミュレーターは以下を行います：

1. **ルール読み込み**: `output/{PAIR}/positive(negative)/pool/zrp01a.txt` から上位Nルールを読み込み
2. **シグナル生成**: 全履歴データ `forex_data/gnminer_individual/{PAIR}.txt` でルール条件をマッチング
3. **トレード実行**: BUY/SELLシグナルに基づいてトレードを実行
4. **パフォーマンス評価**: 勝率、総利益、ルール別パフォーマンスを計算
5. **レポート生成**: 累積リターンチャート、ルールパフォーマンスチャート、CSVレポート

## ファイル構成

```
simulation/
├── README.md                      # このファイル
├── rule_simulator.py              # メインシミュレータースクリプト
├── rule_loader.py                 # ルール読み込みモジュール
├── signal_generator.py            # シグナル生成モジュール
├── trade_simulator.py             # トレード実行モジュール
├── performance_evaluator.py       # パフォーマンス評価モジュール
└── results/                       # シミュレーション結果（自動生成）
    └── {PAIR}/
        ├── {PAIR}_cumulative_return.png    # 累積リターンチャート
        ├── {PAIR}_rule_performance.png     # ルールパフォーマンスチャート
        ├── {PAIR}_rule_stats.csv           # ルール別統計CSV
        └── {PAIR}_trades.csv               # 個別トレードCSV（--save-tradesオプション使用時）
```

## 使用方法

### 基本使用

```bash
# デフォルト設定（上位10ルール、サポート数順）
python3 simulation/rule_simulator.py GBPAUD

# 上位15ルールをSNRでソート
python3 simulation/rule_simulator.py GBPAUD --top 15 --sort-by snr

# 上位20ルールをExtremeScoreでソート、トレードCSV保存
python3 simulation/rule_simulator.py USDJPY --top 20 --sort-by extreme_score --save-trades
```

### オプション

```
positional arguments:
  pair                  通貨ペア (例: GBPAUD, USDJPY)

optional arguments:
  --top N               各方向（positive/negative）の上位ルール数 (デフォルト: 10)
                        合計: N × 2 ルール

  --sort-by CRITERION   ソート基準 (デフォルト: support)
                        選択肢: support, extreme_score, snr, extremeness, discovery

  --output-dir DIR      結果出力ディレクトリ (デフォルト: simulation/results)

  --save-trades         個別トレードをCSVに保存
```

### ソート基準の説明

| 基準 | 説明 | 推奨用途 |
|------|------|----------|
| **support** (デフォルト) | サポート数（マッチング数）の多い順 | 頑健性を重視 |
| **extreme_score** | ExtremeScoreの高い順 | 総合的な極値シグナル品質 |
| **snr** | SNR（Signal-to-Noise Ratio）の高い順 | クリアなシグナル |
| **extremeness** | 極値の強さの順 | 強い変動パターン |
| **discovery** | 発見順（ファイルの元の順序） | 時系列的な分析 |

## シミュレーションロジック

### 1. シグナル生成

各時点tで全ルールをチェック：

```python
時点t = 0 → 4119:
    # Positive rules (BUY シグナル)
    for rule in positive_rules:
        if 条件マッチ(t-4~t-0の属性):
            → BUY シグナル発生（t+1でX>0を期待）

    # Negative rules (SELL シグナル)
    for rule in negative_rules:
        if 条件マッチ(t-4~t-0の属性):
            → SELL シグナル発生（t+1でX<0を期待）
```

### 2. トレード実行

```python
BUYシグナル:
    - t時点でロングポジション取得
    - t+1時点のX値で利益/損失確定
    - profit = X_t+1（Xが正なら利益）

SELLシグナル:
    - t時点でショートポジション取得
    - t+1時点のX値で利益/損失確定
    - profit = -X_t+1（Xが負なら利益）
```

### 3. 評価指標

- **総取引回数**: BUY + SELL の合計
- **勝率**: (利益取引数 / 総取引数) × 100%
- **平均利益**: 総利益 / 総取引数
- **累積リターン**: Σ(profit)
- **最大ドローダウン**: 累積曲線の最大下落幅
- **ルール別統計**: 各ルールの個別パフォーマンス

## 出力例

### コンソール出力

```
================================================================================
Rule-Based Trade Simulator
================================================================================
Currency Pair: GBPAUD
Top N rules:   10 per direction (total: 20)
Sort by:       support
Output dir:    simulation/results
================================================================================

Step 1: Loading rules...
✓ Total rules loaded: 20 (10 BUY + 10 SELL)

Step 2: Generating signals...
✓ Generated 32527 signals
  BUY signals:  16333
  SELL signals: 16194

Step 3: Simulating trades...
✓ Executed 32527 trades

================================================================================
Trade Statistics: GBPAUD
================================================================================
Total Trades:          32527
  BUY trades:          16333
  SELL trades:         16194

Wins / Losses:         16217 /  16310
Win Rate:              49.86%
  BUY win rate:        50.11%
  SELL win rate:       49.60%

Total Return:         +68.944%
  BUY return:         +123.721%
  SELL return:        -54.778%

Average Profit:       +0.0021%
  BUY avg:            +0.0076%
  SELL avg:           -0.0034%

Average Win:          +0.4305%
Average Loss:         -0.4239%
Max Win:              +6.3281%
Max Loss:             -6.3281%
================================================================================

Step 4: Evaluating performance...

====================================================================================================
Rule Performance Analysis: GBPAUD
====================================================================================================
Rule ID    Signal Direction  Trades   Win Rate   Avg Profit   Total Return
----------------------------------------------------------------------------------------------------
10218      BUY    positive   1591       52.29%     +0.0385%     +61.259%
15628      BUY    positive   1599       50.41%     +0.0153%     +24.469%
5294       BUY    positive   1615       49.47%     +0.0113%     +18.307%
...
====================================================================================================

Maximum Drawdown: -70.811%

Step 5: Generating reports...
✓ Cumulative return plot saved
✓ Rule performance plot saved
✓ Rule statistics saved

================================================================================
Simulation Complete
================================================================================
Results saved to: simulation/results/GBPAUD
================================================================================
```

### 可視化

#### 1. 累積リターンチャート (`{PAIR}_cumulative_return.png`)

- **上段**: 累積リターンの時系列推移
- **下段**: ドローダウンの時系列推移
- **統計ボックス**: 総取引数、勝率、総利益、最大ドローダウン

#### 2. ルールパフォーマンスチャート (`{PAIR}_rule_performance.png`)

- **左**: ルール別総リターン（緑=BUY、赤=SELL）
- **右**: ルール別勝率

## 実行例

### Example 1: GBPAUD（サポート数Top 10）

```bash
python3 simulation/rule_simulator.py GBPAUD --top 10 --sort-by support --save-trades
```

**結果**:
- 総取引: 32,527回
- 勝率: 49.86%
- 総利益: +68.944%
- 最大ドローダウン: -70.811%

**Top 3ルール**:
1. Rule 10218 (BUY): 1591取引, 52.29%勝率, +61.259%リターン
2. Rule 15628 (BUY): 1599取引, 50.41%勝率, +24.469%リターン
3. Rule 5294 (BUY): 1615取引, 49.47%勝率, +18.307%リターン

### Example 2: EURAUD（SNR Top 15）

```bash
python3 simulation/rule_simulator.py EURAUD --top 15 --sort-by snr
```

## 重要な注意事項

### 1. データ要件

シミュレーションには以下のファイルが必要です：

```
output/{PAIR}/positive/pool/zrp01a.txt    # Positiveルール
output/{PAIR}/negative/pool/zrp01a.txt    # Negativeルール
forex_data/gnminer_individual/{PAIR}.txt  # 全履歴データ
```

### 2. シミュレーションの前提

- **トランザクションコスト**: 現在は考慮していません
- **スリッページ**: 考慮していません
- **ポジションサイズ**: 全て同一（1単位）
- **保有期間**: 常に1時点（t → t+1）
- **複数シグナル**: 同時に複数のルールがマッチした場合、全てトレード実行

### 3. 解釈上の注意

- **過去データ**: バックテストであり、将来のパフォーマンスを保証するものではありません
- **オーバーフィッティング**: 極値データで学習したルールを全データでテストしているため、実戦では性能低下の可能性があります
- **勝率50%前後**: ランダムに近い結果の場合、ルールの有効性は限定的です

## トラブルシューティング

### エラー: "Rule file not found"

**原因**: 指定した通貨ペアのルールファイルが存在しない

**解決策**:
```bash
# 既存のルールファイルを確認
find output -name "zrp01a.txt" -type f

# 該当通貨ペアでルール発見を実行
./main {PAIR} positive
./main {PAIR} negative
```

### エラー: "Data file not found"

**原因**: 全履歴データファイルが存在しない

**解決策**:
```bash
# データファイルの存在確認
ls forex_data/gnminer_individual/{PAIR}.txt

# データファイルが存在しない場合は作成が必要
```

### メモリエラー

**原因**: 大量のシグナル生成によるメモリ不足

**解決策**:
- `--top` オプションでルール数を減らす
- より選択的なソート基準（snr, extremenessなど）を使用

## 依存パッケージ

```bash
pip install pandas numpy matplotlib
```

## 今後の拡張予定

- [ ] トランザクションコストの考慮
- [ ] リスク管理機能（ストップロス、利確）
- [ ] 複数通貨ペアのバッチ処理
- [ ] ポートフォリオ最適化
- [ ] リアルタイムシミュレーション機能

## 更新履歴

- **2025-10-30**: 初版リリース
  - 基本シミュレーション機能
  - ルール別パフォーマンス分析
  - 累積リターン可視化
