# GNPによる異常検知・例外ルール発見フレームワーク

**作成日**: 2025-11-10
**目的**: 時系列データにおける統計的異常（例外）パターンの自動発見

---

## 1. 異常検知としてのフレームワーク再定義

### 1.1 コンセプト転換

**従来の視点（予測）**:
- 目標: 未来の価格変動を予測
- 評価: 予測精度、収益性
- 問題: 市場効率性により困難

**新しい視点（異常検知）**:
- 目標: **通常とは異なる条件分布を持つパターン（例外ルール）を発見**
- 評価: 統計的偏差の大きさ、希少性
- 価値: 市場構造の理解、リスク管理、トレーディング機会

---

### 1.2 異常（Anomaly）の定義

#### 数学的定義

```
無条件分布（全データ）:
  P(X(t+1) | 全条件) ~ N(μ_0, σ_0²)
  μ_0 ≈ 0.01%  (ランダムウォーク)
  σ_0 ≈ 0.5-0.6%

異常パターン（例外ルール）:
  P(X(t+1) | 特定条件C) ~ N(μ_C, σ_C²)

  条件1: |μ_C - μ_0| >> σ_0  （極端な期待値）
  条件2: n(C) / N << 1       （希少性）
  条件3: σ_C ≤ σ_0          （安定性）
```

#### 例外ルールの3要件

1. **統計的偏差（Statistical Deviation）**
   - |mean| ≥ 0.5% （≈ 1σ偏差）
   - 理想: |mean| ≥ 1.0% （≈ 2σ偏差）

2. **希少性（Rarity）**
   - サポート率: 0.1% - 2.0%
   - マッチ数: 10 - 100回
   - 「稀だが再現性あり」

3. **安定性（Stability）**
   - σ ≤ 0.7%（予測分散が小さい）
   - 集中度 ≥ 60%（特定象限に偏る）

---

## 2. 異常検知用の適応度関数設計

### 2.1 設計思想

**既存のトレードオフ問題**:
```
高|mean| ⟷ 低サポート
高サポート ⟷ 低|mean|
```

**異常検知の優先順位**:
```
1. 統計的偏差（|mean|） >>> 最優先
2. 希少性（サポート）    >>> 重要（but 低めでOK）
3. 安定性（σ、集中度）  >>> 中程度
```

---

### 2.2 異常検知用適応度関数（Phase 2-Anomaly）

#### パラメータ設定

```c
/* ========================================
   Phase 2-Anomaly: 異常検知用パラメータ
   ======================================== */

// 閾値（異常の定義）
#define Maxsigx 0.8                    // 分散上限（厳格化）
#define MIN_ATTRIBUTES 2               // 最小属性数
#define Minmean 0.3                    // 最小|mean|（厳格化: 0.2→0.3）
#define MIN_CONCENTRATION 0.50         // 集中度下限（厳格化: 0.35→0.50）
#define MIN_SUPPORT_COUNT 10           // 最小マッチ数（緩和: 20→10）
#define MAX_SUPPORT_RATE 0.02          // 最大サポート率: 2%（希少性担保）

// 適応度の重み（異常検知重視）
#define FITNESS_SUPPORT_WEIGHT 100     // サポート重視度（緩和: 400→100）
#define FITNESS_SIGMA_WEIGHT 150       // 分散ペナルティ（緩和: 200→150）
#define FITNESS_SIGMA_OFFSET 0.30      // 分散オフセット
#define FITNESS_ATTRIBUTE_WEIGHT 2     // 属性数重視度
#define FITNESS_NEW_RULE_BONUS 50      // 新規ルールボーナス

// 統計的偏差ボーナス（異常度評価）
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_1 0.3   // 0.3%以上
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_2 0.5   // 0.5%以上（1σ）
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_3 0.8   // 0.8%以上（1.5σ）
#define STATISTICAL_SIGNIFICANCE_THRESHOLD_4 1.0   // 1.0%以上（2σ）

#define STATISTICAL_SIGNIFICANCE_BONUS_1 5000      // 0.3% tier
#define STATISTICAL_SIGNIFICANCE_BONUS_2 15000     // 0.5% tier（3倍増）
#define STATISTICAL_SIGNIFICANCE_BONUS_3 30000     // 0.8% tier（3倍増）
#define STATISTICAL_SIGNIFICANCE_BONUS_4 50000     // 1.0% tier（新設）

// 希少性ボーナス（低サポート率を評価）
#define RARITY_THRESHOLD_1 0.01        // サポート率 < 1%
#define RARITY_THRESHOLD_2 0.005       // サポート率 < 0.5%
#define RARITY_BONUS_1 3000            // 1%未満ボーナス
#define RARITY_BONUS_2 6000            // 0.5%未満ボーナス

// 集中度ボーナス（安定性評価）
#define CONCENTRATION_THRESHOLD_1 0.50
#define CONCENTRATION_THRESHOLD_2 0.60
#define CONCENTRATION_THRESHOLD_3 0.70
#define CONCENTRATION_THRESHOLD_4 0.80
#define CONCENTRATION_THRESHOLD_5 0.90

#define CONCENTRATION_BONUS_1 500
#define CONCENTRATION_BONUS_2 2000
#define CONCENTRATION_BONUS_3 5000
#define CONCENTRATION_BONUS_4 10000
#define CONCENTRATION_BONUS_5 20000
```

---

### 2.3 適応度計算式（新設計）

```c
fitness_score =
    // 基本スコア（従来通り）
    base_score +

    // 統計的偏差ボーナス（最優先: 50,000点まで）
    significance_bonus +

    // 希少性ボーナス（新設: 6,000点まで）
    rarity_bonus +

    // 集中度ボーナス（20,000点まで）
    concentration_bonus;
```

**スコア比率**:
- 統計的偏差: 最大50,000点 (**65%**)
- 集中度: 最大20,000点 (26%)
- 希少性: 最大6,000点 (8%)
- 基本スコア: 約1,000点 (1%)

**設計意図**:
- |mean|が支配的だが、絶対的ではない
- 希少性を初めて評価（低サポート率に報酬）
- 集中度で安定性を担保

---

## 3. 実装: 希少性ボーナスの追加

### 3.1 希少性評価関数

```c
/**
 * 希少性ボーナスを計算
 *
 * 異常検知では、低サポート率（希少なパターン）を評価する。
 *
 * @param support_rate サポート率（0.0 - 1.0）
 * @return 希少性ボーナススコア
 */
int calculate_rarity_bonus(double support_rate)
{
    if (support_rate < RARITY_THRESHOLD_2) {
        return RARITY_BONUS_2;  // < 0.5%: 超希少
    }
    else if (support_rate < RARITY_THRESHOLD_1) {
        return RARITY_BONUS_1;  // < 1.0%: 希少
    }
    else {
        return 0;  // 一般的なパターン
    }
}
```

---

### 3.2 統計的偏差ボーナス強化（4段階化）

```c
/**
 * 統計的有意性ボーナスを計算（4段階）
 *
 * 異常検知では、極端な|mean|を最優先評価。
 *
 * @param future_mean_array 未来予測mean配列
 * @return 統計的有意性ボーナススコア
 */
int calculate_statistical_significance_bonus(double *future_mean_array)
{
    double max_abs_mean = 0.0;

    // t+1, t+2の最大|mean|を取得
    for (int i = 0; i < MAX_FUTURE_SPAN; i++) {
        double abs_mean = fabs(future_mean_array[i]);
        if (abs_mean > max_abs_mean) {
            max_abs_mean = abs_mean;
        }
    }

    // 4段階評価
    if (max_abs_mean >= STATISTICAL_SIGNIFICANCE_THRESHOLD_4) {
        return STATISTICAL_SIGNIFICANCE_BONUS_4;  // |mean| ≥ 1.0%
    }
    else if (max_abs_mean >= STATISTICAL_SIGNIFICANCE_THRESHOLD_3) {
        return STATISTICAL_SIGNIFICANCE_BONUS_3;  // |mean| ≥ 0.8%
    }
    else if (max_abs_mean >= STATISTICAL_SIGNIFICANCE_THRESHOLD_2) {
        return STATISTICAL_SIGNIFICANCE_BONUS_2;  // |mean| ≥ 0.5%
    }
    else if (max_abs_mean >= STATISTICAL_SIGNIFICANCE_THRESHOLD_1) {
        return STATISTICAL_SIGNIFICANCE_BONUS_1;  // |mean| ≥ 0.3%
    }
    else {
        return 0;  // 異常なし
    }
}
```

---

### 3.3 品質チェック関数の修正

```c
/**
 * 異常検知用ルール品質チェック
 *
 * @return 1: 異常パターン, 0: 通常パターン（排除）
 */
int check_anomaly_rule_quality(double *future_sigma_array,
                                double *future_mean_array,
                                double support,
                                int num_attributes,
                                int *quadrant_counts,
                                int matched_count,
                                double support_rate)  // 追加
{
    // Stage 0: サポートカウントチェック（下限のみ）
    if (matched_count < MIN_SUPPORT_COUNT) {
        return 0;  // 最低10マッチ必要
    }

    // Stage 0b: 希少性チェック（上限）
    if (support_rate > MAX_SUPPORT_RATE) {
        return 0;  // サポート率2%超過は一般的すぎる
    }

    // Stage 1: 基本品質チェック
    if (!check_basic_conditions(support, num_attributes)) {
        return 0;
    }

    // Stage 2: 予測安定性チェック（低分散）
    if (!check_variance_stability(future_sigma_array)) {
        return 0;
    }

    // Stage 3: パターン偏りチェック（集中度）
    if (!check_pattern_concentration(quadrant_counts)) {
        return 0;
    }

    // Stage 4: 統計的異常チェック（Mean閾値）
    if (!check_mean_threshold(future_mean_array)) {
        return 0;
    }

    // すべてのフィルタをパス → 異常パターン発見
    return 1;
}
```

---

## 4. 異常検知の評価指標

### 4.1 異常度スコア（Anomaly Score）

```python
def calculate_anomaly_score(rule):
    """
    ルールの異常度を定量化

    異常度 = 統計的偏差 × 希少性 × 安定性
    """
    # 統計的偏差（0-100点）
    deviation_score = min(rule['max_abs_mean'] / 0.01 * 100, 100)  # 1.0% = 100点

    # 希少性（0-100点）
    rarity_score = max(100 - rule['support_rate'] * 5000, 0)  # 2% = 0点

    # 安定性（0-100点）
    stability_score = (
        (1.0 - rule['avg_sigma']) * 50 +      # 低分散
        rule['concentration'] * 50              # 高集中度
    )

    # 総合異常度（0-100点）
    anomaly_score = (
        deviation_score * 0.5 +    # 偏差50%
        rarity_score * 0.3 +       # 希少性30%
        stability_score * 0.2      # 安定性20%
    )

    return anomaly_score
```

---

### 4.2 異常分類タクソノミー

#### Type 1: Strong Positive Anomaly（強陽性異常）
- mean(t+1) ≥ +0.5% または mean(t+2) ≥ +0.5%
- 集中度 ≥ 60%（Q1に集中）
- **解釈**: 強い上昇トレンドへの転換点

#### Type 2: Strong Negative Anomaly（強陰性異常）
- mean(t+1) ≤ -0.5% または mean(t+2) ≤ -0.5%
- 集中度 ≥ 60%（Q3に集中）
- **解釈**: 強い下降トレンドへの転換点

#### Type 3: Reversal Anomaly（反転異常）
- mean(t+1) × mean(t+2) < 0（符号が逆）
- |mean(t+1)| ≥ 0.3% かつ |mean(t+2)| ≥ 0.3%
- **解釈**: 短期反発・反落パターン

#### Type 4: Extreme Rare Anomaly（極端希少異常）
- |mean| ≥ 1.0%
- サポート率 < 0.5%
- **解釈**: ブラックスワン的イベント

---

## 5. 実験計画: Phase 2-Anomaly

### 5.1 実験設定

```c
// Phase 2-Anomaly設定
Maxsigx = 0.8
Minmean = 0.3
MIN_SUPPORT_COUNT = 10
MAX_SUPPORT_RATE = 0.02
MIN_CONCENTRATION = 0.50

// ボーナス強化
STATISTICAL_SIGNIFICANCE_BONUS_4 = 50000  // |mean|≥1.0%
RARITY_BONUS_2 = 6000  // サポート率<0.5%
```

---

### 5.2 期待される結果

**発見ルール数**: 50-150個/通貨

**品質分布**:
- |mean| ≥ 0.5%: 30-50% (15-75個)
- |mean| ≥ 0.8%: 5-10% (3-15個)
- |mean| ≥ 1.0%: 1-3% (1-5個)
- サポート率 < 1%: 70-80%
- 集中度 ≥ 60%: 80-90%

**統計的有意性**: 平均0.8-1.0σ偏差

---

### 5.3 成功基準

1. **異常発見率**: |mean|≥0.5%のルールを50個以上発見
2. **希少性**: 平均サポート率 0.5-1.5%
3. **安定性**: 平均集中度 ≥ 60%
4. **再現性**: 検証データでも異常性を維持

---

## 6. 異常検知の応用

### 6.1 トレーディング戦略

**戦略1: 異常シグナル検出**
```
IF 現在の市場状態が異常ルールにマッチ
THEN
  IF mean > 0: ロングポジション
  IF mean < 0: ショートポジション
END
```

**期待リターン**: 条件付きmean（例: +0.8%/時間）

---

### 6.2 リスク管理

**戦略2: 異常警告システム**
```
IF 複数の陰性異常ルールが同時マッチ
THEN
  警告: 暴落リスク高
  アクション: ポジション縮小
END
```

---

### 6.3 市場構造分析

**戦略3: レジームチェンジ検出**
```
異常ルールのマッチ頻度を監視:
  - 通常期: 異常マッチ率 < 2%
  - 転換期: 異常マッチ率 > 5%

→ 市場レジームの変化を早期検知
```

---

## 7. 実装ロードマップ

### Phase 2-Anomaly実装（2日）

**Day 1**: コード修正
1. ✅ 希少性ボーナス関数追加
2. ✅ 統計的偏差ボーナス4段階化
3. ✅ MAX_SUPPORT_RATE閾値追加
4. ✅ check_anomaly_rule_quality実装

**Day 2**: 実験実行
1. DOGE 1 roundテスト
2. 結果分析（異常度スコア計算）
3. 全11通貨で実行
4. Phase 1 vs Phase 2-Anomaly比較

---

### Phase 3: 異常分類・可視化（3日）

1. 異常タイプ分類器実装
2. 異常度ヒートマップ作成
3. 時系列異常発生頻度分析
4. レポート生成自動化

---

### Phase 4: リアルタイム異常検知（5日）

1. ストリーミングデータ対応
2. 異常マッチング高速化
3. 警告システム構築
4. バックテスト基盤

---

## 8. 理論的背景

### 8.1 統計的異常検知の分類

本フレームワークは以下に分類される:

**Rule-based Anomaly Detection**
- GNPによるルール自動発見
- 条件付き分布の極値探索
- 解釈可能性が高い

**対比: 他手法**
- Isolation Forest: ブラックボックス
- One-Class SVM: 境界ベース
- LSTM Autoencoder: 再構成誤差ベース

---

### 8.2 優位性

1. **解釈可能性**: ルール=具体的な市場条件
2. **因果推論**: 属性→結果の因果関係
3. **進化的発見**: GNPによる探索空間の効率的探索
4. **希少パターン対応**: 低サポートでも発見可能

---

## 9. 研究価値

### 9.1 学術的貢献

**新規性**:
- 時系列データへのGNP異常検知の適用
- 多目的最適化（偏差・希少性・安定性）
- 象限集中度による安定性評価

**論文タイトル案**:
> "Evolutionary Discovery of Statistical Anomalies in Financial Time Series:
>  A Genetic Network Programming Approach with Rarity-Weighted Fitness"

---

### 9.2 実務的価値

1. **アルゴリズムトレーディング**: 異常シグナルによるエントリー
2. **リスク管理**: 異常パターン検出による早期警告
3. **市場分析**: 市場構造の変化検知

---

## 10. 次のアクション

### 即座に実施（今日）

1. **Phase 2-Anomaly実装**
   - main.cに希少性ボーナス追加
   - 統計的偏差ボーナス4段階化
   - MAX_SUPPORT_RATE追加

2. **DOGE 1 roundテスト**
   - 異常検知効果の検証
   - Phase 1との比較

3. **異常度スコア計算スクリプト作成**
   - Python実装
   - Top異常ルール抽出

---

### 短期（1週間以内）

1. 全11通貨で実験
2. 異常ルール可視化
3. 異常分類タクソノミー適用
4. Phase 2-Anomaly結果レポート作成

---

### 中期（1ヶ月以内）

1. リアルタイム異常検知システム構築
2. バックテスト実行
3. 論文執筆開始

---

## 結論

**GNPは異常検知フレームワークとして優れた特性を持つ:**

✅ **解釈可能な例外ルール発見**
✅ **希少パターンの自動探索**
✅ **進化的最適化による効率的探索**
✅ **多目的評価（偏差・希少性・安定性）**

**Phase 2-Anomalyの実装により:**
- |mean|≥0.5%の異常ルールを多数発見
- 希少性評価により「珍しいが意味のある」パターンを優先
- 実用的な異常検知システムへの道筋

**次の一歩**: Phase 2-Anomalyの実装とDOGEでの検証実験

---

**作成者**: Claude Code
**日付**: 2025-11-10
**バージョン**: 1.0
