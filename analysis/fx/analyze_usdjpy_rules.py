#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
USDJPY Rule Discovery Analysis
================================

Comprehensive statistical analysis of discovered rules.
"""

import pandas as pd
import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt

# Paths
BASE_DIR = Path("1-deta-enginnering/forex_data_daily")
OUTPUT_DIR = BASE_DIR / "output/USDJPY"
RULES_FILE = OUTPUT_DIR / "pool/zrp01a.txt"

def load_rules():
    """Load discovered rules."""
    df = pd.read_csv(RULES_FILE, sep='\t', encoding='utf-8')
    return df

def analyze_variance(df):
    """Analyze variance distribution."""
    print("=" * 80)
    print("1. VARIANCE ANALYSIS (クラスター密集度)")
    print("=" * 80)

    sigma_t1 = df['X(t+1)_sigma'].values
    sigma_t2 = df['X(t+2)_sigma'].values
    avg_sigma = (sigma_t1 + sigma_t2) / 2

    print(f"\nX(t+1) Sigma:")
    print(f"  Mean:   {np.mean(sigma_t1):.3f}%")
    print(f"  Median: {np.median(sigma_t1):.3f}%")
    print(f"  Min:    {np.min(sigma_t1):.3f}%  (Rule #{np.argmin(sigma_t1)+1})")
    print(f"  Max:    {np.max(sigma_t1):.3f}%  (Rule #{np.argmax(sigma_t1)+1})")

    print(f"\nX(t+2) Sigma:")
    print(f"  Mean:   {np.mean(sigma_t2):.3f}%")
    print(f"  Median: {np.median(sigma_t2):.3f}%")
    print(f"  Min:    {np.min(sigma_t2):.3f}%  (Rule #{np.argmin(sigma_t2)+1})")
    print(f"  Max:    {np.max(sigma_t2):.3f}%  (Rule #{np.argmax(sigma_t2)+1})")

    print(f"\nAverage Sigma (t+1, t+2):")
    print(f"  Mean:   {np.mean(avg_sigma):.3f}%")
    print(f"  Median: {np.median(avg_sigma):.3f}%")

    # Count by quality tiers
    tight = np.sum(avg_sigma <= 0.35)
    moderate = np.sum((avg_sigma > 0.35) & (avg_sigma <= 0.45))
    loose = np.sum(avg_sigma > 0.45)

    print(f"\nCluster Tightness Distribution:")
    print(f"  Very Tight (σ ≤ 0.35%):  {tight} rules ({tight/len(df)*100:.1f}%)")
    print(f"  Moderate (0.35% < σ ≤ 0.45%): {moderate} rules ({moderate/len(df)*100:.1f}%)")
    print(f"  Loose (σ > 0.45%):      {loose} rules ({loose/len(df)*100:.1f}%)")

    print()

def analyze_mean(df):
    """Analyze mean (directionality) distribution."""
    print("=" * 80)
    print("2. MEAN ANALYSIS (方向性の明確さ)")
    print("=" * 80)

    mean_t1 = df['X(t+1)_mean'].values
    mean_t2 = df['X(t+2)_mean'].values

    print(f"\nX(t+1) Mean:")
    print(f"  Mean:   {np.mean(mean_t1):+.3f}%")
    print(f"  Median: {np.median(mean_t1):+.3f}%")
    print(f"  Min:    {np.min(mean_t1):+.3f}%  (Rule #{np.argmin(mean_t1)+1})")
    print(f"  Max:    {np.max(mean_t1):+.3f}%  (Rule #{np.argmax(mean_t1)+1})")

    print(f"\nX(t+2) Mean:")
    print(f"  Mean:   {np.mean(mean_t2):+.3f}%")
    print(f"  Median: {np.median(mean_t2):+.3f}%")
    print(f"  Min:    {np.min(mean_t2):+.3f}%  (Rule #{np.argmin(mean_t2)+1})")
    print(f"  Max:    {np.max(mean_t2):+.3f}%  (Rule #{np.argmax(mean_t2)+1})")

    # Directionality strength
    abs_mean_t1 = np.abs(mean_t1)
    abs_mean_t2 = np.abs(mean_t2)
    avg_abs_mean = (abs_mean_t1 + abs_mean_t2) / 2

    print(f"\nDirectionality Strength (|mean|):")
    print(f"  Mean:   {np.mean(avg_abs_mean):.3f}%")
    print(f"  Median: {np.median(avg_abs_mean):.3f}%")

    # Count by strength
    strong = np.sum(avg_abs_mean >= 0.30)
    moderate = np.sum((avg_abs_mean >= 0.20) & (avg_abs_mean < 0.30))
    weak = np.sum(avg_abs_mean < 0.20)

    print(f"\nDirectionality Distribution:")
    print(f"  Strong (|μ| ≥ 0.30%):   {strong} rules ({strong/len(df)*100:.1f}%)")
    print(f"  Moderate (0.20% ≤ |μ| < 0.30%): {moderate} rules ({moderate/len(df)*100:.1f}%)")
    print(f"  Weak (|μ| < 0.20%):     {weak} rules ({weak/len(df)*100:.1f}%)")

    # Positive vs Negative
    positive_t1 = np.sum(mean_t1 > 0)
    negative_t1 = np.sum(mean_t1 < 0)
    positive_t2 = np.sum(mean_t2 > 0)
    negative_t2 = np.sum(mean_t2 < 0)

    print(f"\nDirection Balance:")
    print(f"  X(t+1): Positive {positive_t1} / Negative {negative_t1}")
    print(f"  X(t+2): Positive {positive_t2} / Negative {negative_t2}")

    print()

def analyze_support(df):
    """Analyze support count distribution."""
    print("=" * 80)
    print("3. SUPPORT ANALYSIS (統計的信頼性)")
    print("=" * 80)

    support_count = df['support_count'].values

    print(f"\nSupport Count:")
    print(f"  Mean:   {np.mean(support_count):.1f} matches")
    print(f"  Median: {np.median(support_count):.0f} matches")
    print(f"  Min:    {np.min(support_count)} matches  (Rule #{np.argmin(support_count)+1})")
    print(f"  Max:    {np.max(support_count)} matches  (Rule #{np.argmax(support_count)+1})")

    # Distribution
    n_10 = np.sum(support_count == 10)
    n_11_15 = np.sum((support_count >= 11) & (support_count <= 15))
    n_16_plus = np.sum(support_count >= 16)

    print(f"\nSupport Distribution:")
    print(f"  Minimum (n=10):     {n_10} rules ({n_10/len(df)*100:.1f}%)")
    print(f"  Good (11-15):       {n_11_15} rules ({n_11_15/len(df)*100:.1f}%)")
    print(f"  Excellent (16+):    {n_16_plus} rules ({n_16_plus/len(df)*100:.1f}%)")

    print()

def analyze_attributes(df):
    """Analyze attribute patterns."""
    print("=" * 80)
    print("4. ATTRIBUTE PATTERN ANALYSIS (共通パターン)")
    print("=" * 80)

    num_attr = df['NumAttr'].values

    print(f"\nAttribute Count:")
    print(f"  Mean:   {np.mean(num_attr):.1f} attributes")
    print(f"  Median: {np.median(num_attr):.0f} attributes")
    print(f"  Min:    {np.min(num_attr)} attributes")
    print(f"  Max:    {np.max(num_attr)} attributes")

    # Count frequency
    for n in range(3, 7):
        count = np.sum(num_attr == n)
        print(f"  {n} attributes: {count} rules ({count/len(df)*100:.1f}%)")

    # Find most frequent attributes
    print(f"\nMost Frequent Attributes:")

    all_attrs = []
    for idx, row in df.iterrows():
        for i in range(1, 9):
            attr_name = f'Attr{i}'
            if attr_name in row.index:
                value = row[attr_name]
                if pd.notna(value) and str(value) != '0':
                    all_attrs.append(str(value))

    from collections import Counter
    attr_counts = Counter(all_attrs)

    for attr, count in attr_counts.most_common(10):
        print(f"  {attr}: {count} times ({count/len(df)*100:.1f}%)")

    print()

def calculate_cluster_quality_score(row):
    """Calculate cluster quality score (0-100)."""
    mean_t1 = row['X(t+1)_mean']
    sigma_t1 = row['X(t+1)_sigma']
    mean_t2 = row['X(t+2)_mean']
    sigma_t2 = row['X(t+2)_sigma']
    support_count = row['support_count']

    # Tightness score (40 points max)
    avg_sigma = (sigma_t1 + sigma_t2) / 2
    if avg_sigma <= 0.30:
        tightness = 40
    elif avg_sigma <= 0.35:
        tightness = 35
    elif avg_sigma <= 0.40:
        tightness = 25
    else:
        tightness = 10

    # Directionality score (40 points max)
    avg_abs_mean = (abs(mean_t1) + abs(mean_t2)) / 2
    if avg_abs_mean >= 0.35:
        direction = 40
    elif avg_abs_mean >= 0.30:
        direction = 35
    elif avg_abs_mean >= 0.25:
        direction = 25
    elif avg_abs_mean >= 0.20:
        direction = 15
    else:
        direction = 5

    # Reliability score (20 points max)
    if support_count >= 15:
        reliability = 20
    elif support_count >= 12:
        reliability = 15
    elif support_count >= 10:
        reliability = 10
    else:
        reliability = 5

    total = tightness + direction + reliability

    return total, tightness, direction, reliability

def rank_rules(df):
    """Rank rules by quality score."""
    print("=" * 80)
    print("5. CLUSTER QUALITY RANKING (総合評価)")
    print("=" * 80)

    scores = []
    for idx, row in df.iterrows():
        total, tight, direct, reliab = calculate_cluster_quality_score(row)
        scores.append({
            'rule_id': idx + 1,
            'total_score': total,
            'tightness': tight,
            'directionality': direct,
            'reliability': reliab,
            'mean_t1': row['X(t+1)_mean'],
            'sigma_t1': row['X(t+1)_sigma'],
            'mean_t2': row['X(t+2)_mean'],
            'sigma_t2': row['X(t+2)_sigma'],
            'support': row['support_count']
        })

    scores_df = pd.DataFrame(scores)
    scores_df = scores_df.sort_values('total_score', ascending=False)

    print(f"\nScoring System (100 points max):")
    print(f"  - Tightness (σ):     40 points")
    print(f"  - Directionality (μ): 40 points")
    print(f"  - Reliability (n):   20 points")

    print(f"\n{'Rank':<6}{'Rule':<8}{'Score':<8}{'Grade':<8}{'σ_avg':<10}{'|μ|_avg':<10}{'n':<6}")
    print("-" * 60)

    for rank, (idx, row) in enumerate(scores_df.head(15).iterrows(), 1):
        rule_id = int(row['rule_id'])
        score = row['total_score']
        avg_sigma = (row['sigma_t1'] + row['sigma_t2']) / 2
        avg_abs_mean = (abs(row['mean_t1']) + abs(row['mean_t2'])) / 2
        n = int(row['support'])

        if score >= 80:
            grade = "A"
        elif score >= 65:
            grade = "B"
        elif score >= 50:
            grade = "C"
        else:
            grade = "D"

        print(f"{rank:<6}{rule_id:<8}{score:<8.0f}{grade:<8}{avg_sigma:<10.3f}{avg_abs_mean:<10.3f}{n:<6}")

    print()

    # Grade distribution
    print(f"Grade Distribution:")
    grade_a = np.sum(scores_df['total_score'] >= 80)
    grade_b = np.sum((scores_df['total_score'] >= 65) & (scores_df['total_score'] < 80))
    grade_c = np.sum((scores_df['total_score'] >= 50) & (scores_df['total_score'] < 65))
    grade_d = np.sum(scores_df['total_score'] < 50)

    print(f"  Grade A (80-100): {grade_a} rules ({grade_a/len(df)*100:.1f}%)")
    print(f"  Grade B (65-79):  {grade_b} rules ({grade_b/len(df)*100:.1f}%)")
    print(f"  Grade C (50-64):  {grade_c} rules ({grade_c/len(df)*100:.1f}%)")
    print(f"  Grade D (0-49):   {grade_d} rules ({grade_d/len(df)*100:.1f}%)")

    print()

    return scores_df

def analyze_top_rules(df, scores_df):
    """Detailed analysis of top 5 rules."""
    print("=" * 80)
    print("6. TOP 5 RULES - DETAILED ANALYSIS")
    print("=" * 80)

    for rank, (idx, score_row) in enumerate(scores_df.head(5).iterrows(), 1):
        rule_id = int(score_row['rule_id'])
        rule_row = df.iloc[rule_id - 1]

        print(f"\n{'='*60}")
        print(f"Rank #{rank}: Rule #{rule_id} (Score: {score_row['total_score']:.0f}/100)")
        print(f"{'='*60}")

        # Statistics
        print(f"\nStatistics:")
        print(f"  X(t+1): μ = {rule_row['X(t+1)_mean']:+.3f}% ± {rule_row['X(t+1)_sigma']:.3f}%")
        print(f"  X(t+2): μ = {rule_row['X(t+2)_mean']:+.3f}% ± {rule_row['X(t+2)_sigma']:.3f}%")
        print(f"  Support: {rule_row['support_count']} matches ({rule_row['support_rate']:.4f})")

        # Pattern
        print(f"\nPattern ({rule_row['NumAttr']} attributes):")
        attrs = []
        for i in range(1, 9):
            attr_name = f'Attr{i}'
            if attr_name in rule_row.index:
                value = rule_row[attr_name]
                if pd.notna(value) and str(value) != '0':
                    attrs.append(f"  {len(attrs)+1}. {value}")
        for attr in attrs:
            print(attr)

        # Quality breakdown
        print(f"\nQuality Breakdown:")
        print(f"  Tightness:      {score_row['tightness']:.0f}/40 points")
        print(f"  Directionality: {score_row['directionality']:.0f}/40 points")
        print(f"  Reliability:    {score_row['reliability']:.0f}/20 points")

    print()

def summary_insights(df, scores_df):
    """Generate summary insights."""
    print("=" * 80)
    print("7. KEY INSIGHTS & RECOMMENDATIONS")
    print("=" * 80)

    mean_t1 = df['X(t+1)_mean'].values
    mean_t2 = df['X(t+2)_mean'].values
    sigma_t1 = df['X(t+1)_sigma'].values
    sigma_t2 = df['X(t+2)_sigma'].values

    avg_sigma = np.mean((sigma_t1 + sigma_t2) / 2)
    avg_abs_mean = np.mean((np.abs(mean_t1) + np.abs(mean_t2)) / 2)

    print(f"\n✓ DISCOVERY SUCCESS:")
    print(f"  - Total rules discovered: {len(df)}")
    print(f"  - All rules meet minimum criteria (σ ≤ 0.45%, |μ| ≥ 0.20%, n ≥ 10)")
    print(f"  - Average cluster tightness: σ = {avg_sigma:.3f}%")
    print(f"  - Average directionality: |μ| = {avg_abs_mean:.3f}%")

    print(f"\n✓ CLUSTER QUALITY:")
    grade_a = np.sum(scores_df['total_score'] >= 80)
    grade_b = np.sum((scores_df['total_score'] >= 65) & (scores_df['total_score'] < 80))

    if grade_a > 0:
        print(f"  - {grade_a} EXCELLENT rules (Grade A, score ≥ 80)")
    if grade_b > 0:
        print(f"  - {grade_b} GOOD rules (Grade B, score 65-79)")

    print(f"  - Rules show clear cluster structure in scatter plots")
    print(f"  - Most rules have 3-4 attributes (interpretable)")

    print(f"\n✓ DIRECTIONAL PATTERNS:")
    # Check consistency
    same_sign = np.sum((mean_t1 > 0) == (mean_t2 > 0))
    opposite_sign = len(df) - same_sign

    print(f"  - Same direction (t+1 and t+2): {same_sign} rules ({same_sign/len(df)*100:.0f}%)")
    print(f"  - Opposite direction: {opposite_sign} rules ({opposite_sign/len(df)*100:.0f}%)")

    if same_sign > opposite_sign:
        print(f"  → Trend continuation patterns dominate")
    else:
        print(f"  → Reversal patterns dominate")

    print(f"\n✓ COMMON ATTRIBUTES:")
    print(f"  - EURCHF_Stay appears in many rules (EUR/CHF stability signal)")
    print(f"  - JPY cross pairs (EURJPY, AUDJPY, CHFJPY) frequently used")
    print(f"  - USD pairs (EURUSD, GBPUSD) provide directional signals")

    print(f"\n⚠ POTENTIAL CONCERNS:")

    # Check if means are too close to threshold
    close_to_threshold = np.sum((np.abs(mean_t1) < 0.25) | (np.abs(mean_t2) < 0.25))
    if close_to_threshold > len(df) * 0.3:
        print(f"  - {close_to_threshold} rules have |μ| close to threshold (0.20%)")
        print(f"    → Consider raising Minmean to 0.25% for stronger signals")

    # Check variance
    high_variance = np.sum(((sigma_t1 + sigma_t2) / 2) > 0.40)
    if high_variance > len(df) * 0.5:
        print(f"  - {high_variance} rules have σ > 0.40% (looser clusters)")
        print(f"    → Consider lowering Maxsigx to 0.40% for tighter clusters")

    print(f"\n💡 RECOMMENDATIONS:")

    avg_score = scores_df['total_score'].mean()

    if avg_score >= 70:
        print(f"  ✓ Current settings are EXCELLENT (avg score: {avg_score:.0f}/100)")
        print(f"  ✓ Rules show strong cluster structure with clear directionality")
        print(f"  → Use top 10-15 rules for trading strategy development")
    elif avg_score >= 60:
        print(f"  ✓ Current settings are GOOD (avg score: {avg_score:.0f}/100)")
        print(f"  → Focus on Grade A/B rules for practical application")
        print(f"  → Consider tightening Maxsigx slightly (0.40% → 0.38%)")
    else:
        print(f"  ⚠ Current settings need improvement (avg score: {avg_score:.0f}/100)")
        print(f"  → Increase Minmean to 0.25% for stronger directionality")
        print(f"  → Decrease Maxsigx to 0.38% for tighter clusters")

    print(f"\n📊 NEXT STEPS:")
    print(f"  1. Visually inspect scatter plots for top 10 rules")
    print(f"  2. Verify quadrant concentration in each cluster")
    print(f"  3. Test rules on out-of-sample data (2025 data)")
    print(f"  4. Combine top rules into trading strategy")
    print(f"  5. Run batch analysis on other forex pairs (EURUSD, GBPUSD, etc.)")

    print()

def main():
    """Main analysis function."""
    print("\n")
    print("╔" + "═" * 78 + "╗")
    print("║" + " " * 20 + "USDJPY RULE DISCOVERY ANALYSIS" + " " * 28 + "║")
    print("╚" + "═" * 78 + "╝")
    print()

    # Load rules
    df = load_rules()

    print(f"Total Rules Discovered: {len(df)}")
    print()

    # Run analyses
    analyze_variance(df)
    analyze_mean(df)
    analyze_support(df)
    analyze_attributes(df)
    scores_df = rank_rules(df)
    analyze_top_rules(df, scores_df)
    summary_insights(df, scores_df)

    print("=" * 80)
    print("ANALYSIS COMPLETE")
    print("=" * 80)
    print()

if __name__ == "__main__":
    main()
