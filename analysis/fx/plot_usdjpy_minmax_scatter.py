#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USDJPY Rule Cluster Visualization with MinMax Ranges
=====================================================

This script generates X(t+1) vs X(t+2) scatter plots showing:
- All USDJPY data (background)
- Rule matched points
- Mean lines
- MinMax rectangles (showing the range of each rule)

Output:
  - 1-deta-enginnering/forex_data_daily/output/USDJPY/scatter_minmax/
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
import os
from pathlib import Path

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
OUTPUT_DIR = BASE_DIR / "output/USDJPY"
VERIFICATION_DIR = OUTPUT_DIR / "verification"
RULES_FILE = OUTPUT_DIR / "pool/zrp01a.txt"
SCATTER_DIR = OUTPUT_DIR / "scatter_minmax"

# Data file for background scatter
DATA_FILE = BASE_DIR / "USDJPY.txt"

# Create output directory
SCATTER_DIR.mkdir(exist_ok=True)

def load_all_data():
    """Load all USDJPY data for background scatter."""
    print(f"Loading all USDJPY data from {DATA_FILE}...")

    # Read all data (CSV format with comma separator)
    df = pd.read_csv(DATA_FILE, encoding='utf-8')

    # Extract X column
    x_values = df['X'].values

    # Create X(t+1) and X(t+2)
    x_t1 = []
    x_t2 = []

    for i in range(len(x_values) - 2):
        x_t1.append(x_values[i + 1])
        x_t2.append(x_values[i + 2])

    result = pd.DataFrame({
        'X_t1': x_t1,
        'X_t2': x_t2
    })

    print(f"  Total points: {len(result)}")
    return result

def load_rules():
    """Load discovered rules from zrp01a.txt."""
    print(f"Loading rules from {RULES_FILE}...")

    df = pd.read_csv(RULES_FILE, sep='\t', encoding='utf-8')

    print(f"  Total rules: {len(df)}")
    return df

def load_rule_matches(rule_id):
    """Load verification CSV for a specific rule."""
    csv_file = VERIFICATION_DIR / f"rule_{rule_id:03d}.csv"

    if not csv_file.exists():
        print(f"  Warning: {csv_file} not found")
        return None

    df = pd.read_csv(csv_file, encoding='utf-8')

    # Extract X(t+1) and X(t+2) columns
    x_t1 = df['X(t+1)'].values
    x_t2 = df['X(t+2)'].values

    return pd.DataFrame({
        'X_t1': x_t1,
        'X_t2': x_t2
    })

def get_rule_attributes(row):
    """Extract rule attributes as a readable string."""
    attributes = []

    for i in range(1, 9):
        attr_name = f'Attr{i}'
        if attr_name in row.index:
            value = row[attr_name]
            if pd.notna(value) and str(value) != '0':
                attributes.append(str(value))

    return attributes

def plot_cluster_with_minmax(rule_id, rule_row, matched_data, all_data):
    """Generate scatter plot with MinMax rectangle for one rule."""

    # Extract rule info
    mean_t1 = rule_row['X(t+1)_mean']
    sigma_t1 = rule_row['X(t+1)_sigma']
    min_t1 = rule_row['X(t+1)_min']
    max_t1 = rule_row['X(t+1)_max']

    mean_t2 = rule_row['X(t+2)_mean']
    sigma_t2 = rule_row['X(t+2)_sigma']
    min_t2 = rule_row['X(t+2)_min']
    max_t2 = rule_row['X(t+2)_max']

    support_count = rule_row['support_count']
    support_rate = rule_row['support_rate']
    num_attr = rule_row['NumAttr']

    # Calculate center and range
    center_t1 = (min_t1 + max_t1) / 2.0
    center_t2 = (min_t2 + max_t2) / 2.0
    range_t1 = max_t1 - min_t1
    range_t2 = max_t2 - min_t2

    # Get attributes
    attributes = get_rule_attributes(rule_row)

    # Create figure
    fig, ax = plt.subplots(figsize=(13, 11))

    # Plot all data (gray, transparent)
    n_all = len(all_data)
    ax.scatter(all_data['X_t1'], all_data['X_t2'],
               alpha=0.2, s=10, c='gray', label=f'All data (n={n_all:,})', zorder=1)

    # Plot matched points (red, prominent)
    actual_matches = len(matched_data)
    ax.scatter(matched_data['X_t1'], matched_data['X_t2'],
               alpha=0.8, s=80, c='red', edgecolors='darkred',
               linewidths=1.5, label=f'Rule matches (n={actual_matches})', zorder=3)

    # Add MinMax rectangle
    rect_width = range_t1
    rect_height = range_t2
    rect_x = min_t1
    rect_y = min_t2

    minmax_rect = patches.Rectangle(
        (rect_x, rect_y), rect_width, rect_height,
        linewidth=3, edgecolor='blue', facecolor='cyan', alpha=0.15,
        linestyle='--', label=f'MinMax Range', zorder=2
    )
    ax.add_patch(minmax_rect)

    # Add center point
    ax.scatter([center_t1], [center_t2],
               s=200, c='blue', marker='X', edgecolors='darkblue',
               linewidths=2, label=f'Center ({center_t1:.3f}%, {center_t2:.3f}%)', zorder=4)

    # Add mean lines
    ax.axvline(mean_t1, color='orange', linestyle='-.', linewidth=2,
               alpha=0.7, label=f'Mean X(t+1) = {mean_t1:.3f}%', zorder=2)
    ax.axhline(mean_t2, color='green', linestyle='-.', linewidth=2,
               alpha=0.7, label=f'Mean X(t+2) = {mean_t2:.3f}%', zorder=2)

    # Add origin lines (darker, more prominent)
    ax.axvline(0, color='black', linestyle='-', linewidth=2, alpha=0.6, zorder=1)
    ax.axhline(0, color='black', linestyle='-', linewidth=2, alpha=0.6, zorder=1)

    # Statistics box
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Support: {support_count} ({support_rate:.4f}%)\n'
    stats_text += f'Attributes: {num_attr}\n'
    stats_text += f'\n'
    stats_text += f'X(t+1):\n'
    stats_text += f'  Mean  = {mean_t1:+.3f}%\n'
    stats_text += f'  Sigma = {sigma_t1:.3f}%\n'
    stats_text += f'  Min   = {min_t1:+.3f}%\n'
    stats_text += f'  Max   = {max_t1:+.3f}%\n'
    stats_text += f'  Range = {range_t1:.3f}%\n'
    stats_text += f'\n'
    stats_text += f'X(t+2):\n'
    stats_text += f'  Mean  = {mean_t2:+.3f}%\n'
    stats_text += f'  Sigma = {sigma_t2:.3f}%\n'
    stats_text += f'  Min   = {min_t2:+.3f}%\n'
    stats_text += f'  Max   = {max_t2:+.3f}%\n'
    stats_text += f'  Range = {range_t2:.3f}%\n'
    stats_text += f'\n'
    stats_text += f'Center:\n'
    stats_text += f'  ({center_t1:+.3f}%, {center_t2:+.3f}%)\n'
    stats_text += f'\n'
    stats_text += f'Pattern:\n'
    for i, attr in enumerate(attributes[:5], 1):  # Show first 5
        stats_text += f'  {i}. {attr}\n'
    if len(attributes) > 5:
        stats_text += f'  ... +{len(attributes)-5} more\n'

    # Position stats box
    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            verticalalignment='top',
            fontsize=8,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.95),
            zorder=5)

    # Quadrant counts
    q_pp = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] > 0))
    q_pn = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] < 0))
    q_np = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] > 0))
    q_nn = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] < 0))

    quadrant_text = f'Quadrants:\n'
    quadrant_text += f'Q1 (+,+): {q_pp:2d}\n'
    quadrant_text += f'Q2 (-,+): {q_np:2d}\n'
    quadrant_text += f'Q3 (-,-): {q_nn:2d}\n'
    quadrant_text += f'Q4 (+,-): {q_pn:2d}'

    ax.text(0.98, 0.02, quadrant_text,
            transform=ax.transAxes,
            verticalalignment='bottom',
            horizontalalignment='right',
            fontsize=10,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.9),
            zorder=5)

    # Labels and title
    ax.set_xlabel('X(t+1) [%]', fontsize=14, fontweight='bold')
    ax.set_ylabel('X(t+2) [%]', fontsize=14, fontweight='bold')

    # Main title
    main_title = f'USDJPY Rule #{rule_id}: MinMax Range Visualization'
    subtitle = f'Range: [{min_t1:+.2f}%, {max_t1:+.2f}] × [{min_t2:+.2f}%, {max_t2:+.2f}]  |  Center: ({center_t1:+.3f}%, {center_t2:+.3f}%)'
    ax.set_title(f'{main_title}\n{subtitle}', fontsize=13, fontweight='bold', pad=15)

    # Legend
    ax.legend(loc='upper right', fontsize=9, framealpha=0.95)

    # Grid
    ax.grid(True, alpha=0.3, linestyle=':', linewidth=0.5)

    # Adjust axis limits to show MinMax rectangle clearly
    padding = 0.5  # Add 0.5% padding
    max_x = max(abs(min_t1), abs(max_t1)) + padding
    max_y = max(abs(min_t2), abs(max_t2)) + padding
    max_range = max(max_x, max_y, 2.5)  # Ensure minimum 2.5% range

    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    # Tight layout
    plt.tight_layout()

    # Save
    output_file = SCATTER_DIR / f"rule_{rule_id:03d}_minmax.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"  ✓ Saved: {output_file}")

def main():
    """Main function."""
    print("=" * 70)
    print("USDJPY Rule MinMax Range Visualization")
    print("=" * 70)
    print()

    # Load all data
    all_data = load_all_data()
    print()

    # Load rules
    rules_df = load_rules()
    print()

    # Generate plots
    print(f"Generating MinMax scatter plots for {len(rules_df)} rules...")
    print()

    for idx, row in rules_df.iterrows():
        rule_id = idx + 1  # 1-indexed

        print(f"[{rule_id}/{len(rules_df)}] Processing Rule #{rule_id}...")

        # Load matched data
        matched_data = load_rule_matches(rule_id)

        if matched_data is None or len(matched_data) == 0:
            print(f"  ✗ Skipped: No match data")
            continue

        # Generate plot
        plot_cluster_with_minmax(rule_id, row, matched_data, all_data)

    print()
    print("=" * 70)
    print(f"✓ All MinMax scatter plots saved to: {SCATTER_DIR}")
    print("=" * 70)

if __name__ == "__main__":
    main()
