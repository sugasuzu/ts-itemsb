# 統計的に興味深い小集団の発見戦略

## 現在の状況分析

### 現在のシステムが発見しているもの

```
419ルール発見（BTC、1試行）
平均的なルール例:
  X(t+1) = 0.131 ± 0.498%
  X(t+1) = -0.144 ± 0.459%

ベースライン（全データ）:
  μ = 0.0077%
  σ = 0.5121%
```

**問題:**
- 発見された平均値（0.131%, -0.144%）≈ ノイズレベル（0.5σ）
- **統計的有意性が低い** ❌

---

## 研究目標の明確化

### 「統計的に興味深い」とは何か？

#### 定義1: 統計的異常（Statistical Anomaly）

```
条件付き分布が無条件分布から大きく乖離している

|μ_rule - μ_data| >> σ_data

例:
  全データ: μ = 0.0077%, σ = 0.5121%
  ルール発見: μ_rule = +1.0%
  → 偏差: (1.0 - 0.0077) / 0.5121 = 1.94σ
  → 統計的に有意（α=0.05水準）
```

#### 定義2: 予測的価値（Predictive Value）

```
そのルールを使うと、ランダムより良い予測ができる

情報ゲイン = H(X) - H(X|Rule)
リターン改善 = E[X|Rule] - E[X]
シャープレシオ = (μ_rule - r_f) / σ_rule
```

#### 定義3: 実用的価値（Actionable Insight）

```
トレーディングや意思決定に使える

- サポート率 >= 0.1%（年間8.76回以上発生）
- 平均リターン >= 0.5%（取引コスト考慮後でも利益）
- 標準偏差 <= 0.5%（リスク管理可能）
- 勝率 >= 60%（心理的に継続可能）
```

---

## 戦略1: 閾値ベースのフィルタリング（即実装可能）

### 現在の設定

```c
#define Minsup 0.001              // 0.1% (年間8.76回)
#define Maxsigx 1.0               // 1.0%
#define Minmean 0.1               // 0.1% ← 低すぎる
#define MIN_CONCENTRATION 0.4     // 40%
```

### 提案1: 強い統計的シグナル（Conservative）

```c
#define Minsup 0.0005             // 0.05% (希少パターンOK)
#define Maxsigx 0.45              // 0.45% (低リスク)
#define Minmean_Positive 0.5      // +0.5% (μ + 1σ)
#define Minmean_Negative -0.5     // -0.5% (μ - 1σ)
#define MIN_CONCENTRATION 0.5     // 50% (明確な方向性)
#define MIN_SUPPORT_COUNT 10      // 最低10回の観測
```

**期待される結果:**
- ルール数: 50-200個（419 → 1/8程度）
- 平均リターン: ±0.5-1.0%
- 統計的有意性: 1σ以上
- 研究価値: 中〜高

### 提案2: 極端な異常（Aggressive）

```c
#define Minsup 0.0002             // 0.02% (非常に希少)
#define Maxsigx 0.35              // 0.35% (非常に低リスク)
#define Minmean_Positive 1.0      // +1.0% (μ + 2σ)
#define Minmean_Negative -1.0     // -1.0% (μ - 2σ)
#define MIN_CONCENTRATION 0.6     // 60% (強い方向性)
#define MIN_SUPPORT_COUNT 5       // 希少パターンなので5回でOK
```

**期待される結果:**
- ルール数: 5-30個（419 → 1/80程度）
- 平均リターン: ±1.0-2.0%
- 統計的有意性: 2σ以上
- 研究価値: 非常に高（"ブラックスワン"パターン）

---

## 戦略2: 統計的検定の導入（要実装）

### 実装すべき統計的指標

#### 1. t検定（t-test）

```c
double calculate_t_statistic(double mean, double sigma, int n) {
    // H0: μ_rule = μ_data (= 0.0077%)
    double mu_data = 0.0077;
    double t = (mean - mu_data) / (sigma / sqrt(n));
    return t;
}

// 使用例
if (fabs(t) > 1.96) {
    // p < 0.05 (95%信頼区間)
    rule.is_statistically_significant = 1;
}
```

#### 2. 情報ゲイン（Information Gain）

```c
double calculate_information_gain(int total_count, int rule_count,
                                   double data_entropy, double rule_entropy) {
    double prior = data_entropy;
    double posterior = (rule_count / (double)total_count) * rule_entropy;
    return prior - posterior;
}
```

#### 3. シャープレシオ（Sharpe Ratio）

```c
double calculate_sharpe_ratio(double mean, double sigma) {
    double risk_free_rate = 0.0; // または実際の無リスク金利
    return (mean - risk_free_rate) / sigma;
}

// 良いルールの基準
// Sharpe >= 1.0 (良い)
// Sharpe >= 2.0 (優秀)
// Sharpe >= 3.0 (exceptional)
```

#### 4. 勝率（Win Rate）

```c
struct rule_quality {
    int total_matches;
    int positive_outcomes;  // X(t+1) > 0
    int negative_outcomes;  // X(t+1) < 0
    double win_rate;        // positive / total
};

// 計算
win_rate = positive_outcomes / (double)total_matches;

// 基準
// win_rate >= 0.55 (まあまあ)
// win_rate >= 0.60 (良い)
// win_rate >= 0.65 (優秀)
```

---

## 戦略3: 多段階フィルタリング（推奨）

### Phase 1: 粗いフィルタ（GNP進化中）

```c
// 現在のcheck_rule_quality()を維持
if (support >= Minsup &&
    sigma <= Maxsigx &&
    fabs(mean) >= Minmean) {
    // ルールプールに追加
}
```

### Phase 2: 統計的検定（ルール抽出後）

```c
for (int i = 0; i < rule_count; i++) {
    // t検定
    double t_stat = calculate_t_statistic(rule[i].mean, rule[i].sigma, rule[i].support_count);

    if (fabs(t_stat) < 1.96) {
        continue; // 統計的に有意でない
    }

    // シャープレシオ
    double sharpe = calculate_sharpe_ratio(rule[i].mean, rule[i].sigma);

    if (sharpe < 1.0) {
        continue; // リスク調整後リターンが低い
    }

    // 勝率
    double win_rate = calculate_win_rate(rule[i]);

    if (win_rate < 0.55) {
        continue; // 勝率が低い
    }

    // すべての基準をパスしたルールのみ採用
    high_quality_rules[high_quality_count++] = rule[i];
}
```

### Phase 3: 実用性評価（バックテスト）

```python
# verification/rule_XXXX.csv を使ってバックテスト
def backtest_rule(rule_id):
    df = pd.read_csv(f'verification/rule_{rule_id}.csv')

    # エントリー: matched == 1
    entries = df[df['matched'] == 1]

    # 取引コスト考慮
    transaction_cost = 0.001  # 0.1%

    # リターン計算
    returns = entries['X_t+1'] - transaction_cost

    # 評価指標
    mean_return = returns.mean()
    sharpe = returns.mean() / returns.std()
    win_rate = (returns > 0).mean()
    max_drawdown = calculate_max_drawdown(returns)

    return {
        'mean': mean_return,
        'sharpe': sharpe,
        'win_rate': win_rate,
        'max_dd': max_drawdown,
        'n_trades': len(entries)
    }
```

---

## 戦略4: 方向性の明確化（Bidirectional）

### 現在の問題

```c
// 現在: 絶対値でチェック
if (fabs(mean) >= Minmean) { ... }

// 問題: 正負の区別がない
```

### 解決策: 明示的な方向性

```c
enum RuleDirection {
    POSITIVE_PATTERN,  // 上昇パターン
    NEGATIVE_PATTERN,  // 下降パターン
    NEUTRAL_PATTERN    // 中立（使わない）
};

struct enhanced_rule {
    // 既存のフィールド
    int antecedent_attrs[MAX_ATTRIBUTES];
    double mean;
    double sigma;

    // 新規追加
    enum RuleDirection direction;
    double mean_positive;   // 正方向の平均
    double mean_negative;   // 負方向の平均
    int positive_count;     // 正の結果の回数
    int negative_count;     // 負の結果の回数
};

// フィルタリング
if (mean >= +0.5) {
    rule.direction = POSITIVE_PATTERN;
} else if (mean <= -0.5) {
    rule.direction = NEGATIVE_PATTERN;
} else {
    continue; // 中立パターンは破棄
}
```

---

## 戦略5: データ固有の特性を利用

### 仮想通貨市場の特性

#### 特性1: ボラティリティクラスタリング

```
高ボラ期 → 高ボラ期
低ボラ期 → 低ボラ期

活用:
  - ボラティリティレジームごとにルールを分類
  - 高ボラ期専用ルール、低ボラ期専用ルールを発見
```

#### 特性2: モメンタム効果

```
上昇 → 上昇（継続）
下降 → 下降（継続）

活用:
  - 連続上昇/下降パターンを探索
  - time_delay機能を活用（t-3, t-2, t-1の連続性）
```

#### 特性3: 平均回帰

```
大きな上昇 → 下降
大きな下降 → 上昇

活用:
  - 極値後の反転パターン
  - |X(t)| > 1σ のときの t+1 の挙動
```

### 株式市場の特性

#### 特性1: 曜日効果（Day-of-Week Effect）

```
月曜効果: 週初めは下降傾向
金曜効果: 週末前は上昇傾向

活用:
  - T列（タイムスタンプ）から曜日を抽出
  - 曜日別のルールを発見
```

#### 特性2: 月初・月末効果

```
月初: リバランス需要 → 上昇
月末: ウィンドウドレッシング → 変動大

活用:
  - 日付から月初/月末を判定
  - 該当期間のパターンを重点探索
```

#### 特性3: セクター連動

```
日経225: 輸出関連銘柄は円安で上昇
金融株: 金利上昇で上昇

活用:
  - 複数銘柄の同時パターン
  - 属性間の相関を利用（time_delay機能）
```

### 為替市場（FX）の特性

#### 特性1: キャリートレード

```
高金利通貨 → 買われやすい
低金利通貨 → 売られやすい

活用:
  - 金利差の変化パターン
  - 長期保有のリターンパターン
```

#### 特性2: インターマーケット連動

```
USD/JPY ↑ → 日経225 ↑
EUR/USD ↓ → 米国株 ↑

活用:
  - 複数通貨ペアの同時パターン
  - クロス分析（EUR/USD × GBP/USD）
```

---

## 戦略6: アンサンブル評価

### 複数の指標を統合

```python
def calculate_composite_score(rule):
    # 1. 統計的有意性 (0-10点)
    t_stat = abs(calculate_t_statistic(rule.mean, rule.sigma, rule.n))
    sig_score = min(t_stat, 10)

    # 2. シャープレシオ (0-10点)
    sharpe = calculate_sharpe_ratio(rule.mean, rule.sigma)
    sharpe_score = min(sharpe * 5, 10)

    # 3. 勝率 (0-10点)
    win_rate = rule.positive_count / rule.total_count
    win_score = (win_rate - 0.5) * 20

    # 4. サポート率 (0-10点)
    support_score = min(rule.support_rate * 100, 10)

    # 5. 象限集中度 (0-10点)
    concentration = calculate_concentration(rule)
    conc_score = concentration * 10

    # 重み付き平均
    weights = {
        'significance': 0.3,
        'sharpe': 0.3,
        'win_rate': 0.2,
        'support': 0.1,
        'concentration': 0.1
    }

    total_score = (
        sig_score * weights['significance'] +
        sharpe_score * weights['sharpe'] +
        win_score * weights['win_rate'] +
        support_score * weights['support'] +
        conc_score * weights['concentration']
    )

    return total_score

# 閾値
# score >= 7.0: Excellent
# score >= 5.0: Good
# score >= 3.0: Acceptable
# score < 3.0:  Reject
```

---

## 実装の優先順位

### Phase 1: 即実装可能（今週）

1. **閾値の引き上げ**
   ```c
   #define Minmean 0.5  // 0.1 → 0.5
   ```

2. **方向性の明確化**
   ```c
   if (mean >= +0.5 || mean <= -0.5) { ... }
   ```

3. **テスト実行**
   ```bash
   ./main BTC 10
   # 発見されるルール数を確認
   ```

### Phase 2: 統計的検定（来週）

1. **t検定の実装**
2. **シャープレシオの実装**
3. **勝率の実装**
4. **Phase 2フィルタの追加**

### Phase 3: バックテスト（再来週）

1. **Pythonスクリプトの作成**
2. **verification CSVの分析**
3. **実用性評価**

---

## 期待される成果

### Conservative設定（Minmean=0.5）

```
入力: BTC, 10試行
期待出力:
  - ルール数: 500-2000個（現在の419 × 10試行の1/2〜1/4）
  - 平均リターン: ±0.5-1.0%
  - 統計的有意性: 1σ以上
  - 実用性: 中程度
```

### Aggressive設定（Minmean=1.0）

```
入力: BTC, 10試行
期待出力:
  - ルール数: 50-300個（非常に選別的）
  - 平均リターン: ±1.0-2.0%
  - 統計的有意性: 2σ以上
  - 実用性: 高（論文化可能）
```

---

## まとめ

### 現在の問題

- ✅ Time-delay機能は完璧に動作
- ❌ 閾値が低すぎて、ノイズレベルのルールを発見
- ❌ 統計的有意性の評価がない

### 推奨アクション（優先度順）

1. **Minmean を 0.5 に変更**（最優先）
2. **10試行実行して結果を確認**
3. **統計的検定の実装**
4. **バックテストによる実用性評価**
5. **論文執筆**

**次のステップ:** Minmean=0.5 でテスト実行しましょうか？
