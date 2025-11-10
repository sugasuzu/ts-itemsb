# 象限集中度分析ツール

## 目的

X(t+1) vs X(t+2) の散布図で、**特定の象限にデータが集中していること**を可視化します。

これは、CLAUDE.mdで定義された研究目標「散布図で1象限に集中するパターンを発見」に対応しています。

## 出力される図

### 1. 個別ルールの散布図 (`BTC_rule_XXX_quadrant.png`)

各ルールについて:
- **散布図**: 各マッチポイントを色分け表示
  - 🟢 Q1 (++): 両方正 (右上)
  - 🔵 Q2 (-+): t+1負, t+2正 (左上)
  - 🔴 Q3 (--): 両方負 (左下)
  - 🟠 Q4 (+-): t+1正, t+2負 (右下)
- **背景色**: 最大集中象限を強調
- **統計ボックス**: 各象限の割合を表示
- **平均値**: 紫色のX印で表示

### 2. 集中度比較チャート (`BTC_concentration_comparison.png`)

すべてのルールの集中度を棒グラフで比較:
- 各棒の色 = 最大集中象限
- 閾値ライン:
  - 赤線 (40%): MIN_CONCENTRATION (フィルタ通過ライン)
  - 橙線 (50%): 目標ライン
  - 緑線 (60%): 理想ライン

## 使用方法

### 基本的な使い方

```bash
python3 analysis/crypt/plot_quadrant_concentration.py \
  --pool "1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt" \
  --verification-dir "1-deta-enginnering/crypto_data_hourly/output/BTC/verification" \
  --symbol BTC \
  --top-n 15 \
  --output-dir "output/quadrant_analysis"
```

### オプション

| オプション | 短縮形 | 説明 | デフォルト |
|-----------|--------|------|-----------|
| `--pool` | `-p` | ルールプールファイルのパス | 必須 |
| `--verification-dir` | `-v` | 検証データディレクトリ | 必須 |
| `--symbol` | `-s` | 銘柄名（タイトル用） | BTC |
| `--top-n` | `-n` | 処理するルール数 | 10 |
| `--output-dir` | `-o` | 出力ディレクトリ | output/quadrant_analysis |
| `--min-concentration` | `-m` | 最小集中度閾値 | 0.40 (40%) |

### 例: より厳しい閾値で実行

```bash
python3 analysis/crypt/plot_quadrant_concentration.py \
  --pool "1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt" \
  --verification-dir "1-deta-enginnering/crypto_data_hourly/output/BTC/verification" \
  --symbol BTC \
  --top-n 20 \
  --min-concentration 0.50 \
  --output-dir "output/quadrant_strict"
```

### 例: 全88ルールをチェック

```bash
python3 analysis/crypt/plot_quadrant_concentration.py \
  --pool "1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt" \
  --verification-dir "1-deta-enginnering/crypto_data_hourly/output/BTC/verification" \
  --symbol BTC \
  --top-n 88 \
  --min-concentration 0.40 \
  --output-dir "output/quadrant_all"
```

## 実行結果の例

```
======================================================================
  Quadrant Concentration Visualizer
======================================================================
Pool file: 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt
Verification dir: 1-deta-enginnering/crypto_data_hourly/output/BTC/verification
Symbol: BTC
Top N: 15
Min concentration: 40.0%
======================================================================

[1/3] Loading rule pool...
  ✓ Loaded 88 rules
  ✓ Rule 1: Q1(++) = 40.0%
  ✓ Rule 2: Q2(-+) = 41.7%
  ✓ Rule 3: Q2(-+) = 41.8%
  ⊘ Rule 4: Max concentration 38.1% < threshold
  ✓ Rule 5: Q3(--) = 48.0%
  ✓ Rule 6: Q3(--) = 50.0%
  ✓ Rule 7: Q3(--) = 50.0%
  ...
  → 11 rules pass concentration threshold

[2/3] Generating individual scatter plots...
  ✓ Saved: output/quadrant_analysis/BTC_rule_001_quadrant.png
  ✓ Saved: output/quadrant_analysis/BTC_rule_002_quadrant.png
  ...

[3/3] Generating concentration comparison chart...
  ✓ Saved: output/quadrant_analysis/BTC_concentration_comparison.png

======================================================================
  Complete!
  Generated 11 scatter plots
  Output directory: output/quadrant_analysis
======================================================================
```

## 結果の解釈

### 良いルールの特徴

1. **高い集中度**: 1つの象限に50%以上集中
2. **明確な方向性**: 平均値が象限の中心に位置
3. **統計的有意性**: マッチ数が十分多い (n > 20)

### 象限の意味

| 象限 | 記号 | 意味 | トレード戦略例 |
|-----|------|------|--------------|
| Q1  | (++) | 両期間上昇 | 強気継続パターン |
| Q2  | (-+) | 反転上昇 | 売られすぎからの反発 |
| Q3  | (--) | 両期間下落 | 弱気継続パターン |
| Q4  | (+-) | 反転下落 | 買われすぎからの調整 |

## CLAUDE.md研究目標との対応

このツールは以下の研究目標に対応しています:

### 象限集中度ボーナス (CONCENTRATION_THRESHOLD_*)

```c
#define MIN_CONCENTRATION 0.40  // 40%以上のルールのみ登録
#define CONCENTRATION_THRESHOLD_3 0.50  // 50%以上（目標ライン）
#define CONCENTRATION_THRESHOLD_5 0.60  // 60%以上（理想ライン）
```

### 可視化で確認できること

1. **フィルタの妥当性**: 40%閾値が適切か？
2. **分布の偏り**: 本当に特定象限に集中しているか？
3. **ルール間の比較**: どのルールが最も集中しているか？
4. **統計的有意性**: 偶然ではなく、構造的なパターンか？

## トラブルシューティング

### エラー: "Pool file not found"

パスが正しいか確認してください:
```bash
ls -la 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt
```

### エラー: "No verification data"

verificationディレクトリにCSVファイルが存在するか確認:
```bash
ls -la 1-deta-enginnering/crypto_data_hourly/output/BTC/verification/
```

main.cで検証データが出力されている必要があります。

### すべてのルールが閾値を下回る

`--min-concentration` を下げてみてください:
```bash
--min-concentration 0.35  # 35%に緩和
```

## 関連ファイル

- `plot_2d_future_scatter.py`: 2σ楕円を含む標準的な散布図
- `plot_directional_bias_scatter.py`: 方向性バイアスの可視化
- `../../main.c`: ルール発見とフィルタリングの実装
- `../../CLAUDE.md`: 研究目標の詳細定義
