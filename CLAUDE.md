# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

This is a **time-series data mining system** that uses **Genetic Network Programming (GNP)** to discover temporal patterns in cryptocurrency markets. The system identifies rare but statistically significant patterns where conditional distributions deviate substantially from the unconditional distribution.

---

## Research Objective

### Core Goal: Discovery of Statistical Anomalies

**Discover rare pattern groups where conditional mean is far from zero (statistically anomalous)**

### What We Are Looking For

**Small subgroups with extreme conditional expectations:**
- Positive extreme: `mean >= +0.5%` (strong upward patterns)
- Negative extreme: `mean <= -0.5%` (strong downward patterns)
- NOT interested in: `mean ≈ 0` (near-zero patterns, e.g., 0.05%, 0.08%)

### Definition of "Statistical Anomaly"

```
Unconditional distribution (全データ):
  μ = 0.0077%
  σ = 0.5121%

Target conditional distribution (発見したいルール):
  |μ_rule - μ_data| >> σ_data

Example:
  μ_rule = +1.0%  → +1.0σ from data mean (statistically significant)
  μ_rule = -0.8%  → -0.8σ from data mean (statistically significant)
```

### Characteristics of Target Patterns

1. **Rare but strong**: Low support count OK, but clear statistical signal
2. **Far from zero**: Absolute mean value >> baseline noise
3. **Statistically significant**: Deviation from data mean is substantial
4. **Bidirectional**: Both positive AND negative extremes are valuable

### What Makes This Research Valuable?

**Discovery of regime changes in market structure:**
- Normal state: μ ≈ 0.0077% (random walk)
- Pattern-triggered state: μ >> 0.5% or μ << -0.5% (structural shift)
- → Evidence of **conditional market inefficiency**

---

## Current System Architecture

### Build Commands

```bash
# Compile
make

# Run with default settings (BTC, 10 trials)
make test

# Run with specific symbol
./main BTC 10
```

### Main Program: `main.c`

**Key components:**
1. **GNP Evolution** (201 generations, 120 individuals)
2. **Time-series pattern matching** (t-4 to t-0 → predict t+1, t+2)
3. **Rule quality filtering** (support, sigma, mean constraints)
4. **Verification data output** (CSV files with all match points)

### Data Structure

**Input:** `1-deta-enginnering/crypto_data_hourly/BTC.txt`
- 17,482 hourly records (Nov 2023 - Nov 2025)
- 60 binary attributes (20 cryptos × 3 states: Up/Stay/Down)
- X column: hourly return (%)
- T column: timestamp

**Output:**
- `output/pool/zrp01a.txt`: Rule pool with statistics
- `output/verification/rule_XXXX.csv`: Match-by-match verification data

---

## Critical Parameters (Current Settings)

### Quality Thresholds

```c
#define Minsup 0.001              // Minimum support: 0.1%
#define Maxsigx 0.5               // Maximum sigma: 0.5%
#define Min_Mean 0.05             // Minimum mean: 0.05%
#define MIN_ATTRIBUTES 2          // Minimum attributes per rule
#define MIN_SUPPORT_COUNT 2       // Minimum match count
#define ENABLE_SIMPLE_POSITIVE_FILTER 1  // Enable AND logic filter
```

### Current Filter Logic

```c
// In check_rule_quality() function (lines 1833-1867):
// Requires BOTH conditions:
//   1. Low variance: sigma <= Maxsigx for all future spans
//   2. High mean: mean >= Min_Mean for t+1 AND t+2
```

---

## Problem: Current Settings Do NOT Match Research Goal

### Current Results

With `Min_Mean = 0.05%`:
- Discovered: 6,715 rules
- Average mean: 0.077% = **0.14σ** (statistically noise level)
- Research value: **LOW** (too close to zero, not anomalous)

### Why This Fails

```
Target: |mean| >> 0.5% (statistical anomalies)
Current: mean ≈ 0.08% (near-zero patterns)
→ Mismatch between goal and implementation
```

---

## Recommended Threshold Settings for Research Goal

### Option 1: Bidirectional Extremes (Recommended)

```c
#define Min_Mean_Positive 0.5     // For positive patterns: >= +0.5%
#define Min_Mean_Negative -0.5    // For negative patterns: <= -0.5%
#define Maxsigx 0.45              // Moderate variance control
#define MIN_SUPPORT_COUNT 10      // At least 10 matches for reliability
#define Minsup 0.0005             // Lower support OK for rare patterns
```

**Expected results:**
- ~50-200 rules (positive + negative)
- Clear statistical significance (>1σ from data mean)
- Interpretable as market anomalies

### Option 2: Strong Anomalies Only

```c
#define Min_Mean_Positive 1.0     // Very strong positive: >= +1.0%
#define Min_Mean_Negative -1.0    // Very strong negative: <= -1.0%
#define Maxsigx 0.4               // Tighter variance
#define MIN_SUPPORT_COUNT 5       // Rare patterns OK
```

**Expected results:**
- ~5-30 rules (very rare)
- Extremely significant (>2σ from data mean)
- Potential "black swan" pattern discovery

---

## Implementation Strategy

### Required Code Changes

1. **Add negative pattern support** (currently only positive)
2. **Implement absolute deviation filter**: `|mean - μ_data| >= threshold`
3. **Separate positive/negative rule pools**
4. **Statistical significance testing** (t-test for rule means)

### Workflow

```
1. Set higher thresholds (Min_Mean = 0.5 or higher)
2. Run GNP evolution
3. Discover ~50-200 extreme patterns
4. Verify statistical significance
5. Analyze conditional vs. unconditional distributions
6. Publication: "Discovery of Conditional Market Anomalies via GNP"
```

---

## Data Characteristics (BTC Hourly)

### Baseline Statistics

| Metric | Value |
|--------|-------|
| Records | 17,482 hours (~2 years) |
| μ (mean) | 0.0077% |
| σ (stddev) | 0.5121% |
| Distribution | ~Normal, symmetric |
| Autocorrelation | -0.008 (random walk) |
| Positive rate | 50.24% |
| Negative rate | 48.44% |

### Statistical Significance Levels

```
μ + 1σ = 0.52%  → Upper 16% (α = 0.05 level)
μ + 2σ = 1.03%  → Upper 2.5% (α = 0.01 level)
μ + 3σ = 1.54%  → Upper 0.15% (very rare)

μ - 1σ = -0.51% → Lower 16%
μ - 2σ = -1.02% → Lower 2.5%
μ - 3σ = -1.53% → Lower 0.15%
```

### Target Thresholds Interpretation

```c
Min_Mean = 0.5%  → Approximately μ + 1σ (statistically significant)
Min_Mean = 1.0%  → Approximately μ + 2σ (highly significant)
```

---

## Visualization and Analysis

### Scatter Plot Analysis

For discovered rules, generate X(t+1) vs X(t+2) scatter plots:

```bash
python3 analysis/crypt/plot_single_rule_2d_future.py --rule-id XXXX --symbol BTC
```

Shows:
- Quadrant distribution (++, +-, -+, --)
- Mean lines and statistics
- Visual confirmation of pattern strength

---

## Important Notes

- **Character encoding**: UTF-8 for new files, legacy files may be Shift-JIS
- **Mathematical dependencies**: Link with `-lm` flag
- **Memory limits**: `Nrulemax = 2002` rules maximum
- **Time delays**: System uses t-4, t-3, t-2, t-1, t-0 for pattern matching
- **Prediction span**: Forecasts t+1 and t+2

---

## Research Value Checklist

A discovered rule has research value if:

- [ ] |mean| >= 0.5% (far from zero)
- [ ] Support count >= 10 (statistical reliability)
- [ ] |mean - μ_data| >= 1σ (statistically significant)
- [ ] Pattern is interpretable (meaningful attribute combinations)
- [ ] Verified on out-of-sample data (not overfitted)

Current system with `Min_Mean = 0.05%` fails the first criterion.

---

## Next Steps for Alignment with Research Goal

1. **Modify thresholds** to capture extreme patterns only
2. **Implement negative pattern detection** (currently missing)
3. **Add statistical significance tests** (t-test, p-values)
4. **Create comparative analysis**: conditional vs. unconditional distributions
5. **Document discovered anomalies** with interpretation
