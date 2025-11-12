#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GBPJPY Rules Visualization (All 13 Rules)
==========================================

Generate scatter plots for all discovered rules:
1. X(t+1) vs X(t+2): 2D scatter with quadrant concentration
2. X(t+1) vs Time:   Time series view
3. X(t+2) vs Time:   Time series view

Output:
  - scatter_plots/rule_XXX_xt1_xt2.png
  - scatter_plots/rule_XXX_xt1_time.png
  - scatter_plots/rule_XXX_xt2_time.png
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
OUTPUT_DIR = BASE_DIR / "output/GBPJPY"
VERIFICATION_DIR = OUTPUT_DIR / "verification"
RULES_FILE = OUTPUT_DIR / "pool/zrp01a.txt"
SCATTER_DIR = OUTPUT_DIR / "scatter_plots"
DATA_FILE = BASE_DIR / "GBPJPY.txt"

# Quadrant threshold (象限判定閾値)
QUADRANT_THRESHOLD = 0.1  # 0.1%

# Create output directory
SCATTER_DIR.mkdir(parents=True, exist_ok=True)

def load_all_data():
    """Load all GBPJPY data."""
    print(f"Loading GBPJPY data from {DATA_FILE}...")
    df = pd.read_csv(DATA_FILE, encoding='utf-8')  # CSV format, not TSV
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

def calculate_quadrant_concentration(matched_data, threshold):
    """Calculate quadrant concentration with threshold."""
    quadrant_counts = [0, 0, 0, 0]
    in_quadrant = 0

    for _, row in matched_data.iterrows():
        t1 = row['X_t1']
        t2 = row['X_t2']

        if t1 >= threshold and t2 >= threshold:
            quadrant_counts[0] += 1
            in_quadrant += 1
        elif t1 < -threshold and t2 >= threshold:
            quadrant_counts[1] += 1
            in_quadrant += 1
        elif t1 < -threshold and t2 < -threshold:
            quadrant_counts[2] += 1
            in_quadrant += 1
        elif t1 >= threshold and t2 < -threshold:
            quadrant_counts[3] += 1
            in_quadrant += 1

    if in_quadrant == 0:
        return 0.0, 0, quadrant_counts

    max_count = max(quadrant_counts)
    dominant_quadrant = quadrant_counts.index(max_count) + 1
    concentration = max_count / in_quadrant

    return concentration, dominant_quadrant, quadrant_counts

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

def plot_xt1_xt2(rule_id, rule_row, matched_data, all_data, concentration, dominant_quadrant, quadrant_counts):
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

    # Quadrant threshold lines
    ax.axvline(QUADRANT_THRESHOLD, color='purple', linestyle=':', linewidth=1,
               alpha=0.4, label=f'Threshold (±{QUADRANT_THRESHOLD}%)', zorder=1)
    ax.axvline(-QUADRANT_THRESHOLD, color='purple', linestyle=':', linewidth=1, alpha=0.4, zorder=1)
    ax.axhline(QUADRANT_THRESHOLD, color='purple', linestyle=':', linewidth=1, alpha=0.4, zorder=1)
    ax.axhline(-QUADRANT_THRESHOLD, color='purple', linestyle=':', linewidth=1, alpha=0.4, zorder=1)

    # ±1σ circle
    circle_1sigma = plt.Circle((mean_t1, mean_t2),
                               (sigma_t1 + sigma_t2) / 2,
                               color='orange', fill=False,
                               linestyle='--', linewidth=2, alpha=0.5,
                               label=f'±1σ (avg={((sigma_t1+sigma_t2)/2):.3f}%)', zorder=2)
    ax.add_patch(circle_1sigma)

    # Statistics box
    quadrant_names = ['Q1(++)', 'Q2(-+)', 'Q3(--)', 'Q4(+-)']
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Dominant: {quadrant_names[dominant_quadrant-1]}\n'
    stats_text += f'Concentration: {concentration*100:.1f}%\n'
    stats_text += f'\n'
    stats_text += f'Support: {support_count} matches ({support_rate*100:.2f}%)\n'
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

    # Quadrant counts (with threshold)
    quadrant_text = f'In-Quadrant Counts:\n'
    quadrant_text += f'(threshold ≥ {QUADRANT_THRESHOLD}%)\n'
    quadrant_text += f'Q1(++): {quadrant_counts[0]}\n'
    quadrant_text += f'Q2(-+): {quadrant_counts[1]}\n'
    quadrant_text += f'Q3(--): {quadrant_counts[2]}\n'
    quadrant_text += f'Q4(+-): {quadrant_counts[3]}\n'
    quadrant_text += f'Total in-quadrant: {sum(quadrant_counts)}'

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
    ax.set_title(f'GBPJPY Rule #{rule_id}: X(t+1) vs X(t+2) (Concentration={concentration*100:.1f}%)',
                 fontsize=15, fontweight='bold', pad=20)
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    max_x = max(abs(mean_t1) + sigma_t1 * 4, 2.0)
    max_y = max(abs(mean_t2) + sigma_t2 * 4, 2.0)
    max_range = max(max_x, max_y, 3.0)
    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    plt.tight_layout()

    output_file = SCATTER_DIR / f"rule_{rule_id:03d}_xt1_xt2.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    return output_file

def plot_time_series(rule_id, rule_row, matched_data, all_data, plot_type='xt1'):
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

    # Threshold lines
    ax.axhline(QUADRANT_THRESHOLD, color='purple', linestyle=':', linewidth=1,
               alpha=0.3, label=f'Threshold (±{QUADRANT_THRESHOLD}%)', zorder=1)
    ax.axhline(-QUADRANT_THRESHOLD, color='purple', linestyle=':', linewidth=1, alpha=0.3, zorder=1)

    # Statistics box
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'\n'
    stats_text += f'Support: {support_count} matches ({support_rate*100:.2f}%)\n'
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
    ax.set_title(f'GBPJPY Rule #{rule_id}: {title_suffix}',
                 fontsize=15, fontweight='bold', pad=20)
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    # Set Y-axis range
    ax.set_ylim(-4.0, 4.0)

    fig.autofmt_xdate()
    plt.tight_layout()

    output_file = SCATTER_DIR / f"rule_{rule_id:03d}_{plot_type}_time.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    return output_file

def main():
    """Main function."""
    print("=" * 70)
    print("GBPJPY Rules Visualization (All 13 Rules)")
    print("=" * 70)
    print()

    # Load data
    all_data = load_all_data()
    rules_df = load_rules()
    print()

    total_rules = len(rules_df)
    print(f"Generating plots for all {total_rules} rules...")
    print()

    # Generate plots for each rule
    for idx, row in rules_df.iterrows():
        rule_id = idx + 1

        matched_data = load_rule_matches(rule_id)
        if matched_data is None or len(matched_data) == 0:
            print(f"  Rule #{rule_id}: No match data, skipping...")
            continue

        # Calculate concentration
        concentration, dominant_quadrant, quadrant_counts = calculate_quadrant_concentration(matched_data, QUADRANT_THRESHOLD)

        # Plot 1: X(t+1) vs X(t+2)
        file_2d = plot_xt1_xt2(rule_id, row, matched_data, all_data, concentration, dominant_quadrant, quadrant_counts)

        # Plot 2: X(t+1) vs Time
        file_xt1 = plot_time_series(rule_id, row, matched_data, all_data, 'xt1')

        # Plot 3: X(t+2) vs Time
        file_xt2 = plot_time_series(rule_id, row, matched_data, all_data, 'xt2')

        print(f"  [{rule_id}/{total_rules}] ✓ Rule #{rule_id}: Concentration={concentration*100:.1f}%, Dominant=Q{dominant_quadrant}")

    print()
    print("=" * 70)
    print(f"✓ All plots saved to: {SCATTER_DIR}")
    print(f"  Total files: {total_rules * 3} ({total_rules} rules × 3 plot types)")
    print("=" * 70)

if __name__ == "__main__":
    main()
