#!/usr/bin/env python3
"""
ルールベーストレードシミュレーター v2（最適化版）

改善点:
1. 時間分割バックテスト（学習期間とテスト期間を分離）
2. トランザクションコスト考慮（スプレッド・手数料・スリッページ）
3. シグナル重複排除（1時点1トレード、サポート数優先）

使用方法:
    python simulation/rule_simulator_v2.py GBPAUD
    python simulation/rule_simulator_v2.py GBPAUD --top 20 --test-start 2021-01-01
    python simulation/rule_simulator_v2.py GBPAUD --spread 0.0003 --no-dedup
"""

import argparse
from pathlib import Path
import sys

from rule_loader import RuleLoader
from signal_generator_v2 import SignalGeneratorV2
from trade_simulator_v2 import TradeSimulatorV2
from performance_evaluator import PerformanceEvaluator


def main():
    parser = argparse.ArgumentParser(
        description='Rule-based trade simulator v2 (Optimized)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic usage (optimized: test period 2021 onwards)
  python simulation/rule_simulator_v2.py GBPAUD

  # Custom test start date
  python simulation/rule_simulator_v2.py GBPAUD --test-start 2022-01-01

  # With higher transaction costs
  python simulation/rule_simulator_v2.py GBPAUD --spread 0.0005 --commission 0.0002

  # Disable signal deduplication (not recommended)
  python simulation/rule_simulator_v2.py GBPAUD --no-dedup

  # Compare with v1 (old method)
  python simulation/rule_simulator.py GBPAUD    # v1 (biased)
  python simulation/rule_simulator_v2.py GBPAUD # v2 (realistic)
        """
    )

    parser.add_argument('pair', type=str, help='Currency pair (e.g., GBPAUD)')
    parser.add_argument('--top', type=int, default=20,
                       help='Number of top rules per direction (default: 20)')
    parser.add_argument('--sort-by', type=str, default='support',
                       choices=['support', 'extreme_score', 'snr', 'extremeness', 'discovery'],
                       help='Sort criterion (default: support)')
    parser.add_argument('--test-start', type=str, default='2021-01-01',
                       help='Test period start date (default: 2021-01-01)')
    parser.add_argument('--spread', type=float, default=0.0002,
                       help='Spread cost (default: 0.0002 = 0.02%%)')
    parser.add_argument('--commission', type=float, default=0.0001,
                       help='Commission cost (default: 0.0001 = 0.01%%)')
    parser.add_argument('--slippage', type=float, default=0.0001,
                       help='Slippage cost (default: 0.0001 = 0.01%%)')
    parser.add_argument('--no-dedup', action='store_true',
                       help='Disable signal deduplication (allow multiple signals per timepoint)')
    parser.add_argument('--output-dir', type=str, default='simulation/results_v2',
                       help='Output directory for results (default: simulation/results_v2)')
    parser.add_argument('--save-trades', action='store_true',
                       help='Save individual trades to CSV')

    args = parser.parse_args()

    print(f"\n{'='*80}")
    print(f"Rule-Based Trade Simulator v2 (Optimized)")
    print(f"{'='*80}")
    print(f"Currency Pair:     {args.pair}")
    print(f"Top N rules:       {args.top} per direction (total: {args.top * 2})")
    print(f"Sort by:           {args.sort_by}")
    print(f"Test period start: {args.test_start}")
    print(f"Deduplication:     {'OFF (multiple signals allowed)' if args.no_dedup else 'ON (support-based priority)'}")
    print(f"Transaction costs:")
    print(f"  Spread:          {args.spread*100:.3f}%")
    print(f"  Commission:      {args.commission*100:.3f}%")
    print(f"  Slippage:        {args.slippage*100:.3f}%")
    print(f"  Total:           {(args.spread + args.commission + args.slippage)*100:.3f}%")
    print(f"Output dir:        {args.output_dir}")
    print(f"{'='*80}\n")

    try:
        # Step 1: ルール読み込み
        print("Step 1: Loading rules...")
        loader = RuleLoader(args.pair)
        pos_rules, neg_rules = loader.load_all_rules(top_n=args.top, sort_by=args.sort_by)

        if not pos_rules and not neg_rules:
            print("ERROR: No rules loaded. Exiting.")
            sys.exit(1)

        # Step 2: シグナル生成（テスト期間のみ、重複排除）
        print("Step 2: Generating signals (test period only)...")
        generator = SignalGeneratorV2(args.pair, test_start_date=args.test_start)
        generator.load_data()
        signals = generator.generate_signals(pos_rules, neg_rules, deduplicate=not args.no_dedup)

        if not signals:
            print("ERROR: No signals generated. Exiting.")
            sys.exit(1)

        # Step 3: トレードシミュレーション（コスト考慮）
        print("Step 3: Simulating trades (with transaction costs)...")
        simulator = TradeSimulatorV2(args.pair,
                                     spread=args.spread,
                                     commission=args.commission,
                                     slippage=args.slippage)
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
            trade_file = output_dir / f"{args.pair}_trades_v2.csv"
            simulator.save_trades(trade_file)

        # サマリーファイル作成
        summary_file = output_dir / f"{args.pair}_summary_v2.txt"
        with open(summary_file, 'w') as f:
            f.write(f"Optimized Simulation Summary (v2)\n")
            f.write(f"{'='*80}\n\n")
            f.write(f"Settings:\n")
            f.write(f"  Pair: {args.pair}\n")
            f.write(f"  Rules: {args.top} per direction (total: {args.top * 2})\n")
            f.write(f"  Sort: {args.sort_by}\n")
            f.write(f"  Test period: {args.test_start} onwards\n")
            f.write(f"  Deduplication: {not args.no_dedup}\n")
            f.write(f"  Transaction costs: {(args.spread + args.commission + args.slippage)*100:.3f}%\n\n")

            stats = simulator.get_basic_stats()
            f.write(f"Results:\n")
            f.write(f"  Total trades: {stats['total_trades']}\n")
            f.write(f"  Win rate: {stats['win_rate']:.2f}%\n")
            f.write(f"  Total return (before cost): {stats['total_return_before_cost']:+.3f}%\n")
            f.write(f"  Total return (after cost): {stats['total_return']:+.3f}%\n")
            f.write(f"  Cost impact: {stats['total_return_before_cost'] - stats['total_return']:+.3f}%\n")
            f.write(f"  Max drawdown: {max_dd:+.3f}%\n")

        print(f"✓ Summary saved to: {summary_file}")

        print(f"\n{'='*80}")
        print(f"Simulation Complete (Optimized v2)")
        print(f"{'='*80}")
        print(f"Results saved to: {output_dir}")
        print(f"\n⚠️  IMPORTANT COMPARISON:")
        print(f"  v1 (biased):    Trains and tests on same period → inflated results")
        print(f"  v2 (realistic): Tests only on future data → true performance")
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
