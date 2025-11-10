#!/usr/bin/env python3
"""
頻出ルール分析スクリプト
サポート率（マッチ数）が高いルールを特定し、散布図を生成
"""

import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

def analyze_rules_by_frequency(stock_code, output_dir):
    """
    各ルールをサポート率（マッチ数）でランク付け
    """

    pool_file = f"{output_dir}/{stock_code}/pool/zrp01a.txt"
    verification_dir = f"{output_dir}/{stock_code}/verification"

    if not os.path.exists(pool_file):
        print(f"Error: Pool file not found: {pool_file}")
        return None

    # プールファイルを読み込み
    df_pool = pd.read_csv(pool_file, sep='\t')

    results = []

    for idx, row in df_pool.iterrows():
        rule_id = idx + 1
        verification_file = f"{verification_dir}/rule_{rule_id:03d}.csv"

        if not os.path.exists(verification_file):
            continue

        # 検証ファイルを読み込み
        df_ver = pd.read_csv(verification_file)

        # X(t+1), X(t+2)を取得
        x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values
        x_t2 = pd.to_numeric(df_ver['X(t+2)'], errors='coerce').values

        # NaNを除外
        valid_mask = ~np.isnan(x_t1) & ~np.isnan(x_t2)
        x_t1 = x_t1[valid_mask]
        x_t2 = x_t2[valid_mask]

        if len(x_t1) == 0:
            continue

        # 統計計算
        mean_t1 = np.mean(x_t1)
        sigma_t1 = np.std(x_t1, ddof=1) if len(x_t1) > 1 else 0.0
        mean_t2 = np.mean(x_t2)
        sigma_t2 = np.std(x_t2, ddof=1) if len(x_t2) > 1 else 0.0

        # Quality Score計算
        if sigma_t1 > 0:
            quality_score = abs(mean_t1) / sigma_t1
        else:
            quality_score = 0.0

        matches = len(x_t1)

        # サポート率（全データに対する割合）
        # 株式データは5268日
        support_rate = matches / 5268.0

        results.append({
            'rule_id': rule_id,
            'matches': matches,
            'support_rate': support_rate,
            'quality_score': quality_score,
            'mean_t1': mean_t1,
            'sigma_t1': sigma_t1,
            'mean_t2': mean_t2,
            'sigma_t2': sigma_t2,
        })

    if len(results) == 0:
        return None

    # DataFrameに変換してソート（マッチ数降順）
    df_results = pd.DataFrame(results)
    df_results = df_results.sort_values('matches', ascending=False)

    return df_results


def plot_frequent_rule_x1x2(stock_code, rule_id, rule_info, output_dir):
    """
    頻出ルールのX(t+1) vs X(t+2) 散布図を作成
    """

    verification_file = f"{output_dir}/{stock_code}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        print(f"Warning: Verification file not found: {verification_file}")
        return False

    # データ読み込み
    df = pd.read_csv(verification_file)

    x_t1 = pd.to_numeric(df['X(t+1)'], errors='coerce').values
    x_t2 = pd.to_numeric(df['X(t+2)'], errors='coerce').values

    # NaNを除外
    valid_mask = ~np.isnan(x_t1) & ~np.isnan(x_t2)
    x_t1 = x_t1[valid_mask]
    x_t2 = x_t2[valid_mask]

    if len(x_t1) == 0:
        return False

    # 統計
    mean_t1 = rule_info['mean_t1']
    mean_t2 = rule_info['mean_t2']
    sigma_t1 = rule_info['sigma_t1']
    sigma_t2 = rule_info['sigma_t2']
    quality_score = rule_info['quality_score']
    matches = rule_info['matches']
    support_rate = rule_info['support_rate']

    # プロット
    fig, ax = plt.subplots(figsize=(10, 8))

    ax.scatter(x_t1, x_t2, alpha=0.8, s=80, color='blue',
               edgecolors='darkblue', linewidths=1.5, label=f'Rule Matches (n={matches})')

    # 平均線
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5, alpha=0.3)
    ax.axvline(x=0, color='black', linestyle='-', linewidth=0.5, alpha=0.3)
    ax.axhline(y=mean_t2, color='red', linestyle='--', linewidth=1.5, alpha=0.7,
               label=f'Mean X(t+2)={mean_t2:.3f}%')
    ax.axvline(x=mean_t1, color='green', linestyle='--', linewidth=1.5, alpha=0.7,
               label=f'Mean X(t+1)={mean_t1:.3f}%')

    # タイトル
    title = f"Stock {stock_code}: Rule {rule_id:03d} - X(t+1) vs X(t+2) [Frequent Rule]\n"
    title += f"μ(t+1)={mean_t1:.3f}%, σ(t+1)={sigma_t1:.3f}%, μ(t+2)={mean_t2:.3f}%, σ(t+2)={sigma_t2:.3f}%\n"
    title += f"Quality={quality_score:.3f}, Matches: {matches} (Support: {support_rate*100:.2f}%)"
    ax.set_title(title, fontsize=11, fontweight='bold')

    ax.set_xlabel('X(t+1) [%]', fontsize=10)
    ax.set_ylabel('X(t+2) [%]', fontsize=10)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    # 保存
    output_file = f"{output_dir}/{stock_code}/scatter_frequent/rule_{rule_id:03d}_x1x2.png"
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_file, dpi=100, bbox_inches='tight')
    plt.close()

    return True


def plot_frequent_rule_xt(stock_code, rule_id, rule_info, output_dir):
    """
    頻出ルールのX(t+1) vs Time 散布図を作成
    """

    verification_file = f"{output_dir}/{stock_code}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        return False

    # データ読み込み
    df = pd.read_csv(verification_file)

    x_t1 = pd.to_numeric(df['X(t+1)'], errors='coerce').values
    time_values = pd.to_datetime(df['Timestamp'], errors='coerce')

    # NaNを除外
    valid_mask = ~np.isnan(x_t1) & ~time_values.isna()
    x_t1 = x_t1[valid_mask]
    time_values = time_values[valid_mask]

    if len(x_t1) == 0:
        return False

    # 統計
    mean_t1 = rule_info['mean_t1']
    sigma_t1 = rule_info['sigma_t1']
    quality_score = rule_info['quality_score']
    matches = rule_info['matches']
    support_rate = rule_info['support_rate']

    # プロット
    fig, ax = plt.subplots(figsize=(12, 6))

    ax.scatter(time_values, x_t1, alpha=0.7, s=50, color='blue',
               edgecolors='darkblue', linewidths=1, label=f'Rule Matches (n={matches})')

    # 平均線
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5, alpha=0.3)
    ax.axhline(y=mean_t1, color='green', linestyle='--', linewidth=1.5, alpha=0.7,
               label=f'Mean X(t+1)={mean_t1:.3f}%')

    # タイトル
    title = f"Stock {stock_code}: Rule {rule_id:03d} - X(t+1) vs Time [Frequent Rule]\n"
    title += f"μ(t+1)={mean_t1:.3f}%, σ(t+1)={sigma_t1:.3f}%, Quality={quality_score:.3f}\n"
    title += f"Matches: {matches} (Support: {support_rate*100:.2f}%)"
    ax.set_title(title, fontsize=11, fontweight='bold')

    ax.set_xlabel('Time', fontsize=10)
    ax.set_ylabel('X(t+1) [%]', fontsize=10)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    # X軸フォーマット
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
    ax.xaxis.set_major_locator(mdates.YearLocator(2))
    plt.xticks(rotation=45)

    # 保存
    output_file = f"{output_dir}/{stock_code}/scatter_frequent/rule_{rule_id:03d}_xt.png"
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_file, dpi=100, bbox_inches='tight')
    plt.close()

    return True


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_frequent_rules.py <stock_code> [num_rules] [min_matches]")
        print("Example: python3 analyze_frequent_rules.py 3436 10 15")
        sys.exit(1)

    stock_code = sys.argv[1]
    num_rules = int(sys.argv[2]) if len(sys.argv) >= 3 else 10
    min_matches = int(sys.argv[3]) if len(sys.argv) >= 4 else 10
    output_dir = "output"

    print("="*70)
    print(f"Frequent Rule Analysis for Stock {stock_code}")
    print("="*70)
    print()

    # ルール分析
    print(f"Analyzing rules by frequency (min_matches >= {min_matches})...")
    df_rules = analyze_rules_by_frequency(stock_code, output_dir)

    if df_rules is None or len(df_rules) == 0:
        print(f"Error: No rules found for stock {stock_code}")
        sys.exit(1)

    # 最小マッチ数でフィルタ
    df_rules = df_rules[df_rules['matches'] >= min_matches]

    print(f"Found {len(df_rules)} rules with >= {min_matches} matches")
    print()

    if len(df_rules) == 0:
        print(f"No rules found with >= {min_matches} matches")
        print("Try lowering min_matches parameter")
        sys.exit(1)

    # 統計サマリー
    print("="*70)
    print("Frequency Statistics")
    print("="*70)
    print(f"Total rules: {len(df_rules)}")
    print(f"Max matches: {df_rules['matches'].max()}")
    print(f"Mean matches: {df_rules['matches'].mean():.1f}")
    print(f"Median matches: {df_rules['matches'].median():.1f}")
    print(f"Max support rate: {df_rules['support_rate'].max()*100:.2f}%")
    print()

    # Top N ルールを選択
    top_rules = df_rules.head(num_rules)

    print(f"Generating scatter plots for top {num_rules} frequent rules...")
    print("-"*70)

    success_count = 0

    for idx, row in top_rules.iterrows():
        rule_id = int(row['rule_id'])

        print(f"Rule {rule_id:03d}: Matches={int(row['matches'])} ({row['support_rate']*100:.2f}%), "
              f"Quality={row['quality_score']:.3f}, "
              f"μ(t+1)={row['mean_t1']:.3f}%, σ(t+1)={row['sigma_t1']:.3f}%")

        # X(t+1) vs X(t+2) 散布図
        if plot_frequent_rule_x1x2(stock_code, rule_id, row, output_dir):
            print(f"  ✓ X(t+1) vs X(t+2) plot created")

        # X(t+1) vs Time 散布図
        if plot_frequent_rule_xt(stock_code, rule_id, row, output_dir):
            print(f"  ✓ X(t+1) vs Time plot created")
            success_count += 1

        print()

    print("="*70)
    print(f"✓ Frequent rule analysis complete")
    print(f"  Successfully created plots for {success_count}/{num_rules} rules")
    print(f"  Output directory: {output_dir}/{stock_code}/scatter_frequent/")
    print("="*70)

    # サマリーCSVを作成
    summary_file = f"{output_dir}/{stock_code}/scatter_frequent/top{num_rules}_frequent_summary.csv"
    top_rules.to_csv(summary_file, index=False)
    print(f"  Summary saved: {summary_file}")
    print()

    # 品質 vs 頻度の分析
    print("="*70)
    print("Quality vs Frequency Trade-off")
    print("="*70)
    for idx, row in top_rules.head(5).iterrows():
        quality_rank = df_rules[df_rules['quality_score'] >= row['quality_score']].shape[0]
        print(f"Rule {int(row['rule_id']):03d}: Frequency Rank #{idx+1}, "
              f"Quality Rank ~#{quality_rank}, Trade-off Score: {row['quality_score'] * row['support_rate'] * 1000:.2f}")


if __name__ == "__main__":
    main()
