#!/usr/bin/env python3
"""
実験結果可視化スクリプト

集中度分布、集中度 vs Mean の散布図などを生成します。

使い方:
    python3 visualize_results.py

出力:
    - concentration_distribution.png: 集中度のヒストグラム
    - concentration_vs_mean.png: 集中度 vs Mean の散布図
    - quadrant_distribution.png: 象限分布の円グラフ
"""

import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def load_all_rules(verification_dir, min_support=30):
    """
    全ルールの統計情報を読み込む

    Args:
        verification_dir: verification ディレクトリのパス
        min_support: 最小サポート数

    Returns:
        DataFrame: 全ルールの統計情報
    """
    results = []

    for filename in os.listdir(verification_dir):
        if filename.startswith('rule_') and filename.endswith('.csv'):
            rule_id = int(filename.replace('rule_', '').replace('.csv', ''))
            csv_path = os.path.join(verification_dir, filename)

            df_csv = pd.read_csv(csv_path)
            if len(df_csv) < min_support:
                continue

            # X列を数値型に変換
            df_csv['X(t+1)'] = pd.to_numeric(df_csv['X(t+1)'], errors='coerce')
            df_csv['X(t+2)'] = pd.to_numeric(df_csv['X(t+2)'], errors='coerce')

            # 象限分布
            q1 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] > 0)).sum()
            q2 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] <= 0)).sum()
            q3 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] <= 0)).sum()
            q4 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] > 0)).sum()

            max_q = max(q1, q2, q3, q4)
            concentration = (max_q / len(df_csv)) * 100

            # 支配象限
            if max_q == q1:
                dominant = "Q1(++)"
            elif max_q == q2:
                dominant = "Q2(+-)"
            elif max_q == q3:
                dominant = "Q3(--)"
            else:
                dominant = "Q4(-+)"

            results.append({
                'rule_id': rule_id,
                'concentration': concentration,
                'support': len(df_csv),
                'mean_t1': df_csv['X(t+1)'].mean(),
                'mean_t2': df_csv['X(t+2)'].mean(),
                'sigma_t1': df_csv['X(t+1)'].std(),
                'sigma_t2': df_csv['X(t+2)'].std(),
                'dominant': dominant,
                'q1': q1, 'q2': q2, 'q3': q3, 'q4': q4
            })

    return pd.DataFrame(results)

def plot_concentration_distribution(df, output_file='concentration_distribution.png'):
    """
    集中度のヒストグラムを生成

    Args:
        df: ルールのDataFrame
        output_file: 出力ファイル名
    """
    plt.figure(figsize=(10, 6))

    # ヒストグラム
    n, bins, patches = plt.hist(df['concentration'], bins=50, edgecolor='black', alpha=0.7, color='steelblue')

    # 統計線
    mean_conc = df['concentration'].mean()
    plt.axvline(x=mean_conc, color='green', linestyle='--', linewidth=2,
                label=f'Mean: {mean_conc:.1f}%')
    plt.axvline(x=50, color='red', linestyle='--', linewidth=2,
                label='Target: 50%')

    # ラベルとタイトル
    plt.xlabel('Concentration (%)', fontsize=12, fontweight='bold')
    plt.ylabel('Number of Rules', fontsize=12, fontweight='bold')
    plt.title('Distribution of Quadrant Concentration', fontsize=14, fontweight='bold')
    plt.legend(fontsize=11)
    plt.grid(True, alpha=0.3, axis='y')

    # 統計情報を追加
    textstr = f'Total Rules: {len(df)}\nMax: {df["concentration"].max():.1f}%\nStd: {df["concentration"].std():.1f}%'
    plt.text(0.02, 0.98, textstr, transform=plt.gca().transAxes,
             fontsize=10, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"✓ 保存: {output_file}")
    plt.close()

def plot_concentration_vs_mean(df, output_file='concentration_vs_mean.png'):
    """
    集中度 vs Mean の散布図を生成

    Args:
        df: ルールのDataFrame
        output_file: 出力ファイル名
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # X(t+1)
    ax1.scatter(df['concentration'], df['mean_t1'].abs(), alpha=0.5, s=20, color='steelblue')
    ax1.axhline(y=0.5, color='r', linestyle='--', linewidth=1.5, label='|mean| = 0.5%')
    ax1.axvline(x=50, color='g', linestyle='--', linewidth=1.5, label='Conc = 50%')
    ax1.set_xlabel('Concentration (%)', fontsize=12, fontweight='bold')
    ax1.set_ylabel('|X(t+1)_mean| (%)', fontsize=12, fontweight='bold')
    ax1.set_title('Concentration vs |Mean| for X(t+1)', fontsize=13, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend(fontsize=10)

    # 相関係数
    corr_t1 = df['concentration'].corr(df['mean_t1'].abs())
    ax1.text(0.05, 0.95, f'Correlation: {corr_t1:.3f}', transform=ax1.transAxes,
             fontsize=11, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # X(t+2)
    ax2.scatter(df['concentration'], df['mean_t2'].abs(), alpha=0.5, s=20, color='darkorange')
    ax2.axhline(y=0.5, color='r', linestyle='--', linewidth=1.5, label='|mean| = 0.5%')
    ax2.axvline(x=50, color='g', linestyle='--', linewidth=1.5, label='Conc = 50%')
    ax2.set_xlabel('Concentration (%)', fontsize=12, fontweight='bold')
    ax2.set_ylabel('|X(t+2)_mean| (%)', fontsize=12, fontweight='bold')
    ax2.set_title('Concentration vs |Mean| for X(t+2)', fontsize=13, fontweight='bold')
    ax2.grid(True, alpha=0.3)
    ax2.legend(fontsize=10)

    # 相関係数
    corr_t2 = df['concentration'].corr(df['mean_t2'].abs())
    ax2.text(0.05, 0.95, f'Correlation: {corr_t2:.3f}', transform=ax2.transAxes,
             fontsize=11, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"✓ 保存: {output_file}")
    plt.close()

def plot_quadrant_distribution(df, output_file='quadrant_distribution.png'):
    """
    象限分布の円グラフを生成

    Args:
        df: ルールのDataFrame
        output_file: 出力ファイル名
    """
    # 象限カウント
    quadrant_counts = df['dominant'].value_counts()

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))

    # 全ルールの象限分布（円グラフ）
    colors = ['#ff9999', '#66b3ff', '#99ff99', '#ffcc99']
    explode = (0.05, 0, 0, 0)  # Q1を少し分離

    ax1.pie(quadrant_counts, labels=quadrant_counts.index, autopct='%1.1f%%',
            startangle=90, colors=colors, explode=explode,
            textprops={'fontsize': 11, 'fontweight': 'bold'})
    ax1.set_title('Dominant Quadrant Distribution (All Rules)', fontsize=13, fontweight='bold')

    # 高集中度ルール（50%以上）の象限分布
    df_high = df[df['concentration'] >= 50.0]
    if len(df_high) > 0:
        quadrant_counts_high = df_high['dominant'].value_counts()

        ax2.pie(quadrant_counts_high, labels=quadrant_counts_high.index, autopct='%1.1f%%',
                startangle=90, colors=colors, explode=explode,
                textprops={'fontsize': 11, 'fontweight': 'bold'})
        ax2.set_title(f'Dominant Quadrant Distribution (Conc >= 50%)\nn={len(df_high)} rules',
                      fontsize=13, fontweight='bold')
    else:
        ax2.text(0.5, 0.5, 'No rules with\nConc >= 50%', ha='center', va='center',
                 fontsize=14, fontweight='bold', transform=ax2.transAxes)
        ax2.axis('off')

    plt.tight_layout()
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"✓ 保存: {output_file}")
    plt.close()

def main():
    # verification ディレクトリ
    verification_dir = "1-deta-enginnering/crypto_data_hourly/output/BTC/verification/"

    if not os.path.exists(verification_dir):
        print(f"Error: {verification_dir} が見つかりません")
        return

    print("データ読み込み中...")
    df = load_all_rules(verification_dir)

    if len(df) == 0:
        print("Error: 有効なルールが見つかりません")
        return

    print(f"総ルール数: {len(df)}")
    print()

    # 可視化生成
    print("可視化生成中...")
    plot_concentration_distribution(df)
    plot_concentration_vs_mean(df)
    plot_quadrant_distribution(df)

    print()
    print("=" * 60)
    print("可視化完了！")
    print("=" * 60)
    print()
    print("生成されたファイル:")
    print("  - concentration_distribution.png")
    print("  - concentration_vs_mean.png")
    print("  - quadrant_distribution.png")

if __name__ == "__main__":
    main()
