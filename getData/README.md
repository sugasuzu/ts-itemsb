# getData - FXデータ取得・前処理ツール

このディレクトリには、為替（FX）データのダウンロード、GNMiner形式への変換、および極値データセット作成のためのスクリプトが含まれています。

---

## 📁 ファイル一覧

| ファイル名 | 説明 | 用途 |
|-----------|------|------|
| **download_forex_data.py** | FXデータダウンロード | Yahoo Financeから為替データを取得 |
| **convert_to_gnminer_format.py** | GNMiner形式変換 | ダウンロードしたデータをGNMiner用に変換 |
| **create_extreme_dataset.py** | 極値データ作成（閾値1.0%） | 通常の極値抽出（デフォルト） |
| **create_extreme_dataset_0.5.py** | 極値データ作成（閾値0.5%） | より緩い閾値での極値抽出 |
| **main.ipynb** | Jupyterノートブック | 試験的な分析・検証用 |

---

## 🔄 データ処理フロー

```
1. ダウンロード
   download_forex_data.py
   ↓
   forex_data/raw/

2. GNMiner形式変換
   convert_to_gnminer_format.py
   ↓
   forex_data/gnminer_individual/

3. 極値抽出
   create_extreme_dataset.py (閾値1.0%) または
   create_extreme_dataset_0.5.py (閾値0.5%)
   ↓
   forex_data/extreme_gnminer/

4. ルール発見（GNMiner）
   main.c
   ↓
   output/{PAIR}/{positive,negative}/
```

---

## 📖 各スクリプトの使い方

### 1. download_forex_data.py

**目的**: Yahoo FinanceからFXデータをダウンロード

**基本的な使い方**:
```bash
# 全通貨ペアをダウンロード
python3 getData/download_forex_data.py

# 特定の通貨ペアのみダウンロード
python3 getData/download_forex_data.py --pairs USDJPY EURJPY GBPJPY

# 期間を指定してダウンロード
python3 getData/download_forex_data.py --start 2020-01-01 --end 2025-12-31
```

**オプション**:
- `--pairs`: ダウンロードする通貨ペアを指定（省略時は全ペア）
- `--start`: 開始日（デフォルト: 2010-01-01）
- `--end`: 終了日（デフォルト: 今日）
- `--output`: 出力ディレクトリ（デフォルト: forex_data/raw）

**出力**:
- `forex_data/raw/{PAIR}.csv` - 各通貨ペアの生データ

---

### 2. convert_to_gnminer_format.py

**目的**: ダウンロードしたFXデータをGNMiner形式に変換

**基本的な使い方**:
```bash
# 全通貨ペアを変換
python3 getData/convert_to_gnminer_format.py

# 特定の通貨ペアのみ変換
python3 getData/convert_to_gnminer_format.py --pairs USDJPY EURJPY

# カスタムディレクトリを指定
python3 getData/convert_to_gnminer_format.py \
  --input forex_data/raw \
  --output forex_data/gnminer_individual
```

**GNMiner形式の特徴**:
- 各通貨ペアについて、他の全ペアの動き（Up/Stay/Down）を属性として使用
- 属性数: 60（20ペア × 3状態）
- ターゲット変数: `X`（対象ペアの変化率%）
- タイムスタンプ: `T`（YYYY-MM-DD）

**出力**:
- `forex_data/gnminer_individual/{PAIR}.txt` - GNMiner形式データ
- 各ファイルには約4,000-4,500レコード（2010-2025の日次データ）

---

### 3. create_extreme_dataset.py（閾値1.0%）

**目的**: 極値データのみを抽出（|X| >= 1.0%）

**基本的な使い方**:
```bash
# 全通貨ペアの極値を抽出（閾値1.0%）
python3 getData/create_extreme_dataset.py

# 特定の通貨ペアのみ処理
python3 getData/create_extreme_dataset.py --pairs USDJPY EURJPY GBPJPY

# カスタム閾値を指定（例: 1.5%）
python3 getData/create_extreme_dataset.py --threshold 1.5

# カスタムディレクトリを指定
python3 getData/create_extreme_dataset.py \
  --input forex_data/gnminer_individual \
  --output forex_data/extreme_gnminer
```

**オプション**:
- `--input`: 入力ディレクトリ（デフォルト: forex_data/gnminer_individual）
- `--output`: 出力ディレクトリ（デフォルト: forex_data/extreme_gnminer）
- `--threshold`: 極値判定の閾値（デフォルト: 1.0）
- `--pairs`: 処理する通貨ペア（省略時は全ペア）

**抽出条件**:
- **Positive極値**: X >= 1.0%（上昇トレンド、BUYシグナル）
- **Negative極値**: X <= -1.0%（下降トレンド、SELLシグナル）
- **除外データ**: -1.0% < X < 1.0%（通常の変動）

**出力**:
- `forex_data/extreme_gnminer/{PAIR}_positive.txt` - 正の極値
- `forex_data/extreme_gnminer/{PAIR}_negative.txt` - 負の極値
- `forex_data/extreme_gnminer/extreme_dataset_stats.csv` - 統計情報
- `forex_data/extreme_gnminer/README.md` - データセット説明

**出力例**:
```
USDJPY: 4,120レコード → 正 428個 + 負 411個 = 839個（20.4%保持）
EURJPY: 4,120レコード → 正 395個 + 負 387個 = 782個（19.0%保持）
```

---

### 4. create_extreme_dataset_0.5.py（閾値0.5%）

**目的**: より緩い基準での極値抽出（|X| >= 0.5%）

**基本的な使い方**:
```bash
# 全通貨ペアの極値を抽出（閾値0.5%）
python3 getData/create_extreme_dataset_0.5.py

# 特定の通貨ペアのみ処理
python3 getData/create_extreme_dataset_0.5.py --pairs USDJPY EURJPY GBPJPY

# カスタム出力ディレクトリを指定（デフォルトと区別）
python3 getData/create_extreme_dataset_0.5.py \
  --output forex_data/extreme_gnminer_0.5
```

**抽出条件**:
- **Positive極値**: X >= 0.5%
- **Negative極値**: X <= -0.5%
- **除外データ**: -0.5% < X < 0.5%

**閾値1.0%との違い**:
- より多くのデータが抽出される（約40-50%保持 vs 20%保持）
- より多くのルールが発見される
- ただし、ノイズも増える可能性

**推奨される使い分け**:
- **閾値1.0%**: より明確なトレンド、高品質ルール
- **閾値0.5%**: より多くのルール発見、緩いトレンド検出

---

## 🎯 典型的な使用シナリオ

### シナリオA: 初めてのセットアップ

```bash
# ステップ1: データダウンロード
python3 getData/download_forex_data.py

# ステップ2: GNMiner形式に変換
python3 getData/convert_to_gnminer_format.py

# ステップ3: 極値データ作成（閾値1.0%）
python3 getData/create_extreme_dataset.py

# ステップ4: ルール発見（GNMiner）
# 例: GBPAUDのpositive（BUY）ルール発見
./main GBPAUD positive
```

### シナリオB: 特定の通貨ペアのみ処理

```bash
# 3つの通貨ペアだけを処理
PAIRS="USDJPY EURJPY GBPJPY"

# ダウンロード
python3 getData/download_forex_data.py --pairs $PAIRS

# 変換
python3 getData/convert_to_gnminer_format.py --pairs $PAIRS

# 極値抽出
python3 getData/create_extreme_dataset.py --pairs $PAIRS

# ルール発見（各ペア・各方向）
for pair in $PAIRS; do
  ./main $pair positive
  ./main $pair negative
done
```

### シナリオC: 閾値0.5%でルール発見

```bash
# 極値データ作成（閾値0.5%、別ディレクトリ）
python3 getData/create_extreme_dataset_0.5.py \
  --output forex_data/extreme_gnminer_0.5

# ルール発見スクリプトで入力ディレクトリを変更する必要あり
# または、シンボリックリンクで対応
mv forex_data/extreme_gnminer forex_data/extreme_gnminer_1.0_backup
ln -s forex_data/extreme_gnminer_0.5 forex_data/extreme_gnminer

# ルール発見
./main GBPAUD positive
./main GBPAUD negative
```

### シナリオD: 最新データで更新

```bash
# 最新データをダウンロード（既存データは上書き）
python3 getData/download_forex_data.py

# 再変換
python3 getData/convert_to_gnminer_format.py

# 再度極値抽出
python3 getData/create_extreme_dataset.py

# ルール再発見
./main GBPAUD positive
./main GBPAUD negative
```

---

## 📊 対応通貨ペア

全20通貨ペアに対応:

### 対円ペア（7ペア）
- USDJPY, EURJPY, GBPJPY, AUDJPY, NZDJPY, CADJPY, CHFJPY

### 主要クロスペア（6ペア）
- EURUSD, GBPUSD, AUDUSD, NZDUSD, USDCAD, USDCHF

### その他の主要ペア（7ペア）
- EURGBP, EURAUD, EURCHF, GBPAUD, GBPCAD, AUDCAD, AUDNZD

---

## 📂 出力ディレクトリ構造

```
forex_data/
├── raw/                              # ダウンロードしたCSVデータ
│   ├── USDJPY.csv
│   ├── EURJPY.csv
│   └── ...
│
├── gnminer_individual/               # GNMiner形式データ
│   ├── USDJPY.txt
│   ├── EURJPY.txt
│   └── ...
│
└── extreme_gnminer/                  # 極値データ（閾値1.0%）
    ├── USDJPY_positive.txt           # 正の極値（BUY）
    ├── USDJPY_negative.txt           # 負の極値（SELL）
    ├── EURJPY_positive.txt
    ├── EURJPY_negative.txt
    ├── ...
    ├── extreme_dataset_stats.csv     # 統計情報
    └── README.md                     # データセット説明
```

---

## ⚙️ データ形式の詳細

### GNMiner形式ファイルの構造

**ヘッダー（例: GBPAUD.txt）**:
```
USDJPY_Up(t-1),USDJPY_Stay(t-1),USDJPY_Down(t-1),EURJPY_Up(t-1),EURJPY_Stay(t-1),...,X,T
```

**データ行**:
```
1,0,0,0,1,0,1,0,0,...,+1.523,2020-01-05
0,1,0,1,0,0,0,1,0,...,-0.891,2020-01-06
```

- 属性: 60列（20ペア × 3状態: Up/Stay/Down）
- X: ターゲット変数（GBPAUD自身の変化率%）
- T: タイムスタンプ

### 極値ファイルの構造

positive/negativeファイルも同じ形式ですが、Xの値が閾値でフィルタリングされています:

**positive.txt**: X >= 1.0%（または0.5%）のレコードのみ
**negative.txt**: X <= -1.0%（または-0.5%）のレコードのみ

---

## 🔧 トラブルシューティング

### エラー: "File not found"

**原因**: 前のステップが完了していない

**解決策**:
```bash
# データが存在するか確認
ls -la forex_data/raw/
ls -la forex_data/gnminer_individual/

# 存在しない場合は、前のステップから実行
python3 getData/download_forex_data.py
python3 getData/convert_to_gnminer_format.py
```

### エラー: "No module named 'yfinance'"

**原因**: 必要なPythonライブラリがインストールされていない

**解決策**:
```bash
pip install yfinance pandas numpy
# または
pip3 install yfinance pandas numpy
```

### エラー: "極値データが少なすぎる"

**原因**: 閾値が高すぎる

**解決策**:
```bash
# より緩い閾値を使用
python3 getData/create_extreme_dataset.py --threshold 0.5

# または閾値0.5%版を使用
python3 getData/create_extreme_dataset_0.5.py
```

### 問題: "ダウンロード速度が遅い"

**原因**: Yahoo Finance APIの制限

**解決策**:
```bash
# 特定のペアだけダウンロード
python3 getData/download_forex_data.py --pairs USDJPY EURJPY

# または、既存のデータを使用
```

---

## 📝 注意事項

### データダウンロードについて
- Yahoo Finance APIを使用（無料、登録不要）
- APIレート制限あり（大量ダウンロード時は時間がかかる）
- 市場休場日はデータなし（週末、祝日）

### 閾値の選択について
- **閾値1.0%**: 明確なトレンド、ルール品質高い、データ量少ない
- **閾値0.5%**: データ量多い、ルール発見多い、ノイズ増加

### 推奨事項
1. 最初は閾値1.0%で試す
2. ルールが少なすぎる場合は閾値0.5%を試す
3. 両方の結果を比較して最適な閾値を決定

---

## 🔗 関連ドキュメント

- **ルール発見**: `FOREX_SETUP_GUIDE.md` - GNMinerでのルール発見方法
- **シミュレーション**: `simulation/README.md` - 発見したルールでのトレードシミュレーション
- **分析**: `simulation/docs/` - 詳細な分析レポート

---

## 📞 サポート

問題が発生した場合:
1. このREADMEのトラブルシューティングを確認
2. 出力ログを確認（エラーメッセージが詳細に表示される）
3. データファイルが正しく生成されているか確認（`ls -la forex_data/`）

---

**更新日**: 2025-10-30
**バージョン**: 1.0.0
