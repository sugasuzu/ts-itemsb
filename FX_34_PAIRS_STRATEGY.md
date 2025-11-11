# 為替34ペア戦略：散布図クラスター発見による実用的トレード

## 研究目的の再定義

### 変更前（統計的異常）
```
目的: |mean| >> 0.5% の極端な平均値を持つパターン
手法: 閾値フィルタリング（Minmean引き上げ）
問題: 実用性が不明確
```

### 変更後（散布図クラスター）✅
```
目的: X(t+1) vs X(t+2) 散布図上で明確に分離された小集団
手法: 象限集中度 + 低分散 + 実取引可能性
利点: SBI FXTRADEで実際にトレード可能
```

---

## なぜ為替34ペアが最適か？

### 属性数の比較（GNMinerの本質）

| データ | 銘柄数 | 属性数 | 計算コスト | パターン多様性 | 実用性 |
|--------|-------|--------|----------|-------------|--------|
| BTC | 20 | 60次元 | 低 | 中 | 中 |
| **為替34ペア** | **34** | **102次元** | **中** | **高** | **⭐⭐⭐⭐⭐** |
| 株式220 | 220 | 660次元 | 高 | 極高 | 低 |

**決定的な理由:**

1. **属性数が豊富 (102次元)**
   - BTCの1.7倍 → より複雑なパターン発見可能
   - クロス通貨の組み合わせ爆発
   - 例: `USDJPY_Up(t-1) AND EURUSD_Down(t-2) AND GBPJPY_Up(t-0)`

2. **計算コストが現実的**
   - 株式220銘柄の1/6の計算量
   - GNP進化が実用的な時間で完了

3. **実用性が最高**
   - SBI FXTRADEで全34ペアが取引可能
   - 24時間取引 → いつでもパターン活用可能
   - 流動性が高い → スリッページが少ない

4. **クロス通貨連動性**
   - USDJPY, EURJPY, EURUSD は三角関係
   - `EURUSD × USDJPY ≈ EURJPY`
   - → 複雑だが明確なパターンが存在

---

## 現在のデータと追加が必要なペア

### 現在のデータ（20ペア）✅

| カテゴリ | ペア数 | 通貨ペア |
|---------|-------|---------|
| **JPYクロス** | 7 | USDJPY, EURJPY, GBPJPY, AUDJPY, NZDJPY, CADJPY, CHFJPY |
| **USDクロス** | 6 | EURUSD, GBPUSD, AUDUSD, NZDUSD, USDCAD, USDCHF |
| **その他クロス** | 7 | EURGBP, EURAUD, EURCHF, GBPAUD, GBPCAD, AUDCAD, AUDNZD |

### 追加が必要なペア（14ペア）⚠️

| カテゴリ | ペア数 | 通貨ペア |
|---------|-------|---------|
| **JPYクロス** | 11 | ZAR/JPY, TRY/JPY, CNH/JPY, KRW/JPY, HKD/JPY, RUB/JPY, BRL/JPY, PLN/JPY, SEK/JPY, NOK/JPY, MXN/JPY, SGD/JPY |
| **USDクロス** | 1 | USD/CNH |
| **その他** | 2 | GBP/CHF, AUD/CHF |

**重要な判断:**
- 現在20ペア → 60次元
- 目標34ペア → 102次元
- **まずは現在の20ペアでテスト実行すべき** ✅

---

## 散布図クラスター発見のための閾値設定

### 目的別の閾値

#### 目的1: 実用的トレード戦略
```c
// トレードに使える小集団
#define Minsup 0.002              // 0.2% (年間8回程度の希少パターン)
#define Maxsigx 0.40              // 0.40% (予測可能性重視)
#define Minmean 0.3               // 0.3% (取引コスト考慮後でも利益)
#define MIN_CONCENTRATION 0.5     // 50% (明確な方向性)
#define MIN_SUPPORT_COUNT 8       // 最低8回観測（統計的信頼性）
```

**期待される結果:**
- ルール数: 30-100個
- 散布図: 明確なクラスター（++象限に50%以上集中）
- 実用性: 取引コスト考慮後でも年間5-10%リターン可能

#### 目的2: 研究用（より厳格）
```c
// 論文化可能な強いパターン
#define Minsup 0.001              // 0.1% (非常に希少OK)
#define Maxsigx 0.35              // 0.35% (低リスク)
#define Minmean 0.5               // 0.5% (統計的有意性)
#define MIN_CONCENTRATION 0.6     // 60% (非常に明確)
#define MIN_SUPPORT_COUNT 5       // 希少パターンなので5回でOK
```

**期待される結果:**
- ルール数: 10-50個
- 散布図: 非常に明確なクラスター
- 研究価値: 論文化可能

---

## 為替34ペアで発見されるパターン例

### パターン1: 円安トレンド（JPYクロス連動）

**ルール例:**
```
IF USDJPY_Up(t-1) AND EURJPY_Up(t-1) AND GBPJPY_Up(t-2)
THEN:
  X(t+1) = +0.45%, σ = 0.32%
  X(t+2) = +0.38%, σ = 0.35%
  Support: 0.8% (年間32回)

散布図:
     X(t+2)
      |
    ++|+++++  ← 88% が (++) 象限に集中
      |
  ----+---- X(t+1)
      |
```

**解釈:**
- 日本円が全面安（円キャリートレード）
- USD, EUR, GBP すべてが円に対して上昇
- → 円安トレンド継続の高い確率

**トレード戦略:**
- エントリー: パターンマッチ時に USDJPY ロング
- 利確: +0.4% または t+2 時点
- 損切り: -0.3%

---

### パターン2: ドル全面高（USDクロス連動）

**ルール例:**
```
IF EURUSD_Down(t-1) AND GBPUSD_Down(t-1) AND AUDUSD_Down(t-2)
THEN:
  USDJPY: X(t+1) = +0.52%, σ = 0.28%
  Support: 0.5% (年間20回)

散布図:
     X(t+2)
      |
    ++|++++   ← 75% が (++) 象限
      |
  ----+----
      |
```

**解釈:**
- USD が主要通貨（EUR, GBP, AUD）に対して全面高
- リスクオフ or FED利上げ期待
- → USDJPY も上昇（ドル買い圧力）

---

### パターン3: クロス通貨三角裁定

**ルール例:**
```
IF EURUSD_Up(t-1) AND USDJPY_Up(t-1) AND EURJPY_Stay(t-0)
THEN:
  EURJPY: X(t+1) = +0.65%, σ = 0.25%
  Support: 0.3% (年間12回、非常に希少)

散布図:
     X(t+2)
      |
    ++|+++    ← 90% が (++) 象限（非常に明確）
      |
  ----+----
      |
```

**解釈:**
- EURUSD ↑ AND USDJPY ↑ なのに EURJPY が動いていない
- → 理論的には EURJPY = EURUSD × USDJPY ↑↑ のはず
- → 三角裁定機会（EURJPY が追いつく）

---

### パターン4: 平均回帰（過度な下落後の反発）

**ルール例:**
```
IF USDJPY_Down(t-1) < -1.2% AND Volume_High(t-0) AND EURUSD_Up(t-0)
THEN:
  X(t+1) = +0.58%, σ = 0.38%
  X(t+2) = +0.22%, σ = 0.42%
  Support: 0.4% (年間16回)

散布図:
     X(t+2)
      |
    +-|--     ← 70% が (+?) 領域（反発パターン）
      |
  ----+----
      |
```

**解釈:**
- 前日に大きく下落（-1.2% = -2σ）
- 過度な売られすぎ → 翌日反発
- t+2 では勢いが弱まる（平均回帰完了）

---

## データ準備の優先順位

### Phase 1: 現在の20ペアでテスト（今週）✅

**理由:**
- 既にデータがある（4,121日 × 20ペア）
- 60次元でも十分なパターン発見可能
- システム動作確認

**実行コマンド:**
```bash
# 1. 為替データを GNMiner 形式に変換
python3 convert_fx_to_gnminer.py

# 2. USDJPY でテスト実行
./main USDJPY 10

# 3. 散布図確認
python3 analysis/fx/plot_single_rule_2d_future.py --rule-id XXXX --symbol USDJPY
```

---

### Phase 2: 34ペアへの拡張（来週）

**追加が必要なペア:**
1. **エマージング通貨** (ZAR, TRY, BRL, MXN)
   - 高ボラティリティ → 明確なパターン
   - 注意: 流動性がやや低い

2. **アジア通貨** (CNH, KRW, HKD, SGD)
   - 時間帯効果が明確
   - 中国経済との連動性

3. **北欧通貨** (SEK, NOK)
   - 資源価格との連動性
   - EUR との相関

**データ取得方法:**
- Yahoo Finance API
- OANDA API
- または SBI FXTRADEの過去データ

---

## 散布図評価指標

### 指標1: 象限集中度（Quadrant Concentration）

```python
def calculate_concentration(x_t1, x_t2):
    """
    散布図上での象限集中度
    """
    quadrants = {
        '++': ((x_t1 > 0) & (x_t2 > 0)).sum(),
        '+-': ((x_t1 > 0) & (x_t2 < 0)).sum(),
        '-+': ((x_t1 < 0) & (x_t2 > 0)).sum(),
        '--': ((x_t1 < 0) & (x_t2 < 0)).sum(),
    }

    total = sum(quadrants.values())
    max_quadrant = max(quadrants.values())
    concentration = max_quadrant / total

    return concentration, quadrants

# 基準:
# concentration >= 0.5  → 明確な小集団
# concentration >= 0.6  → 非常に明確
# concentration >= 0.7  → 極めて明確（トレード可能）
```

### 指標2: クラスター分離度（Cluster Separation）

```python
def calculate_separation(rule_mean, rule_sigma, data_mean, data_sigma):
    """
    全データとの分離度
    """
    # マハラノビス距離
    distance = abs(rule_mean - data_mean) / rule_sigma

    # 基準:
    # distance >= 1.0  → 明確に分離
    # distance >= 2.0  → 非常に明確
    # distance >= 3.0  → 極めて明確

    return distance
```

### 指標3: シャープレシオ（Sharpe Ratio）

```python
def calculate_sharpe_ratio(mean, sigma, risk_free_rate=0.0):
    """
    リスク調整後リターン
    """
    sharpe = (mean - risk_free_rate) / sigma

    # 基準（年率換算前提）:
    # sharpe >= 0.5  → 許容可能
    # sharpe >= 1.0  → 良い
    # sharpe >= 2.0  → 優秀（実用的）

    return sharpe
```

---

## 実装計画

### Step 1: データ変換スクリプト作成

```python
# convert_fx_to_gnminer.py
import pandas as pd
import numpy as np

def convert_fx_data():
    # forex_raw.csv を読み込み
    df = pd.read_csv('forex_data/forex_raw.csv')

    # 各ペアごとに Up/Stay/Down を計算
    # BTC.txt と同じフォーマットで出力
    # ...
```

### Step 2: 閾値調整

```c
// main.c の閾値を実用的トレード向けに調整
#define Minsup 0.002              // 0.2%
#define Maxsigx 0.40              // 0.40%
#define Minmean 0.3               // 0.3%
#define MIN_CONCENTRATION 0.5     // 50%
```

### Step 3: テスト実行

```bash
# USDJPY で 10試行
./main USDJPY 10

# 期待:
# - ルール数: 30-100個
# - 散布図で明確なクラスター
# - 実用的なトレード戦略
```

### Step 4: 散布図可視化

```bash
# 各ルールの散布図を生成
for rule_id in {1..100}; do
    python3 analysis/fx/plot_single_rule_2d_future.py \
        --rule-id $rule_id \
        --symbol USDJPY
done
```

### Step 5: バックテスト

```python
# backtest_fx_rules.py
def backtest_rule(rule_id, transaction_cost=0.001):
    """
    取引コスト考慮後のリターン評価
    """
    # verification/rule_XXXX.csv を読み込み
    # エントリー: matched == 1
    # リターン: X(t+1) - transaction_cost
    # ...
```

---

## 期待される成果

### 研究成果

1. **散布図クラスター発見**
   - X(t+1) vs X(t+2) 平面上での明確な小集団
   - 象限集中度 >= 50%
   - 全データとの明確な分離

2. **為替市場の構造発見**
   - JPYクロス連動パターン
   - USDクロス連動パターン
   - 三角裁定機会
   - 平均回帰パターン

3. **論文化可能性**
   - タイトル: "Discovery of Scatter Plot Clusters in FX Markets via GNP"
   - 貢献: 102次元空間での希少パターン発見
   - 実用性: 実際に取引可能な戦略

### 実用成果

1. **トレード戦略**
   - 年間20-50回のエントリー機会
   - 平均リターン: +0.3-0.5%/回
   - 勝率: 60-70%
   - シャープレシオ: 1.0以上

2. **リスク管理**
   - 低分散 (σ <= 0.4%)
   - 明確な損切りライン
   - ポジションサイズ最適化

---

## 最終推奨

### 🎯 今すぐやるべきこと

1. **現在の20ペアデータでテスト**（最優先）
   ```bash
   # データ変換
   python3 convert_fx_to_gnminer.py

   # テスト実行
   ./main USDJPY 10
   ```

2. **閾値を実用的トレード向けに調整**
   ```c
   #define Minmean 0.3   // 0.1 → 0.3 (取引コスト考慮)
   #define Maxsigx 0.40  // 0.5 → 0.40 (予測可能性重視)
   ```

3. **散布図可視化で小集団を確認**
   ```bash
   python3 plot_scatter_clusters.py
   ```

### 📊 20ペア vs 34ペアの判断

**結論: まずは20ペアで十分** ✅

| 項目 | 20ペア | 34ペア |
|------|--------|--------|
| 属性数 | 60次元 | 102次元 |
| パターン数 | 2^60 ≈ 10^18 | 2^102 ≈ 10^30 |
| 計算時間 | 60秒/試行 | 150秒/試行 (推定) |
| データ入手 | ✅ 完了 | ⚠️ 追加取得必要 |

**20ペアでも十分な理由:**
- 属性数60次元 = 1,152,921,504,606,846,976通りのパターン
- クロス通貨連動性は既に含まれている（USDJPY, EURJPY, EURUSD）
- 実用的な主要ペアはすべて含まれている

---

## まとめ

### 研究方向の変更

**統計的異常 → 散布図クラスター** ✅

| 観点 | 統計的異常 | 散布図クラスター |
|------|----------|---------------|
| 目的 | 極端な平均値 | 明確な小集団 |
| 評価指標 | |mean| >> 0.5% | 象限集中度 >= 50% |
| 実用性 | 不明確 | 取引可能 |
| 可視化 | 困難 | 容易（散布図） |
| 論文価値 | 中 | 高 |

### データ選択

**BTC → 為替20ペア** ✅

| 観点 | BTC | 為替20ペア |
|------|-----|-----------|
| 属性数 | 60次元 | 60次元 |
| 実用性 | 中 | ⭐⭐⭐⭐⭐ |
| 取引可能性 | 仮想通貨取引所 | SBI FXTRADE |
| 市場時間 | 24時間 | 24時間 |
| 流動性 | 高 | 極高 |

**次のステップ:** 為替データ変換スクリプトを作成しますか？
