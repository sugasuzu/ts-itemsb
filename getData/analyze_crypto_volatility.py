#!/usr/bin/env python3
"""
仮想通貨ボラティリティ分析スクリプト
各ペアの変化率分布を分析し、高ボラティリティペアを特定
"""

import pandas as pd
import numpy as np
import argparse
import os

def analyze_volatility(input_file, output_report=None):
    """
    ボラティリティを分析

    Parameters:
    -----------
    input_file : str
        入力CSVファイル（crypto_raw.csvなど）
    output_report : str
        出力レポートファイル（Noneの場合は標準出力のみ）

    Returns:
    --------
    pd.DataFrame
        統計情報を含むDataFrame
    """
    print(f"=== Cryptocurrency Volatility Analysis ===")
    print(f"Input file: {input_file}")
    print()

    # データ読み込み
    df = pd.read_csv(input_file)
    print(f"Loaded {len(df)} records")

    # timestampカラムを除外
    price_columns = [col for col in df.columns if col not in ['timestamp', 'T']]
    print(f"Found {len(price_columns)} cryptocurrency pairs")
    print()

    # 統計情報を格納する辞書
    stats_list = []

    print("=== Calculating Returns and Statistics ===")
    for col in price_columns:
        if col not in df.columns:
            continue

        # 日次変化率を計算（%）
        returns = df[col].pct_change() * 100
        returns = returns.dropna()

        if len(returns) == 0:
            continue

        # 統計量を計算
        stats = {
            'Pair': col,
            'Mean(%)': returns.mean(),
            'Std(%)': returns.std(),
            'Variance': returns.var(),
            'Min(%)': returns.min(),
            'Max(%)': returns.max(),
            'Median(%)': returns.median(),
            'Q25(%)': returns.quantile(0.25),
            'Q75(%)': returns.quantile(0.75),
            'Within_±1%': (returns.abs() <= 1.0).sum() / len(returns) * 100,
            'Within_±2%': (returns.abs() <= 2.0).sum() / len(returns) * 100,
            'Within_±5%': (returns.abs() <= 5.0).sum() / len(returns) * 100,
            'Records': len(returns),
        }

        stats_list.append(stats)
        print(f"  {col}: std={stats['Std(%)']:.2f}%, var={stats['Variance']:.1f}, ±1%={stats['Within_±1%']:.1f}%")

    # DataFrameに変換
    stats_df = pd.DataFrame(stats_list)

    # 標準偏差でソート（降順）
    stats_df = stats_df.sort_values('Std(%)', ascending=False)

    print()
    print("=== Volatility Ranking (Top 10) ===")
    print(stats_df.head(10).to_string(index=False))

    # レポート出力
    if output_report:
        with open(output_report, 'w') as f:
            f.write("=== Cryptocurrency Volatility Analysis Report ===\n\n")
            f.write(f"Analysis Date: {pd.Timestamp.now()}\n")
            f.write(f"Input File: {input_file}\n")
            f.write(f"Total Pairs Analyzed: {len(stats_df)}\n\n")

            f.write("=== Top 10 High Volatility Pairs ===\n")
            f.write(stats_df.head(10).to_string(index=False))
            f.write("\n\n")

            f.write("=== All Pairs Statistics ===\n")
            f.write(stats_df.to_string(index=False))
            f.write("\n\n")

            # サマリー統計
            f.write("=== Summary Statistics ===\n")
            f.write(f"Average Std Deviation: {stats_df['Std(%)'].mean():.2f}%\n")
            f.write(f"Median Std Deviation: {stats_df['Std(%)'].median():.2f}%\n")
            f.write(f"Max Std Deviation: {stats_df['Std(%)'].max():.2f}% ({stats_df.iloc[0]['Pair']})\n")
            f.write(f"Min Std Deviation: {stats_df['Std(%)'].min():.2f}% ({stats_df.iloc[-1]['Pair']})\n\n")

            # 参考：株価・為替との比較
            f.write("=== Comparison with Traditional Assets ===\n")
            f.write("(For reference)\n")
            f.write("Stock (Nikkei225 avg): std≈0.7%, var≈0.5, within±1%≈93%\n")
            f.write("Forex (USDJPY): std≈0.7%, var≈0.5, within±1%≈94%\n")
            f.write("Forex (EURUSD): std≈0.6%, var≈0.3, within±1%≈96%\n\n")

            # 推奨ペア
            f.write("=== Recommended Pairs for GNMiner ===\n")
            f.write("(High volatility = easier pattern discovery)\n\n")
            recommended = stats_df[stats_df['Std(%)'] >= 3.0].head(10)
            for idx, row in recommended.iterrows():
                f.write(f"{row['Pair']}: std={row['Std(%)']:.2f}%, ")
                f.write(f"var={row['Variance']:.1f}, ")
                f.write(f"±1%={row['Within_±1%']:.1f}%\n")

        print(f"\n✓ Report saved to: {output_report}")

    return stats_df

def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Analyze cryptocurrency volatility'
    )
    parser.add_argument(
        'input_file',
        nargs='?',
        default='../crypto_data/crypto_raw.csv',
        help='Input CSV file (default: ../crypto_data/crypto_raw.csv)'
    )
    parser.add_argument(
        '-o', '--output',
        default='../crypto_data/volatility_analysis.txt',
        help='Output report file (default: ../crypto_data/volatility_analysis.txt)'
    )

    args = parser.parse_args()

    # 分析実行
    stats_df = analyze_volatility(args.input_file, args.output)

    print()
    print("=== Analysis Complete! ===")
    print(f"Analyzed {len(stats_df)} cryptocurrency pairs")
    print(f"Highest volatility: {stats_df.iloc[0]['Pair']} (std={stats_df.iloc[0]['Std(%)']:.2f}%)")
    print(f"Average volatility: {stats_df['Std(%)'].mean():.2f}%")

if __name__ == '__main__':
    main()
