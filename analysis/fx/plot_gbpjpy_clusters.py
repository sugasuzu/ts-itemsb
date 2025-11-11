#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GBPJPY Rule Cluster Visualization
==================================

This script generates X(t+1) vs X(t+2) scatter plots for all discovered rules.

Output:
  - 1-deta-enginnering/forex_data_daily/output/GBPJPY/scatter_plots/
  - One PNG file per rule showing cluster visualization
"""

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import os
from pathlib import Path

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
OUTPUT_DIR = BASE_DIR / "output/GBPJPY"
VERIFICATION_DIR = OUTPUT_DIR / "verification"
RULES_FILE = OUTPUT_DIR / "pool/zrp01a.txt"
SCATTER_DIR = OUTPUT_DIR / "scatter_plots"

# Data file for background scatter
DATA_FILE = BASE_DIR / "GBPJPY.txt"

# Create output directory
SCATTER_DIR.mkdir(exist_ok=True)

def load_all_data():
    """Load all GBPJPY data for background scatter."""
    print(f"Loading all GBPJPY data from {DATA_FILE}...")

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

def plot_cluster(rule_id, rule_row, matched_data, all_data):
    """Generate scatter plot for one rule."""

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

    # Add Q1 threshold line (0.10%)
    Q1_THRESHOLD = 0.10
    ax.axvline(Q1_THRESHOLD, color='orange', linestyle='-', linewidth=2,
               alpha=0.5, label=f'Q1 Threshold ({Q1_THRESHOLD}%)', zorder=2)
    ax.axhline(Q1_THRESHOLD, color='orange', linestyle='-', linewidth=2, alpha=0.5, zorder=2)

    # Add origin lines (darker, more prominent)
    ax.axvline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)
    ax.axhline(0, color='black', linestyle='-', linewidth=1.5, alpha=0.5, zorder=1)

    # Statistics box
    stats_text = f'Rule #{rule_id}\n'
    stats_text += f'━━━━━━━━━━━━━━━━━━━━\n'
    stats_text += f'Support: {support_count} matches ({support_rate:.4f})\n'
    stats_text += f'Attributes: {num_attr}\n'
    stats_text += f'\n'
    stats_text += f'X(t+1): μ={mean_t1:+.3f}%, σ={sigma_t1:.3f}%\n'
    stats_text += f'X(t+2): μ={mean_t2:+.3f}%, σ={sigma_t2:.3f}%\n'
    stats_text += f'\n'
    stats_text += f'Pattern:\n'
    for i, attr in enumerate(attributes[:5], 1):  # Show first 5 attributes
        stats_text += f'  {i}. {attr}\n'

    # Position stats box
    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            verticalalignment='top',
            fontsize=9,
            family='monospace',
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.9),
            zorder=4)

    # Quadrant counts (with Q1 threshold)
    q_pp = np.sum((matched_data['X_t1'] >= Q1_THRESHOLD) & (matched_data['X_t2'] >= Q1_THRESHOLD))
    q_pn = np.sum((matched_data['X_t1'] >= Q1_THRESHOLD) & (matched_data['X_t2'] < Q1_THRESHOLD))
    q_np = np.sum((matched_data['X_t1'] < Q1_THRESHOLD) & (matched_data['X_t2'] >= Q1_THRESHOLD))
    q_nn = np.sum((matched_data['X_t1'] < Q1_THRESHOLD) & (matched_data['X_t2'] < Q1_THRESHOLD))

    q1_precision = (q_pp / actual_matches * 100) if actual_matches > 0 else 0

    quadrant_text = f'Q1 Quadrants (≥{Q1_THRESHOLD}%):\n'
    quadrant_text += f'Q1 (++): {q_pp} ({q_pp/actual_matches*100:.1f}%)\n'
    quadrant_text += f'Q2 (-+): {q_np}\n'
    quadrant_text += f'Q3 (--): {q_nn}\n'
    quadrant_text += f'Q4 (+-): {q_pn}\n'
    quadrant_text += f'\n'
    quadrant_text += f'Q1 Precision: {q1_precision:.1f}%'

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

    # Main title with statistical notation
    main_title = f'GBPJPY Rule #{rule_id}: X(t+1) = {mean_t1:+.3f}% ± {sigma_t1:.3f}%,  X(t+2) = {mean_t2:+.3f}% ± {sigma_t2:.3f}%'
    subtitle = f'Q1 Precision: {q1_precision:.1f}% ({q_pp}/{actual_matches} matches)'
    ax.set_title(f'{main_title}\n{subtitle}', fontsize=14, fontweight='bold', pad=20)

    # Legend
    ax.legend(loc='upper left', fontsize=9, framealpha=0.9)

    # Grid
    ax.grid(True, alpha=0.2, linestyle=':', linewidth=0.5)

    # Adjust axis limits centered on origin (0, 0)
    # Calculate required range to show cluster (mean ± 4σ) and ensure minimum visibility
    max_x = max(abs(mean_t1) + sigma_t1 * 4, 2.0)
    max_y = max(abs(mean_t2) + sigma_t2 * 4, 2.0)
    max_range = max(max_x, max_y, 3.0)  # Ensure minimum 3% range

    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    # Tight layout
    plt.tight_layout()

    # Save
    output_file = SCATTER_DIR / f"rule_{rule_id:03d}_cluster.png"
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"  ✓ Saved: {output_file} (Q1 Precision: {q1_precision:.1f}%)")

def main():
    """Main function."""
    print("=" * 60)
    print("GBPJPY Rule Cluster Visualization")
    print("=" * 60)
    print()

    # Load all data
    all_data = load_all_data()
    print()

    # Load rules
    rules_df = load_rules()
    print()

    # Generate plots
    print(f"Generating scatter plots for {len(rules_df)} rules...")
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
        plot_cluster(rule_id, row, matched_data, all_data)

    print()
    print("=" * 60)
    print(f"✓ All scatter plots saved to: {SCATTER_DIR}")
    print("=" * 60)

if __name__ == "__main__":
    main()
