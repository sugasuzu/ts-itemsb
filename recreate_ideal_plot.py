#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Recreate USDJPY X(t+1) vs X(t+2) Scatter Plot in 16:9 format
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# Data path
DATA_FILE = "1-deta-enginnering/forex_data_daily/USDJPY.txt"

def load_data():
    """Load USDJPY data."""
    df = pd.read_csv(DATA_FILE, sep=',', encoding='utf-8')
    return df

def calculate_future_returns(df):
    """Calculate X(t+1) and X(t+2)."""
    x_t1 = []
    x_t2 = []

    for i in range(len(df) - 2):
        x_t1.append(df.iloc[i + 1]['X'])
        x_t2.append(df.iloc[i + 2]['X'])

    return np.array(x_t1), np.array(x_t2)

def count_quadrants(x_t1, x_t2):
    """Count points in each quadrant."""
    q_pp = np.sum((x_t1 > 0) & (x_t2 > 0))  # (+,+)
    q_pn = np.sum((x_t1 > 0) & (x_t2 < 0))  # (+,-)
    q_np = np.sum((x_t1 < 0) & (x_t2 > 0))  # (-,+)
    q_nn = np.sum((x_t1 < 0) & (x_t2 < 0))  # (-,-)

    total = q_pp + q_pn + q_np + q_nn

    return {
        'q_pp': q_pp,
        'q_pn': q_pn,
        'q_np': q_np,
        'q_nn': q_nn,
        'total': total
    }

def create_plot(x_t1, x_t2, quadrants):
    """Create 16:9 scatter plot."""

    # Create figure with 16:9 aspect ratio
    fig, ax = plt.subplots(figsize=(16, 9))

    # Scatter plot (all data in gray)
    ax.scatter(x_t1, x_t2, alpha=0.3, s=20, color='gray', label=f'All data (n={len(x_t1):,})')

    # Ideal cluster (tight circular cluster near origin)
    ideal_x = 0.5
    ideal_y = 0.5
    ax.scatter([ideal_x], [ideal_y], s=300, color='red', alpha=0.7,
               edgecolors='darkred', linewidths=2, marker='o', zorder=5)
    ax.add_patch(plt.Circle((ideal_x, ideal_y), 0.15, color='red',
                            fill=False, linestyle='--', linewidth=2, label='Ideal Cluster\n(tight, circular)'))

    # Arrow pointing to ideal cluster
    ax.annotate('Both Up/Small Group\n(Cluster)',
                xy=(ideal_x, ideal_y),
                xytext=(2, 1.3),
                fontsize=11, color='red', weight='bold',
                arrowprops=dict(arrowstyle='->', color='red', lw=2))

    # Add quadrant labels
    ax.text(2, 1.5, '(++)\nRebound', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.3))
    ax.text(2, -1.5, '(+-)\nReversal', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.3))
    ax.text(-2, 1.5, '(-+)\nRebound', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.3))
    ax.text(-2, -1.5, '(--)\nBoth Down', fontsize=10, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.3))

    # Add grid lines at 0
    ax.axhline(0, color='black', linewidth=1.5, linestyle='-', alpha=0.8)
    ax.axvline(0, color='black', linewidth=1.5, linestyle='-', alpha=0.8)

    # Calculate correlation
    correlation = np.corrcoef(x_t1, x_t2)[0, 1]

    # Add statistics box
    stats_text = f"""Quadrant Distribution:
(++) Up-Up:     {quadrants['q_pp']} ({quadrants['q_pp']/quadrants['total']*100:.1f}%)
(+-) Up-Down:   {quadrants['q_pn']} ({quadrants['q_pn']/quadrants['total']*100:.1f}%)
(-+) Down-Up:   {quadrants['q_np']} ({quadrants['q_np']/quadrants['total']*100:.1f}%)
(--) Down-Down: {quadrants['q_nn']} ({quadrants['q_nn']/quadrants['total']*100:.1f}%)

Correlation: {correlation:.3f}
Total: {quadrants['total']:,} points"""

    ax.text(0.02, 0.98, stats_text, transform=ax.transAxes,
            fontsize=10, verticalalignment='top',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    # Labels and title
    ax.set_xlabel('X(t+1) - Next Period Return (%)', fontsize=12, weight='bold')
    ax.set_ylabel('X(t+2) - Two Periods Ahead Return (%)', fontsize=12, weight='bold')
    ax.set_title('USDJPY: X(t+1) vs X(t+2) Scatter Plot\nBaseline Data (Random Walk Behavior)',
                 fontsize=14, weight='bold', pad=20)

    # Set axis limits
    ax.set_xlim(-4, 4)
    ax.set_ylim(-4, 4)

    # Grid
    ax.grid(True, alpha=0.3, linestyle='--')

    # Legend
    ax.legend(loc='upper right', fontsize=10)

    plt.tight_layout()

    return fig

def main():
    """Main function."""
    print("Loading USDJPY data...")
    df = load_data()

    print(f"Total records: {len(df)}")

    print("Calculating X(t+1) and X(t+2)...")
    x_t1, x_t2 = calculate_future_returns(df)

    print(f"Valid pairs: {len(x_t1)}")

    print("Counting quadrants...")
    quadrants = count_quadrants(x_t1, x_t2)

    print("\nQuadrant Distribution:")
    print(f"  (++) Up-Up:     {quadrants['q_pp']} ({quadrants['q_pp']/quadrants['total']*100:.1f}%)")
    print(f"  (+-) Up-Down:   {quadrants['q_pn']} ({quadrants['q_pn']/quadrants['total']*100:.1f}%)")
    print(f"  (-+) Down-Up:   {quadrants['q_np']} ({quadrants['q_np']/quadrants['total']*100:.1f}%)")
    print(f"  (--) Down-Down: {quadrants['q_nn']} ({quadrants['q_nn']/quadrants['total']*100:.1f}%)")

    print("\nCreating 16:9 plot...")
    fig = create_plot(x_t1, x_t2, quadrants)

    output_file = "ideal_usdjpy_x_vs_x2_16x9.png"
    fig.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"\n✓ Saved: {output_file}")

    plt.close()

if __name__ == "__main__":
    main()
