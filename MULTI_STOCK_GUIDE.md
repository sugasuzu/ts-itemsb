# 複数銘柄対応版 GNMiner Phase 2 - 使用ガイド

## 📋 概要

このプログラムは、日経225銘柄のデータを使用して、時系列株価予測ルールを自動的に発見します。
Phase 2では、複数の銘柄を自動的に処理できるように拡張されています。

## 🛠️ セットアップ

### 1. コンパイル

```bash
gcc -o main main.c -lm
```

### 2. データファイルの確認

データは `nikkei225_data/gnminer_individual/` ディレクトリに配置されている必要があります：

```
nikkei225_data/gnminer_individual/
├── 7203.txt   (トヨタ)
├── 9984.txt   (ソフトバンク)
└── ... (その他220銘柄)
```

## 🚀 使用方法

### 方法1: 直接実行（単一銘柄）

最も基本的な実行方法です：

```bash
# デフォルト銘柄（7203: トヨタ）で実行
./main

# 特定の銘柄を指定
./main 7203    # トヨタ
./main 9984    # ソフトバンク
./main 6758    # ソニー

# ヘルプを表示
./main --help
```

### 方法2: シェルスクリプトで実行（単一銘柄、推奨）

より使いやすいインターフェースです：

```bash
# 使い方を表示
./run_stock.sh

# 特定の銘柄を分析
./run_stock.sh 7203
./run_stock.sh 9984
```

**特徴:**
- ✅ 色付き出力で進捗が見やすい
- ✅ 実行時間の自動計測
- ✅ エラーハンドリング
- ✅ 結果の詳細なサマリー表示

### 方法3: バッチ処理（全銘柄一括、長時間実行）

全220銘柄を自動的に処理します：

```bash
./run_all_stocks.sh
```

**注意:**
- ⏰ 実行には**非常に長い時間**がかかります（数時間～数日）
- 📊 進捗状況とログを `batch_run_YYYYMMDD_HHMMSS.log` に記録
- 🔄 各銘柄の成功/失敗を追跡
- 📂 銘柄ごとに個別の出力ディレクトリを作成

**実行例:**
```bash
$ ./run_all_stocks.sh
==========================================
  GNMiner Phase 2 - Batch Processing
==========================================

Found 220 stock data files

Process all 220 stocks? (y/n):
```

## 📁 出力ファイル構造

各銘柄ごとに個別のディレクトリが作成されます：

```
output_7203/                    # 銘柄7203の出力
├── IL/                         # Individual Lists (ルールリスト)
│   ├── IL01000.txt
│   ├── IL01001.txt
│   └── ...
├── IA/                         # Individual Analysis (分析レポート)
│   ├── IA01000.txt
│   ├── IA01001.txt
│   └── ...
├── IB/                         # Individual Backup (詳細バックアップ)
│   ├── IB01000.txt
│   ├── IB01001.txt
│   └── ...
├── pool/                       # グローバルルールプール
│   ├── zrp01a.txt             # 詳細版（TSV、actual_X含む）
│   └── zrp01b.txt             # 簡易版
├── doc/                        # ドキュメント・統計
│   └── zrd01.txt              # 統計サマリー（TSV）
└── vis/                        # 可視化用データ
    ├── rule_1000_0001.csv
    ├── rule_1000_0002.csv
    └── ...

output_9984/                    # 銘柄9984の出力
├── IL/
├── IA/
└── ...
```

## 📊 出力ファイルの説明

### 1. ルールリスト (IL/*.txt)

各試行で発見されたルールのリスト：

```
Rule 1:
  Antecedent: attr_42(t-1)=1 AND attr_58(t-2)=1
  Predicted X: mean=1250.5, sigma=3.2
  Current X: mean=1248.3, sigma=3.5    ← Phase 2で追加
  Support: 15.2% (count=45/296)
  Chi-squared: 8.45 (p<0.01)           ← Phase 2で追加
  Period: 2020-01-15 to 2021-06-30
```

### 2. 統計ファイル (doc/zrd01.txt)

TSV形式の統計サマリー：

```
Trial  Rules  HighSup  LowVar  HighChi  Total  TotalHigh  TotalLow  TotalHighChi  Min2Attr  Time(sec)
1      12     5        8       10       12     5          8         10            12        45.23
2      18     7        10      15       30     12         18        25            30        46.12
...
```

### 3. ルールプール (pool/zrp01a.txt)

全試行を通じて発見された全ルールの詳細版：

- **zrp01a.txt**: 詳細版（属性名、actual_X統計含む）
- **zrp01b.txt**: 簡易版（属性番号のみ）

### 4. 可視化データ (vis/*.csv)

各ルールのマッチング時系列データ（Python分析用）：

```csv
index,timestamp,X,matched
0,2020-01-15,1245.5,0
1,2020-01-16,1250.2,1
2,2020-01-17,1248.8,1
...
```

## ⚙️ パラメータ調整

プログラムのパラメータは `main.c` のヘッダー部分で定義されています：

### 主要パラメータ

```c
#define Generation 201        // 進化世代数（201世代）
#define Nkotai 120           // 個体数（120個体）
#define Ntry 100             // 試行回数（100回）

// ルール品質基準
#define Minsup 0.1           // 最小サポート値（10%）
#define Maxsigx 5.0          // 最大標準偏差
#define MIN_ATTRIBUTES 2     // 最小属性数
#define MIN_CHI_SQUARED 3.84 // 最小カイ二乗値（5%有意水準）

// 時系列パラメータ
#define MAX_TIME_DELAY_PHASE2 4  // 最大時間遅延（t-3まで）
#define PREDICTION_SPAN 1        // 予測スパン（t+1）
```

パラメータを変更した場合は、再コンパイルが必要です：

```bash
gcc -o main main.c -lm
```

## 🔬 Phase 2の新機能

1. **actual_X統計**: 現在時点（t）の実際値も記録
2. **Chi-square検定**: 統計的有意性の評価
3. **複数銘柄対応**: 220銘柄を自動処理
4. **銘柄別出力**: 各銘柄ごとに独立したディレクトリ

## 📈 実行時間の目安

| 処理内容 | 銘柄数 | 推定時間 |
|---------|-------|---------|
| 単一銘柄 | 1 | 約5-10分 |
| 少数銘柄 | 5-10 | 約30分-1時間 |
| 全銘柄 | 220 | 約20-40時間 |

※ 実行時間はマシンスペックとデータサイズに依存します

## 🐛 トラブルシューティング

### コンパイルエラー

```bash
# math ライブラリがリンクされているか確認
gcc -o main main.c -lm
```

### データファイルが見つからない

```bash
# データディレクトリの確認
ls nikkei225_data/gnminer_individual/

# ファイルパーミッションの確認
chmod 644 nikkei225_data/gnminer_individual/*.txt
```

### メモリ不足エラー

プログラムは動的メモリを使用します。データが大きい場合は、パラメータを調整してください：

```c
#define Ntry 50              // 試行回数を減らす
#define Generation 100       // 世代数を減らす
```

## 💡 Tips

### 1. 並列実行

複数のターミナルで異なる銘柄を並列実行できます：

```bash
# ターミナル1
./run_stock.sh 7203

# ターミナル2
./run_stock.sh 9984

# ターミナル3
./run_stock.sh 6758
```

### 2. バックグラウンド実行

長時間実行の場合は、バックグラウンドで実行：

```bash
nohup ./run_all_stocks.sh > run_all.log 2>&1 &

# 進捗確認
tail -f run_all.log
```

### 3. 結果の分析

Python/Jupyterで結果を分析する場合：

```python
import pandas as pd

# 統計ファイルを読み込み
stats = pd.read_csv('output_7203/doc/zrd01.txt', sep='\t')

# 可視化データを読み込み
rule_data = pd.read_csv('output_7203/vis/rule_1000_0001.csv')
```

## 📞 サポート

問題が発生した場合は、以下を確認してください：

1. ✅ コンパイルが正常に完了しているか
2. ✅ データファイルが正しいディレクトリにあるか
3. ✅ 実行権限が付与されているか (`chmod +x`)
4. ✅ ディスク容量が十分にあるか

## 📄 ライセンス

このプログラムは研究・教育目的で使用できます。

---

**作成日**: 2025年
**バージョン**: Phase 2 (Multi-Stock Support)
