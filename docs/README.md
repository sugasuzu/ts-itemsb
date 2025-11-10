# ドキュメント・スクリプト集

このディレクトリには、象限集中度ルール発見システムの実験を行うためのドキュメントとスクリプトが含まれています。

---

## 📚 ドキュメント

### [EXPERIMENT_GUIDE.md](EXPERIMENT_GUIDE.md)
**実験の完全ガイド**

以下の内容を網羅しています：
- プロジェクトの目的と概念
- パラメータ一覧と説明
- 閾値の変更方法
- 適応度関数の修正方法
- コンパイルと実行
- 結果の分析方法
- 上位ルールの抽出
- 可視化方法
- 実験ワークフロー例
- トラブルシューティング

**最初に必ずこのドキュメントを読んでください。**

---

## 🔧 スクリプト

### 1. analyze_concentration.py
**集中度の基本統計を計算**

```bash
python3 docs/analyze_concentration.py
```

**出力:**
- 平均集中度、最大集中度
- 集中度分布（60%+, 55-59%, 50-54%, 45-49%, <45%）
- トップ20ルールの詳細（集中度、サポート、象限分布）

**使用タイミング:** 実験完了後、最初に実行して全体像を把握

---

### 2. extract_top_rules.py
**上位ルールの詳細抽出**

```bash
# 基本（上位50件）
python3 docs/extract_top_rules.py

# 上位100件を抽出
python3 docs/extract_top_rules.py --top 100

# 集中度55%以上のみ
python3 docs/extract_top_rules.py --min-conc 55
```

**出力:**
- コンソールに集中度順ランキング表示
- `top_rules.csv`: 詳細データをCSV保存
- 統計サマリー（平均集中度、平均サポート、象限分布など）

**使用タイミング:** 具体的なルールを詳しく分析したいとき

---

### 3. visualize_results.py
**実験結果の可視化**

```bash
python3 docs/visualize_results.py
```

**出力（3つのPNG画像）:**
1. `concentration_distribution.png`: 集中度のヒストグラム
2. `concentration_vs_mean.png`: 集中度 vs Mean の散布図
3. `quadrant_distribution.png`: 象限分布の円グラフ

**使用タイミング:** 結果を視覚的に理解したいとき、論文・プレゼン用の図が欲しいとき

---

### 4. show_parameters.sh
**現在のパラメータ設定を表示**

```bash
bash docs/show_parameters.sh
```

**出力:**
- main.c の全パラメータ（行番号付き）
- 基本フィルタパラメータ
- 適応度関数パラメータ
- 集中度ボーナス設定

**使用タイミング:** パラメータを変更する前、現在の設定を確認したいとき

---

## 📊 典型的な実験フロー

```bash
# 1. 現在のパラメータを確認
bash docs/show_parameters.sh

# 2. パラメータを変更（例: MIN_CONCENTRATION）
vim main.c +88

# 3. コンパイル
make clean && make

# 4. 実行
./main BTC 10 > run_experiment.log 2>&1

# 5. 基本統計を確認
python3 docs/analyze_concentration.py

# 6. 上位ルールを抽出
python3 docs/extract_top_rules.py --top 50

# 7. 可視化
python3 docs/visualize_results.py

# 8. 個別ルールの詳細確認（Rule ID 1231の例）
python3 analysis/crypt/plot_single_rule_2d_future.py --rule-id 1231 --symbol BTC
```

---

## 🎯 実験のヒント

### 初めての実験

1. まず `MIN_CONCENTRATION=0.35`, `MIN_SUPPORT_COUNT=30` でベースラインを確立
2. `analyze_concentration.py` で結果を確認
3. 50%以上のルールが少なければボーナスポイントを増やす
4. 50%以上のルールが50-100件程度になる設定を探す

### パラメータ調整の指針

| 状況 | 対応 |
|------|------|
| ルール数が少なすぎる | MIN_CONCENTRATIONを下げる（0.40 → 0.35） |
| 平均集中度が低い | ボーナスポイントを増やす（指数的増加率を上げる） |
| 50%以上のルールが少ない | CONCENTRATION_BONUS_3以降を大幅増 |
| 進化が遅い | MIN_CONCENTRATIONを下げて初期多様性を確保 |
| 統計的信頼性が心配 | MIN_SUPPORT_COUNTを増やす（30 → 40） |

### 比較実験

異なるパラメータで実験し、結果を比較：

```bash
# 実験A: 緩い基準
vim main.c  # MIN_CONCENTRATION=0.30
./main BTC 10 > run_30.log 2>&1
python3 docs/analyze_concentration.py > results_30.txt

# 実験B: 厳しい基準
vim main.c  # MIN_CONCENTRATION=0.40
./main BTC 10 > run_40.log 2>&1
python3 docs/analyze_concentration.py > results_40.txt

# 比較
diff results_30.txt results_40.txt
```

---

## 📁 出力ファイルの確認

実験後、以下のファイルが生成されます：

```
1-deta-enginnering/crypto_data_hourly/output/BTC/
├── pool/
│   └── zrp01a.txt              # ルール一覧（統計付き）
└── verification/
    ├── rule_001.csv            # Rule 1のマッチデータ
    ├── rule_002.csv
    └── ...
```

**確認コマンド:**

```bash
# ルール数
wc -l 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt

# 上位10ルールを見やすく表示
head -11 1-deta-enginnering/crypto_data_hourly/output/BTC/pool/zrp01a.txt | column -t

# 特定ルール（Rule 1）の詳細
head -5 1-deta-enginnering/crypto_data_hourly/output/BTC/verification/rule_001.csv
```

---

## 🐛 トラブルシューティング

### スクリプトが動かない

```bash
# 実行権限を確認
ls -l docs/*.py docs/*.sh

# 実行権限がない場合
chmod +x docs/*.py docs/*.sh

# Pythonモジュールが不足
pip3 install pandas numpy matplotlib
```

### verification/ が空

実験が正常に完了していない可能性があります：

```bash
# 実行ログを確認
tail -50 run_experiment.log

# "Processing Complete" が表示されているか確認
grep "Processing Complete" run_experiment.log
```

### 集中度が計算されない

verification CSVファイルが正しく生成されているか確認：

```bash
# CSVファイル数
ls 1-deta-enginnering/crypto_data_hourly/output/BTC/verification/ | wc -l

# 最初のCSVを確認
head -5 1-deta-enginnering/crypto_data_hourly/output/BTC/verification/rule_001.csv
```

---

## 📖 さらに詳しく

すべての詳細は **[EXPERIMENT_GUIDE.md](EXPERIMENT_GUIDE.md)** を参照してください。

質問や問題がある場合は、まずEXPERIMENT_GUIDE.mdのトラブルシューティングセクションを確認してください。
