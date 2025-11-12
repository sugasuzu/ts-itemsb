# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

This is a **time-series data mining system** that uses **Genetic Network Programming (GNP)** to discover temporal patterns in **foreign exchange (FX) markets**. The system identifies **quadrant-consistent patterns** where matched data points exhibit coherent directional behavior.

---

## Research Objective (Updated: 2025-11-12)

### Core Goal: Discovery of Quadrant-Concentrated Patterns

**Discover patterns where a majority (≥50%) of matched points fall within a single dominant quadrant, with strict deviation constraints.**

### What We Are Looking For

**Four types of quadrant patterns:**

1. **Q1 (++): Both Up** - t+1 ≥ 0.0% AND t+2 ≥ 0.0%
   - Interpretation: Upward trend continuation
   - Trading strategy: Momentum buying

2. **Q2 (-+): Rebound** - t+1 < 0.0% AND t+2 ≥ 0.0%
   - Interpretation: Bounce after decline
   - Trading strategy: Counter-trend buying (dip buying)

3. **Q3 (--): Both Down** - t+1 < 0.0% AND t+2 < 0.0%
   - Interpretation: Downward trend continuation
   - Trading strategy: Momentum selling

4. **Q4 (+-): Reversal** - t+1 ≥ 0.0% AND t+2 < 0.0%
   - Interpretation: Pullback after rise
   - Trading strategy: Counter-trend selling (profit-taking)

### Definition of "Quadrant Concentration" (v5.0)

```
Concentration Rate = (Dominant Quadrant Count) / (Total Valid Matches)

Requirement: concentration_rate ≥ 50%

Zero-based quadrant determination:
  - No threshold for quadrant assignment (pure sign-based)
  - All points classified into one of 4 quadrants
  - Dominant quadrant = quadrant with maximum count
```

### Key Design Principle: Rate-Based Concentration + Deviation Constraint

**CURRENT APPROACH (v5.0)**: The system uses **concentration rate** + **deviation threshold**.

```c
// Step 1: Count all points by quadrant (0-based, no threshold)
if (t+1 ≥ 0.0 && t+2 ≥ 0.0) → Q1
if (t+1 < 0.0 && t+2 ≥ 0.0) → Q2
if (t+1 < 0.0 && t+2 < 0.0) → Q3
if (t+1 ≥ 0.0 && t+2 < 0.0) → Q4

// Step 2: Calculate concentration rate
concentration_rate = max(Q1, Q2, Q3, Q4) / total_points

// Step 3: Check concentration threshold
if (concentration_rate < 50%) → REJECT

// Step 4: Check deviation constraint (0.5%)
For each point:
  if (dominant_quadrant == Q1 && (t+1 < -0.5% || t+2 < -0.5%)) → REJECT
  if (dominant_quadrant == Q2 && (t+1 > +0.5% || t+2 < -0.5%)) → REJECT
  if (dominant_quadrant == Q3 && (t+1 > +0.5% || t+2 > +0.5%)) → REJECT
  if (dominant_quadrant == Q4 && (t+1 < -0.5% || t+2 > +0.5%)) → REJECT
```

**Why this approach:**
- **Concentration rate**: Ensures majority consistency (≥50%)
- **Deviation constraint**: Prevents extreme outliers from opposite direction
- **0-based quadrants**: Simple sign-based classification, no arbitrary threshold
- **All 4 quadrants**: Equally supported (not biased toward positive)

**Mean is still calculated**: For reference and output, but NOT used as a filter threshold.

---

## Current System Architecture

### Build Commands

```bash
# Compile
make

# Run with specific FX pair
./main USDJPY
./main GBPJPY
./main EURUSD
```

### Main Program: `main.c`

**Key components:**
1. **GNP Evolution** (201 generations, 120 individuals)
2. **Time-series pattern matching** (t-2 to t-0 → predict t+1, t+2)
3. **Quadrant-concentration filtering** (Rate ≥ 50% + Deviation ≤ 0.5%)
4. **Verification data output** (CSV files with all match points)

### Data Structure

**Input:** `1-deta-enginnering/forex_data_daily/{PAIR}.txt`
- Daily FX data for 20 currency pairs
- 60 binary attributes (20 pairs × 3 states: Up/Stay/Down)
- X column: daily return rate (%)
- T column: timestamp (date)

**Example pairs:**
- USDJPY, EURUSD, GBPJPY, AUDJPY, NZDJPY, CADJPY, CHFJPY
- GBPUSD, AUDUSD, NZDUSD, USDCAD, USDCHF
- EURGBP, EURAUD, EURCHF, GBPAUD, GBPCAD, AUDCAD, AUDNZD
- (20 pairs total)

**Output:**
- `output/{PAIR}/pool/zrp01a.txt`: Rule pool with statistics
- `output/{PAIR}/verification/rule_XXXX.csv`: Match-by-match verification data

---

## Current Threshold Settings (main.c Implementation v5.0)

### Active Quality Thresholds

```c
// main.c lines 33-38 (current implementation)
#define Minsup 0.003                    // 0.3% support rate (全マッチベース)
#define MIN_SUPPORT_COUNT 20            // Minimum 20 matches
#define QUADRANT_THRESHOLD_RATE 0.50    // 50% quadrant concentration rate
#define DEVIATION_THRESHOLD 0.5         // 0.5% deviation tolerance (UPDATED: 2025-11-12)
#define Maxsigx 999.0                   // Maximum sigma (effectively disabled)
```

### Filter Sequence

```
Stage 1: Minimum Attributes Check
  → num_attributes ≥ MIN_ATTRIBUTES (1)

Stage 2: Quadrant Concentration Check (NEW in v5.0)
  → Rematch rule pattern to get all matched indices
  → Classify all points into 4 quadrants (0-based)
  → Calculate concentration_rate = max_quadrant_count / total_valid_matches
  → REJECT if concentration_rate < 50%

Stage 3: Deviation Constraint Check (UPDATED: 0.5%)
  → For each matched point:
    → Check if deviation from dominant quadrant exceeds 0.5%
    → REJECT if any point violates deviation constraint

Stage 4: Support Rate Check
  → support_rate = matched_count / Nrd ≥ 0.3%
  → matched_count ≥ 20

Stage 5: Variance Check (effectively disabled)
  → sigma ≤ 999.0% (all rules pass)
```

**Key changes from previous versions:**
- ❌ **Removed**: Mean-based filtering (Minmean)
- ❌ **Removed**: Min/Max absolute thresholds
- ✅ **Added**: Concentration rate (50%)
- ✅ **Added**: Deviation constraint (0.5%, down from 1.0%)
- ✅ **Simplified**: 0-based quadrant determination (pure sign-based)

---

## Implementation Details

### Core Functions

#### 1. Quadrant Determination (main.c:440-559)

```c
int determine_quadrant_by_rate_with_concentration(
    int *matched_indices,
    int match_count,
    double *concentration_out,
    int *in_quadrant_count_out
)
```

**Logic:**
1. Count all points by quadrant (0-based sign classification)
2. Find dominant quadrant (max count)
3. Calculate concentration rate
4. Check if concentration ≥ 50%
5. Check deviation constraint (0.5%)
6. Return dominant quadrant (1-4) or 0 (rejected)

#### 2. Rule Quality Check (main.c:2037-2158)

```c
int check_rule_quality(
    double *future_sigma_array,
    double *future_mean_array,
    double support,
    int num_attributes,
    double *future_min_array,
    double *future_max_array,
    int matched_count,
    int *rule_attributes,
    int *time_delays
)
```

**Filter sequence:**
1. MIN_ATTRIBUTES check
2. Rematch pattern to get matched_indices
3. Quadrant concentration + deviation check
4. Support rate check (0.3%)
5. Minimum support count check (20)
6. Variance check (999.0%, effectively disabled)

### Fitness Function (main.c:2449-2453, 2489-2491)

```c
// New rule:
fitness = support_rate × 10.0 +        // Support rate (frequency indicator)
          concentration_rate × 100.0 + // Concentration rate (quality indicator)
          20.0;                        // New rule bonus

// Duplicate rule (no bonus):
fitness = support_rate × 10.0 +       // Support rate
          concentration_rate × 100.0; // Concentration rate
```

**Key features:**
- ✅ Simple 3-component formula
- ✅ Concentration rate heavily weighted (100×)
- ✅ Support rate moderately weighted (10×)
- ❌ No attribute count bonus
- ❌ No variance-based penalty
- ❌ No complex bonuses

---

## Expected Results (v5.0 System)

### Current Settings Performance

**With QUADRANT_THRESHOLD_RATE=50%, DEVIATION_THRESHOLD=0.5%:**
- Expected rules: Variable (depends on market characteristics)
- Quadrant distribution: Balanced across all 4 quadrants
- Concentration: ≥50% of points in dominant quadrant
- Deviation: All points within ±0.5% of dominant quadrant direction

**Quality characteristics:**
- ✓ Majority consistency (≥50% in one quadrant)
- ✓ Strict deviation control (0.5%)
- ✓ Clear directional bias
- ✓ All 4 pattern types supported

### Pattern Quality Example

```
Rule Example (Q1 - Both Up):
  Total matches: 45
  Q1 count: 28 (62.2%) ← dominant
  Q2 count: 8 (17.8%)
  Q3 count: 5 (11.1%)
  Q4 count: 4 (8.9%)

  Concentration rate: 62.2% ≥ 50% ✓

  Deviation check:
    All Q2/Q3/Q4 points within -0.5% to +0.5% range ✓
    (No extreme negative deviations from Q1)

  → ACCEPTED as Q1 pattern
```

---

## Data Characteristics (FX Daily)

### Typical FX Pair Statistics

| Metric | Typical Range |
|--------|--------------|
| Records | ~3,500-5,000 days (~10-14 years) |
| μ (mean) | -0.01% to +0.01% (near zero) |
| σ (stddev) | 0.4% to 0.8% (varies by pair) |
| Distribution | Approximately normal |
| Positive rate | 48-52% |
| Negative rate | 48-52% |

### Threshold Interpretation

```
DEVIATION_THRESHOLD = 0.5%
  → Approximately 1σ for most FX pairs
  → Prevents extreme outliers from opposite direction
  → Allows minor noise but maintains directional consistency

QUADRANT_THRESHOLD_RATE = 0.50 (50%)
  → Majority rule: more than half points in one quadrant
  → Weaker than 100% consensus, stronger than random (25%)
  → Balances pattern discovery vs. purity
```

---

## Output Format

### Rule Pool Output (pool/zrp01a.txt)

**Current format:**
```
Rule_ID,Support,Mean_t1,Sigma_t1,Min_t1,Max_t1,Mean_t2,Sigma_t2,Min_t2,Max_t2,Attributes
1,0.0234,0.000735,0.003521,0.001200,0.002500,0.000842,0.003158,0.001050,0.002800,"USDJPY_Up EURJPY_Up"
```

**Columns:**
- `Rule_ID`: Unique rule identifier
- `Support`: Support rate (matched_count / total_records)
- `Mean_t1`, `Sigma_t1`: t+1 statistics
- `Min_t1`, `Max_t1`: t+1 min/max values (for reference)
- `Mean_t2`, `Sigma_t2`: t+2 statistics
- `Min_t2`, `Max_t2`: t+2 min/max values (for reference)
- `Attributes`: Space-separated attribute names

**Note:** Quadrant information is NOT in CSV output, but can be derived from statistics.

### Verification CSV (verification/rule_XXXX.csv)

**Format:**
```
Time_Index,X_t1,X_t2,Timestamp
245,0.0012,0.0015,2010-03-15
389,0.0008,0.0018,2010-06-22
...
```

Shows all matched time points for a specific rule.

---

## Visualization and Analysis

### Scatter Plot Analysis

For discovered rules, generate X(t+1) vs X(t+2) scatter plots:

```bash
python3 analysis/fx/plot_gbpjpy_rate50_rules.py
```

**Shows:**
- Quadrant distribution with 0-based boundaries
- Deviation threshold lines (±0.5%)
- Concentration statistics
- Visual confirmation of quadrant dominance

**Key visualization features (v5.0):**
- Origin lines (0%) as quadrant boundaries (black solid)
- Mean lines (blue/green dashed)
- Deviation threshold lines (red dashed, ±0.5%)
- Background scatter of all data (gray)
- Rule matches (red)

---

## Important Notes

- **Market**: Foreign exchange (FX), daily data
- **Pairs**: 20 major currency pairs (60-dimensional binary feature space)
- **Character encoding**: UTF-8 for new files
- **Mathematical dependencies**: Link with `-lm` flag
- **Memory limits**: `Nrulemax = 2002` rules maximum
- **Time delays**: System uses t-2, t-1, t-0 for pattern matching (adaptive delay)
- **Prediction span**: Forecasts t+1 and t+2 (2 days ahead)
- **Random seed**: Fixed at 1 for reproducibility (main.c:3898)

---

## Research Value Checklist (v5.0)

A discovered rule has research value if:

- [ ] **Quadrant concentrated**: ≥50% of points in dominant quadrant
- [ ] **Deviation constrained**: All points within ±0.5% of quadrant direction
- [ ] **Support count ≥ 20**: Statistical reliability
- [ ] **Support rate ≥ 0.3%**: Sufficient frequency
- [ ] **Pattern is interpretable**: Meaningful attribute combinations
- [ ] **Verified on out-of-sample data**: Not overfitted

**Note:** Mean value is calculated but NOT used as a filter criterion. Concentration rate and deviation constraint are the key quality measures.

---

## System Evolution History

### v5.0 (Current - 2025-11-12)
- **0-based quadrant determination**: Pure sign-based classification
- **Concentration rate filter**: 50% majority rule
- **Deviation constraint**: 0.5% strict control (updated from 1.0%)
- **Simplified fitness**: Support + Concentration only
- **Target market**: FX daily data

### Previous Versions (Deprecated)
- **v4.x**: MinMax-based quadrant determination with absolute thresholds
- **v3.x**: Mean-based filtering with concentration bonuses
- **v2.x**: Complex multi-level bonus system
- **v1.x**: Original cryptocurrency hourly data system

---

## Key Differences from Previous Approach

| Aspect | OLD (Mean-based) | CURRENT (v5.0 Concentration-based) |
|--------|------------------|-----------------------------------|
| **Primary filter** | mean ≥ 0.05% | concentration_rate ≥ 50% |
| **Quadrant determination** | MinMax thresholds | 0-based (sign only) |
| **Deviation control** | None | ±0.5% strict constraint |
| **Mean usage** | Filter threshold | Output only (reference) |
| **Directionality** | Average-based | Majority-based |
| **Pattern types** | Mainly Q1 (positive) | All 4 quadrants equally |
| **Fitness function** | Complex bonuses | Simple: support + concentration |
| **Target data** | Crypto hourly | FX daily |

---

## Configuration Files and Scripts

### Analysis Scripts

- **FX visualization**: `analysis/fx/plot_gbpjpy_rate50_rules.py`
  - 0-based quadrant boundaries
  - Deviation threshold visualization
  - Concentration statistics display

### Data Files

- **Input directory**: `1-deta-enginnering/forex_data_daily/`
- **Output directory**: `output/{PAIR}/`
  - `pool/zrp01a.txt`: Global rule pool
  - `verification/rule_*.csv`: Per-rule verification data
  - `IA/`: Analysis reports
  - `IB/`: Backup files
  - `doc/`: Documentation and statistics

### Build System

- **Makefile**: Compilation settings
- **Compiler flags**: `-lm` for math library
- **Debug mode**: Enable with `-DDEBUG` flag

---

## Future Improvements (Potential)

### Possible Enhancements

1. **Dynamic deviation threshold**: Adapt based on market volatility
2. **Multi-timeframe analysis**: Combine daily, weekly patterns
3. **Confidence intervals**: Statistical significance testing
4. **Out-of-sample validation**: Automated backtesting framework
5. **Attribute importance**: Ranking of contributing features

### Research Questions

1. Does 50% concentration rate provide optimal balance?
2. Is 0.5% deviation threshold appropriate for all FX pairs?
3. Can we predict which quadrant will dominate?
4. What is the relationship between concentration and profitability?
5. How do discovered patterns perform in different market regimes?

---

**Document version**: 5.0 (Concentration-based filtering with deviation constraint)
**Last updated**: 2025-11-12
**Status**: Production (FX daily data)
**Main implementation**: `main.c` (3936 lines)
