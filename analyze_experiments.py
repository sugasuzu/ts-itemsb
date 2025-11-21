#!/usr/bin/env python3
"""
実験結果分析スクリプト（v2.0 - MAX_TIME_DELAY対応）

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
    print("実験結果分析 v2.0")
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

    # MAX_TIME_DELAYごとの集計（新規追加）
    print("=" * 80)
    print("MAX_TIME_DELAY別の平均ルール発見数")
    print("=" * 80)
    delay_summary = df.groupby('MAX_TIME_DELAY')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max', 'count'])
    print(delay_summary)
    print()

    # Minsupごとの集計
    print("=" * 80)
    print("Minsup別の平均ルール発見数")
    print("=" * 80)
    minsup_summary = df.groupby('Minsup')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max', 'count'])
    print(minsup_summary)
    print()

    # MIN_CONCENTRATIONごとの集計
    print("=" * 80)
    print("MIN_CONCENTRATION別の平均ルール発見数")
    print("=" * 80)
    conc_summary = df.groupby('MIN_CONCENTRATION')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max', 'count'])
    print(conc_summary)
    print()

    # MAX_DEVIATIONごとの集計
    print("=" * 80)
    print("MAX_DEVIATION別の平均ルール発見数")
    print("=" * 80)
    dev_summary = df.groupby('MAX_DEVIATION')['Rules_Discovered'].agg(['mean', 'std', 'min', 'max', 'count'])
    print(dev_summary)
    print()

    # 2次元クロス集計（新規追加）
    print("=" * 80)
    print("2次元クロス集計: MAX_TIME_DELAY × Minsup")
    print("=" * 80)
    cross_delay_minsup = df.pivot_table(values='Rules_Discovered',
                                         index='MAX_TIME_DELAY',
                                         columns='Minsup',
                                         aggfunc='mean')
    print(cross_delay_minsup)
    print()

    print("=" * 80)
    print("2次元クロス集計: MIN_CONCENTRATION × MAX_DEVIATION")
    print("=" * 80)
    cross_conc_dev = df.pivot_table(values='Rules_Discovered',
                                     index='MIN_CONCENTRATION',
                                     columns='MAX_DEVIATION',
                                     aggfunc='mean')
    print(cross_conc_dev)
    print()

    # Top 10実験
    print("=" * 80)
    print("Top 10実験（ルール発見数順）")
    print("=" * 80)
    top10 = df.nlargest(10, 'Rules_Discovered')[['Experiment', 'MAX_TIME_DELAY', 'Minsup', 'MIN_CONCENTRATION', 'MAX_DEVIATION', 'Rules_Discovered']]
    print(top10.to_string(index=False))
    print()

    # Bottom 10実験
    print("=" * 80)
    print("Bottom 10実験（ルール発見数順）")
    print("=" * 80)
    bottom10 = df.nsmallest(10, 'Rules_Discovered')[['Experiment', 'MAX_TIME_DELAY', 'Minsup', 'MIN_CONCENTRATION', 'MAX_DEVIATION', 'Rules_Discovered']]
    print(bottom10.to_string(index=False))
    print()

    # パラメータ組み合わせ別の詳細（上位30件のみ表示）
    print("=" * 80)
    print("全実験の詳細（ルール発見数降順、上位30件）")
    print("=" * 80)
    detailed = df[['Experiment', 'MAX_TIME_DELAY', 'Minsup', 'MIN_CONCENTRATION', 'MAX_DEVIATION', 'Rules_Discovered', 'Duration_Sec']].sort_values('Rules_Discovered', ascending=False)
    print(detailed.head(30).to_string(index=False))
    if len(detailed) > 30:
        print(f"\n... 他 {len(detailed) - 30} 件")
    print()

    # 実行時間の統計
    print("=" * 80)
    print("実行時間統計")
    print("=" * 80)
    print(f"平均実行時間: {df['Duration_Sec'].mean():.1f}秒 ({df['Duration_Sec'].mean()/60:.1f}分)")
    print(f"最短実行時間: {df['Duration_Sec'].min()}秒")
    print(f"最長実行時間: {df['Duration_Sec'].max()}秒")
    total_duration = df['Duration_Sec'].sum()
    total_hours = total_duration / 3600
    total_mins = (total_duration % 3600) / 60
    print(f"総実行時間: {total_duration:.0f}秒 ({total_hours:.1f}時間 = {total_hours:.0f}時間{total_mins:.0f}分)")
    print()

    # 最適パラメータの推奨
    print("=" * 80)
    print("推奨パラメータ（ルール発見数が最大の設定）")
    print("=" * 80)
    best_exp = df.loc[df['Rules_Discovered'].idxmax()]
    print(f"実験名: {best_exp['Experiment']}")
    print(f"MAX_TIME_DELAY:    {best_exp['MAX_TIME_DELAY']}")
    print(f"Minsup:            {best_exp['Minsup']}")
    print(f"MIN_CONCENTRATION: {best_exp['MIN_CONCENTRATION']}")
    print(f"MAX_DEVIATION:     {best_exp['MAX_DEVIATION']}")
    print(f"ルール発見数:      {best_exp['Rules_Discovered']}")
    print(f"実行時間:          {best_exp['Duration_Sec']}秒 ({best_exp['Duration_Sec']/60:.1f}分)")
    print()

    return 0

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 analyze_experiments.py <experiment_directory_or_csv>")
        print()
        print("Example:")
        print("  python3 analyze_experiments.py experiments/20251114_120000/")
        print("  python3 analyze_experiments.py experiments/20251114_120000/experiment_summary.csv")
        return 1

    input_path = sys.argv[1]

    # ディレクトリが指定された場合は、その中のexperiment_summary.csvを探す
    if os.path.isdir(input_path):
        summary_csv = os.path.join(input_path, 'experiment_summary.csv')
    else:
        summary_csv = input_path

    return analyze_experiments(summary_csv)

if __name__ == '__main__':
    sys.exit(main())
