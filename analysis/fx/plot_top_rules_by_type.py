#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USDJPY Top Rules Visualization by Plot Type
============================================

Generate plots for top-scored rules, with different scoring for each plot type:
1. X(t+1) vs X(t+2): support_rate × mean_avg × concentration / sigma_avg
2. X(t+1) vs Time:   support_rate × |mean_t1| × 1.0 / sigma_t1 (no quadrant)
3. X(t+2) vs Time:   support_rate × |mean_t2| × 1.0 / sigma_t2 (no quadrant)

Output:
  - scatter_plots_01/best_xt1_xt2/
  - scatter_plots_01/best_xt1_time/
  - scatter_plots_01/best_xt2_time/
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
OUTPUT_DIR = BASE_DIR / "output/USDJPY"
VERIFICATION_DIR = OUTPUT_DIR / "verification"
RULES_FILE = OUTPUT_DIR / "pool/zrp01a.txt"
BASE_SCATTER_DIR = OUTPUT_DIR / "scatter_plots_01"
DATA_FILE = BASE_DIR / "USDJPY.txt"

# Number of top rules per plot type
TOP_N = 10

# Create output directories
SCATTER_DIR_2D = BASE_SCATTER_DIR / "best_xt1_xt2"
SCATTER_DIR_XT1 = BASE_SCATTER_DIR / "best_xt1_time"
SCATTER_DIR_XT2 = BASE_SCATTER_DIR / "best_xt2_time"

SCATTER_DIR_2D.mkdir(parents=True, exist_ok=True)
SCATTER_DIR_XT1.mkdir(parents=True, exist_ok=True)
SCATTER_DIR_XT2.mkdir(parents=True, exist_ok=True)

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

def load_rules():
    """Load rules from zrp01a.txt."""
    print(f"Loading rules from {RULES_FILE}...")
    df = pd.read_csv(RULES_FILE, sep='\t', encoding='utf-8')
    print(f"  Total rules: {len(df)}")
    return df

def load_rule_matches(rule_id):
    """Load verification CSV for a specific rule."""
    csv_file = VERIFICATION_DIR / f"rule_{rule_id:03d}.csv"

    if not csv_file.exists():
        return None

    df = pd.read_csv(csv_file, encoding='utf-8')
    timestamps = pd.to_datetime(df['Timestamp'])
    x_t1 = df['X(t+1)'].values
    x_t2 = df['X(t+2)'].values

    return pd.DataFrame({
        'Timestamp': timestamps,
        'X_t1': x_t1,
        'X_t2': x_t2
    })

def calculate_quadrant_concentration(q_pp, q_pn, q_np, q_nn):
    """Calculate quadrant concentration ratio."""
    total = q_pp + q_pn + q_np + q_nn
    if total == 0:
        return 0.0
    max_count = max(q_pp, q_pn, q_np, q_nn)
    return max_count / total

def calculate_score_2d(support_rate, mean_t1, mean_t2, sigma_t1, sigma_t2, concentration):
    """
    Score for X(t+1) vs X(t+2) plot.
    Formula: support_rate × mean_avg × concentration / sigma_avg
    """
    mean_avg = (abs(mean_t1) + abs(mean_t2)) / 2
    sigma_avg = (sigma_t1 + sigma_t2) / 2

    if sigma_avg > 0:
        score = support_rate * mean_avg * concentration / sigma_avg
    else:
        score = 0.0

    return score

def calculate_score_xt1(support_rate, mean_t1, sigma_t1):
    """
    Score for X(t+1) vs Time plot.
    Formula: support_rate × |mean_t1| × 1.0 / sigma_t1
    concentration = 1.0 (no quadrant concept in 1D)
    """
    if sigma_t1 > 0:
        score = support_rate * abs(mean_t1) * 1.0 / sigma_t1
    else:
        score = 0.0

    return score

def calculate_score_xt2(support_rate, mean_t2, sigma_t2):
    """
    Score for X(t+2) vs Time plot.
    Formula: support_rate × |mean_t2| × 1.0 / sigma_t2
    concentration = 1.0 (no quadrant concept in 1D)
    """
    if sigma_t2 > 0:
        score = support_rate * abs(mean_t2) * 1.0 / sigma_t2
    else:
        score = 0.0

    return score

def get_rule_attributes(row):
    """Extract rule attributes."""
    attributes = []
    for i in range(1, 9):
        attr_name = f'Attr{i}'
        if attr_name in row.index:
            value = row[attr_name]
            if pd.notna(value) and str(value) != '0':
                attributes.append(str(value))
    return attributes

def plot_xt1_xt2(rule_id, rule_row, matched_data, all_data, score, concentration, output_dir):
    """Generate X(t+1) vs X(t+2) scatter plot."""

    mean_t1 = rule_row['X(t+1)_mean']
    sigma_t1 = rule_row['X(t+1)_sigma']
    mean_t2 = rule_row['X(t+2)_mean']
    sigma_t2 = rule_row['X(t+2)_sigma']
    support_count = rule_row['support_count']
    support_rate = rule_row['support_rate']
    num_attr = rule_row['NumAttr']
    attributes = get_rule_attributes(rule_row)

    fig, ax = plt.subplots(figsize=(12, 10))

    # Background: all data
    ax.scatter(all_data['X_t1'], all_data['X_t2'],
               alpha=0.3, s=15, c='gray', label=f'All data (n={len(all_data):,})', zorder=1)

    # Foreground: matched points
    ax.scatter(matched_data['X_t1'], matched_data['X_t2'],
               alpha=0.8, s=80, c='red', edgecolors='darkred',
               linewidths=1.5, label=f'Rule matches (n={len(matched_data)})', zorder=3)

    # Mean lines
    ax.axvline(mean_t1, color='blue', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean X(t+1) = {mean_t1:.3f}%', zorder=2)
    ax.axhline(mean_t2, color='green', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean X(t+2) = {mean_t2:.3f}%', zorder=2)

    # Origin lines
    ax.axvline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)
    ax.axhline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)

    # ±1σ circle
    circle_1sigma = plt.Circle((mean_t1, mean_t2),
                               (sigma_t1 + sigma_t2) / 2,
                               color='orange', fill=False,
                               linestyle='--', linewidth=2, alpha=0.5,
                               label=f'±1σ (avg={((sigma_t1+sigma_t2)/2):.3f}%)', zorder=2)
    ax.add_patch(circle_1sigma)

    # Statistics box
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Score (2D): {score:.6f}\n'
    stats_text += f'Concentration: {concentration:.3f}\n'
    stats_text += f'\n'
    stats_text += f'Support: {support_count} matches ({support_rate:.4f})\n'
    stats_text += f'Attributes: {num_attr}\n'
    stats_text += f'\n'
    stats_text += f'X(t+1): μ={mean_t1:+.3f}%, σ={sigma_t1:.3f}%\n'
    stats_text += f'X(t+2): μ={mean_t2:+.3f}%, σ={sigma_t2:.3f}%\n'
    stats_text += f'\n'
    stats_text += f'Pattern:\n'
    for i, attr in enumerate(attributes[:5], 1):
        stats_text += f'  {i}. {attr}\n'
    if len(attributes) > 5:
        stats_text += f'  ... +{len(attributes)-5} more\n'

    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            verticalalignment='top',
            fontsize=9,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.9),
            zorder=4)

    # Quadrant counts
    q_pp = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] > 0))
    q_pn = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] < 0))
    q_np = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] > 0))
    q_nn = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] < 0))

    quadrant_text = f'Quadrants:\n'
    quadrant_text += f'(+,+): {q_pp}\n'
    quadrant_text += f'(+,-): {q_pn}\n'
    quadrant_text += f'(-,+): {q_np}\n'
    quadrant_text += f'(-,-): {q_nn}'

    ax.text(0.98, 0.02, quadrant_text,
            transform=ax.transAxes,
            verticalalignment='bottom',
            horizontalalignment='right',
            fontsize=10,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8),
            zorder=4)

    ax.set_xlabel('X(t+1) [%]', fontsize=14, fontweight='bold')
    ax.set_ylabel('X(t+2) [%]', fontsize=14, fontweight='bold')
    ax.set_title(f'USDJPY Rule #{rule_id}: X(t+1) vs X(t+2) (Score={score:.6f})',
                 fontsize=15, fontweight='bold', pad=20)
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    max_x = max(abs(mean_t1) + sigma_t1 * 4, 2.0)
    max_y = max(abs(mean_t2) + sigma_t2 * 4, 2.0)
    max_range = max(max_x, max_y, 3.0)
    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    plt.tight_layout()

    output_file = output_dir / f"rule_{rule_id:03d}_xt1_xt2.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    return output_file

def plot_time_series(rule_id, rule_row, matched_data, all_data, score, output_dir, plot_type='xt1'):
    """Generate time series scatter plot."""

    mean_t1 = rule_row['X(t+1)_mean']
    sigma_t1 = rule_row['X(t+1)_sigma']
    mean_t2 = rule_row['X(t+2)_mean']
    sigma_t2 = rule_row['X(t+2)_sigma']
    support_count = rule_row['support_count']
    support_rate = rule_row['support_rate']
    num_attr = rule_row['NumAttr']
    attributes = get_rule_attributes(rule_row)

    if plot_type == 'xt1':
        y_col = 'X_t1'
        mean_val = mean_t1
        sigma_val = sigma_t1
        y_label = 'X(t+1) [%]'
        title_suffix = 'X(t+1) vs Time'
    else:
        y_col = 'X_t2'
        mean_val = mean_t2
        sigma_val = sigma_t2
        y_label = 'X(t+2) [%]'
        title_suffix = 'X(t+2) vs Time'

    fig, ax = plt.subplots(figsize=(16, 8))

    # Background: all data
    ax.scatter(all_data['Timestamp'], all_data[y_col],
               alpha=0.3, s=10, c='gray', label=f'All data (n={len(all_data):,})', zorder=1)

    # Foreground: matched points
    ax.scatter(matched_data['Timestamp'], matched_data[y_col],
               alpha=0.8, s=100, c='red', edgecolors='darkred',
               linewidths=1.5, label=f'Rule matches (n={len(matched_data)})', zorder=3)

    # Mean line
    ax.axhline(mean_val, color='blue', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean = {mean_val:.3f}%', zorder=2)

    # ±1σ, ±2σ bands
    ax.axhline(mean_val + sigma_val, color='orange', linestyle=':', linewidth=1.5,
               alpha=0.5, label=f'Mean ± 1σ', zorder=2)
    ax.axhline(mean_val - sigma_val, color='orange', linestyle=':', linewidth=1.5,
               alpha=0.5, zorder=2)
    ax.axhline(mean_val + 2*sigma_val, color='purple', linestyle=':', linewidth=1,
               alpha=0.4, label=f'Mean ± 2σ', zorder=2)
    ax.axhline(mean_val - 2*sigma_val, color='purple', linestyle=':', linewidth=1,
               alpha=0.4, zorder=2)

    # Zero line
    ax.axhline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)

    # Statistics box (concentration = 1.0 for time series)
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Score (1D): {score:.6f}\n'
    stats_text += f'Concentration: 1.0 (N/A)\n'
    stats_text += f'\n'
    stats_text += f'Support: {support_count} matches ({support_rate:.4f})\n'
    stats_text += f'Attributes: {num_attr}\n'
    stats_text += f'\n'
    stats_text += f'{y_label}: μ={mean_val:+.3f}%, σ={sigma_val:.3f}%\n'
    stats_text += f'\n'
    stats_text += f'Pattern:\n'
    for i, attr in enumerate(attributes[:5], 1):
        stats_text += f'  {i}. {attr}\n'
    if len(attributes) > 5:
        stats_text += f'  ... +{len(attributes)-5} more\n'

    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            verticalalignment='top',
            fontsize=9,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.9),
            zorder=4)

    ax.set_xlabel('Time', fontsize=14, fontweight='bold')
    ax.set_ylabel(y_label, fontsize=14, fontweight='bold')
    ax.set_title(f'USDJPY Rule #{rule_id}: {title_suffix} (Score={score:.6f})',
                 fontsize=15, fontweight='bold', pad=20)
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    # Set Y-axis range: -4% to +4% (centered at 0)
    ax.set_ylim(-4.0, 4.0)

    fig.autofmt_xdate()
    plt.tight_layout()

    output_file = output_dir / f"rule_{rule_id:03d}_{plot_type}_time.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    return output_file

def main():
    """Main function."""
    print("=" * 60)
    print("USDJPY Top Rules Visualization by Plot Type")
    print("=" * 60)
    print()

    # Load data
    all_data = load_all_data()
    rules_df = load_rules()
    print()

    # Calculate scores for all rules (all 3 types)
    print("Calculating scores for all rules (3 types)...")
    scores_2d = []
    scores_xt1 = []
    scores_xt2 = []

    for idx, row in rules_df.iterrows():
        rule_id = idx + 1

        matched_data = load_rule_matches(rule_id)
        if matched_data is None or len(matched_data) == 0:
            continue

        # Calculate concentration (for 2D plot only)
        q_pp = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] > 0))
        q_pn = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] < 0))
        q_np = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] > 0))
        q_nn = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] < 0))
        concentration = calculate_quadrant_concentration(q_pp, q_pn, q_np, q_nn)

        # Score 1: X(t+1) vs X(t+2) - 2D with quadrant concentration
        score_2d = calculate_score_2d(
            row['support_rate'],
            row['X(t+1)_mean'],
            row['X(t+2)_mean'],
            row['X(t+1)_sigma'],
            row['X(t+2)_sigma'],
            concentration
        )

        # Score 2: X(t+1) vs Time - 1D, concentration = 1.0
        score_xt1 = calculate_score_xt1(
            row['support_rate'],
            row['X(t+1)_mean'],
            row['X(t+1)_sigma']
        )

        # Score 3: X(t+2) vs Time - 1D, concentration = 1.0
        score_xt2 = calculate_score_xt2(
            row['support_rate'],
            row['X(t+2)_mean'],
            row['X(t+2)_sigma']
        )

        scores_2d.append({
            'rule_id': rule_id,
            'score': score_2d,
            'concentration': concentration,
            'row': row
        })

        scores_xt1.append({
            'rule_id': rule_id,
            'score': score_xt1,
            'row': row
        })

        scores_xt2.append({
            'rule_id': rule_id,
            'score': score_xt2,
            'row': row
        })

    print(f"  Calculated scores for {len(scores_2d)} rules")
    print()

    # Sort each list by score
    scores_2d.sort(key=lambda x: x['score'], reverse=True)
    scores_xt1.sort(key=lambda x: x['score'], reverse=True)
    scores_xt2.sort(key=lambda x: x['score'], reverse=True)

    # Display top rules for each type
    print(f"Top {TOP_N} Rules for X(t+1) vs X(t+2):")
    print("=" * 60)
    for i, item in enumerate(scores_2d[:TOP_N], 1):
        print(f"  {i:2d}. Rule #{item['rule_id']:3d}: Score={item['score']:.6f}, "
              f"Conc={item['concentration']:.3f}")
    print()

    print(f"Top {TOP_N} Rules for X(t+1) vs Time:")
    print("=" * 60)
    for i, item in enumerate(scores_xt1[:TOP_N], 1):
        print(f"  {i:2d}. Rule #{item['rule_id']:3d}: Score={item['score']:.6f}")
    print()

    print(f"Top {TOP_N} Rules for X(t+2) vs Time:")
    print("=" * 60)
    for i, item in enumerate(scores_xt2[:TOP_N], 1):
        print(f"  {i:2d}. Rule #{item['rule_id']:3d}: Score={item['score']:.6f}")
    print("=" * 60)
    print()

    # Generate plots
    print(f"Generating plots for top {TOP_N} rules of each type...")
    print()

    # Type 1: X(t+1) vs X(t+2)
    print(f"[1/3] Processing X(t+1) vs X(t+2) plots...")
    for i, item in enumerate(scores_2d[:TOP_N], 1):
        rule_id = item['rule_id']
        matched_data = load_rule_matches(rule_id)
        file = plot_xt1_xt2(rule_id, item['row'], matched_data, all_data,
                            item['score'], item['concentration'], SCATTER_DIR_2D)
        print(f"  [{i}/{TOP_N}] ✓ {file.name}")

    # Type 2: X(t+1) vs Time
    print(f"\n[2/3] Processing X(t+1) vs Time plots...")
    for i, item in enumerate(scores_xt1[:TOP_N], 1):
        rule_id = item['rule_id']
        matched_data = load_rule_matches(rule_id)
        file = plot_time_series(rule_id, item['row'], matched_data, all_data,
                                item['score'], SCATTER_DIR_XT1, 'xt1')
        print(f"  [{i}/{TOP_N}] ✓ {file.name}")

    # Type 3: X(t+2) vs Time
    print(f"\n[3/3] Processing X(t+2) vs Time plots...")
    for i, item in enumerate(scores_xt2[:TOP_N], 1):
        rule_id = item['rule_id']
        matched_data = load_rule_matches(rule_id)
        file = plot_time_series(rule_id, item['row'], matched_data, all_data,
                                item['score'], SCATTER_DIR_XT2, 'xt2')
        print(f"  [{i}/{TOP_N}] ✓ {file.name}")

    print()
    print("=" * 60)
    print("✓ All plots saved:")
    print(f"  X(t+1) vs X(t+2): {SCATTER_DIR_2D} ({TOP_N} files)")
    print(f"  X(t+1) vs Time:   {SCATTER_DIR_XT1} ({TOP_N} files)")
    print(f"  X(t+2) vs Time:   {SCATTER_DIR_XT2} ({TOP_N} files)")
    print(f"  Total: {TOP_N * 3} files")
    print("=" * 60)

if __name__ == "__main__":
    main()
