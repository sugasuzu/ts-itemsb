# 局所分布分析 (Local Distribution Analysis)

## 📊 概要

このツールは、**GNPアルゴリズムが発見したルールによる分散減少効果**を可視化・定量化します。

### 核心的な主張

一見普通に見えるX,Tの散布図において、特定のルール（条件）を適用すると：

1. **全体分布**: X値の標準偏差が大きい（σ_global = 例: 5.0）
2. **局所分布**: ルール適用後のX値の標準偏差が劇的に小さい（σ_local = 例: 1.5）
3. **統計的有意性**: この差は偶然ではなく、統計的に有意

これにより、GNPが**意味のあるパターン**を発見していることを証明します。

---

## 🎯 何を示すか

### Before (全体分布)
```
X値の分布: 広く散らばっている
標準偏差: σ = 5.0
分散: σ² = 25.0
```

### After (ルール適用後の局所分布)
```
ルール: IF volume_high(t-2) ∧ rsi_low(t-1) ∧ macd_cross(t-0)
X値の分布: 狭く集中している
標準偏差: σ = 1.5
分散: σ² = 2.25
分散減少率: 91% ↓
```

**これはMaxsigx=10.0の制約により、GNPが局所的に低分散な領域を自動発見したことを示します。**

---

## 🚀 使用方法

### 1. 基本実行（単一通貨ペア）

```bash
cd /home/user/ts-itemsb/analysis/fx
bash run_local_distribution.sh USDJPY
```

### 2. 全通貨ペア分析

```bash
bash run_local_distribution.sh
```

### 3. Python直接実行

```bash
cd /home/user/ts-itemsb/analysis/fx
python3 local_distribution_analysis.py USDJPY
```

---

## 📁 出力ファイル

### 1. 個別ルール分析
```
analysis/fx/<PAIR>/local_distribution_rule_0000.png
analysis/fx/<PAIR>/local_distribution_rule_0001.png
...
```

各ルールごとに以下の8つのサブプロットを含む包括的な分析図：

1. **全体のX,T散布図**: グレーで全データを表示
2. **ルール適用後のX,T散布図**: 赤で条件を満たすデータを強調
3. **ズームビュー**: 局所領域を拡大
4. **ヒストグラム比較**: 全体分布 vs 局所分布
5. **箱ひげ図比較**: 分散の視覚的比較
6. **分散減少の棒グラフ**: 減少率を明示
7. **統計サマリーテーブル**: 数値データ
8. **ルール情報と統計検定**: 条件と有意性

### 2. サマリーCSV
```
analysis/fx/<PAIR>/local_distribution_summary.csv
```

カラム:
- `rule_idx`: ルール番号
- `global_mean`, `global_std`: 全体の平均・標準偏差
- `local_mean`, `local_std`: 局所の平均・標準偏差
- `local_count`: サンプル数
- `variance_reduction_%`: 分散減少率（%）
- `variance_ratio`: 分散比（Global / Local）
- `levene_p`: Leveneの等分散性検定のp値
- `significant`: 統計的に有意かどうか（True/False）

### 3. サマリー可視化
```
analysis/fx/<PAIR>/local_distribution_summary_viz.png
```

4つのサブプロット:
- 分散減少率ランキング
- 局所標準偏差 vs サンプル数の散布図
- 分散比の分布
- 統計的有意性の円グラフ

---

## 📈 解釈ガイド

### 分散減少率（Variance Reduction）

```python
variance_reduction = 1 - (σ_local / σ_global)
```

- **90%以上**: 非常に強い局所集中（優れたルール）
- **70-90%**: 強い局所集中
- **50-70%**: 中程度の局所集中
- **50%未満**: 弱い局所集中

### 分散比（Variance Ratio）

```python
variance_ratio = σ²_global / σ²_local
```

- **10以上**: 非常に強い効果
- **5-10**: 強い効果
- **2-5**: 中程度の効果
- **1-2**: 弱い効果
- **1未満**: 効果なし（局所分散の方が大きい）

### 統計的有意性

Leveneの等分散性検定を使用：

- **p < 0.05** かつ **分散比 > 1.5**: 統計的に有意な分散減少
- **p ≥ 0.05**: 有意でない（偶然の可能性）

---

## 🔬 技術的詳細

### アルゴリズムの流れ

1. **データ読み込み**
   - 元データ: `forex_data/gnminer_individual/<PAIR>.txt`
   - ルールプール: `output_<PAIR>/pool/zrp01a.txt`

2. **ルール条件抽出**
   - 各ルールの前件部（Attr1-8）を解析
   - 属性名と時間遅延を取得
   - 例: `"volume_high(t-2)"` → 属性=volume_high, 遅延=2

3. **データフィルタリング**
   - ルールの全条件を満たすデータポイントを抽出
   - 時間遅延を考慮してマッチング

4. **統計量計算**
   - 全体: μ_global, σ_global
   - 局所: μ_local, σ_local
   - 分散減少率: (σ²_global - σ²_local) / σ²_global

5. **統計検定**
   - Leveneの等分散性検定（分散の違い）
   - t検定（平均の違い）
   - F検定（分散比）

6. **可視化**
   - Matplotlib/Seabornによる8プロット生成

---

## 🎨 カスタマイズ

### 分析するルール数を変更

`local_distribution_analysis.py` の末尾を編集：

```python
# 上位10ルールを分析（デフォルト）
analyzer.analyze_top_rules(n_rules=10)

# 上位20ルールに変更
analyzer.analyze_top_rules(n_rules=20)
```

### 統計的有意性の閾値を変更

`statistical_significance()` メソッド内：

```python
'significant_variance_reduction': levene_p < 0.05 and f_stat > 1.5
```

を変更（例: `f_stat > 2.0` により厳しい基準に）

---

## 📊 結果の例

### ルール #42の分析結果

```
全体分布:
  件数: 1000
  平均: 123.45
  標準偏差: 5.67
  分散: 32.15

局所分布（ルール適用後）:
  件数: 85 (8.5%)
  平均: 125.67
  標準偏差: 1.23  ← 劇的に減少！
  分散: 1.51

効果:
  分散減少率: 95.3% ↓
  分散比: 21.3x
  Levene's p: 0.0001 (有意)
  統計的有意性: ✓ YES
```

**解釈**: このルールは、全体の8.5%のデータポイントを抽出し、そのX値の標準偏差を5.67から1.23へ（78%減少）させている。これは統計的に有意であり、ルールが予測に有用なパターンを捉えていることを示す。

---

## ⚠️ 注意事項

### データ要件

1. **データファイル**: `forex_data/gnminer_individual/<PAIR>.txt`
   - フォーマット: CSV（カンマ区切り）
   - 必須カラム: `T` (タイムスタンプ), `X` (目的変数)
   - 属性カラム: 任意の数の二値属性

2. **ルールプール**: `output_<PAIR>/pool/zrp01a.txt`
   - フォーマット: TSV（タブ区切り）
   - 必須カラム: `Attr1`-`Attr8`, `X_mean`, `X_sigma`, etc.

### 実行前の確認

```bash
# データファイルの存在確認
ls forex_data/gnminer_individual/USDJPY.txt

# ルールプールの存在確認
ls output_USDJPY/pool/zrp01a.txt

# 依存パッケージのインストール
pip install pandas numpy matplotlib seaborn scipy
```

---

## 🐛 トラブルシューティング

### エラー: "No data points match rule X"

**原因**: ルールの条件が厳しすぎて、該当するデータポイントが0件

**対処**:
- 他のルールを分析
- ルールの品質閾値（Minsup）を下げてmain.cを再実行

### エラー: "Rule pool not found"

**原因**: ルールプールファイルが存在しない

**対処**:
```bash
# main.cを実行してルールを生成
cd /home/user/ts-itemsb
./main USDJPY
```

### 警告: "insufficient_data" in statistical test

**原因**: サンプル数が30未満で統計検定が不適切

**対処**: これは正常。サンプル数が少ないルールはスキップされます。

---

## 📚 理論的背景

### なぜ局所分布が重要か

1. **予測精度**: 分散が小さい = 予測の信頼区間が狭い
2. **過学習の検証**: 統計的有意性により、偶然でないことを確認
3. **解釈可能性**: どの条件下で予測が安定するかを理解

### GNPとの関連

GNPは以下の適応度関数で個体を評価：

```c
fitness = support_bonus + variance_penalty + new_rule_bonus
```

`Maxsigx = 10.0` の制約により、σ > 10のルールは抽出されない。
これにより、進化過程で自動的に低分散（局所集中）なパターンを探索する。

---

## 🎓 参考文献

### 統計検定

- **Leveneの等分散性検定**: 2群の分散が等しいかを検定
  - H0: σ²_global = σ²_local
  - H1: σ²_global ≠ σ²_local

- **Welchのt検定**: 等分散を仮定しない平均の差の検定

- **F検定**: 分散比の検定

### GNP関連論文

- Hirasawa, K., et al. (2006). "Genetic Network Programming and Its Extensions."
- Shimada, K., et al. (2010). "Association Rule Mining for Continuous Attributes using GNP."

---

## 📞 サポート

問題が発生した場合:

1. README.mdのトラブルシューティングセクションを確認
2. ログファイルを確認
3. GitHubのIssuesで報告

---

**作成日**: 2024-10-26
**バージョン**: 1.0
**対応Phase**: GNMiner-TS Phase 2.1
