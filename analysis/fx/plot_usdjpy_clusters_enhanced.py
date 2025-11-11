#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USDJPY Rule Cluster Visualization (Enhanced Version)
====================================================

This script generates three types of scatter plots for all discovered rules:
1. X(t+1) vs X(t+2) - 2D cluster visualization
2. X(t+1) vs Time - Time series visualization
3. X(t+2) vs Time - Time series visualization

Features:
- Score calculation: support_rate * |mean_avg| * quadrant_concentration / sigma_avg
- Quadrant concentration analysis
- Time series context

Output:
  - 1-deta-enginnering/forex_data_daily/output/USDJPY/scatter_plots_01/
  - Three PNG files per rule
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
from pathlib import Path
from datetime import datetime

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
OUTPUT_DIR = BASE_DIR / "output/USDJPY"
VERIFICATION_DIR = OUTPUT_DIR / "verification"
RULES_FILE = OUTPUT_DIR / "pool/zrp01a.txt"
SCATTER_DIR = OUTPUT_DIR / "scatter_plots_01"

# Data file for background scatter
DATA_FILE = BASE_DIR / "USDJPY.txt"

# Create output directory
SCATTER_DIR.mkdir(exist_ok=True)

def load_all_data():
    """Load all USDJPY data for background scatter."""
    print(f"Loading all USDJPY data from {DATA_FILE}...")

    # Read all data (CSV format with comma separator)
    df = pd.read_csv(DATA_FILE, encoding='utf-8')

    # Extract X column and timestamps
    x_values = df['X'].values
    timestamps = pd.to_datetime(df['T'])

    # Create X(t+1) and X(t+2) with aligned timestamps
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

    # Parse timestamps
    timestamps = pd.to_datetime(df['Timestamp'])

    # Extract X(t+1) and X(t+2) columns
    x_t1 = df['X(t+1)'].values
    x_t2 = df['X(t+2)'].values

    return pd.DataFrame({
        'Timestamp': timestamps,
        'X_t1': x_t1,
        'X_t2': x_t2
    })

def calculate_quadrant_concentration(q_pp, q_pn, q_np, q_nn):
    """
    Calculate quadrant concentration ratio.

    Returns:
        float: Concentration ratio (0.0 to 1.0)
        - 0.25 = 完全に均等分散（各象限25%ずつ）
        - 0.50 = 半分が1象限に集中
        - 1.00 = 全てが1象限に集中（完全な方向性）
    """
    total = q_pp + q_pn + q_np + q_nn

    if total == 0:
        return 0.0

    max_count = max(q_pp, q_pn, q_np, q_nn)
    return max_count / total

def calculate_score(support_rate, mean_t1, mean_t2, sigma_t1, sigma_t2, concentration):
    """
    Calculate rule score: support_rate * |mean_avg| * quadrant_concentration / sigma_avg

    Args:
        support_rate: Support rate (0.0 to 1.0)
        mean_t1: Mean of X(t+1)
        mean_t2: Mean of X(t+2)
        sigma_t1: Sigma of X(t+1)
        sigma_t2: Sigma of X(t+2)
        concentration: Quadrant concentration (0.0 to 1.0)

    Returns:
        float: Score value
    """
    mean_avg = (abs(mean_t1) + abs(mean_t2)) / 2
    sigma_avg = (sigma_t1 + sigma_t2) / 2

    if sigma_avg > 0:
        score = support_rate * mean_avg * concentration / sigma_avg
    else:
        score = 0.0

    return score

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

def plot_xt1_xt2(rule_id, rule_row, matched_data, all_data, score, concentration):
    """Generate X(t+1) vs X(t+2) scatter plot."""

    # Extract rule info
    mean_t1 = rule_row['X(t+1)_mean']
    sigma_t1 = rule_row['X(t+1)_sigma']
    mean_t2 = rule_row['X(t+2)_mean']
    sigma_t2 = rule_row['X(t+2)_sigma']
    support_count = rule_row['support_count']
    support_rate = rule_row['support_rate']
    num_attr = rule_row['NumAttr']

    # Get attributes
    attributes = get_rule_attributes(rule_row)

    # Create figure
    fig, ax = plt.subplots(figsize=(12, 10))

    # Plot all data (gray, transparent)
    n_all = len(all_data)
    ax.scatter(all_data['X_t1'], all_data['X_t2'],
               alpha=0.3, s=15, c='gray', label=f'All data (n={n_all:,})', zorder=1)

    # Plot matched points (red, prominent)
    actual_matches = len(matched_data)
    ax.scatter(matched_data['X_t1'], matched_data['X_t2'],
               alpha=0.8, s=80, c='red', edgecolors='darkred',
               linewidths=1.5, label=f'Rule matches (n={actual_matches})', zorder=3)

    # Add mean lines
    ax.axvline(mean_t1, color='blue', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean X(t+1) = {mean_t1:.3f}%', zorder=2)
    ax.axhline(mean_t2, color='green', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean X(t+2) = {mean_t2:.3f}%', zorder=2)

    # Add origin lines
    ax.axvline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)
    ax.axhline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)

    # Add ±1σ circle
    circle_1sigma = plt.Circle((mean_t1, mean_t2),
                               (sigma_t1 + sigma_t2) / 2,
                               color='orange', fill=False,
                               linestyle='--', linewidth=2, alpha=0.5,
                               label=f'±1σ (avg={((sigma_t1+sigma_t2)/2):.3f}%)', zorder=2)
    ax.add_patch(circle_1sigma)

    # Statistics box
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Score: {score:.6f}\n'
    stats_text += f'Concentration: {concentration:.3f}\n'
    stats_text += f'\n'
    stats_text += f'Support: {support_count} matches ({support_rate:.4f})\n'
    stats_text += f'Attributes: {num_attr}\n'
    stats_text += f'\n'
    stats_text += f'X(t+1): μ={mean_t1:+.3f}%, σ={sigma_t1:.3f}%\n'
    stats_text += f'X(t+2): μ={mean_t2:+.3f}%, σ={sigma_t2:.3f}%\n'
    stats_text += f'\n'
    stats_text += f'Pattern:\n'
    for i, attr in enumerate(attributes[:5], 1):  # Limit to 5 attributes
        stats_text += f'  {i}. {attr}\n'
    if len(attributes) > 5:
        stats_text += f'  ... +{len(attributes)-5} more\n'

    # Position stats box
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

    # Labels and title
    ax.set_xlabel('X(t+1) [%]', fontsize=14, fontweight='bold')
    ax.set_ylabel('X(t+2) [%]', fontsize=14, fontweight='bold')

    main_title = f'USDJPY Rule #{rule_id}: X(t+1) vs X(t+2) (Score={score:.6f})'
    ax.set_title(main_title, fontsize=15, fontweight='bold', pad=20)

    # Legend
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)

    # Grid
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    # Adjust axis limits
    max_x = max(abs(mean_t1) + sigma_t1 * 4, 2.0)
    max_y = max(abs(mean_t2) + sigma_t2 * 4, 2.0)
    max_range = max(max_x, max_y, 3.0)

    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    plt.tight_layout()

    # Save
    output_file = SCATTER_DIR / f"rule_{rule_id:03d}_xt1_xt2.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    return output_file

def plot_time_series(rule_id, rule_row, matched_data, all_data, score, concentration, plot_type='xt1'):
    """
    Generate time series scatter plot (X(t+i) vs Time).

    Args:
        plot_type: 'xt1' for X(t+1) vs Time, 'xt2' for X(t+2) vs Time
    """

    # Extract rule info
    mean_t1 = rule_row['X(t+1)_mean']
    sigma_t1 = rule_row['X(t+1)_sigma']
    mean_t2 = rule_row['X(t+2)_mean']
    sigma_t2 = rule_row['X(t+2)_sigma']
    support_count = rule_row['support_count']
    support_rate = rule_row['support_rate']
    num_attr = rule_row['NumAttr']

    # Select data column
    if plot_type == 'xt1':
        y_col = 'X_t1'
        mean_val = mean_t1
        sigma_val = sigma_t1
        y_label = 'X(t+1) [%]'
        title_suffix = 'X(t+1) vs Time'
    else:  # xt2
        y_col = 'X_t2'
        mean_val = mean_t2
        sigma_val = sigma_t2
        y_label = 'X(t+2) [%]'
        title_suffix = 'X(t+2) vs Time'

    # Get attributes
    attributes = get_rule_attributes(rule_row)

    # Create figure
    fig, ax = plt.subplots(figsize=(16, 8))

    # Plot all data (gray, transparent)
    ax.scatter(all_data['Timestamp'], all_data[y_col],
               alpha=0.3, s=10, c='gray', label=f'All data (n={len(all_data):,})', zorder=1)

    # Plot matched points (red, prominent)
    ax.scatter(matched_data['Timestamp'], matched_data[y_col],
               alpha=0.8, s=100, c='red', edgecolors='darkred',
               linewidths=1.5, label=f'Rule matches (n={len(matched_data)})', zorder=3)

    # Add mean line
    ax.axhline(mean_val, color='blue', linestyle='--', linewidth=2,
               alpha=0.7, label=f'Mean = {mean_val:.3f}%', zorder=2)

    # Add ±1σ, ±2σ bands
    ax.axhline(mean_val + sigma_val, color='orange', linestyle=':', linewidth=1.5,
               alpha=0.5, label=f'Mean ± 1σ', zorder=2)
    ax.axhline(mean_val - sigma_val, color='orange', linestyle=':', linewidth=1.5,
               alpha=0.5, zorder=2)
    ax.axhline(mean_val + 2*sigma_val, color='purple', linestyle=':', linewidth=1,
               alpha=0.4, label=f'Mean ± 2σ', zorder=2)
    ax.axhline(mean_val - 2*sigma_val, color='purple', linestyle=':', linewidth=1,
               alpha=0.4, zorder=2)

    # Add zero line
    ax.axhline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)

    # Statistics box
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Score: {score:.6f}\n'
    stats_text += f'Concentration: {concentration:.3f}\n'
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

    # Position stats box
    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            verticalalignment='top',
            fontsize=9,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.9),
            zorder=4)

    # Labels and title
    ax.set_xlabel('Time', fontsize=14, fontweight='bold')
    ax.set_ylabel(y_label, fontsize=14, fontweight='bold')

    main_title = f'USDJPY Rule #{rule_id}: {title_suffix} (Score={score:.6f})'
    ax.set_title(main_title, fontsize=15, fontweight='bold', pad=20)

    # Legend
    ax.legend(loc='upper right', fontsize=10, framealpha=0.9)

    # Grid
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    # Format x-axis dates
    fig.autofmt_xdate()

    plt.tight_layout()

    # Save
    output_file = SCATTER_DIR / f"rule_{rule_id:03d}_{plot_type}_time.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    return output_file

def process_rule(rule_id, rule_row, all_data):
    """Process one rule and generate all three plots."""

    print(f"[{rule_id}] Processing Rule #{rule_id}...")

    # Load matched data
    matched_data = load_rule_matches(rule_id)

    if matched_data is None or len(matched_data) == 0:
        print(f"  ✗ Skipped: No match data")
        return None

    # Calculate quadrant concentration
    q_pp = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] > 0))
    q_pn = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] < 0))
    q_np = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] > 0))
    q_nn = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] < 0))

    concentration = calculate_quadrant_concentration(q_pp, q_pn, q_np, q_nn)

    # Calculate score
    score = calculate_score(
        rule_row['support_rate'],
        rule_row['X(t+1)_mean'],
        rule_row['X(t+2)_mean'],
        rule_row['X(t+1)_sigma'],
        rule_row['X(t+2)_sigma'],
        concentration
    )

    # Generate three plots
    file1 = plot_xt1_xt2(rule_id, rule_row, matched_data, all_data, score, concentration)
    file2 = plot_time_series(rule_id, rule_row, matched_data, all_data, score, concentration, 'xt1')
    file3 = plot_time_series(rule_id, rule_row, matched_data, all_data, score, concentration, 'xt2')

    print(f"  ✓ Saved: {file1.name}, {file2.name}, {file3.name}")
    print(f"     Score={score:.6f}, Concentration={concentration:.3f}")

    return {
        'rule_id': rule_id,
        'score': score,
        'concentration': concentration,
        'support_count': rule_row['support_count']
    }

def main():
    """Main function."""
    print("=" * 60)
    print("USDJPY Rule Cluster Visualization (Enhanced)")
    print("=" * 60)
    print()

    # Load all data
    all_data = load_all_data()
    print()

    # Load rules
    rules_df = load_rules()
    print()

    # Generate plots
    print(f"Generating 3 plots for each of {len(rules_df)} rules...")
    print()

    results = []
    for idx, row in rules_df.iterrows():
        rule_id = idx + 1  # 1-indexed

        result = process_rule(rule_id, row, all_data)
        if result is not None:
            results.append(result)

    print()
    print("=" * 60)
    print(f"✓ All scatter plots saved to: {SCATTER_DIR}")
    print(f"  Total files: {len(results) * 3}")
    print("=" * 60)
    print()

    # Display top 10 rules by score
    if results:
        results_df = pd.DataFrame(results)
        results_df = results_df.sort_values('score', ascending=False)

        print("Top 10 Rules by Score:")
        print("=" * 60)
        for i, row in results_df.head(10).iterrows():
            print(f"  Rule #{int(row['rule_id']):3d}: Score={row['score']:.6f}, "
                  f"Conc={row['concentration']:.3f}, Support={int(row['support_count'])}")
        print("=" * 60)

if __name__ == "__main__":
    main()
