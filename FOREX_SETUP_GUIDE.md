# 為替データセットアップガイド

## 概要

このガイドでは、GNMiner Forex Predictor用の為替データをダウンロードして準備する手順を説明します。

## 重要な変更点

### Stay判定の閾値
- **変更前**: ±0.1% → Stay
- **変更後**: 0.0%（完全一致のみ） → Stay

### 予測対象変数 X
- **変更前**: 全通貨ペアの平均変化率
- **変更後**: **各通貨ペア自身の前日比変化率**（%）

例: `USDJPY.txt`の場合、XはUSDJPYの翌日の変化率（shift(-1)）

## セットアップ手順

### 1. データダウンロード

```bash
cd getData
python3 download_forex_data.py
```

**出力**: `forex_data/forex_raw.csv`
- 期間: 2010-01-01 ～ 今日まで
- 通貨ペア: 20ペア（USD/JPY, EUR/JPYなど）

### 2. GNMinerフォーマットに変換

```bash
cd getData
python3 convert_to_gnminer_format.py forex_data/forex_raw.csv --split
```

**出力**:
- `forex_data/forex_raw_gnminer.csv` - 統合データ
- `forex_data/gnminer_individual/*.txt` - 個別ペアファイル（20ファイル）

### 3. GNMinerで実行

```bash
# 単一ペアの処理
./main USDJPY

# 全20ペアの一括処理
./main --all
```

## 出力ファイル構造

```
forex_data/
├── forex_raw.csv                 # 生の価格データ
├── forex_raw_gnminer.csv         # 変換後の統合データ
└── gnminer_individual/           # 個別ペアファイル
    ├── USDJPY.txt               # USD/JPY データ
    ├── EURJPY.txt               # EUR/JPY データ
    ├── GBPJPY.txt               # GBP/JPY データ
    ├── AUDJPY.txt               # AUD/JPY データ
    ├── NZDJPY.txt               # NZD/JPY データ
    ├── CADJPY.txt               # CAD/JPY データ
    ├── CHFJPY.txt               # CHF/JPY データ
    ├── EURUSD.txt               # EUR/USD データ
    ├── GBPUSD.txt               # GBP/USD データ
    ├── AUDUSD.txt               # AUD/USD データ
    ├── NZDUSD.txt               # NZD/USD データ
    ├── USDCAD.txt               # USD/CAD データ
    ├── USDCHF.txt               # USD/CHF データ
    ├── EURGBP.txt               # EUR/GBP データ
    ├── EURAUD.txt               # EUR/AUD データ
    ├── EURCHF.txt               # EUR/CHF データ
    ├── GBPAUD.txt               # GBP/AUD データ
    ├── GBPCAD.txt               # GBP/CAD データ
    ├── AUDCAD.txt               # AUD/CAD データ
    └── AUDNZD.txt               # AUD/NZD データ

output/
├── USDJPY/                       # USD/JPY実行結果
│   ├── IL/                       # ルールリスト
│   ├── IA/                       # 分析レポート
│   ├── IB/                       # バックアップ（actual_X付き）
│   ├── pool/                     # ルールプール
│   ├── doc/                      # 統計ドキュメント
│   └── vis/                      # 可視化データ
├── EURJPY/                       # EUR/JPY実行結果
└── ...
```

## データファイルの構造

各`.txt`ファイルは以下の形式：

```csv
timestamp,USDJPY_Up,USDJPY_Stay,USDJPY_Down,...,X,T
2010-01-05,1,0,0,...,0.52,2010-01-05
2010-01-06,0,0,1,...,-0.31,2010-01-06
```

- **Up/Stay/Down**: 前日比の方向（0 or 1）
- **X**: 翌日のこのペアの変化率（%）← **予測対象**
- **T**: タイムスタンプ

## カスタマイズ

### データ期間を変更

`download_forex_data.py`の121行目を編集：

```python
df = download_forex_data(start_date='2015-01-01')  # 開始日を変更
```

### Stay閾値を変更

```bash
python3 convert_to_gnminer_format.py forex_data/forex_raw.csv --stay-threshold 0.1 --split
```

## トラブルシューティング

### yfinanceのインストールエラー

```bash
pip3 install yfinance pandas numpy
```

### データダウンロード失敗

- インターネット接続を確認
- Yahoo Financeがメンテナンス中の可能性

### ペアがスキップされる

一部のペアは土日や祝日でデータがない場合があります。正常です。

## 実行時間の目安

- データダウンロード: 約1-2分
- データ変換: 約10-30秒
- GNMiner実行（1ペア）: 約20-30分
- GNMiner実行（全20ペア）: 約6-10時間
