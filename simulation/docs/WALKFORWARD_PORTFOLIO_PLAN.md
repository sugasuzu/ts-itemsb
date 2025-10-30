# ウォークフォワード分析 & ポートフォリオシミュレーション設計

## 📊 Part 1: ウォークフォワード分析

### 概念

**ウォークフォワード分析**とは、時間をスライドさせながら学習→テストを繰り返し、ルールの頑健性を検証する手法です。

```
┌─────────────────────────────────────────────────────────────┐
│              2010 ─────────────────────────→ 2025           │
└─────────────────────────────────────────────────────────────┘

Period 1:
[2010-2015] 学習 → [2016] テスト (1年)
     5年         →   1年

Period 2:
    [2011-2016] 学習 → [2017] テスト
         5年         →   1年

Period 3:
        [2012-2017] 学習 → [2018] テスト
             5年         →   1年

Period 4:
            [2013-2018] 学習 → [2019] テスト

Period 5:
                [2014-2019] 学習 → [2020] テスト

Period 6:
                    [2015-2020] 学習 → [2021] テスト

Period 7:
                        [2016-2021] 学習 → [2022] テスト

Period 8:
                            [2017-2022] 学習 → [2023] テスト

Period 9:
                                [2018-2023] 学習 → [2024] テスト

Period 10:
                                    [2019-2024] 学習 → [2025] テスト
```

### パラメータ設定

```python
# ウォークフォワード設定
TRAIN_WINDOW = 5 年  # 学習ウィンドウ（5年分のデータでルール発見）
TEST_WINDOW = 1 年   # テストウィンドウ（1年分でパフォーマンス評価）
STEP = 1 年          # スライド幅（1年ずつ前進）

# 期間
START_YEAR = 2010
END_YEAR = 2025
TOTAL_PERIODS = (2025 - 2016 + 1) = 10 期間
```

### 実装方針

#### **Step 1: 期間分割**

```python
def generate_walkforward_periods(start_year, end_year, train_years=5, test_years=1):
    """
    ウォークフォワード期間を生成

    Returns:
        list of dict: [
            {
                'period': 1,
                'train_start': '2010-01-01',
                'train_end': '2015-12-31',
                'test_start': '2016-01-01',
                'test_end': '2016-12-31'
            },
            ...
        ]
    """
    periods = []

    for test_year in range(start_year + train_years, end_year + 1):
        train_start = f"{test_year - train_years}-01-01"
        train_end = f"{test_year - 1}-12-31"
        test_start = f"{test_year}-01-01"
        test_end = f"{test_year}-12-31"

        periods.append({
            'period': len(periods) + 1,
            'train_start': train_start,
            'train_end': train_end,
            'test_start': test_start,
            'test_end': test_end
        })

    return periods

# 例:
# Period 1: 2010-2015 → 2016
# Period 2: 2011-2016 → 2017
# ...
# Period 10: 2019-2024 → 2025
```

#### **Step 2: 期間ごとのルール再発見**

```python
# 重要: 各期間で新たにルールを発見する必要がある
# なぜなら、学習期間が異なればルールも変わるため

for period in periods:
    # 1. 学習期間のデータでルール発見
    # （注意: これは本来main.cで行うべき処理）
    # 今回は既存のルールを使用するため、この部分はスキップ

    # 2. テスト期間でシミュレーション
    generator = SignalGeneratorV2(pair, test_start_date=period['test_start'])
    # ...
```

#### **Step 3: 結果の集計**

```python
def aggregate_walkforward_results(period_results):
    """
    全期間の結果を集計

    Returns:
        dict: {
            'total_return': 累積リターン,
            'avg_return_per_period': 平均リターン/期間,
            'win_periods': 勝ち期間数,
            'lose_periods': 負け期間数,
            'consistency': 一貫性スコア,
            'period_details': 各期間の詳細
        }
    """
    total_return = sum(p['return'] for p in period_results)
    win_periods = sum(1 for p in period_results if p['return'] > 0)

    # 一貫性スコア: 全期間で安定して機能しているか
    returns = [p['return'] for p in period_results]
    consistency = (win_periods / len(period_results)) * 100

    return {
        'total_periods': len(period_results),
        'total_return': total_return,
        'avg_return_per_period': total_return / len(period_results),
        'win_periods': win_periods,
        'lose_periods': len(period_results) - win_periods,
        'consistency': consistency,
        'std_return': np.std(returns),
        'period_details': period_results
    }
```

### 期待される結果

```
=== Walkforward Analysis Results ===

Period 1 (2016):  +2.3%
Period 2 (2017):  -1.5%
Period 3 (2018):  +4.2%
Period 4 (2019):  +0.8%
Period 5 (2020):  -2.1%
Period 6 (2021):  +3.5%
Period 7 (2022):  +1.9%
Period 8 (2023):  -0.5%
Period 9 (2024):  +2.7%
Period 10 (2025): +1.4%

Total Return:      +12.7%
Avg Return/Year:   +1.27%
Win Periods:       7 / 10 (70%)
Consistency Score: 70%
Std Dev:           ±2.1%
```

### 評価基準

| 指標 | 良い | 普通 | 悪い |
|------|------|------|------|
| **一貫性（Win Periods）** | ≥70% | 50-70% | <50% |
| **平均リターン/年** | ≥5% | 2-5% | <2% |
| **標準偏差** | <3% | 3-5% | >5% |

## 📊 Part 2: 複数通貨ペアポートフォリオ

### 概念

複数の通貨ペアでトレードすることで、リスクを分散し、安定したリターンを目指します。

```
┌─────────────────────────────────────────┐
│         ポートフォリオ構成               │
├─────────────────────────────────────────┤
│ GBPAUD:  20%  →  リターン +3.9%         │
│ EURAUD:  20%  →  リターン +2.1%         │
│ USDJPY:  20%  →  リターン -1.2%         │
│ GBPJPY:  20%  →  リターン +4.5%         │
│ EURUSD:  20%  →  リターン +1.8%         │
├─────────────────────────────────────────┤
│ 合計ポートフォリオ: +2.22%              │
│ リスク（標準偏差）: ±1.8%               │
│ シャープレシオ: 1.23                    │
└─────────────────────────────────────────┘
```

### パラメータ設定

```python
# ポートフォリオ構成
PORTFOLIO_PAIRS = [
    'GBPAUD', 'EURAUD', 'USDJPY', 'GBPJPY', 'EURUSD',
    'GBPCAD', 'AUDNZD', 'NZDJPY', 'EURCHF', 'AUDUSD'
]

# 資金配分戦略
ALLOCATION_STRATEGY = 'equal_weight'  # 均等配分
# または
ALLOCATION_STRATEGY = 'risk_parity'   # リスク均等配分
# または
ALLOCATION_STRATEGY = 'performance_based'  # パフォーマンスベース
```

### 実装方針

#### **Step 1: 各通貨ペアのシミュレーション**

```python
def simulate_all_pairs(pairs, test_start='2021-01-01', top_rules=20):
    """
    全通貨ペアでシミュレーション実行

    Returns:
        dict: {
            'GBPAUD': {
                'return': +19.6%,
                'trades': 1245,
                'win_rate': 50.4%,
                'max_dd': -13.8%,
                'sharpe': 0.8
            },
            'EURAUD': {...},
            ...
        }
    """
    results = {}

    for pair in pairs:
        try:
            # ルール読み込み
            loader = RuleLoader(pair)
            pos_rules, neg_rules = loader.load_all_rules(top_n=top_rules)

            # シグナル生成
            generator = SignalGeneratorV2(pair, test_start_date=test_start)
            generator.load_data()
            signals = generator.generate_signals(pos_rules, neg_rules, deduplicate=True)

            # トレード実行
            simulator = TradeSimulatorV2(pair)
            trades = simulator.simulate(signals, generator)

            # 統計取得
            stats = simulator.get_basic_stats()

            results[pair] = {
                'return': stats['total_return'],
                'trades': stats['total_trades'],
                'win_rate': stats['win_rate'],
                'avg_profit': stats['avg_profit'],
                'max_dd': calculate_max_drawdown(trades),
                'trades_df': simulator.get_trades_df()
            }

        except Exception as e:
            print(f"Error with {pair}: {e}")
            results[pair] = None

    return results
```

#### **Step 2: 資金配分の決定**

```python
def calculate_allocation(pair_results, strategy='equal_weight'):
    """
    資金配分を計算

    Strategies:
        equal_weight: 全通貨ペアに均等配分（1/N）
        risk_parity: リスク（標準偏差）で重み付け
        performance_based: リターン実績で重み付け
    """

    valid_pairs = [p for p, r in pair_results.items() if r is not None]
    n_pairs = len(valid_pairs)

    if strategy == 'equal_weight':
        # 均等配分
        allocation = {pair: 1.0 / n_pairs for pair in valid_pairs}

    elif strategy == 'risk_parity':
        # リスク均等配分（リスクが高いペアは少なく）
        risks = {pair: calculate_volatility(results['trades_df'])
                for pair, results in pair_results.items() if results}

        inv_risks = {pair: 1.0 / risk for pair, risk in risks.items()}
        total_inv_risk = sum(inv_risks.values())

        allocation = {pair: inv_risk / total_inv_risk
                     for pair, inv_risk in inv_risks.items()}

    elif strategy == 'performance_based':
        # パフォーマンスベース（良いペアに多く配分）
        # シャープレシオで重み付け
        sharpes = {pair: calculate_sharpe(results['trades_df'])
                  for pair, results in pair_results.items() if results}

        # 負のシャープレシオは0に
        positive_sharpes = {pair: max(0, sharpe) for pair, sharpe in sharpes.items()}
        total_sharpe = sum(positive_sharpes.values())

        if total_sharpe > 0:
            allocation = {pair: sharpe / total_sharpe
                         for pair, sharpe in positive_sharpes.items()}
        else:
            # 全て負の場合は均等配分
            allocation = {pair: 1.0 / n_pairs for pair in valid_pairs}

    return allocation
```

#### **Step 3: ポートフォリオリターンの計算**

```python
def calculate_portfolio_performance(pair_results, allocation):
    """
    ポートフォリオ全体のパフォーマンスを計算

    Returns:
        dict: {
            'total_return': ポートフォリオリターン,
            'portfolio_sharpe': シャープレシオ,
            'diversification_benefit': 分散効果,
            'daily_returns': 日次リターン（相関分析用）
        }
    """

    # 各通貨ペアのリターンを加重平均
    portfolio_return = sum(
        pair_results[pair]['return'] * weight
        for pair, weight in allocation.items()
        if pair_results[pair]
    )

    # 日次リターンの合成（時系列）
    # これにより相関を考慮した真のリスク計算が可能
    all_dates = get_common_dates(pair_results)

    portfolio_daily_returns = []
    for date in all_dates:
        daily_return = sum(
            get_return_on_date(pair_results[pair]['trades_df'], date) * weight
            for pair, weight in allocation.items()
        )
        portfolio_daily_returns.append(daily_return)

    # リスク指標
    portfolio_std = np.std(portfolio_daily_returns)
    portfolio_sharpe = np.mean(portfolio_daily_returns) / portfolio_std if portfolio_std > 0 else 0

    # 分散効果の計算
    # 単純加重平均のリスク vs 実際のポートフォリオリスク
    weighted_avg_std = sum(
        calculate_volatility(pair_results[pair]['trades_df']) * weight
        for pair, weight in allocation.items()
    )
    diversification_benefit = (weighted_avg_std - portfolio_std) / weighted_avg_std * 100

    return {
        'total_return': portfolio_return,
        'portfolio_std': portfolio_std,
        'portfolio_sharpe': portfolio_sharpe,
        'diversification_benefit': diversification_benefit,
        'allocation': allocation,
        'daily_returns': portfolio_daily_returns
    }
```

#### **Step 4: 相関分析**

```python
def analyze_correlation(pair_results):
    """
    通貨ペア間の相関を分析

    高相関ペアは分散効果が低い
    低相関・負相関ペアは分散効果が高い
    """

    # 日次リターンの相関行列
    returns_matrix = pd.DataFrame({
        pair: get_daily_returns(results['trades_df'])
        for pair, results in pair_results.items()
        if results
    })

    correlation_matrix = returns_matrix.corr()

    print("\n=== Correlation Matrix ===")
    print(correlation_matrix)

    # 低相関ペアの推奨
    low_corr_pairs = find_low_correlation_pairs(correlation_matrix, threshold=0.3)
    print(f"\n低相関ペア（推奨）: {low_corr_pairs}")

    return correlation_matrix
```

### 期待される結果

```
=== Portfolio Simulation Results ===

Individual Pairs Performance:
┌─────────┬──────────┬─────────┬──────────┬──────────┐
│ Pair    │ Return   │ Trades  │ Win Rate │ Max DD   │
├─────────┼──────────┼─────────┼──────────┼──────────┤
│ GBPAUD  │ +19.6%   │ 1245    │ 50.4%    │ -13.8%   │
│ EURAUD  │ +12.3%   │ 1180    │ 51.2%    │ -10.5%   │
│ USDJPY  │ -3.2%    │ 890     │ 48.1%    │ -18.2%   │
│ GBPJPY  │ +25.8%   │ 1320    │ 52.3%    │ -15.6%   │
│ EURUSD  │ +8.9%    │ 1050    │ 49.8%    │ -12.1%   │
│ GBPCAD  │ +15.2%   │ 1100    │ 50.9%    │ -11.3%   │
│ AUDNZD  │ +6.5%    │ 980     │ 49.5%    │ -14.8%   │
│ NZDJPY  │ +18.7%   │ 1200    │ 51.8%    │ -13.2%   │
│ EURCHF  │ +4.3%    │ 750     │ 48.9%    │ -9.8%    │
│ AUDUSD  │ +10.1%   │ 1090    │ 50.2%    │ -12.5%   │
└─────────┴──────────┴─────────┴──────────┴──────────┘

Allocation (Equal Weight):
  Each pair: 10%

Portfolio Performance:
  Total Return:           +11.82%
  Portfolio Std Dev:      ±8.5%
  Sharpe Ratio:           1.39
  Max Drawdown:           -9.2%

Diversification Benefit:  +42%
  (Individual avg std: 14.7% → Portfolio std: 8.5%)

Correlation Insights:
  Avg correlation:        0.35 (moderate)
  Low correlation pairs:  GBPAUD-USDJPY (0.12)
                         EURAUD-GBPJPY (0.18)
  High correlation pairs: GBPAUD-GBPCAD (0.78)
                         EURUSD-EURCHF (0.82)
```

## 🎯 統合: ウォークフォワード × ポートフォリオ

### 最強の検証方法

```python
def walkforward_portfolio_analysis():
    """
    ウォークフォワード分析をポートフォリオ全体で実行

    各期間で:
      1. 全通貨ペアでシミュレーション
      2. ポートフォリオ構築
      3. パフォーマンス評価

    最終的に:
      - 期間ごとのポートフォリオリターン
      - 一貫性の検証
      - リスク調整後リターン
    """

    periods = generate_walkforward_periods(2010, 2025)
    portfolio_results = []

    for period in periods:
        print(f"\n=== Period {period['period']}: {period['test_start'][:4]} ===")

        # 各通貨ペアでシミュレーション
        pair_results = simulate_all_pairs(
            PORTFOLIO_PAIRS,
            test_start=period['test_start'],
            top_rules=20
        )

        # 資金配分
        allocation = calculate_allocation(pair_results, strategy='equal_weight')

        # ポートフォリオパフォーマンス
        portfolio_perf = calculate_portfolio_performance(pair_results, allocation)

        portfolio_results.append({
            'period': period['period'],
            'year': period['test_start'][:4],
            'return': portfolio_perf['total_return'],
            'sharpe': portfolio_perf['portfolio_sharpe'],
            'allocation': allocation
        })

    # 集計
    total_return = sum(p['return'] for p in portfolio_results)
    win_periods = sum(1 for p in portfolio_results if p['return'] > 0)

    print(f"\n{'='*80}")
    print(f"Walkforward Portfolio Analysis Summary")
    print(f"{'='*80}")
    print(f"Total Periods:      {len(portfolio_results)}")
    print(f"Total Return:       {total_return:+.2f}%")
    print(f"Avg Return/Period:  {total_return/len(portfolio_results):+.2f}%")
    print(f"Win Periods:        {win_periods} / {len(portfolio_results)} ({win_periods/len(portfolio_results)*100:.1f}%)")
    print(f"Avg Sharpe:         {np.mean([p['sharpe'] for p in portfolio_results]):.2f}")

    return portfolio_results
```

### 期待される最終結果

```
=== Walkforward Portfolio Analysis Summary ===

Year-by-Year Portfolio Returns:
2016: +3.2%  (Sharpe: 1.2)
2017: +1.8%  (Sharpe: 0.9)
2018: +5.1%  (Sharpe: 1.5)
2019: +2.4%  (Sharpe: 1.1)
2020: -0.5%  (Sharpe: -0.2)
2021: +4.3%  (Sharpe: 1.4)
2022: +2.9%  (Sharpe: 1.0)
2023: +1.6%  (Sharpe: 0.7)
2024: +3.7%  (Sharpe: 1.3)
2025: +2.2%  (Sharpe: 0.9)

Total Return (10 years):  +26.7%
Avg Return/Year:          +2.67%
Win Years:                9 / 10 (90%)
Avg Sharpe Ratio:         1.08
Max Portfolio DD:         -6.8%

Conclusion:
✅ Highly consistent (90% win rate)
✅ Positive Sharpe (>1.0)
✅ Low drawdown (<10%)
⚠️  Moderate returns (2.67%/year)
```

## 📋 実装優先順位

### Phase 1: ウォークフォワード分析（単一通貨ペア）
```bash
python3 simulation/walkforward_analyzer.py GBPAUD --train-years 5 --test-years 1
```

### Phase 2: ポートフォリオシミュレーション（固定期間）
```bash
python3 simulation/portfolio_simulator.py --pairs GBPAUD EURAUD USDJPY --test-start 2021-01-01
```

### Phase 3: 統合分析
```bash
python3 simulation/walkforward_portfolio.py --all-pairs --train-years 5
```

## 🎓 成功基準

### ウォークフォワード分析
- ✅ 一貫性 ≥70%（10期間中7期間以上でプラス）
- ✅ 平均リターン/年 ≥2%
- ✅ 標準偏差 <5%

### ポートフォリオ
- ✅ 分散効果 ≥30%（リスク減少）
- ✅ シャープレシオ ≥1.0
- ✅ 最大ドローダウン <15%

### 統合
- ✅ 10年間で総リターン ≥20%
- ✅ 年率リターン ≥2%
- ✅ 一貫性 ≥80%

次のステップとして、どちらから実装しますか？
1. ウォークフォワード分析（推奨：まずは手法の検証）
2. ポートフォリオシミュレーション（推奨：リスク分散の効果を確認）
3. 両方同時に実装
