#!/usr/bin/env python3
"""
USDJPY ベースラインデータの散布図可視化

2つの散布図を作成：
1. X(t+1) vs Time - 時系列の変化率
2. X(t+1) vs X(t+2) - 将来予測の関係性
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime

# 日本語フォント設定
plt.rcParams['font.family'] = 'DejaVu Sans'
plt.rcParams['axes.unicode_minus'] = False

def load_usdjpy_data():
    """USDJPYデータを読み込み"""
    df = pd.read_csv('1-deta-enginnering/forex_data_daily/USDJPY.txt')

    # Tカラムを日付型に変換
    df['T'] = pd.to_datetime(df['T'])

    # X(t+1), X(t+2)を計算（次の行のXを参照）
    df['X_t+1'] = df['X'].shift(-1)
    df['X_t+2'] = df['X'].shift(-2)

    # NaNを除去
    df = df.dropna(subset=['X_t+1', 'X_t+2'])

    return df

def plot_x_vs_time(df, output_file='usdjpy_x_vs_time.png'):
    """
    X(t+1) vs Time 散布図

    時系列での変化率の分布を可視化
    """
    fig, ax = plt.subplots(figsize=(14, 6))

    # 散布図
    scatter = ax.scatter(df['T'], df['X_t+1'],
                        c=df['X_t+1'],
                        cmap='RdYlGn',
                        alpha=0.6,
                        s=10,
                        vmin=-2, vmax=2)

    # 横軸: ゼロライン
    ax.axhline(y=0, color='black', linestyle='-', linewidth=0.8, alpha=0.3)

    # ±1σ, ±2σ ライン
    sigma = df['X_t+1'].std()
    mean = df['X_t+1'].mean()

    ax.axhline(y=mean + sigma, color='blue', linestyle='--', linewidth=1, alpha=0.5, label=f'+1σ ({mean+sigma:.2f}%)')
    ax.axhline(y=mean - sigma, color='blue', linestyle='--', linewidth=1, alpha=0.5, label=f'-1σ ({mean-sigma:.2f}%)')
    ax.axhline(y=mean + 2*sigma, color='red', linestyle='--', linewidth=1, alpha=0.5, label=f'+2σ ({mean+2*sigma:.2f}%)')
    ax.axhline(y=mean - 2*sigma, color='red', linestyle='--', linewidth=1, alpha=0.5, label=f'-2σ ({mean-2*sigma:.2f}%)')

    # 軸ラベル
    ax.set_xlabel('Time', fontsize=12, fontweight='bold')
    ax.set_ylabel('X(t+1) - Return (%)', fontsize=12, fontweight='bold')
    ax.set_title('USDJPY: X(t+1) vs Time\nBaseline Data Distribution (All Records)',
                 fontsize=14, fontweight='bold', pad=20)

    # X軸の日付フォーマット
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
    ax.xaxis.set_major_locator(mdates.YearLocator())
    plt.xticks(rotation=45)

    # グリッド
    ax.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

    # カラーバー
    cbar = plt.colorbar(scatter, ax=ax)
    cbar.set_label('Return (%)', fontsize=10)

    # 凡例
    ax.legend(loc='upper left', fontsize=9)

    # 統計情報を追加
    stats_text = f'Records: {len(df):,}\n'
    stats_text += f'Mean: {mean:.4f}%\n'
    stats_text += f'Std: {sigma:.4f}%\n'
    stats_text += f'Min: {df["X_t+1"].min():.2f}%\n'
    stats_text += f'Max: {df["X_t+1"].max():.2f}%'

    ax.text(0.98, 0.98, stats_text,
            transform=ax.transAxes,
            fontsize=9,
            verticalalignment='top',
            horizontalalignment='right',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_file}")
    plt.close()

def plot_x_vs_x2(df, output_file='usdjpy_x_vs_x2.png'):
    """
    X(t+1) vs X(t+2) 散布図

    理想的な小集団: 円形/正方形に密集した点群
    """
    fig, ax = plt.subplots(figsize=(10, 10))

    # 散布図（全データ）
    ax.scatter(df['X_t+1'], df['X_t+2'],
              alpha=0.3,
              s=15,
              c='gray',
              label=f'All data (n={len(df):,})')

    # 象限を分ける線
    ax.axhline(y=0, color='black', linestyle='-', linewidth=1.5, alpha=0.5)
    ax.axvline(x=0, color='black', linestyle='-', linewidth=1.5, alpha=0.5)

    # 象限ラベル
    ax.text(1.5, 1.5, '(++)\nBoth Up', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.3))
    ax.text(-1.5, 1.5, '(-+)\nRebound', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.3))
    ax.text(-1.5, -1.5, '(--)\nBoth Down', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.3))
    ax.text(1.5, -1.5, '(+-)\nReversal', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.3))

    # 象限ごとの統計
    quadrants = {
        '++': ((df['X_t+1'] > 0) & (df['X_t+2'] > 0)).sum(),
        '+-': ((df['X_t+1'] > 0) & (df['X_t+2'] < 0)).sum(),
        '-+': ((df['X_t+1'] < 0) & (df['X_t+2'] > 0)).sum(),
        '--': ((df['X_t+1'] < 0) & (df['X_t+2'] < 0)).sum(),
    }

    total = sum(quadrants.values())
    quadrant_percentages = {k: v/total*100 for k, v in quadrants.items()}

    # 軸ラベル
    ax.set_xlabel('X(t+1) - Next Period Return (%)', fontsize=12, fontweight='bold')
    ax.set_ylabel('X(t+2) - Two Periods Ahead Return (%)', fontsize=12, fontweight='bold')
    ax.set_title('USDJPY: X(t+1) vs X(t+2) Scatter Plot\nBaseline Data (Random Walk Behavior)',
                 fontsize=14, fontweight='bold', pad=20)

    # グリッド
    ax.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

    # 軸の範囲を統一（正方形）
    max_range = max(abs(df['X_t+1']).max(), abs(df['X_t+2']).max()) * 1.1
    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    # アスペクト比を1:1に
    ax.set_aspect('equal', adjustable='box')

    # 統計情報を追加
    stats_text = 'Quadrant Distribution:\n'
    stats_text += f'(++) Up-Up:     {quadrants["++"]:<4d} ({quadrant_percentages["++"]:.1f}%)\n'
    stats_text += f'(+-) Up-Down:   {quadrants["+-"]:<4d} ({quadrant_percentages["+-"]:.1f}%)\n'
    stats_text += f'(-+) Down-Up:   {quadrants["-+"]:<4d} ({quadrant_percentages["-+"]:.1f}%)\n'
    stats_text += f'(--) Down-Down: {quadrants["--"]:<4d} ({quadrant_percentages["--"]:.1f}%)\n'
    stats_text += '\n'
    stats_text += f'Correlation: {df["X_t+1"].corr(df["X_t+2"]):.3f}\n'
    stats_text += f'Total: {total:,} points'

    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            fontsize=9,
            verticalalignment='top',
            horizontalalignment='left',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
            family='monospace')

    # 理想的な小集団の例を示す（仮想的な円）
    # 例: (X_t+1, X_t+2) = (0.5, 0.5) を中心とした小集団
    example_center_x = 0.5
    example_center_y = 0.5
    example_radius = 0.2

    circle = plt.Circle((example_center_x, example_center_y),
                        example_radius,
                        color='red',
                        fill=False,
                        linestyle='--',
                        linewidth=2,
                        label='Ideal cluster\n(tight, circular)')
    ax.add_patch(circle)

    ax.annotate('Ideal Small Group\n(Cluster)',
                xy=(example_center_x, example_center_y),
                xytext=(example_center_x + 0.8, example_center_y + 0.8),
                arrowprops=dict(arrowstyle='->', color='red', lw=2),
                fontsize=10,
                color='red',
                fontweight='bold')

    ax.legend(loc='upper right', fontsize=9)

    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"✓ Saved: {output_file}")
    plt.close()

def main():
    """メイン処理"""
    print("=== USDJPY Baseline Data Visualization ===\n")

    # データ読み込み
    print("Loading USDJPY data...")
    df = load_usdjpy_data()
    print(f"Loaded {len(df):,} records\n")

    # 基本統計
    print("=== Basic Statistics ===")
    print(f"X(t+1) Mean:  {df['X_t+1'].mean():.4f}%")
    print(f"X(t+1) Std:   {df['X_t+1'].std():.4f}%")
    print(f"X(t+2) Mean:  {df['X_t+2'].mean():.4f}%")
    print(f"X(t+2) Std:   {df['X_t+2'].std():.4f}%")
    print(f"Correlation:  {df['X_t+1'].corr(df['X_t+2']):.4f}")
    print()

    # 散布図1: X(t+1) vs Time
    print("Creating X(t+1) vs Time scatter plot...")
    plot_x_vs_time(df)

    # 散布図2: X(t+1) vs X(t+2)
    print("Creating X(t+1) vs X(t+2) scatter plot...")
    plot_x_vs_x2(df)

    print("\n=== Visualization Complete ===")
    print("Files created:")
    print("  1. usdjpy_x_vs_time.png  - Time series scatter")
    print("  2. usdjpy_x_vs_x2.png    - Future correlation scatter")
    print("\nKey Observations:")
    print("  - Random walk behavior: correlation ≈ 0")
    print("  - No clear clusters in baseline data")
    print("  - Ideal clusters should be tight, circular groups")
    print("  - GNMiner goal: Find conditions for such clusters")

if __name__ == '__main__':
    main()
