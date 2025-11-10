#!/usr/bin/env python3
"""
ルールタイプ比較レポート生成スクリプト
高品質ルール vs 頻出ルール vs バランス型ルールの比較分析
"""

import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

def analyze_all_rules(stock_code, output_dir):
    """
    全ルールを分析し、品質スコアと頻度の両方を計算
    """

    pool_file = f"{output_dir}/{stock_code}/pool/zrp01a.txt"
    verification_dir = f"{output_dir}/{stock_code}/verification"

    if not os.path.exists(pool_file):
        print(f"Error: Pool file not found: {pool_file}")
        return None

    df_pool = pd.read_csv(pool_file, sep='\t')
    results = []

    for idx, row in df_pool.iterrows():
        rule_id = idx + 1
        verification_file = f"{verification_dir}/rule_{rule_id:03d}.csv"

        if not os.path.exists(verification_file):
            continue

        df_ver = pd.read_csv(verification_file)

        x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values
        x_t2 = pd.to_numeric(df_ver['X(t+2)'], errors='coerce').values

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

        # Quality Score
        if sigma_t1 > 0:
            quality_score = abs(mean_t1) / sigma_t1
        else:
            quality_score = 0.0

        matches = len(x_t1)
        support_rate = matches / 5268.0

        # 実用性スコア（Quality × Support Rate × 1000）
        practicality_score = quality_score * support_rate * 1000

        results.append({
            'rule_id': rule_id,
            'matches': matches,
            'support_rate': support_rate,
            'quality_score': quality_score,
            'practicality_score': practicality_score,
            'mean_t1': mean_t1,
            'sigma_t1': sigma_t1,
            'mean_t2': mean_t2,
            'sigma_t2': sigma_t2,
        })

    df = pd.DataFrame(results)
    return df


def classify_rules(df):
    """
    ルールを3つのタイプに分類
    """

    # 閾値設定
    HIGH_QUALITY_THRESHOLD = 0.6      # Quality Score >= 0.6
    HIGH_FREQUENCY_THRESHOLD = 20     # Matches >= 20
    BALANCED_QUALITY_MIN = 0.3        # 0.3 <= Quality Score < 0.6
    BALANCED_FREQUENCY_MIN = 15       # Matches >= 15

    df['rule_type'] = 'Other'

    # 高品質ルール
    high_quality_mask = df['quality_score'] >= HIGH_QUALITY_THRESHOLD
    df.loc[high_quality_mask, 'rule_type'] = 'High-Quality'

    # 頻出ルール（高品質でない）
    high_frequency_mask = (
        (df['matches'] >= HIGH_FREQUENCY_THRESHOLD) &
        (df['quality_score'] < HIGH_QUALITY_THRESHOLD)
    )
    df.loc[high_frequency_mask, 'rule_type'] = 'High-Frequency'

    # バランス型ルール（高品質でも頻出でもない、中間レンジ）
    balanced_mask = (
        (df['quality_score'] >= BALANCED_QUALITY_MIN) &
        (df['quality_score'] < HIGH_QUALITY_THRESHOLD) &
        (df['matches'] >= BALANCED_FREQUENCY_MIN) &
        (df['matches'] < HIGH_FREQUENCY_THRESHOLD)
    )
    df.loc[balanced_mask, 'rule_type'] = 'Balanced'

    return df


def create_comparison_report(df, stock_code, output_dir):
    """
    比較レポートを生成
    """

    report_lines = []
    report_lines.append("=" * 80)
    report_lines.append(f"Rule Type Comparison Report - Stock {stock_code}")
    report_lines.append("=" * 80)
    report_lines.append("")

    # タイプ別の統計
    for rule_type in ['High-Quality', 'Balanced', 'High-Frequency', 'Other']:
        df_type = df[df['rule_type'] == rule_type]

        if len(df_type) == 0:
            continue

        report_lines.append("-" * 80)
        report_lines.append(f"{rule_type} Rules")
        report_lines.append("-" * 80)
        report_lines.append(f"Count: {len(df_type)} rules")
        report_lines.append(f"Quality Score: mean={df_type['quality_score'].mean():.3f}, "
                          f"median={df_type['quality_score'].median():.3f}, "
                          f"min={df_type['quality_score'].min():.3f}, "
                          f"max={df_type['quality_score'].max():.3f}")
        report_lines.append(f"Matches: mean={df_type['matches'].mean():.1f}, "
                          f"median={df_type['matches'].median():.1f}, "
                          f"min={df_type['matches'].min()}, "
                          f"max={df_type['matches'].max()}")
        report_lines.append(f"Support Rate: mean={df_type['support_rate'].mean()*100:.3f}%, "
                          f"max={df_type['support_rate'].max()*100:.3f}%")
        report_lines.append(f"|μ(t+1)|: mean={df_type['mean_t1'].abs().mean():.3f}%, "
                          f"max={df_type['mean_t1'].abs().max():.3f}%")
        report_lines.append(f"σ(t+1): mean={df_type['sigma_t1'].mean():.3f}%, "
                          f"median={df_type['sigma_t1'].median():.3f}%")
        report_lines.append(f"Practicality Score: mean={df_type['practicality_score'].mean():.2f}, "
                          f"max={df_type['practicality_score'].max():.2f}")
        report_lines.append("")

        # Top 3を表示
        report_lines.append(f"Top 3 {rule_type} Rules by Practicality Score:")
        top3 = df_type.nlargest(3, 'practicality_score')
        for idx, row in top3.iterrows():
            report_lines.append(f"  Rule {int(row['rule_id']):03d}: "
                              f"Quality={row['quality_score']:.3f}, "
                              f"Matches={int(row['matches'])}, "
                              f"μ(t+1)={row['mean_t1']:.3f}%, "
                              f"Practicality={row['practicality_score']:.2f}")
        report_lines.append("")

    # 全体サマリー
    report_lines.append("=" * 80)
    report_lines.append("Overall Summary")
    report_lines.append("=" * 80)
    report_lines.append(f"Total Rules: {len(df)}")
    report_lines.append(f"  High-Quality: {len(df[df['rule_type'] == 'High-Quality'])} "
                      f"({len(df[df['rule_type'] == 'High-Quality'])/len(df)*100:.1f}%)")
    report_lines.append(f"  Balanced: {len(df[df['rule_type'] == 'Balanced'])} "
                      f"({len(df[df['rule_type'] == 'Balanced'])/len(df)*100:.1f}%)")
    report_lines.append(f"  High-Frequency: {len(df[df['rule_type'] == 'High-Frequency'])} "
                      f"({len(df[df['rule_type'] == 'High-Frequency'])/len(df)*100:.1f}%)")
    report_lines.append(f"  Other: {len(df[df['rule_type'] == 'Other'])} "
                      f"({len(df[df['rule_type'] == 'Other'])/len(df)*100:.1f}%)")
    report_lines.append("")

    # Top 10総合（実用性スコア順）
    report_lines.append("=" * 80)
    report_lines.append("Top 10 Rules by Practicality Score (Quality × Support)")
    report_lines.append("=" * 80)
    top10_overall = df.nlargest(10, 'practicality_score')
    for rank, (idx, row) in enumerate(top10_overall.iterrows(), 1):
        report_lines.append(f"{rank:2d}. Rule {int(row['rule_id']):03d} [{row['rule_type']:15s}]: "
                          f"Practicality={row['practicality_score']:.2f}, "
                          f"Quality={row['quality_score']:.3f}, "
                          f"Matches={int(row['matches']):2d}, "
                          f"μ(t+1)={row['mean_t1']:+.3f}%")
    report_lines.append("")

    # ファイルに保存
    report_file = f"{output_dir}/{stock_code}/rule_type_comparison.txt"
    with open(report_file, 'w') as f:
        f.write('\n'.join(report_lines))

    # コンソールにも出力
    print('\n'.join(report_lines))

    return report_file


def create_scatter_plot(df, stock_code, output_dir):
    """
    Quality Score vs Matches の散布図を作成（タイプ別色分け）
    """

    fig, ax = plt.subplots(figsize=(12, 8))

    # タイプ別にプロット
    colors = {
        'High-Quality': 'red',
        'Balanced': 'green',
        'High-Frequency': 'blue',
        'Other': 'gray'
    }

    for rule_type, color in colors.items():
        df_type = df[df['rule_type'] == rule_type]
        if len(df_type) > 0:
            ax.scatter(df_type['matches'], df_type['quality_score'],
                      alpha=0.6, s=60, color=color, edgecolors='black',
                      linewidths=0.5, label=f'{rule_type} (n={len(df_type)})')

    # 閾値ライン
    ax.axhline(y=0.6, color='red', linestyle='--', linewidth=1, alpha=0.5,
               label='High-Quality Threshold')
    ax.axhline(y=0.3, color='green', linestyle='--', linewidth=1, alpha=0.5,
               label='Balanced Min Threshold')
    ax.axvline(x=20, color='blue', linestyle='--', linewidth=1, alpha=0.5,
               label='High-Frequency Threshold')
    ax.axvline(x=15, color='green', linestyle='--', linewidth=1, alpha=0.5,
               label='Balanced Min Threshold')

    ax.set_xlabel('Matches (Frequency)', fontsize=12)
    ax.set_ylabel('Quality Score (|μ|/σ)', fontsize=12)
    ax.set_title(f'Stock {stock_code}: Rule Type Classification\n'
                 f'Quality Score vs Frequency', fontsize=13, fontweight='bold')
    ax.legend(loc='upper right', fontsize=9)
    ax.grid(True, alpha=0.3)

    # 保存
    output_file = f"{output_dir}/{stock_code}/rule_type_scatter.png"
    plt.tight_layout()
    plt.savefig(output_file, dpi=120, bbox_inches='tight')
    plt.close()

    return output_file


def create_distribution_plot(df, stock_code, output_dir):
    """
    タイプ別の分布比較（箱ひげ図）
    """

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # Quality Score分布
    ax = axes[0, 0]
    data = [df[df['rule_type'] == rt]['quality_score'].values
            for rt in ['High-Quality', 'Balanced', 'High-Frequency', 'Other']
            if len(df[df['rule_type'] == rt]) > 0]
    labels = [rt for rt in ['High-Quality', 'Balanced', 'High-Frequency', 'Other']
              if len(df[df['rule_type'] == rt]) > 0]
    bp = ax.boxplot(data, labels=labels, patch_artist=True)
    for patch, color in zip(bp['boxes'], ['red', 'green', 'blue', 'gray']):
        patch.set_facecolor(color)
        patch.set_alpha(0.5)
    ax.set_ylabel('Quality Score', fontsize=10)
    ax.set_title('Quality Score Distribution', fontsize=11, fontweight='bold')
    ax.grid(True, alpha=0.3, axis='y')

    # Matches分布
    ax = axes[0, 1]
    data = [df[df['rule_type'] == rt]['matches'].values
            for rt in ['High-Quality', 'Balanced', 'High-Frequency', 'Other']
            if len(df[df['rule_type'] == rt]) > 0]
    bp = ax.boxplot(data, labels=labels, patch_artist=True)
    for patch, color in zip(bp['boxes'], ['red', 'green', 'blue', 'gray']):
        patch.set_facecolor(color)
        patch.set_alpha(0.5)
    ax.set_ylabel('Matches', fontsize=10)
    ax.set_title('Frequency Distribution', fontsize=11, fontweight='bold')
    ax.grid(True, alpha=0.3, axis='y')

    # |μ(t+1)|分布
    ax = axes[1, 0]
    data = [df[df['rule_type'] == rt]['mean_t1'].abs().values
            for rt in ['High-Quality', 'Balanced', 'High-Frequency', 'Other']
            if len(df[df['rule_type'] == rt]) > 0]
    bp = ax.boxplot(data, labels=labels, patch_artist=True)
    for patch, color in zip(bp['boxes'], ['red', 'green', 'blue', 'gray']):
        patch.set_facecolor(color)
        patch.set_alpha(0.5)
    ax.set_ylabel('|μ(t+1)| [%]', fontsize=10)
    ax.set_title('Mean Magnitude Distribution', fontsize=11, fontweight='bold')
    ax.grid(True, alpha=0.3, axis='y')

    # Practicality Score分布
    ax = axes[1, 1]
    data = [df[df['rule_type'] == rt]['practicality_score'].values
            for rt in ['High-Quality', 'Balanced', 'High-Frequency', 'Other']
            if len(df[df['rule_type'] == rt]) > 0]
    bp = ax.boxplot(data, labels=labels, patch_artist=True)
    for patch, color in zip(bp['boxes'], ['red', 'green', 'blue', 'gray']):
        patch.set_facecolor(color)
        patch.set_alpha(0.5)
    ax.set_ylabel('Practicality Score', fontsize=10)
    ax.set_title('Practicality Score Distribution', fontsize=11, fontweight='bold')
    ax.grid(True, alpha=0.3, axis='y')

    plt.suptitle(f'Stock {stock_code}: Rule Type Comparison',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    # 保存
    output_file = f"{output_dir}/{stock_code}/rule_type_distribution.png"
    plt.savefig(output_file, dpi=120, bbox_inches='tight')
    plt.close()

    return output_file


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 compare_rule_types.py <stock_code>")
        print("Example: python3 compare_rule_types.py 3436")
        sys.exit(1)

    stock_code = sys.argv[1]
    output_dir = "output"

    print("=" * 80)
    print(f"Rule Type Comparison Analysis - Stock {stock_code}")
    print("=" * 80)
    print()

    # 全ルール分析
    print("Loading and analyzing all rules...")
    df = analyze_all_rules(stock_code, output_dir)

    if df is None or len(df) == 0:
        print("Error: No rules found")
        sys.exit(1)

    print(f"Total rules: {len(df)}")
    print()

    # ルール分類
    print("Classifying rules into types...")
    df = classify_rules(df)
    print()

    # レポート生成
    print("Generating comparison report...")
    report_file = create_comparison_report(df, stock_code, output_dir)
    print(f"Report saved: {report_file}")
    print()

    # 散布図生成
    print("Creating scatter plot...")
    scatter_file = create_scatter_plot(df, stock_code, output_dir)
    print(f"Scatter plot saved: {scatter_file}")
    print()

    # 分布図生成
    print("Creating distribution plots...")
    dist_file = create_distribution_plot(df, stock_code, output_dir)
    print(f"Distribution plots saved: {dist_file}")
    print()

    # CSVエクスポート
    csv_file = f"{output_dir}/{stock_code}/rule_type_comparison.csv"
    df.to_csv(csv_file, index=False)
    print(f"Data exported: {csv_file}")
    print()

    print("=" * 80)
    print("✓ Rule type comparison complete")
    print("=" * 80)


if __name__ == "__main__":
    main()
