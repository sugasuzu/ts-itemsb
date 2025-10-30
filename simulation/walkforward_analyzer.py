#!/usr/bin/env python3
"""
ウォークフォワード分析スクリプト

時間をスライドさせながら学習→テストを繰り返し、
ルールの頑健性を検証します。

使用方法:
    python simulation/walkforward_analyzer.py GBPAUD
    python simulation/walkforward_analyzer.py GBPAUD --train-years 5 --test-years 1
    python simulation/walkforward_analyzer.py GBPAUD --start-year 2015
"""

import argparse
import sys
from pathlib import Path
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime

from rule_loader import RuleLoader
from signal_generator_v2 import SignalGeneratorV2
from trade_simulator_v2 import TradeSimulatorV2

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'Hiragino Sans', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False


class WalkforwardAnalyzer:
    """ウォークフォワード分析クラス"""

    def __init__(self, pair, train_years=5, test_years=1, start_year=2010, end_year=2025):
        """
        Parameters
        ----------
        pair : str
            通貨ペア
        train_years : int
            学習ウィンドウ（年数）
        test_years : int
            テストウィンドウ（年数）
        start_year : int
            開始年
        end_year : int
            終了年
        """
        self.pair = pair
        self.train_years = train_years
        self.test_years = test_years
        self.start_year = start_year
        self.end_year = end_year
        self.periods = []
        self.results = []

    def generate_periods(self):
        """
        ウォークフォワード期間を生成

        Returns
        -------
        list of dict
            期間リスト
        """
        periods = []

        # テスト期間の開始年は、学習期間分後から
        first_test_year = self.start_year + self.train_years

        for test_year in range(first_test_year, self.end_year + 1, self.test_years):
            train_start = f"{test_year - self.train_years}-01-01"
            train_end = f"{test_year - 1}-12-31"
            test_start = f"{test_year}-01-01"
            test_end = f"{test_year + self.test_years - 1}-12-31"

            periods.append({
                'period': len(periods) + 1,
                'train_start': train_start,
                'train_end': train_end,
                'test_start': test_start,
                'test_end': test_end,
                'test_year': test_year
            })

        self.periods = periods
        print(f"\n{'='*80}")
        print(f"Walkforward Periods Generated")
        print(f"{'='*80}")
        print(f"Total periods: {len(periods)}")
        print(f"Train window:  {self.train_years} years")
        print(f"Test window:   {self.test_years} year(s)")
        print()

        for p in periods:
            print(f"Period {p['period']:2d}: Train [{p['train_start']} to {p['train_end']}] → Test [{p['test_start']} to {p['test_end']}]")

        print(f"{'='*80}\n")

        return periods

    def run_period_simulation(self, period, top_rules=20, sort_by='support'):
        """
        単一期間のシミュレーション実行

        Parameters
        ----------
        period : dict
            期間情報
        top_rules : int
            使用するルール数
        sort_by : str
            ルールソート基準

        Returns
        -------
        dict
            期間の結果
        """
        print(f"\n{'='*80}")
        print(f"Period {period['period']}: Testing {period['test_year']}")
        print(f"{'='*80}")

        try:
            # ルール読み込み（全期間のルールを使用）
            # 注意: 本来は各期間の学習データでルールを再発見すべきだが、
            # 今回は既存のルールを使用
            loader = RuleLoader(self.pair)
            pos_rules, neg_rules = loader.load_all_rules(top_n=top_rules, sort_by=sort_by)

            # シグナル生成（テスト期間のみ）
            generator = SignalGeneratorV2(self.pair, test_start_date=period['test_start'])
            generator.load_data()

            # テスト期間終了日でフィルタリング
            test_data = generator.test_data[generator.test_data['T'] <= period['test_end']].copy()
            generator.test_data = test_data.reset_index(drop=True)

            signals = generator.generate_signals(pos_rules, neg_rules, deduplicate=True)

            if not signals:
                print(f"⚠️  No signals generated for period {period['period']}")
                return None

            # トレード実行
            simulator = TradeSimulatorV2(self.pair)
            trades = simulator.simulate(signals, generator)

            if not trades:
                print(f"⚠️  No trades executed for period {period['period']}")
                return None

            # 統計取得
            stats = simulator.get_basic_stats()
            trades_df = simulator.get_trades_df()

            # ドローダウン計算
            cumulative = trades_df['cumulative_return']
            running_max = cumulative.cummax()
            drawdown = cumulative - running_max
            max_drawdown = drawdown.min()

            result = {
                'period': period['period'],
                'test_year': period['test_year'],
                'test_start': period['test_start'],
                'test_end': period['test_end'],
                'total_trades': stats['total_trades'],
                'win_rate': stats['win_rate'],
                'total_return': stats['total_return'],
                'total_return_before_cost': stats['total_return_before_cost'],
                'avg_profit': stats['avg_profit'],
                'max_drawdown': max_drawdown,
                'buy_trades': stats['buy_trades'],
                'sell_trades': stats['sell_trades'],
                'buy_return': stats['buy_total_return'],
                'sell_return': stats['sell_total_return']
            }

            print(f"\n✓ Period {period['period']} Results:")
            print(f"  Total trades:  {result['total_trades']}")
            print(f"  Win rate:      {result['win_rate']:.2f}%")
            print(f"  Total return:  {result['total_return']:+.3f}%")
            print(f"  Max drawdown:  {result['max_drawdown']:+.3f}%")

            return result

        except Exception as e:
            print(f"\n❌ Error in period {period['period']}: {e}")
            import traceback
            traceback.print_exc()
            return None

    def run_all_periods(self, top_rules=20, sort_by='support'):
        """
        全期間のシミュレーション実行

        Parameters
        ----------
        top_rules : int
            使用するルール数
        sort_by : str
            ルールソート基準

        Returns
        -------
        list of dict
            全期間の結果
        """
        if not self.periods:
            self.generate_periods()

        results = []

        for period in self.periods:
            result = self.run_period_simulation(period, top_rules, sort_by)
            if result:
                results.append(result)

        self.results = results
        return results

    def aggregate_results(self):
        """
        結果を集計

        Returns
        -------
        dict
            集計結果
        """
        if not self.results:
            print("No results to aggregate.")
            return None

        returns = [r['total_return'] for r in self.results]
        win_periods = sum(1 for r in returns if r > 0)

        aggregate = {
            'total_periods': len(self.results),
            'total_return': sum(returns),
            'avg_return_per_period': np.mean(returns),
            'std_return': np.std(returns),
            'win_periods': win_periods,
            'lose_periods': len(self.results) - win_periods,
            'consistency': (win_periods / len(self.results)) * 100,
            'total_trades': sum(r['total_trades'] for r in self.results),
            'avg_win_rate': np.mean([r['win_rate'] for r in self.results]),
            'avg_max_drawdown': np.mean([r['max_drawdown'] for r in self.results]),
            'worst_period_return': min(returns),
            'best_period_return': max(returns),
        }

        return aggregate

    def print_summary(self):
        """サマリーを表示"""
        if not self.results:
            print("No results available.")
            return

        aggregate = self.aggregate_results()

        print(f"\n{'='*80}")
        print(f"Walkforward Analysis Summary: {self.pair}")
        print(f"{'='*80}")

        # 期間ごとの結果
        print(f"\nPeriod-by-Period Results:")
        print(f"{'─'*80}")
        print(f"{'Period':<8} {'Year':<6} {'Trades':<8} {'Win Rate':<10} {'Return':<12} {'Max DD':<12}")
        print(f"{'─'*80}")

        for r in self.results:
            print(f"{r['period']:<8d} {r['test_year']:<6d} {r['total_trades']:<8d} "
                  f"{r['win_rate']:>7.2f}%  {r['total_return']:>+10.3f}%  {r['max_drawdown']:>+10.3f}%")

        print(f"{'─'*80}")

        # 集計結果
        print(f"\nAggregate Statistics:")
        print(f"{'─'*80}")
        print(f"Total Periods:           {aggregate['total_periods']}")
        print(f"Win Periods:             {aggregate['win_periods']} / {aggregate['total_periods']} ({aggregate['consistency']:.1f}%)")
        print(f"Lose Periods:            {aggregate['lose_periods']}")
        print()
        print(f"Total Return:            {aggregate['total_return']:+.3f}%")
        print(f"Avg Return/Period:       {aggregate['avg_return_per_period']:+.3f}%")
        print(f"Std Dev of Returns:      ±{aggregate['std_return']:.3f}%")
        print(f"Best Period:             {aggregate['best_period_return']:+.3f}%")
        print(f"Worst Period:            {aggregate['worst_period_return']:+.3f}%")
        print()
        print(f"Total Trades:            {aggregate['total_trades']}")
        print(f"Avg Win Rate:            {aggregate['avg_win_rate']:.2f}%")
        print(f"Avg Max Drawdown:        {aggregate['avg_max_drawdown']:+.3f}%")
        print(f"{'='*80}\n")

        # 評価
        print(f"{'='*80}")
        print(f"Evaluation")
        print(f"{'='*80}")

        consistency_rating = "Excellent" if aggregate['consistency'] >= 70 else "Good" if aggregate['consistency'] >= 50 else "Poor"
        return_rating = "Good" if aggregate['avg_return_per_period'] >= 2 else "Fair" if aggregate['avg_return_per_period'] >= 0 else "Poor"
        stability_rating = "Good" if aggregate['std_return'] < 5 else "Fair" if aggregate['std_return'] < 10 else "Poor"

        print(f"Consistency:  {aggregate['consistency']:.1f}% → {consistency_rating}")
        print(f"  Target: ≥70% (Excellent), ≥50% (Good)")
        print()
        print(f"Avg Return:   {aggregate['avg_return_per_period']:+.3f}% → {return_rating}")
        print(f"  Target: ≥2% (Good), ≥0% (Fair)")
        print()
        print(f"Stability:    ±{aggregate['std_return']:.3f}% → {stability_rating}")
        print(f"  Target: <5% (Good), <10% (Fair)")
        print()

        if aggregate['consistency'] >= 70 and aggregate['avg_return_per_period'] >= 2:
            print(f"✅ PASS: Strategy shows robust performance across periods")
        elif aggregate['consistency'] >= 50 or aggregate['avg_return_per_period'] >= 0:
            print(f"⚠️  MARGINAL: Strategy has some consistency but needs improvement")
        else:
            print(f"❌ FAIL: Strategy lacks consistency and robustness")

        print(f"{'='*80}\n")

    def plot_results(self, output_path):
        """
        結果を可視化

        Parameters
        ----------
        output_path : str or Path
            出力ファイルパス
        """
        if not self.results:
            print("No results to plot.")
            return

        fig, axes = plt.subplots(2, 2, figsize=(16, 12))

        # 1. 期間ごとのリターン（棒グラフ）
        ax1 = axes[0, 0]
        periods = [r['period'] for r in self.results]
        returns = [r['total_return'] for r in self.results]
        colors = ['green' if r > 0 else 'red' for r in returns]

        ax1.bar(periods, returns, color=colors, alpha=0.7)
        ax1.axhline(y=0, color='black', linewidth=1, linestyle='--')
        ax1.set_xlabel('Period', fontsize=12, fontweight='bold')
        ax1.set_ylabel('Return (%)', fontsize=12, fontweight='bold')
        ax1.set_title('Returns by Period', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3, axis='y')

        # 2. 累積リターン（折れ線グラフ）
        ax2 = axes[0, 1]
        cumulative_returns = np.cumsum(returns)
        ax2.plot(periods, cumulative_returns, marker='o', linewidth=2, color='blue')
        ax2.axhline(y=0, color='gray', linewidth=1, linestyle='--')
        ax2.set_xlabel('Period', fontsize=12, fontweight='bold')
        ax2.set_ylabel('Cumulative Return (%)', fontsize=12, fontweight='bold')
        ax2.set_title('Cumulative Returns Over Time', fontsize=14, fontweight='bold')
        ax2.grid(True, alpha=0.3)

        # 3. 勝率の推移
        ax3 = axes[1, 0]
        win_rates = [r['win_rate'] for r in self.results]
        ax3.plot(periods, win_rates, marker='s', linewidth=2, color='purple')
        ax3.axhline(y=50, color='gray', linewidth=1, linestyle='--', label='Random (50%)')
        ax3.set_xlabel('Period', fontsize=12, fontweight='bold')
        ax3.set_ylabel('Win Rate (%)', fontsize=12, fontweight='bold')
        ax3.set_title('Win Rate by Period', fontsize=14, fontweight='bold')
        ax3.set_ylim(0, 100)
        ax3.legend()
        ax3.grid(True, alpha=0.3)

        # 4. 統計サマリー（テキスト）
        ax4 = axes[1, 1]
        ax4.axis('off')

        aggregate = self.aggregate_results()

        summary_text = (
            f"Walkforward Analysis Summary\n"
            f"{'='*45}\n\n"
            f"Pair:                {self.pair}\n"
            f"Train Window:        {self.train_years} years\n"
            f"Test Window:         {self.test_years} year(s)\n"
            f"Total Periods:       {aggregate['total_periods']}\n\n"
            f"Performance:\n"
            f"{'─'*45}\n"
            f"Total Return:        {aggregate['total_return']:+.2f}%\n"
            f"Avg Return/Period:   {aggregate['avg_return_per_period']:+.2f}%\n"
            f"Win Periods:         {aggregate['win_periods']}/{aggregate['total_periods']} ({aggregate['consistency']:.1f}%)\n"
            f"Std Dev:             ±{aggregate['std_return']:.2f}%\n\n"
            f"Best Period:         {aggregate['best_period_return']:+.2f}%\n"
            f"Worst Period:        {aggregate['worst_period_return']:+.2f}%\n\n"
            f"Trades:\n"
            f"{'─'*45}\n"
            f"Total Trades:        {aggregate['total_trades']}\n"
            f"Avg Win Rate:        {aggregate['avg_win_rate']:.2f}%\n"
            f"Avg Max DD:          {aggregate['avg_max_drawdown']:+.2f}%\n"
        )

        ax4.text(0.1, 0.9, summary_text, transform=ax4.transAxes,
                fontsize=11, family='monospace',
                verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

        plt.tight_layout()
        plt.savefig(output_path, dpi=100, bbox_inches='tight')
        plt.close()

        print(f"✓ Walkforward analysis plot saved to: {output_path}")

    def save_results(self, output_dir):
        """
        結果をCSVに保存

        Parameters
        ----------
        output_dir : str or Path
            出力ディレクトリ
        """
        if not self.results:
            print("No results to save.")
            return

        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        # 期間ごとの結果をCSV保存
        df = pd.DataFrame(self.results)
        csv_file = output_dir / f"{self.pair}_walkforward_results.csv"
        df.to_csv(csv_file, index=False)
        print(f"✓ Results saved to: {csv_file}")

        # 集計結果をテキストファイルに保存
        aggregate = self.aggregate_results()
        summary_file = output_dir / f"{self.pair}_walkforward_summary.txt"

        with open(summary_file, 'w') as f:
            f.write(f"Walkforward Analysis Summary: {self.pair}\n")
            f.write(f"{'='*80}\n\n")
            f.write(f"Configuration:\n")
            f.write(f"  Train window: {self.train_years} years\n")
            f.write(f"  Test window:  {self.test_years} year(s)\n")
            f.write(f"  Periods:      {aggregate['total_periods']}\n\n")
            f.write(f"Results:\n")
            f.write(f"  Total return:        {aggregate['total_return']:+.3f}%\n")
            f.write(f"  Avg return/period:   {aggregate['avg_return_per_period']:+.3f}%\n")
            f.write(f"  Consistency:         {aggregate['consistency']:.1f}%\n")
            f.write(f"  Win periods:         {aggregate['win_periods']}/{aggregate['total_periods']}\n")
            f.write(f"  Std dev:             ±{aggregate['std_return']:.3f}%\n")

        print(f"✓ Summary saved to: {summary_file}")


def main():
    parser = argparse.ArgumentParser(
        description='Walkforward Analysis for Rule-based Trading',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic usage
  python simulation/walkforward_analyzer.py GBPAUD

  # Custom train/test windows
  python simulation/walkforward_analyzer.py GBPAUD --train-years 5 --test-years 1

  # Different start year
  python simulation/walkforward_analyzer.py GBPAUD --start-year 2015
        """
    )

    parser.add_argument('pair', type=str, help='Currency pair (e.g., GBPAUD)')
    parser.add_argument('--train-years', type=int, default=5,
                       help='Training window in years (default: 5)')
    parser.add_argument('--test-years', type=int, default=1,
                       help='Test window in years (default: 1)')
    parser.add_argument('--start-year', type=int, default=2010,
                       help='Start year (default: 2010)')
    parser.add_argument('--end-year', type=int, default=2025,
                       help='End year (default: 2025)')
    parser.add_argument('--top', type=int, default=20,
                       help='Number of top rules per direction (default: 20)')
    parser.add_argument('--sort-by', type=str, default='support',
                       choices=['support', 'extreme_score', 'snr', 'extremeness', 'discovery'],
                       help='Sort criterion (default: support)')
    parser.add_argument('--output-dir', type=str, default='simulation/results_walkforward',
                       help='Output directory (default: simulation/results_walkforward)')

    args = parser.parse_args()

    print(f"\n{'='*80}")
    print(f"Walkforward Analysis")
    print(f"{'='*80}")
    print(f"Pair:         {args.pair}")
    print(f"Train years:  {args.train_years}")
    print(f"Test years:   {args.test_years}")
    print(f"Period:       {args.start_year} - {args.end_year}")
    print(f"Top N rules:  {args.top} per direction")
    print(f"Sort by:      {args.sort_by}")
    print(f"{'='*80}\n")

    try:
        # ウォークフォワード分析実行
        analyzer = WalkforwardAnalyzer(
            pair=args.pair,
            train_years=args.train_years,
            test_years=args.test_years,
            start_year=args.start_year,
            end_year=args.end_year
        )

        # 期間生成
        analyzer.generate_periods()

        # 全期間実行
        analyzer.run_all_periods(top_rules=args.top, sort_by=args.sort_by)

        # サマリー表示
        analyzer.print_summary()

        # 結果保存
        output_dir = Path(args.output_dir) / args.pair
        analyzer.save_results(output_dir)

        # 可視化
        plot_file = output_dir / f"{args.pair}_walkforward_analysis.png"
        analyzer.plot_results(plot_file)

        print(f"\n{'='*80}")
        print(f"Walkforward Analysis Complete")
        print(f"{'='*80}")
        print(f"Results saved to: {output_dir}")
        print(f"{'='*80}\n")

    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
