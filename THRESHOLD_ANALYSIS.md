# 閾値分析：なぜルールが発見されないのか？

## 問題

Very Tight Cluster設定でもTight Cluster設定でも**0個のルール**しか発見されない。

## 設定の比較

| 設定 | Maxsigx | Minmean | MIN_SUPPORT_COUNT | 結果 |
|------|---------|---------|-------------------|------|
| **元の設定** | 1.0% | 0.1% | 2回 | 2,000個 ✅ |
| **Very Tight** | 0.25% | 0.5% | 8回 | 0個 ❌ |
| **Tight** | 0.35% | 0.3% | 10回 | 0個 ❌ |

## 原因分析

### 仮説1: Minmeanが厳しすぎる

```
USDJPY統計:
  μ (平均) = 0.0141%
  σ (標準偏差) = 0.5822%

Minmean = 0.3% の意味:
  → μ + 0.5σ に相当
  → 上位30%程度のパターン

問題: 為替市場はランダムウォーク（相関≈0）
     → 平均が0.3%を超えるパターンは極めて希少
```

### 仮説2: MIN_SUPPORT_COUNTが厳しすぎる

```
MIN_SUPPORT_COUNT = 10回

4,132レコードで10回 = 0.24%のサポート率
しかも、Minmean=0.3%の条件も満たす必要がある

→ 非常に厳しい組み合わせ
```

### 仮説3: Maxsigxが厳しすぎる

```
Maxsigx = 0.35%

全データの σ = 0.5822%
→ 0.35% = 0.6σ

密集したクラスターを要求しているが、
為替市場のボラティリティでは達成困難
```

## 解決策：段階的な緩和

### Step 1: Minmeanを緩める（最優先）

```c
#define Minmean 0.2   // 0.3 → 0.2
```

**理由:**
- 0.3%は厳しすぎる（μ + 0.5σ）
- 0.2%でも十分な方向性（μ + 0.3σ）
- ランダムウォーク市場では妥当

### Step 2: MIN_SUPPORT_COUNTを緩める

```c
#define MIN_SUPPORT_COUNT 5   // 10 → 5
```

**理由:**
- 10回は統計的に理想だが、希少パターンでは厳しい
- 5回でも偶然性は排除できる（p < 0.05）

### Step 3: Maxsigxをやや緩める

```c
#define Maxsigx 0.45   // 0.35 → 0.45
```

**理由:**
- 0.35%は厳しすぎる（全データσの60%）
- 0.45%でもランダムよりは密集（全データσの77%）

## 推奨設定

### バランス設定（Moderate Cluster）

```c
/* ルールマイニング制約 - Moderate Cluster 設定 */
#define Minsup 0.0005          // 0.05%サポート率
#define Maxsigx 0.45           // 中程度の分散
#define MIN_ATTRIBUTES 2       // 最小属性数
#define Minmean 0.2            // 適度な方向性
#define MIN_CONCENTRATION 0.5  // 象限集中50%
#define MIN_SUPPORT_COUNT 5    // 最低5回観測
```

**期待される結果:**
- ルール数: 10-100個
- σ: 0.35-0.45%（中程度に密集）
- mean: 0.2-0.5%（適度な方向性）

## 実装テスト計画

### テスト1: Minmean = 0.2

```bash
# main.c を修正
#define Minmean 0.2

# コンパイル & 実行
gcc -o main main.c -lm -O2
./main USDJPY 1
```

### テスト2: MIN_SUPPORT_COUNT = 5

```bash
# main.c を修正
#define MIN_SUPPORT_COUNT 5

# コンパイル & 実行
gcc -o main main.c -lm -O2
./main USDJPY 1
```

### テスト3: 両方緩める

```bash
# main.c を修正
#define Minmean 0.2
#define MIN_SUPPORT_COUNT 5

# コンパイル & 実行
gcc -o main main.c -lm -O2
./main USDJPY 1
```

## 次のアクション

1. **Minmean を 0.2に変更**
2. **MIN_SUPPORT_COUNT を 5に変更**
3. **テスト実行**
4. **結果を確認**

これで10-100個の高品質ルールが発見できるはずです。
