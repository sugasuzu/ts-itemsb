#!/usr/bin/env python3
"""
全通貨のルールプール分析スクリプト

目的:
- 各通貨の発見ルール数を集計
- 集中度、|mean|、σ、サポート率の統計を計算
- 最高品質ルールを抽出
- 通貨間の比較を行う
"""

import pandas as pd
import numpy as np
from pathlib import Path
import sys

# 基準ディレクトリ
BASE_DIR = Path("/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/1-deta-enginnering/crypto_data_hourly")
OUTPUT_DIR = BASE_DIR / "output"

# 対象通貨
SYMBOLS = ["AAVE", "ADA", "ALGO", "ATOM", "AVAX", "BNB", "BTC", "DOGE", "DOT", "ETC", "ETH"]

def load_pool_file(symbol):
    """ルールプールファイルを読み込む"""
    pool_file = OUTPUT_DIR / symbol / "pool" / "zrp01a.txt"

    if not pool_file.exists():
        return None

    try:
        df = pd.read_csv(pool_file, sep='\t')
        return df
    except Exception as e:
        print(f"Error loading {symbol}: {e}", file=sys.stderr)
        return None

def calculate_concentration(verification_dir, rule_pool_df):
    """各ルールの象限集中度を計算"""
    concentrations = []

    for idx, row in rule_pool_df.iterrows():
        # verification fileから集中度を計算
        # ここでは簡易的にルールプールの情報から推定
        # 実際にはverificationファイルを読む必要がある
        concentrations.append(0.0)  # プレースホルダー

    return concentrations

def analyze_currency(symbol):
    """通貨ごとの分析"""
    df = load_pool_file(symbol)

    if df is None or len(df) == 0:
        return None

    # 基本統計
    stats = {
        'symbol': symbol,
        'rule_count': len(df),
        'mean_t1_mean': df['X(t+1)_mean'].abs().mean(),
        'mean_t1_max': df['X(t+1)_mean'].abs().max(),
        'mean_t2_mean': df['X(t+2)_mean'].abs().mean(),
        'mean_t2_max': df['X(t+2)_mean'].abs().max(),
        'sigma_t1_mean': df['X(t+1)_sigma'].mean(),
        'sigma_t1_min': df['X(t+1)_sigma'].min(),
        'sigma_t2_mean': df['X(t+2)_sigma'].mean(),
        'sigma_t2_min': df['X(t+2)_sigma'].min(),
        'support_count_mean': df['support_count'].mean(),
        'support_count_max': df['support_count'].max(),
        'support_rate_mean': df['support_rate'].mean(),
        'support_rate_max': df['support_rate'].max(),
    }

    # max|mean|を計算（t+1とt+2の大きい方）
    df['max_abs_mean'] = df[['X(t+1)_mean', 'X(t+2)_mean']].abs().max(axis=1)
    stats['max_abs_mean_overall'] = df['max_abs_mean'].max()

    # |mean|≥0.5%のルール数
    stats['high_mean_count'] = (df['max_abs_mean'] >= 0.5).sum()

    # σ<0.3%のルール数
    stats['low_sigma_count'] = ((df['X(t+1)_sigma'] < 0.3) & (df['X(t+2)_sigma'] < 0.3)).sum()

    # サポート≥50マッチのルール数
    stats['high_support_count'] = (df['support_count'] >= 50).sum()

    return stats

def main():
    print("=" * 80)
    print("全通貨ルールプール分析")
    print("=" * 80)
    print()

    all_stats = []

    for symbol in SYMBOLS:
        print(f"Analyzing {symbol}...", end=" ")
        stats = analyze_currency(symbol)

        if stats is None:
            print("No data")
            continue

        all_stats.append(stats)
        print(f"{stats['rule_count']} rules")

    if not all_stats:
        print("\nNo data found!")
        return

    # DataFrameに変換
    df_stats = pd.DataFrame(all_stats)

    # 結果を表示
    print("\n" + "=" * 80)
    print("発見ルール数ランキング")
    print("=" * 80)
    df_sorted = df_stats.sort_values('rule_count', ascending=False)
    print(df_sorted[['symbol', 'rule_count']].to_string(index=False))

    print("\n" + "=" * 80)
    print("最高|mean|ランキング")
    print("=" * 80)
    df_sorted = df_stats.sort_values('max_abs_mean_overall', ascending=False)
    print(df_sorted[['symbol', 'max_abs_mean_overall', 'high_mean_count']].to_string(index=False))

    print("\n" + "=" * 80)
    print("最小σランキング")
    print("=" * 80)
    df_sorted = df_stats.sort_values('sigma_t1_min', ascending=True)
    print(df_sorted[['symbol', 'sigma_t1_min', 'sigma_t2_min', 'low_sigma_count']].to_string(index=False))

    print("\n" + "=" * 80)
    print("最高サポート率ランキング")
    print("=" * 80)
    df_sorted = df_stats.sort_values('support_count_max', ascending=False)
    print(df_sorted[['symbol', 'support_count_max', 'support_rate_max', 'high_support_count']].to_string(index=False))

    print("\n" + "=" * 80)
    print("統計サマリー（全通貨平均）")
    print("=" * 80)
    print(f"総ルール数: {df_stats['rule_count'].sum():.0f}")
    print(f"平均ルール数: {df_stats['rule_count'].mean():.1f}")
    print(f"平均|mean|(t+1): {df_stats['mean_t1_mean'].mean():.3f}%")
    print(f"平均|mean|(t+2): {df_stats['mean_t2_mean'].mean():.3f}%")
    print(f"平均σ(t+1): {df_stats['sigma_t1_mean'].mean():.3f}%")
    print(f"平均σ(t+2): {df_stats['sigma_t2_mean'].mean():.3f}%")
    print(f"平均サポートカウント: {df_stats['support_count_mean'].mean():.1f}")
    print(f"平均サポート率: {df_stats['support_rate_mean'].mean():.4f}")

    print("\n" + "=" * 80)
    print("品質基準達成状況")
    print("=" * 80)
    print(f"|mean|≥0.5%のルール数: {df_stats['high_mean_count'].sum():.0f}")
    print(f"σ<0.3%のルール数: {df_stats['low_sigma_count'].sum():.0f}")
    print(f"サポート≥50マッチのルール数: {df_stats['high_support_count'].sum():.0f}")

    # CSVに保存
    output_file = BASE_DIR / "output" / "currency_analysis_summary.csv"
    df_stats.to_csv(output_file, index=False)
    print(f"\n結果を保存: {output_file}")

    # 詳細なランキング表を作成
    print("\n" + "=" * 80)
    print("総合ランキング（複合スコア）")
    print("=" * 80)

    # 複合スコア = 正規化した(ルール数 + 高mean数 + 低σ数 + 高サポート数)
    df_stats['composite_score'] = (
        (df_stats['rule_count'] / df_stats['rule_count'].max()) * 0.2 +
        (df_stats['high_mean_count'] / df_stats['high_mean_count'].max()) * 0.3 +
        (df_stats['low_sigma_count'] / df_stats['low_sigma_count'].max()) * 0.2 +
        (df_stats['high_support_count'] / df_stats['high_support_count'].max()) * 0.3
    ) * 100

    df_sorted = df_stats.sort_values('composite_score', ascending=False)
    print(df_sorted[['symbol', 'composite_score', 'rule_count',
                     'high_mean_count', 'low_sigma_count', 'high_support_count']].to_string(index=False))

if __name__ == "__main__":
    main()
