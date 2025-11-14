#!/usr/bin/env python3
"""
実験結果分析スクリプト

experiments/YYYYMMDD_HHMMSS/experiment_summary.csv を読み込み、
各パラメータとルール発見数の関係を分析する
"""

import pandas as pd
import sys
import os
from pathlib import Path

def analyze_experiments(summary_csv_path):
    """実験結果を分析"""

    # CSVファイル読み込み
    if not os.path.exists(summary_csv_path):
        print(f"Error: {summary_csv_path} が見つかりません")
        return 1

    df = pd.read_csv(summary_csv_path)

    print("=" * 80)
    print("実験結果分析")
    print("=" * 80)
    print(f"\n入力ファイル: {summary_csv_path}")
    print(f"総実験数: {len(df)}")
    print(f"成功実験数: {len(df[df['Exit_Code'] == 0])}")
    print()

    # 基本統計
    print("=" * 80)
    print("ルール発見数の基本統計")
    print("=" * 80)
    print(f"平均: {df['Rules_Discovered'].mean():.1f}")
    print(f"中央値: {df['Rules_Discovered'].median():.1f}")
    print(f"最小値: {df['Rules_Discovered'].min()}")
    print(f"最大値: {df['Rules_Discovered'].max()}")
    print(f"標準偏差: {df['Rules_Discovered'].std():.1f}")
    print()

    # Minsupごとの集計
    print("=" * 80)
    print("Minsup別の平均ルール発見数")
    print("=" * 80)
    minsup_summary = df.groupby('Minsup')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max'])
    print(minsup_summary)
    print()

    # MIN_CONCENTRATIONごとの集計
    print("=" * 80)
    print("MIN_CONCENTRATION別の平均ルール発見数")
    print("=" * 80)
    conc_summary = df.groupby('MIN_CONCENTRATION')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max'])
    print(conc_summary)
    print()

    # MAX_DEVIATIONごとの集計
    print("=" * 80)
    print("MAX_DEVIATION別の平均ルール発見数")
    print("=" * 80)
    dev_summary = df.groupby('MAX_DEVIATION')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max'])
    print(dev_summary)
    print()

    # Top 5実験
    print("=" * 80)
    print("Top 5実験（ルール発見数順）")
    print("=" * 80)
    top5 = df.nlargest(5, 'Rules_Discovered')[['Experiment', 'Minsup', 'MIN_CONCENTRATION', 'MAX_DEVIATION', 'Rules_Discovered']]
    print(top5.to_string(index=False))
    print()

    # Bottom 5実験
    print("=" * 80)
    print("Bottom 5実験（ルール発見数順）")
    print("=" * 80)
    bottom5 = df.nsmallest(5, 'Rules_Discovered')[['Experiment', 'Minsup', 'MIN_CONCENTRATION', 'MAX_DEVIATION', 'Rules_Discovered']]
    print(bottom5.to_string(index=False))
    print()

    # パラメータ組み合わせ別の詳細
    print("=" * 80)
    print("全実験の詳細（ルール発見数降順）")
    print("=" * 80)
    detailed = df[['Experiment', 'Minsup', 'MIN_CONCENTRATION', 'MAX_DEVIATION', 'Rules_Discovered', 'Duration_Sec']].sort_values('Rules_Discovered', ascending=False)
    print(detailed.to_string(index=False))
    print()

    # 実行時間の統計
    print("=" * 80)
    print("実行時間統計")
    print("=" * 80)
    print(f"平均実行時間: {df['Duration_Sec'].mean():.1f}秒")
    print(f"最短実行時間: {df['Duration_Sec'].min()}秒")
    print(f"最長実行時間: {df['Duration_Sec'].max()}秒")
    print(f"総実行時間: {df['Duration_Sec'].sum():.0f}秒 ({df['Duration_Sec'].sum()/60:.1f}分)")
    print()

    return 0

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_experiments.py <experiment_summary.csv>")
        print()
        print("Example:")
        print("  python3 analyze_experiments.py experiments/20251114_120000/experiment_summary.csv")
        return 1

    summary_csv = sys.argv[1]
    return analyze_experiments(summary_csv)

if __name__ == '__main__':
    sys.exit(main())
