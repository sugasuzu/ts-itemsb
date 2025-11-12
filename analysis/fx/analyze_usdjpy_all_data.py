#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USD/JPY All Data Analysis (No Rule Applied)
============================================

Analyze the baseline characteristics of USD/JPY data:
- Basic statistics (mean, std, min, max)
- X(t+1) vs X(t+2) scatter plot
- Quadrant distribution
- Correlation analysis
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
DATA_FILE = BASE_DIR / "USDJPY.txt"
OUTPUT_DIR = Path("analysis/fx")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

def load_all_data():
    """Load all USDJPY data."""
    print(f"Loading USDJPY data from {DATA_FILE}...")
    df = pd.read_csv(DATA_FILE, encoding='utf-8')
    x_values = df['X'].values
    timestamps = pd.to_datetime(df['T'])

    data_list = []
    for i in range(len(x_values) - 2):
        data_list.append({
            'Timestamp': timestamps[i],
            'X_t1': x_values[i + 1],
            'X_t2': x_values[i + 2]
        })

    result = pd.DataFrame(data_list)
    print(f"  Total points: {len(result)}")
    return result

def calculate_basic_statistics(df):
    """Calculate basic statistics."""
    print("\n" + "=" * 70)
    print("BASIC STATISTICS (USD/JPY)")
    print("=" * 70)

    print(f"\nData period: {df['Timestamp'].min()} to {df['Timestamp'].max()}")
    print(f"Total data points: {len(df):,}")

    print("\n--- X(t+1) Statistics ---")
    print(f"  Mean:   {df['X_t1'].mean():+.4f}%")
    print(f"  Std:    {df['X_t1'].std():.4f}%")
    print(f"  Min:    {df['X_t1'].min():+.4f}%")
    print(f"  Max:    {df['X_t1'].max():+.4f}%")
    print(f"  Median: {df['X_t1'].median():+.4f}%")

    print("\n--- X(t+2) Statistics ---")
    print(f"  Mean:   {df['X_t2'].mean():+.4f}%")
    print(f"  Std:    {df['X_t2'].std():.4f}%")
    print(f"  Min:    {df['X_t2'].min():+.4f}%")
    print(f"  Max:    {df['X_t2'].max():+.4f}%")
    print(f"  Median: {df['X_t2'].median():+.4f}%")

    # Correlation
    corr = df[['X_t1', 'X_t2']].corr().iloc[0, 1]
    print(f"\n--- Correlation ---")
    print(f"  X(t+1) vs X(t+2): {corr:+.4f}")

    return {
        'mean_t1': df['X_t1'].mean(),
        'std_t1': df['X_t1'].std(),
        'mean_t2': df['X_t2'].mean(),
        'std_t2': df['X_t2'].std(),
        'corr': corr
    }

def calculate_quadrant_distribution(df):
    """Calculate quadrant distribution (0-based)."""
    print("\n" + "=" * 70)
    print("QUADRANT DISTRIBUTION (0-based threshold)")
    print("=" * 70)

    q1 = ((df['X_t1'] >= 0.0) & (df['X_t2'] >= 0.0)).sum()  # ++
    q2 = ((df['X_t1'] < 0.0) & (df['X_t2'] >= 0.0)).sum()   # -+
    q3 = ((df['X_t1'] < 0.0) & (df['X_t2'] < 0.0)).sum()    # --
    q4 = ((df['X_t1'] >= 0.0) & (df['X_t2'] < 0.0)).sum()   # +-

    total = len(df)

    print(f"\n  Q1 (++): {q1:5d} points ({q1/total*100:.2f}%)")
    print(f"  Q2 (-+): {q2:5d} points ({q2/total*100:.2f}%)")
    print(f"  Q3 (--): {q3:5d} points ({q3/total*100:.2f}%)")
    print(f"  Q4 (+-): {q4:5d} points ({q4/total*100:.2f}%)")
    print(f"  Total:   {total:5d} points")

    # Dominant quadrant
    counts = [q1, q2, q3, q4]
    max_count = max(counts)
    dominant = counts.index(max_count) + 1
    concentration = max_count / total

    print(f"\n  Dominant quadrant: Q{dominant}")
    print(f"  Concentration: {concentration*100:.2f}%")

    return {
        'q1': q1, 'q2': q2, 'q3': q3, 'q4': q4,
        'dominant': dominant,
        'concentration': concentration
    }

def plot_scatter(df, stats, quad_stats):
    """Generate X(t+1) vs X(t+2) scatter plot."""
    print("\n" + "=" * 70)
    print("GENERATING SCATTER PLOT")
    print("=" * 70)

    fig, ax = plt.subplots(figsize=(16, 9))

    # Scatter plot
    ax.scatter(df['X_t1'], df['X_t2'],
               alpha=0.3, s=15, c='gray', label=f'All data (n={len(df):,})')

    # Origin lines (象限境界: 0%)
    ax.axvline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5)
    ax.axhline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5)

    # Mean lines
    ax.axvline(stats['mean_t1'], color='blue', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean X(t+1) = {stats["mean_t1"]:.3f}%')
    ax.axhline(stats['mean_t2'], color='green', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean X(t+2) = {stats["mean_t2"]:.3f}%')

    # Statistics box
    quadrant_names = ['Q1(++)', 'Q2(-+)', 'Q3(--)', 'Q4(+-)']
    stats_text = f'USD/JPY All Data (No Rule)\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Total points: {len(df):,}\n'
    stats_text += f'\n'
    stats_text += f'X(t+1): μ={stats["mean_t1"]:+.3f}%, σ={stats["std_t1"]:.3f}%\n'
    stats_text += f'X(t+2): μ={stats["mean_t2"]:+.3f}%, σ={stats["std_t2"]:.3f}%\n'
    stats_text += f'Correlation: {stats["corr"]:+.4f}\n'
    stats_text += f'\n'
    stats_text += f'Quadrant Distribution:\n'
    stats_text += f'  Q1(++): {quad_stats["q1"]:,} ({quad_stats["q1"]/len(df)*100:.1f}%)\n'
    stats_text += f'  Q2(-+): {quad_stats["q2"]:,} ({quad_stats["q2"]/len(df)*100:.1f}%)\n'
    stats_text += f'  Q3(--): {quad_stats["q3"]:,} ({quad_stats["q3"]/len(df)*100:.1f}%)\n'
    stats_text += f'  Q4(+-): {quad_stats["q4"]:,} ({quad_stats["q4"]/len(df)*100:.1f}%)\n'
    stats_text += f'\n'
    stats_text += f'Dominant: {quadrant_names[quad_stats["dominant"]-1]}\n'
    stats_text += f'Concentration: {quad_stats["concentration"]*100:.1f}%\n'

    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            verticalalignment='top',
            fontsize=11,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.9),
            zorder=4)

    ax.set_xlabel('X(t+1) [%]', fontsize=14, fontweight='bold')
    ax.set_ylabel('X(t+2) [%]', fontsize=14, fontweight='bold')
    ax.set_title('USD/JPY: X(t+1) vs X(t+2) (All Data, No Rule)',
                 fontsize=15, fontweight='bold', pad=20)
    ax.legend(loc='upper right', fontsize=13, framealpha=0.9)
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    # Set axis range
    max_range = max(abs(stats['mean_t1']) + stats['std_t1'] * 4,
                    abs(stats['mean_t2']) + stats['std_t2'] * 4,
                    3.0)
    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    plt.tight_layout()

    output_file = OUTPUT_DIR / "usdjpy_all_data_scatter.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"  Saved: {output_file}")
    return output_file

def main():
    """Main function."""
    print("=" * 70)
    print("USD/JPY All Data Analysis")
    print("=" * 70)

    # Load data
    df = load_all_data()

    # Calculate statistics
    stats = calculate_basic_statistics(df)
    quad_stats = calculate_quadrant_distribution(df)

    # Plot scatter
    plot_scatter(df, stats, quad_stats)

    print("\n" + "=" * 70)
    print("✓ Analysis complete")
    print("=" * 70)

if __name__ == "__main__":
    main()
