#!/usr/bin/env python3
"""
Portfolio Simulator - Multi-currency pair portfolio backtesting
Implements diversification across multiple currency pairs with different allocation strategies
"""

import argparse
import os
import sys
import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime

# Import v2 optimized components
from rule_loader import RuleLoader
from signal_generator_v2 import SignalGeneratorV2
from trade_simulator_v2 import TradeSimulatorV2
from performance_evaluator import PerformanceEvaluator


class PortfolioSimulator:
    """Simulate trading portfolio across multiple currency pairs"""

    def __init__(self, pairs, test_start='2021-01-01', allocation_strategy='equal_weight'):
        """
        Initialize portfolio simulator

        Args:
            pairs: List of currency pair names (e.g., ['GBPAUD', 'EURAUD'])
            test_start: Start date for testing (YYYY-MM-DD)
            allocation_strategy: 'equal_weight', 'risk_parity', or 'performance_based'
        """
        self.pairs = pairs
        self.test_start = test_start
        self.allocation_strategy = allocation_strategy
        self.pair_results = {}
        self.portfolio_equity_curve = None

    def run_single_pair(self, pair, top_n=20):
        """
        Run simulation for a single currency pair

        Returns:
            dict: Results including trades, returns, and equity curve
        """
        print(f"\n{'='*60}")
        print(f"Running simulation for {pair}...")
        print(f"{'='*60}")

        try:
            # Load rules
            loader = RuleLoader(pair)
            positive_rules, negative_rules = loader.load_all_rules(top_n=top_n, sort_by='support')

            if not positive_rules and not negative_rules:
                print(f"⚠️  No rules found for {pair}, skipping...")
                return None

            print(f"Loaded {len(positive_rules)} positive rules, {len(negative_rules)} negative rules")

            # Generate signals
            signal_gen = SignalGeneratorV2(pair, test_start_date=self.test_start)
            signal_gen.load_data()
            signals = signal_gen.generate_signals(positive_rules, negative_rules, deduplicate=True)

            if not signals:
                print(f"⚠️  No signals generated for {pair}, skipping...")
                return None

            print(f"Generated {len(signals)} signals (after deduplication)")

            # Simulate trades
            simulator = TradeSimulatorV2(pair)
            trades = simulator.simulate(signals, signal_gen)

            if not trades:
                print(f"⚠️  No trades executed for {pair}, skipping...")
                return None

            print(f"Executed {len(trades)} trades")

            # Calculate metrics from trades
            trades_df = pd.DataFrame(trades)
            trades_df['timestamp'] = pd.to_datetime(trades_df['entry_date'])
            trades_df = trades_df.sort_values('timestamp')

            # Calculate equity curve (profit is already in %)
            trades_df['cumulative_return'] = trades_df['profit'].cumsum()
            trades_df['equity'] = (1 + trades_df['cumulative_return'] / 100)

            # Calculate metrics
            total_return = trades_df['cumulative_return'].iloc[-1]
            win_rate = (trades_df['profit'] > 0).mean() * 100
            total_trades = len(trades_df)

            # Calculate drawdown
            cummax = trades_df['cumulative_return'].cummax()
            drawdown = trades_df['cumulative_return'] - cummax
            max_drawdown = drawdown.min()

            # Calculate Sharpe ratio
            if len(trades_df) > 1 and trades_df['profit'].std() > 0:
                sharpe = trades_df['profit'].mean() / trades_df['profit'].std() * np.sqrt(252)
            else:
                sharpe = 0

            metrics = {
                'total_return': total_return,
                'win_rate': win_rate,
                'total_trades': total_trades,
                'max_drawdown': max_drawdown,
                'sharpe_ratio': sharpe
            }

            result = {
                'pair': pair,
                'trades': trades_df,
                'metrics': metrics,
                'equity_curve': trades_df[['timestamp', 'equity']].copy()
            }

            print(f"✓ {pair}: Total Return = {metrics['total_return']:.3f}%, Win Rate = {metrics['win_rate']:.2f}%")

            return result

        except Exception as e:
            print(f"❌ Error processing {pair}: {e}")
            import traceback
            traceback.print_exc()
            return None

    def calculate_allocation_weights(self):
        """
        Calculate allocation weights based on strategy

        Returns:
            dict: Weights for each pair (sum = 1.0)
        """
        n_pairs = len(self.pair_results)

        if self.allocation_strategy == 'equal_weight':
            # Equal allocation
            return {pair: 1.0 / n_pairs for pair in self.pair_results.keys()}

        elif self.allocation_strategy == 'risk_parity':
            # Inverse volatility weighting
            volatilities = {}
            for pair, result in self.pair_results.items():
                returns = result['trades']['profit'].values
                volatilities[pair] = np.std(returns)

            # Inverse volatility
            inv_vols = {pair: 1.0 / vol if vol > 0 else 0 for pair, vol in volatilities.items()}
            total_inv_vol = sum(inv_vols.values())

            if total_inv_vol == 0:
                return {pair: 1.0 / n_pairs for pair in self.pair_results.keys()}

            return {pair: inv_vol / total_inv_vol for pair, inv_vol in inv_vols.items()}

        elif self.allocation_strategy == 'performance_based':
            # Weight by past performance (Sharpe ratio)
            sharpes = {}
            for pair, result in self.pair_results.items():
                sharpes[pair] = result['metrics'].get('sharpe_ratio', 0)

            # Only positive Sharpes get weight
            positive_sharpes = {pair: max(0, sharpe) for pair, sharpe in sharpes.items()}
            total_sharpe = sum(positive_sharpes.values())

            if total_sharpe == 0:
                return {pair: 1.0 / n_pairs for pair in self.pair_results.keys()}

            return {pair: sharpe / total_sharpe for pair, sharpe in positive_sharpes.items()}

        else:
            raise ValueError(f"Unknown allocation strategy: {self.allocation_strategy}")

    def combine_equity_curves(self, weights):
        """
        Combine individual equity curves into portfolio equity curve

        Args:
            weights: Dict of allocation weights per pair

        Returns:
            pd.DataFrame: Portfolio equity curve with timestamp
        """
        # Get all unique timestamps
        all_timestamps = set()
        for result in self.pair_results.values():
            all_timestamps.update(result['equity_curve']['timestamp'])

        all_timestamps = sorted(list(all_timestamps))

        # Create portfolio equity curve
        portfolio_equity = pd.DataFrame({'timestamp': all_timestamps})
        portfolio_equity['equity'] = 1.0

        # For each pair, get equity at each timestamp
        for pair, result in self.pair_results.items():
            weight = weights[pair]
            equity_curve = result['equity_curve'].set_index('timestamp')

            # Forward fill to get equity at all timestamps
            equity_series = equity_curve['equity'].reindex(all_timestamps, method='ffill').fillna(1.0)

            # Weighted contribution
            portfolio_equity['equity'] += weight * (equity_series.values - 1.0)

        return portfolio_equity

    def calculate_portfolio_metrics(self, portfolio_equity):
        """Calculate portfolio-level performance metrics"""

        returns = portfolio_equity['equity'].pct_change().dropna()
        total_return = (portfolio_equity['equity'].iloc[-1] - 1.0) * 100

        # Calculate drawdown
        cummax = portfolio_equity['equity'].expanding(min_periods=1).max()
        drawdown = (portfolio_equity['equity'] - cummax) / cummax * 100
        max_drawdown = drawdown.min()

        # Sharpe ratio (assuming daily data, annualize with sqrt(252))
        if len(returns) > 1 and returns.std() > 0:
            sharpe = returns.mean() / returns.std() * np.sqrt(252)
        else:
            sharpe = 0

        return {
            'total_return': total_return,
            'max_drawdown': max_drawdown,
            'sharpe_ratio': sharpe,
            'volatility': returns.std() * 100,
            'n_timestamps': len(portfolio_equity)
        }

    def analyze_correlation(self):
        """Analyze correlation between currency pairs"""

        # Get all timestamps
        all_timestamps = set()
        for result in self.pair_results.values():
            all_timestamps.update(result['trades']['timestamp'])

        all_timestamps = sorted(list(all_timestamps))

        # Build returns matrix
        returns_matrix = {}
        for pair, result in self.pair_results.items():
            trades = result['trades'].set_index('timestamp')
            returns_series = trades['profit'].reindex(all_timestamps, fill_value=0)
            returns_matrix[pair] = returns_series.values

        returns_df = pd.DataFrame(returns_matrix)
        correlation_matrix = returns_df.corr()

        return correlation_matrix

    def run_portfolio(self, top_n=20):
        """
        Run complete portfolio simulation

        Args:
            top_n: Number of rules per direction to use
        """
        print(f"\n{'='*80}")
        print(f"PORTFOLIO SIMULATION")
        print(f"{'='*80}")
        print(f"Pairs: {', '.join(self.pairs)}")
        print(f"Test period: {self.test_start} onwards")
        print(f"Allocation strategy: {self.allocation_strategy}")
        print(f"Rules per pair: {top_n} positive + {top_n} negative")
        print(f"{'='*80}")

        # Run simulation for each pair
        for pair in self.pairs:
            result = self.run_single_pair(pair, top_n=top_n)
            if result:
                self.pair_results[pair] = result

        if not self.pair_results:
            print("\n❌ No valid results from any currency pair!")
            return None

        print(f"\n✓ Successfully processed {len(self.pair_results)}/{len(self.pairs)} pairs")

        # Calculate weights
        weights = self.calculate_allocation_weights()

        print(f"\nAllocation weights ({self.allocation_strategy}):")
        for pair, weight in weights.items():
            print(f"  {pair}: {weight*100:.2f}%")

        # Combine equity curves
        self.portfolio_equity_curve = self.combine_equity_curves(weights)

        # Calculate portfolio metrics
        portfolio_metrics = self.calculate_portfolio_metrics(self.portfolio_equity_curve)

        # Correlation analysis
        correlation_matrix = self.analyze_correlation()

        return {
            'pair_results': self.pair_results,
            'weights': weights,
            'portfolio_equity': self.portfolio_equity_curve,
            'portfolio_metrics': portfolio_metrics,
            'correlation_matrix': correlation_matrix
        }

    def generate_report(self, results, output_dir='results_portfolio'):
        """Generate comprehensive portfolio report"""

        output_path = Path(output_dir)
        output_path.mkdir(exist_ok=True)

        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        report_dir = output_path / f"portfolio_{timestamp}"
        report_dir.mkdir(exist_ok=True)

        # 1. Summary text report
        self._write_summary_report(results, report_dir)

        # 2. Detailed CSV
        self._write_detailed_csv(results, report_dir)

        # 3. Visualization
        self._create_visualizations(results, report_dir)

        print(f"\n{'='*80}")
        print(f"Report generated: {report_dir}")
        print(f"{'='*80}")

        return report_dir

    def _write_summary_report(self, results, report_dir):
        """Write text summary report"""

        with open(report_dir / 'portfolio_summary.txt', 'w', encoding='utf-8') as f:
            f.write("="*80 + "\n")
            f.write("PORTFOLIO SIMULATION SUMMARY\n")
            f.write("="*80 + "\n\n")

            f.write(f"Test Period: {self.test_start} onwards\n")
            f.write(f"Allocation Strategy: {self.allocation_strategy}\n")
            f.write(f"Number of Pairs: {len(results['pair_results'])}\n\n")

            f.write("-"*80 + "\n")
            f.write("PORTFOLIO-LEVEL METRICS\n")
            f.write("-"*80 + "\n")
            pm = results['portfolio_metrics']
            f.write(f"Total Return:        {pm['total_return']:+.3f}%\n")
            f.write(f"Max Drawdown:        {pm['max_drawdown']:.3f}%\n")
            f.write(f"Sharpe Ratio:        {pm['sharpe_ratio']:.4f}\n")
            f.write(f"Volatility:          {pm['volatility']:.3f}%\n\n")

            f.write("-"*80 + "\n")
            f.write("INDIVIDUAL PAIR RESULTS\n")
            f.write("-"*80 + "\n\n")

            for pair, result in results['pair_results'].items():
                weight = results['weights'][pair]
                metrics = result['metrics']

                f.write(f"{pair} (Weight: {weight*100:.2f}%)\n")
                f.write(f"  Total Return:      {metrics['total_return']:+.3f}%\n")
                f.write(f"  Win Rate:          {metrics['win_rate']:.2f}%\n")
                f.write(f"  Max Drawdown:      {metrics['max_drawdown']:.3f}%\n")
                f.write(f"  Total Trades:      {metrics['total_trades']}\n")
                f.write(f"  Sharpe Ratio:      {metrics.get('sharpe_ratio', 0):.4f}\n")
                f.write("\n")

            f.write("-"*80 + "\n")
            f.write("CORRELATION MATRIX\n")
            f.write("-"*80 + "\n")
            f.write(results['correlation_matrix'].to_string())
            f.write("\n\n")

            avg_corr = results['correlation_matrix'].values[np.triu_indices_from(results['correlation_matrix'].values, k=1)].mean()
            f.write(f"Average pairwise correlation: {avg_corr:.4f}\n")
            f.write("\n")

            f.write("="*80 + "\n")
            f.write("EVALUATION\n")
            f.write("="*80 + "\n\n")

            # Evaluation criteria
            total_ret = pm['total_return']
            sharpe = pm['sharpe_ratio']
            max_dd = abs(pm['max_drawdown'])

            if total_ret > 50 and sharpe > 1.0 and max_dd < 20:
                evaluation = "🎯 EXCELLENT - Strong risk-adjusted returns"
            elif total_ret > 30 and sharpe > 0.5 and max_dd < 30:
                evaluation = "✓ GOOD - Acceptable performance"
            elif total_ret > 15 and sharpe > 0.3:
                evaluation = "⚠️  MARGINAL - Needs improvement"
            else:
                evaluation = "❌ POOR - Strategy not viable"

            f.write(f"{evaluation}\n\n")

            f.write(f"Diversification benefit: ")
            # Compare portfolio vs average single pair
            avg_single_return = np.mean([r['metrics']['total_return'] for r in results['pair_results'].values()])
            benefit = total_ret - avg_single_return
            f.write(f"{benefit:+.3f}% (Portfolio return - Avg single pair)\n")

    def _write_detailed_csv(self, results, report_dir):
        """Write detailed results to CSV"""

        # Portfolio equity curve
        results['portfolio_equity'].to_csv(report_dir / 'portfolio_equity_curve.csv', index=False)

        # Individual pair metrics
        pair_metrics = []
        for pair, result in results['pair_results'].items():
            metrics = result['metrics'].copy()
            metrics['pair'] = pair
            metrics['weight'] = results['weights'][pair]
            pair_metrics.append(metrics)

        pd.DataFrame(pair_metrics).to_csv(report_dir / 'pair_metrics.csv', index=False)

        # Correlation matrix
        results['correlation_matrix'].to_csv(report_dir / 'correlation_matrix.csv')

    def _create_visualizations(self, results, report_dir):
        """Create visualization plots"""

        fig = plt.figure(figsize=(16, 12))

        # 1. Portfolio equity curve
        ax1 = plt.subplot(2, 2, 1)
        equity = results['portfolio_equity']
        ax1.plot(equity['timestamp'], equity['equity'], linewidth=2, color='blue', label='Portfolio')
        ax1.axhline(y=1.0, color='gray', linestyle='--', alpha=0.5)
        ax1.set_title('Portfolio Equity Curve', fontsize=14, fontweight='bold')
        ax1.set_xlabel('Date')
        ax1.set_ylabel('Equity (1.0 = initial capital)')
        ax1.grid(True, alpha=0.3)
        ax1.legend()

        # 2. Individual pair comparison
        ax2 = plt.subplot(2, 2, 2)
        pair_returns = []
        pair_names = []
        for pair, result in results['pair_results'].items():
            pair_returns.append(result['metrics']['total_return'])
            pair_names.append(pair)

        colors = ['green' if r > 0 else 'red' for r in pair_returns]
        ax2.barh(pair_names, pair_returns, color=colors, alpha=0.7)
        ax2.axvline(x=0, color='black', linestyle='-', linewidth=0.8)
        ax2.set_title('Individual Pair Returns', fontsize=14, fontweight='bold')
        ax2.set_xlabel('Total Return (%)')
        ax2.grid(True, alpha=0.3, axis='x')

        # 3. Correlation heatmap
        ax3 = plt.subplot(2, 2, 3)
        corr = results['correlation_matrix']
        im = ax3.imshow(corr, cmap='coolwarm', aspect='auto', vmin=-1, vmax=1)
        ax3.set_xticks(range(len(corr.columns)))
        ax3.set_yticks(range(len(corr.columns)))
        ax3.set_xticklabels(corr.columns, rotation=45, ha='right')
        ax3.set_yticklabels(corr.columns)
        ax3.set_title('Correlation Matrix', fontsize=14, fontweight='bold')
        plt.colorbar(im, ax=ax3)

        # Add correlation values
        for i in range(len(corr.columns)):
            for j in range(len(corr.columns)):
                text = ax3.text(j, i, f'{corr.iloc[i, j]:.2f}',
                               ha='center', va='center', color='black', fontsize=8)

        # 4. Risk-Return scatter
        ax4 = plt.subplot(2, 2, 4)
        for pair, result in results['pair_results'].items():
            ret = result['metrics']['total_return']
            vol = result['trades']['profit'].std()
            weight = results['weights'][pair]
            ax4.scatter(vol, ret, s=weight*1000, alpha=0.6, label=pair)

        # Portfolio point
        pm = results['portfolio_metrics']
        ax4.scatter(pm['volatility'], pm['total_return'],
                   s=300, marker='*', color='red', edgecolors='black', linewidth=2,
                   label='Portfolio', zorder=10)

        ax4.set_title('Risk-Return Profile', fontsize=14, fontweight='bold')
        ax4.set_xlabel('Volatility (Std Dev %)')
        ax4.set_ylabel('Total Return (%)')
        ax4.grid(True, alpha=0.3)
        ax4.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=8)
        ax4.axhline(y=0, color='gray', linestyle='--', alpha=0.5)

        plt.tight_layout()
        plt.savefig(report_dir / 'portfolio_analysis.png', dpi=150, bbox_inches='tight')
        plt.close()

        print(f"✓ Visualization saved: portfolio_analysis.png")


def main():
    parser = argparse.ArgumentParser(
        description='Portfolio Simulator - Multi-currency pair backtesting',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Equal weight portfolio with 5 pairs
  python3 simulation/portfolio_simulator.py --pairs GBPAUD EURAUD USDJPY GBPJPY EURUSD

  # Risk parity allocation
  python3 simulation/portfolio_simulator.py --pairs GBPAUD EURAUD USDJPY --strategy risk_parity

  # Performance-based allocation with 30 rules per pair
  python3 simulation/portfolio_simulator.py --pairs GBPAUD EURAUD --strategy performance_based --top 30
        """
    )

    parser.add_argument('--pairs', nargs='+', required=True,
                       help='Currency pairs to include in portfolio (e.g., GBPAUD EURAUD USDJPY)')
    parser.add_argument('--test-start', default='2021-01-01',
                       help='Test period start date (default: 2021-01-01)')
    parser.add_argument('--strategy', default='equal_weight',
                       choices=['equal_weight', 'risk_parity', 'performance_based'],
                       help='Allocation strategy (default: equal_weight)')
    parser.add_argument('--top', type=int, default=20,
                       help='Number of rules per direction to use (default: 20)')
    parser.add_argument('--output', default='results_portfolio',
                       help='Output directory for results (default: results_portfolio)')

    args = parser.parse_args()

    # Run portfolio simulation
    portfolio = PortfolioSimulator(
        pairs=args.pairs,
        test_start=args.test_start,
        allocation_strategy=args.strategy
    )

    results = portfolio.run_portfolio(top_n=args.top)

    if results:
        report_dir = portfolio.generate_report(results, output_dir=args.output)

        # Print summary to console
        print("\n" + "="*80)
        print("PORTFOLIO RESULTS SUMMARY")
        print("="*80)
        pm = results['portfolio_metrics']
        print(f"Total Return:        {pm['total_return']:+.3f}%")
        print(f"Max Drawdown:        {pm['max_drawdown']:.3f}%")
        print(f"Sharpe Ratio:        {pm['sharpe_ratio']:.4f}")
        print(f"Volatility:          {pm['volatility']:.3f}%")
        print("="*80)
        print(f"\nFull report: {report_dir}")
    else:
        print("\n❌ Portfolio simulation failed - no valid results")
        sys.exit(1)


if __name__ == '__main__':
    main()
