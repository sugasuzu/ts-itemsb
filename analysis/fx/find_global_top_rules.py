#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
FX Global Top Rules Finder
===========================

Find top 10 rules across ALL currency pairs for each plot type.

Output:
  - Console: Top 10 rules for each plot type
  - CSV: Global top rules ranking
"""

import pandas as pd
import numpy as np
from pathlib import Path

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")

def get_available_pairs():
    """Get list of currency pairs with pool files."""
    output_dir = BASE_DIR / "output"
    pairs = []

    for pair_dir in sorted(output_dir.iterdir()):
        if pair_dir.is_dir():
            pool_file = pair_dir / "pool/zrp01a.txt"
            if pool_file.exists():
                pairs.append(pair_dir.name)

    return pairs

def load_rules(pair):
    """Load rules from zrp01a.txt."""
    rules_file = BASE_DIR / "output" / pair / "pool/zrp01a.txt"

    if not rules_file.exists():
        return None

    df = pd.read_csv(rules_file, sep='\t', encoding='utf-8')
    return df

def load_rule_matches(pair, rule_id):
    """Load verification CSV for a specific rule."""
    csv_file = BASE_DIR / "output" / pair / "verification" / f"rule_{rule_id:03d}.csv"

    if not csv_file.exists():
        return None

    df = pd.read_csv(csv_file, encoding='utf-8')
    x_t1 = df['X(t+1)'].values
    x_t2 = df['X(t+2)'].values

    return pd.DataFrame({
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
    """Score: support_rate × mean_avg × concentration / sigma_avg"""
    mean_avg = (abs(mean_t1) + abs(mean_t2)) / 2
    sigma_avg = (sigma_t1 + sigma_t2) / 2

    if sigma_avg > 0:
        score = support_rate * mean_avg * concentration / sigma_avg
    else:
        score = 0.0

    return score

def calculate_score_xt1(support_rate, mean_t1, sigma_t1):
    """Score: support_rate × |mean_t1| × 1.0 / sigma_t1"""
    if sigma_t1 > 0:
        score = support_rate * abs(mean_t1) * 1.0 / sigma_t1
    else:
        score = 0.0

    return score

def calculate_score_xt2(support_rate, mean_t2, sigma_t2):
    """Score: support_rate × |mean_t2| × 1.0 / sigma_t2"""
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

def collect_all_scores():
    """Collect scores for all rules across all pairs."""

    pairs = get_available_pairs()

    all_scores_2d = []
    all_scores_xt1 = []
    all_scores_xt2 = []

    total_rules = 0

    print(f"Scanning {len(pairs)} currency pairs...")
    print()

    for pair in pairs:
        rules_df = load_rules(pair)
        if rules_df is None:
            continue

        pair_rule_count = 0

        for idx, row in rules_df.iterrows():
            rule_id = idx + 1

            matched_data = load_rule_matches(pair, rule_id)
            if matched_data is None or len(matched_data) == 0:
                continue

            # Calculate concentration
            q_pp = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] > 0))
            q_pn = np.sum((matched_data['X_t1'] > 0) & (matched_data['X_t2'] < 0))
            q_np = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] > 0))
            q_nn = np.sum((matched_data['X_t1'] < 0) & (matched_data['X_t2'] < 0))
            concentration = calculate_quadrant_concentration(q_pp, q_pn, q_np, q_nn)

            # Calculate scores
            score_2d = calculate_score_2d(
                row['support_rate'], row['X(t+1)_mean'], row['X(t+2)_mean'],
                row['X(t+1)_sigma'], row['X(t+2)_sigma'], concentration
            )

            score_xt1 = calculate_score_xt1(
                row['support_rate'], row['X(t+1)_mean'], row['X(t+1)_sigma']
            )

            score_xt2 = calculate_score_xt2(
                row['support_rate'], row['X(t+2)_mean'], row['X(t+2)_sigma']
            )

            # Get attributes
            attributes = get_rule_attributes(row)
            attr_summary = ', '.join(attributes[:3])
            if len(attributes) > 3:
                attr_summary += f' ... (+{len(attributes)-3})'

            # Store results
            base_info = {
                'pair': pair,
                'rule_id': rule_id,
                'support_count': row['support_count'],
                'support_rate': row['support_rate'],
                'mean_t1': row['X(t+1)_mean'],
                'sigma_t1': row['X(t+1)_sigma'],
                'mean_t2': row['X(t+2)_mean'],
                'sigma_t2': row['X(t+2)_sigma'],
                'num_attr': row['NumAttr'],
                'attributes': attr_summary
            }

            all_scores_2d.append({**base_info, 'score': score_2d, 'concentration': concentration})
            all_scores_xt1.append({**base_info, 'score': score_xt1})
            all_scores_xt2.append({**base_info, 'score': score_xt2})

            pair_rule_count += 1

        total_rules += pair_rule_count
        print(f"  {pair}: {pair_rule_count} rules")

    print()
    print(f"Total rules scanned: {total_rules}")
    print()

    return all_scores_2d, all_scores_xt1, all_scores_xt2

def display_top_rules(scores, title, top_n=10):
    """Display top N rules."""

    # Sort by score
    scores_sorted = sorted(scores, key=lambda x: x['score'], reverse=True)

    print(f"\n{'='*80}")
    print(f"{title}")
    print(f"{'='*80}")
    print()

    for i, item in enumerate(scores_sorted[:top_n], 1):
        print(f"[{i}] {item['pair']} Rule #{item['rule_id']}")
        print(f"    Score: {item['score']:.6f}")
        print(f"    Support: {item['support_count']} matches ({item['support_rate']:.4f})")
        print(f"    Mean(t+1): {item['mean_t1']:+.3f}%, Sigma(t+1): {item['sigma_t1']:.3f}%")
        print(f"    Mean(t+2): {item['mean_t2']:+.3f}%, Sigma(t+2): {item['sigma_t2']:.3f}%")
        if 'concentration' in item:
            print(f"    Concentration: {item['concentration']:.3f}")
        print(f"    Attributes ({item['num_attr']}): {item['attributes']}")
        print()

def save_to_csv(scores_2d, scores_xt1, scores_xt2):
    """Save top rules to CSV."""

    output_file = BASE_DIR / "output/global_top_rules.csv"

    # Prepare data
    df_2d = pd.DataFrame(sorted(scores_2d, key=lambda x: x['score'], reverse=True)[:10])
    df_2d['plot_type'] = 'X(t+1) vs X(t+2)'
    df_2d['rank'] = range(1, len(df_2d) + 1)

    df_xt1 = pd.DataFrame(sorted(scores_xt1, key=lambda x: x['score'], reverse=True)[:10])
    df_xt1['plot_type'] = 'X(t+1) vs Time'
    df_xt1['rank'] = range(1, len(df_xt1) + 1)

    df_xt2 = pd.DataFrame(sorted(scores_xt2, key=lambda x: x['score'], reverse=True)[:10])
    df_xt2['plot_type'] = 'X(t+2) vs Time'
    df_xt2['rank'] = range(1, len(df_xt2) + 1)

    # Combine
    df_all = pd.concat([df_2d, df_xt1, df_xt2], ignore_index=True)

    # Reorder columns
    columns = ['plot_type', 'rank', 'pair', 'rule_id', 'score', 'support_count',
               'support_rate', 'mean_t1', 'sigma_t1', 'mean_t2', 'sigma_t2',
               'num_attr', 'attributes']
    if 'concentration' in df_all.columns:
        columns.insert(5, 'concentration')

    df_all = df_all[columns]

    # Save
    df_all.to_csv(output_file, index=False)
    print(f"\n{'='*80}")
    print(f"✓ Results saved to: {output_file}")
    print(f"{'='*80}")

def main():
    """Main function."""

    print("="*80)
    print("FX Global Top Rules Finder")
    print("="*80)
    print()

    # Collect all scores
    scores_2d, scores_xt1, scores_xt2 = collect_all_scores()

    # Display top 10 for each type
    display_top_rules(scores_2d, "Global Top 10: X(t+1) vs X(t+2) (2D Cluster Quality)", 10)
    display_top_rules(scores_xt1, "Global Top 10: X(t+1) vs Time (t+1 Directional Strength)", 10)
    display_top_rules(scores_xt2, "Global Top 10: X(t+2) vs Time (t+2 Directional Strength)", 10)

    # Save to CSV
    save_to_csv(scores_2d, scores_xt1, scores_xt2)

if __name__ == "__main__":
    main()
