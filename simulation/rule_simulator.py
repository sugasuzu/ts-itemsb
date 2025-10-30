#!/usr/bin/env python3
"""
ルールベーストレードシミュレーター

発見されたルールを使って実際のトレードをシミュレートし、
パフォーマンスを評価します。

使用方法:
    python simulation/rule_simulator.py USDJPY
    python simulation/rule_simulator.py USDJPY --top 20 --sort-by support
    python simulation/rule_simulator.py GBPAUD --top 15 --sort-by snr
"""

import argparse
from pathlib import Path
import sys

from rule_loader import RuleLoader
from signal_generator import SignalGenerator
from trade_simulator import TradeSimulator
from performance_evaluator import PerformanceEvaluator


def main():
    parser = argparse.ArgumentParser(
        description='Rule-based trade simulator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic usage (top 20 rules by support)
  python simulation/rule_simulator.py USDJPY

  # Top 15 rules by SNR
  python simulation/rule_simulator.py USDJPY --top 15 --sort-by snr

  # Top 30 rules by ExtremeScore
  python simulation/rule_simulator.py GBPAUD --top 30 --sort-by extreme_score
        """
    )

    parser.add_argument('pair', type=str, help='Currency pair (e.g., USDJPY)')
    parser.add_argument('--top', type=int, default=20,
                       help='Number of top rules per direction (default: 20)')
    parser.add_argument('--sort-by', type=str, default='support',
                       choices=['support', 'extreme_score', 'snr', 'extremeness', 'discovery'],
                       help='Sort criterion (default: support)')
    parser.add_argument('--output-dir', type=str, default='simulation/results',
                       help='Output directory for results (default: simulation/results)')
    parser.add_argument('--save-trades', action='store_true',
                       help='Save individual trades to CSV')

    args = parser.parse_args()

    print(f"\n{'='*80}")
    print(f"Rule-Based Trade Simulator")
    print(f"{'='*80}")
    print(f"Currency Pair: {args.pair}")
    print(f"Top N rules:   {args.top} per direction (total: {args.top * 2})")
    print(f"Sort by:       {args.sort_by}")
    print(f"Output dir:    {args.output_dir}")
    print(f"{'='*80}\n")

    try:
        # Step 1: ルール読み込み
        print("Step 1: Loading rules...")
        loader = RuleLoader(args.pair)
        pos_rules, neg_rules = loader.load_all_rules(top_n=args.top, sort_by=args.sort_by)

        if not pos_rules and not neg_rules:
            print("ERROR: No rules loaded. Exiting.")
            sys.exit(1)

        # Step 2: シグナル生成
        print("Step 2: Generating signals...")
        generator = SignalGenerator(args.pair)
        generator.load_data()
        signals = generator.generate_signals(pos_rules, neg_rules)

        if not signals:
            print("ERROR: No signals generated. Exiting.")
            sys.exit(1)

        # Step 3: トレードシミュレーション
        print("Step 3: Simulating trades...")
        simulator = TradeSimulator(args.pair)
        trades = simulator.simulate(signals, generator)

        if not trades:
            print("ERROR: No trades executed. Exiting.")
            sys.exit(1)

        # 統計表示
        simulator.print_stats()

        # トレード結果DataFrame取得
        trades_df = simulator.get_trades_df()

        # Step 4: パフォーマンス評価
        print("Step 4: Evaluating performance...")
        evaluator = PerformanceEvaluator(args.pair)
        evaluator.set_trades(trades_df)

        # ルール別パフォーマンス表示
        evaluator.print_rule_performance(top_n=20)

        # 最大ドローダウン表示
        max_dd = evaluator.get_max_drawdown()
        print(f"Maximum Drawdown: {max_dd:+.3f}%\n")

        # Step 5: レポート生成
        print("Step 5: Generating reports...")
        output_dir = Path(args.output_dir) / args.pair
        evaluator.generate_report(output_dir)

        # トレードCSV保存（オプション）
        if args.save_trades:
            trade_file = output_dir / f"{args.pair}_trades.csv"
            simulator.save_trades(trade_file)

        print(f"\n{'='*80}")
        print(f"Simulation Complete")
        print(f"{'='*80}")
        print(f"Results saved to: {output_dir}")
        print(f"{'='*80}\n")

    except FileNotFoundError as e:
        print(f"\nERROR: {e}")
        print(f"Please ensure that rule files and data files exist for {args.pair}.")
        sys.exit(1)
    except Exception as e:
        print(f"\nERROR: An unexpected error occurred:")
        print(f"{type(e).__name__}: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == '__main__':
    main()
