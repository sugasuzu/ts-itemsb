#!/usr/bin/env python3
"""
株式データ用の散布図作成スクリプト
X(t+1) vs X(t+2) および X(t+1) vs Time の散布図を生成
"""

import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from pathlib import Path

def analyze_rules_for_xt1_quality(stock_code, output_dir):
    """
    各ルールのX(t+1)品質スコアを計算（X(t+2)を使わない）
    Quality Score = |mean(t+1)| / sigma(t+1)
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

        # X(t+1)のみを取得
        x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values

        # NaNを除外
        x_t1 = x_t1[~np.isnan(x_t1)]

        if len(x_t1) == 0:
            continue

        # 統計計算
        mean_t1 = np.mean(x_t1)
        sigma_t1 = np.std(x_t1, ddof=1) if len(x_t1) > 1 else 0.0

        # Quality Score計算
        if sigma_t1 > 0:
            quality_score = abs(mean_t1) / sigma_t1
        else:
            quality_score = 0.0

        actual_matches = len(x_t1)

        results.append({
            'rule_id': rule_id,
            'quality_score': quality_score,
            'mean_t1': mean_t1,
            'sigma_t1': sigma_t1,
            'matches': actual_matches,
        })

    if len(results) == 0:
        return None

    # DataFrameに変換してソート
    df_results = pd.DataFrame(results)
    df_results = df_results.sort_values('quality_score', ascending=False)

    return df_results


def plot_rule_x1x2_scatter(stock_code, rule_id, rule_info, output_dir):
    """
    X(t+1) vs X(t+2) 散布図を作成
    """

    verification_file = f"{output_dir}/{stock_code}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        print(f"Warning: Verification file not found: {verification_file}")
        return False

    # ルールマッチデータ読み込み
    df_rule = pd.read_csv(verification_file)

    x_t1_rule = pd.to_numeric(df_rule['X(t+1)'], errors='coerce').values
    x_t2_rule = pd.to_numeric(df_rule['X(t+2)'], errors='coerce').values

    # NaNを除外
    valid_mask = ~np.isnan(x_t1_rule) & ~np.isnan(x_t2_rule)
    x_t1_rule = x_t1_rule[valid_mask]
    x_t2_rule = x_t2_rule[valid_mask]

    if len(x_t1_rule) == 0:
        print(f"Warning: No valid data for rule {rule_id}")
        return False

    # 統計計算
    mean_t1 = np.mean(x_t1_rule)
    mean_t2 = np.mean(x_t2_rule)
    sigma_t1 = np.std(x_t1_rule, ddof=1) if len(x_t1_rule) > 1 else 0.0
    sigma_t2 = np.std(x_t2_rule, ddof=1) if len(x_t2_rule) > 1 else 0.0
    quality_score = rule_info['quality_score']

    # プロット作成
    fig, ax = plt.subplots(figsize=(10, 8))

    # ルールマッチのみをプロット（全データは描画しない）
    ax.scatter(x_t1_rule, x_t2_rule, alpha=0.8, s=80, color='red',
               edgecolors='darkred', linewidths=1.5, label=f'Rule Matches (n={len(x_t1_rule)})')

    # 平均線
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5, alpha=0.3)
    ax.axvline(x=0, color='black', linestyle='-', linewidth=0.5, alpha=0.3)
    ax.axhline(y=mean_t2, color='blue', linestyle='--', linewidth=1.5, alpha=0.7, label=f'Mean X(t+2)={mean_t2:.3f}%')
    ax.axvline(x=mean_t1, color='green', linestyle='--', linewidth=1.5, alpha=0.7, label=f'Mean X(t+1)={mean_t1:.3f}%')

    # タイトル
    title = f"Stock {stock_code}: Rule {rule_id:03d} - X(t+1) vs X(t+2)\n"
    title += f"μ(t+1)={mean_t1:.3f}%, σ(t+1)={sigma_t1:.3f}%, μ(t+2)={mean_t2:.3f}%, σ(t+2)={sigma_t2:.3f}%\n"
    title += f"Quality Score={quality_score:.3f}, Matches: {rule_info['matches']}"
    ax.set_title(title, fontsize=11, fontweight='bold')

    ax.set_xlabel('X(t+1) [%]', fontsize=10)
    ax.set_ylabel('X(t+2) [%]', fontsize=10)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    # 保存
    output_file = f"{output_dir}/{stock_code}/scatter/rule_{rule_id:03d}_x1x2.png"
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_file, dpi=100, bbox_inches='tight')
    plt.close()

    return True


def plot_rule_xt_scatter(stock_code, rule_id, rule_info, output_dir):
    """
    X(t+1) vs Time 散布図を作成
    """

    verification_file = f"{output_dir}/{stock_code}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        print(f"Warning: Verification file not found: {verification_file}")
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
        print(f"Warning: No valid data for rule {rule_id}")
        return False

    # 統計計算
    mean_t1 = np.mean(x_t1)
    sigma_t1 = np.std(x_t1, ddof=1) if len(x_t1) > 1 else 0.0
    quality_score = rule_info['quality_score']

    # プロット作成
    fig, ax = plt.subplots(figsize=(12, 6))

    # ルールマッチ（赤）
    ax.scatter(time_values, x_t1, alpha=0.7, s=50, color='red',
               edgecolors='darkred', linewidths=1, label=f'Rule Matches (n={len(x_t1)})')

    # 平均線
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.5, alpha=0.3)
    ax.axhline(y=mean_t1, color='green', linestyle='--', linewidth=1.5, alpha=0.7,
               label=f'Mean X(t+1)={mean_t1:.3f}%')

    # タイトル
    title = f"Stock {stock_code}: Rule {rule_id:03d} - X(t+1) vs Time\n"
    title += f"μ(t+1)={mean_t1:.3f}%, σ(t+1)={sigma_t1:.3f}%, "
    title += f"Quality Score={quality_score:.3f}\n"
    title += f"Matches: {rule_info['matches']}"
    ax.set_title(title, fontsize=11, fontweight='bold')

    ax.set_xlabel('Time', fontsize=10)
    ax.set_ylabel('X(t+1) [%]', fontsize=10)
    ax.legend(loc='best', fontsize=9)
    ax.grid(True, alpha=0.3)

    # X軸のフォーマット
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
    ax.xaxis.set_major_locator(mdates.YearLocator(2))
    plt.xticks(rotation=45)

    # 保存
    output_file = f"{output_dir}/{stock_code}/scatter/rule_{rule_id:03d}_xt.png"
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    plt.tight_layout()
    plt.savefig(output_file, dpi=100, bbox_inches='tight')
    plt.close()

    return True


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 create_scatter_plots.py <stock_code> [num_rules]")
        print("Example: python3 create_scatter_plots.py 3436 10")
        sys.exit(1)

    stock_code = sys.argv[1]
    num_rules = int(sys.argv[2]) if len(sys.argv) >= 3 else 10
    output_dir = "output"

    print("="*70)
    print(f"Scatter Plot Generation for Stock {stock_code}")
    print("="*70)
    print()

    # ルールの品質分析
    print(f"Analyzing rules for stock {stock_code}...")
    df_rules = analyze_rules_for_xt1_quality(stock_code, output_dir)

    if df_rules is None or len(df_rules) == 0:
        print(f"Error: No rules found for stock {stock_code}")
        sys.exit(1)

    print(f"Found {len(df_rules)} rules")
    print()

    # Top N ルールを選択
    top_rules = df_rules.head(num_rules)

    print(f"Generating scatter plots for top {num_rules} rules...")
    print("-"*70)

    success_count = 0

    for idx, row in top_rules.iterrows():
        rule_id = int(row['rule_id'])

        print(f"Rule {rule_id:03d}: Quality={row['quality_score']:.3f}, "
              f"μ(t+1)={row['mean_t1']:.3f}%, σ(t+1)={row['sigma_t1']:.3f}%, "
              f"n={int(row['matches'])}")

        # X(t+1) vs X(t+2) 散布図
        if plot_rule_x1x2_scatter(stock_code, rule_id, row, output_dir):
            print(f"  ✓ X(t+1) vs X(t+2) plot created")

        # X(t+1) vs Time 散布図
        if plot_rule_xt_scatter(stock_code, rule_id, row, output_dir):
            print(f"  ✓ X(t+1) vs Time plot created")
            success_count += 1

        print()

    print("="*70)
    print(f"✓ Scatter plot generation complete")
    print(f"  Successfully created plots for {success_count}/{num_rules} rules")
    print(f"  Output directory: {output_dir}/{stock_code}/scatter/")
    print("="*70)

    # サマリーCSVを作成
    summary_file = f"{output_dir}/{stock_code}/scatter/top{num_rules}_summary.csv"
    top_rules.to_csv(summary_file, index=False)
    print(f"  Summary saved: {summary_file}")


if __name__ == "__main__":
    main()
