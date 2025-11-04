# 2D Future Scatter Plot Generator

発見されたルールの上位3つについて、未来予測値の2次元分布を可視化します。

## 📊 概要

このスクリプトは、**X(t+1) vs X(t+2)** の2次元散布図を作成します：

- **X軸**: X(t+1) - ルール適用翌日の変化率
- **Y軸**: X(t+2) - ルール適用2日後の変化率
- **楕円**: 2σの分散範囲（95%信頼区間相当）
- **中心点**: 平均値（μ_t+1, μ_t+2）

各ルールごとに個別のPNG画像を生成します（1通貨につき3枚）。

---

## 🎯 可視化の目的

### **1. 予測の安定性評価**

```
安定したルール:           不安定なルール:
     ╭──╮                     ╭────────╮
     │●●│                     │ ●   ● │
     ╰──╯                     ╰────────╯
   小さい楕円                 大きい楕円
   → 予測が安定              → 予測がばらつく
```

### **2. 時間的相関の把握**

```
正の相関:                 負の相関:
  Y(t+2)                   Y(t+2)
    ↑                        ↑
    │    ●                   │ ●
    │   ●                    │  ●
    │  ●                     │   ●
    └────→ X(t+1)            └────→ X(t+1)
    右上がり                  右下がり

翌日上がれば2日後も上昇    翌日上がれば2日後は下落
```

### **3. ルール間の比較**

```
ルール1: 右上に集中 → 上昇トレンド予測
ルール2: 左下に集中 → 下落トレンド予測
ルール3: 原点付近   → 横ばい予測
```

---

## 🚀 使用方法

### **基本的な実行**

```bash
cd analysis/crypt
python plot_2d_future_scatter.py --symbol AAVE
```

### **実行結果**

```
======================================================================
  2D Future Scatter Plot Generator [X(t+1) vs X(t+2)]
======================================================================
Symbol: AAVE
Top N Rules: 3
Output Directory: output
======================================================================

[1/5] Loading rule pool...
  ✓ Loaded 8443 rules
  ✓ Selected top 3 rules

  Top Rules:
    Rule   1: Score= 10.1671 | Support=  2 | σ_t+1=0.01 | σ_t+2=0.04
    Rule   2: Score=  1.5562 | Support=  2 | σ_t+1=0.01 | σ_t+2=0.26
    Rule   3: Score=  1.5562 | Support=  2 | σ_t+1=0.01 | σ_t+2=0.26

[2/5] Loading all matches from all rules...
  ✓ Loaded 23 total match points

  All Matches Statistics:
    X(t+1): μ=-3.21, σ=2.30, range=[-7.88, 2.50]
    X(t+2): μ=-1.22, σ=2.79, range=[-4.45, 4.20]

[3/5] Generating 2D scatter plots...
  ✓ Saved: output/AAVE_rule_001_2d_future.png
  ✓ Saved: output/AAVE_rule_002_2d_future.png
  ✓ Saved: output/AAVE_rule_003_2d_future.png

[4/5] Complete!
  ✓ Generated 3 2D scatter plots
  ✓ Output directory: output
```

### **オプション付き実行**

```bash
python plot_2d_future_scatter.py \
    --symbol BTC \
    --top-n 5 \
    --output-dir my_output
```

---

## 📋 コマンドラインオプション

| オプション | 短縮形 | デフォルト | 説明 |
|-----------|--------|-----------|------|
| `--symbol` | `-s` | **必須** | 銘柄コード（例: AAVE, BTC, ETH） |
| `--top-n` | `-n` | `3` | 可視化するルール数 |
| `--output-dir` | `-o` | `output` | 出力ディレクトリ |

---

## 📊 散布図の構成要素

### **1. ベースレイヤー（薄い灰色）**

```python
ax.scatter(all_x_t1, all_x_t2,
           c='lightgray', alpha=0.2, s=15,
           label='All Rules Matches')
```

- **全ルールの全マッチポイント**を表示
- 背景として全体の分布を把握
- 該当ルールとの比較基準

### **2. ルールマッチポイント（赤色）**

```python
ax.scatter(rule_x_t1, rule_x_t2,
           c='red', alpha=0.8, s=120, marker='o',
           edgecolors='darkred', linewidths=2)
```

- **該当ルールのマッチポイント**のみ強調表示
- 大きめのマーカー（size=120）
- 濃い赤色（alpha=0.8）

### **3. 平均値（X印）**

```python
ax.scatter([mean_t1], [mean_t2],
           c='darkred', s=400, marker='X',
           edgecolors='white', linewidths=3)
```

- **ルールの中心的な予測値**
- 最も大きいマーカー（size=400）
- 白い枠線で視認性向上

### **4. 2σ楕円（破線）**

```python
ellipse = Ellipse(
    xy=(mean_t1, mean_t2),
    width=4 * sigma_t1,    # 2σ × 2（両側）
    height=4 * sigma_t2,
    alpha=0.25,
    facecolor='red',
    edgecolor='darkred',
    linewidth=2.5,
    linestyle='--'
)
```

- **95%信頼区間相当**の範囲
- 薄い赤色の塗りつぶし（alpha=0.25）
- 破線の枠線

### **5. ゼロライン（基準線）**

```python
ax.axhline(0, color='black', linestyle='--', alpha=0.4)
ax.axvline(0, color='black', linestyle='--', alpha=0.4)
```

- X軸・Y軸のゼロ位置を示す
- プラス/マイナス方向の判断基準

---

## 🔍 読み取れる情報

### **1. 予測方向の一貫性**

| 位置 | 意味 | 投資戦略 |
|------|------|---------|
| **第1象限（右上）** | t+1も t+2も プラス | ロング継続 |
| **第2象限（左上）** | t+1はマイナス、t+2はプラス | 反転狙い |
| **第3象限（左下）** | t+1も t+2も マイナス | ショート継続 |
| **第4象限（右下）** | t+1はプラス、t+2はマイナス | 早期利確 |

### **2. 楕円の形状分析**

#### **円形に近い**
```
     ╭───╮
     │ ● │
     ╰───╯

σ_t+1 ≈ σ_t+2
→ 両日の予測精度が同程度
```

#### **横長の楕円**
```
    ╭─────╮
    │  ●  │
    ╰─────╯

σ_t+1 > σ_t+2
→ 翌日の予測が不安定
```

#### **縦長の楕円**
```
     ╭─╮
     │●│
     │ │
     ╰─╯

σ_t+1 < σ_t+2
→ 2日後の予測が不安定
```

### **3. 散布点の密集度**

- **密集している** → サンプル数は少ないが予測が安定
- **分散している** → サンプル数は多いが予測が不安定

---

## 📁 出力ファイル

```
analysis/crypt/output/
├── AAVE_rule_001_2d_future.png    # Rule 1の2D散布図
├── AAVE_rule_002_2d_future.png    # Rule 2の2D散布図
└── AAVE_rule_003_2d_future.png    # Rule 3の2D散布図
```

### **ファイル仕様**

- **フォーマット**: PNG
- **解像度**: 300 DPI（高解像度）
- **サイズ**: 12×10 インチ（3600×3000 ピクセル）
- **ファイルサイズ**: 約250-300KB

---

## 📈 応用例

### **全銘柄を一括処理**

```bash
for symbol in AAVE BTC ETH BNB ADA ALGO ATOM AVAX DOGE DOT ETC; do
    echo "Processing $symbol..."
    python plot_2d_future_scatter.py --symbol $symbol
done
```

### **上位5ルールを可視化**

```bash
python plot_2d_future_scatter.py --symbol BTC --top-n 5
```

---

## 🔧 技術仕様

### **依存パッケージ**

```python
pandas>=1.5.0
numpy>=1.23.0
matplotlib>=3.6.0
```

### **アスペクト比**

```python
ax.set_aspect('equal', adjustable='box')
```

- **1:1の比率**で表示（正方形）
- X軸とY軸の単位が同じ（%）なので比較しやすい

### **外れ値対策**

```python
x_margin = 4 * all_x_t1.std()
y_margin = 4 * all_x_t2.std()
ax.set_xlim(all_x_t1.mean() - x_margin, all_x_t1.mean() + x_margin)
ax.set_ylim(all_x_t2.mean() - y_margin, all_x_t2.mean() + y_margin)
```

- 平均±4σの範囲に制限
- 極端な外れ値の影響を軽減

---

## 🆚 従来の可視化との違い

| 項目 | X-T散布図 | 2D未来予測散布図 |
|------|----------|----------------|
| **X軸** | 時間（タイムスタンプ） | X(t+1) - 翌日の変化率 |
| **Y軸** | X値（変化率） | X(t+2) - 2日後の変化率 |
| **目的** | ルール発生のタイミング | 予測値の分布と相関 |
| **分散表現** | なし | 2σ楕円 |
| **分析視点** | いつルールが発動したか | どのような予測をするか |

---

## 💡 次のステップ

### **3次元版（実装予定）**

```python
# X(t+1) vs X(t+2) vs X(t+3)
plot_3d_future_scatter.py --symbol AAVE
```

- 3軸での分布可視化
- 3次元楕円体表現
- インタラクティブHTML出力

---

## ⚠️ 注意事項

### **1. サンプル数が少ない場合**

```
Rule 1: n=2
→ 楕円が正確でない可能性
```

- 最低でも5サンプル以上推奨
- `MIN_SUPPORT_COUNT=5` の設定で対応済み

### **2. 完全に同じ点の場合**

```
σ_t+1 = 0 かつ σ_t+2 = 0
→ 楕円が描画されない
```

- 点だけが表示される
- 分散が0の場合は楕円をスキップ

### **3. 軸の範囲**

- 全ルールの平均±4σに制限
- 該当ルールが範囲外の場合、一部が見切れる可能性

---

## 📧 サポート

問題や改善提案がある場合は、Issue を作成してください。
