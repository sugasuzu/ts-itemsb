# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

This is a **time-series data mining system** that uses **Genetic Network Programming (GNP)** to discover temporal patterns in **foreign exchange (FX) markets**. The system identifies **quadrant-consistent patterns** where matched data points exhibit coherent directional behavior.

---

## Research Objective (Updated: 2025-11-13)

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

### Definition of "Quadrant Concentration" (v5.1)

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

## Current Threshold Settings (main.c Implementation v5.2)

### Active Quality Thresholds (UPDATED: 2025-11-13)

```c
// main.c lines 34-36 (current implementation - 検証用に緩和)
#define Minsup 0.001                    // 0.1% support rate (検証用に緩和)
#define MIN_CONCENTRATION 0.40          // 40% quadrant concentration rate (検証用に緩和)
#define MAX_DEVIATION 1.0               // 1.0% deviation tolerance (検証用に緩和)
#define HIGH_SUPPORT_MULTIPLIER 2.0     // High-support flag threshold (Minsup × 2.0)
```

**注意:** 本番環境では以下の値に戻すことを推奨：
- `Minsup 0.005` (0.5%)
- `MIN_CONCENTRATION 0.50` (50%)
- `MAX_DEVIATION 0.5` (0.5%)

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

Stage 4: Support Rate Check (論文準拠 - 式(3))
  → support_rate = matched_count / (Nrd - S_max(X∪Y)) ≥ 0.1%
  → S_max(X∪Y) = max_delay + FUTURE_SPAN (ルール固有)
  → 実装: calculate_accurate_support_rate() (main.c:1724)

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
1. Rematch pattern to get matched_indices
2. Quadrant concentration + deviation check (Stage 1)
3. Support rate check (0.5%, Stage 2)

### Fitness Function (v5.1 - Enhanced with Nonlinear Bonuses)

**Location:** main.c:2339-2344 (new rules), main.c:2388-2392 (duplicate rules)

```c
// New rule:
fitness = (double)num_attributes +         // Attribute count (complexity bonus: 1-8)
          support_rate × 10.0 +            // Support rate (10× weight)
          concentration_rate × 100.0 +     // Base concentration rate (100× weight)
          concentration_bonus +            // Nonlinear high-concentration bonus (0-10000)
          20.0;                            // New rule bonus

// Duplicate rule (no new rule bonus):
fitness = (double)num_attributes +         // Attribute count
          support_rate × 10.0 +            // Support rate
          concentration_rate × 100.0 +     // Base concentration rate
          concentration_bonus;             // Nonlinear bonus
```

**Support rate calculation:**
```c
support_rate = matched_count / (Nrd - FUTURE_SPAN)
```
- Uses **effective records** (records where future prediction is possible)
- Ensures consistency with filter calculation

**Concentration bonus calculation:**
```c
if (concentration_rate >= 0.45) {
    double excess = (concentration_rate - 0.45) * 20.0;  // Scale to 0.0-1.0
    concentration_bonus = pow(excess, 2) * 10000.0;      // Quadratic scaling
}
// Examples:
// - 45.0%: bonus = 0
// - 47.5%: bonus = 2500 (excess = 0.5)
// - 50.0%: bonus = 10000 (excess = 1.0)
```

**Key features:**
- ✅ **Attribute complexity bonus**: 1-8 points (encourages pattern richness)
- ✅ **Support rate** (10× weight): Frequency indicator
- ✅ **Base concentration** (100× weight): Linear component
- ✅ **Nonlinear concentration bonus**: Exponential reward for high concentration (≥45%)
  - Creates strong evolutionary pressure toward 50%+ concentration
  - Maximum bonus (10000) vastly exceeds all other components
- ✅ **New rule bonus** (20 points): Encourages diversity

**Fitness component magnitude comparison:**
| Component | Typical Range | Weight | Effective Range |
|-----------|--------------|--------|-----------------|
| Attribute count | 1-8 | 1× | 1-8 |
| Support rate (0.5%-2%) | 0.005-0.02 | 10× | 0.05-0.2 |
| Base concentration (50%-80%) | 0.5-0.8 | 100× | 50-80 |
| **Concentration bonus** | **0-10000** | **1×** | **0-10000** |
| New rule bonus | 20 | 1× | 0 or 20 |

→ **Concentration bonus dominates** the fitness landscape for high-quality rules

---

## Expected Results (v5.1 System)

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

**Key visualization features (v5.1):**
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

## Research Value Checklist (v5.1)

A discovered rule has research value if:

- [ ] **Quadrant concentrated**: ≥50% of points in dominant quadrant
- [ ] **Deviation constrained**: All points within ±0.5% of quadrant direction
- [ ] **Support rate ≥ 0.5%**: Sufficient frequency (based on effective records)
- [ ] **Pattern is interpretable**: Meaningful attribute combinations
- [ ] **Verified on out-of-sample data**: Not overfitted

**Note:** Mean value is calculated but NOT used as a filter criterion. Concentration rate and deviation constraint are the key quality measures.

---

## System Evolution History

### v5.2 (Current - 2025-11-13)
- **Code optimization**: Removed legacy `negative_count` computation (~48KB memory saved)
- **Simplified support calculation**: Deleted unused `calculate_support_value()` function
- **Paper-compliant filtering**: Uses `calculate_accurate_support_rate()` (式(3)準拠)
- **Backward compatibility**: Output file format unchanged (fixed value: Nrd - FUTURE_SPAN)
- **Verification thresholds**: Relaxed for testing (Minsup=0.1%, MIN_CONCENTRATION=40%, MAX_DEVIATION=1.0%)
- **Target market**: FX daily data

### v5.1 (2025-11-13)
- **Enhanced fitness function**: Added nonlinear concentration bonus (0-10000 points)
- **Attribute complexity bonus**: Rewards patterns with more attributes (1-8 points)
- **Support rate calculation**: Fixed to use effective records (Nrd - FUTURE_SPAN)
- **Evolutionary pressure**: Strong bias toward 50%+ concentration via exponential rewards
- **Target market**: FX daily data

### v5.0 (2025-11-12)
- **0-based quadrant determination**: Pure sign-based classification
- **Concentration rate filter**: 50% majority rule
- **Deviation constraint**: 0.5% strict control (updated from 1.0%)
- **Simplified fitness**: Support + Concentration (linear only)
- **Target market**: FX daily data

### Previous Versions (Deprecated)
- **v4.x**: MinMax-based quadrant determination with absolute thresholds
- **v3.x**: Mean-based filtering with concentration bonuses
- **v2.x**: Complex multi-level bonus system
- **v1.x**: Original cryptocurrency hourly data system

---

## Key Differences from Previous Approach

| Aspect | OLD (Mean-based) | v5.0 (Linear) | v5.1 (Nonlinear) | v5.2 (Current - Optimized) |
|--------|------------------|---------------|------------------|---------------------------|
| **Primary filter** | mean ≥ 0.05% | concentration ≥ 50% | concentration ≥ 50% | concentration ≥ 40% (検証用) |
| **Quadrant determination** | MinMax thresholds | 0-based (sign only) | 0-based (sign only) | 0-based (sign only) |
| **Deviation control** | None | ±0.5% strict | ±0.5% strict | ±1.0% (検証用に緩和) |
| **Mean usage** | Filter threshold | Output only | Output only | Output only |
| **Directionality** | Average-based | Majority-based | Majority-based | Majority-based |
| **Pattern types** | Mainly Q1 (positive) | All 4 quadrants | All 4 quadrants | All 4 quadrants |
| **Fitness function** | Complex bonuses | Linear: support + concentration | Nonlinear: support + concentration + exponential bonus | Same as v5.1 |
| **Support calculation** | N/A | Nrd (all records) | Nrd - FUTURE_SPAN (effective) | **Paper-compliant (式(3))** |
| **Concentration reward** | N/A | Linear (100×) | Linear (100×) + Exponential (0-10000) | Same as v5.1 |
| **negative_count** | N/A | Computed 3D array | Computed 3D array | **Fixed value (optimized)** |
| **Memory usage** | N/A | ~48KB for negative_count | ~48KB for negative_count | **Saved ~48KB** |
| **Target data** | Crypto hourly | FX daily | FX daily | FX daily |

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
6. **[v5.1]** Does the nonlinear concentration bonus (45%-50% exponential reward) create better patterns than linear weighting?
7. **[v5.1]** What is the optimal threshold for triggering the exponential bonus (currently 45%)?

---

## Recent Optimizations (2025-11-13)

### Code Cleanup - Removed Legacy negative_count System

**Motivation:** Code review revealed that the `negative_count` 3D array and related functions were completely unused in the actual filtering logic, wasting ~48KB of memory.

**What was removed:**

1. **Functions deleted:**
   - `calculate_support_value()` (main.c:1724-1731) - Computed `matched_count / negative_count_val` but result was never used
   - `calculate_negative_counts()` (main.c:1657-1671) - Copied `match_count[i][k][0]` to all depths of `negative_count` array

2. **Data structures removed:**
   - `int ***negative_count` global 3D array (120 × 10 × 10 = 12,000 integers = ~48KB)
   - All allocations, initializations, and deallocations related to this array

3. **Function signatures simplified:**
   - `check_rule_quality()`: Removed unused `support` parameter
   - `register_new_rule()`: Removed `negative_count_val` parameter (now computed internally as fixed value)

4. **Call sites updated:**
   - Removed `calculate_negative_counts()` call from main evolution loop (line 3351)
   - Removed unused `support` calculation in `extract_rules_single_individual()`
   - Updated all function calls to match new signatures

**What was preserved (Option 2 - Backward Compatibility):**

- **Output file format unchanged**: `rule_pool[idx].negative_count` still exists as a struct member
- **Fixed value used**: Set to `Nrd - FUTURE_SPAN` in `register_new_rule()` (main.c:1872-1876)
- **Rationale**: Maintains compatibility with existing analysis scripts that read CSV output

**Benefits:**
- **Memory**: Saved ~48KB (removed 12,000-element 3D array)
- **CPU**: Eliminated useless computation loop in hot path
- **Code clarity**: Removed 50+ lines of dead code
- **Maintainability**: Clearer what's actually used vs. legacy output fields

**Verification:**
```bash
$ make clean && make
✓ Compilation: No warnings or errors

$ ./main GBPJPY
✓ Execution: Normal operation

$ head -2 1-deta-enginnering/forex_data_daily/output/GBPJPY/pool/zrp01a.txt
✓ Output: Negative column = 4130 (= Nrd - FUTURE_SPAN = 4132 - 2)
```

**Related documentation:**
- Problem analysis: `/docs/20251113_critical_bug_matched_count_range.md`
- Implementation log: `/docs/20251113_fix_implementation_summary.md`

---

**Document version**: 5.2 (Optimized with legacy code removal)
**Last updated**: 2025-11-13
**Status**: Production (FX daily data, 検証用閾値で実行中)
**Main implementation**: `main.c` (~3850 lines, -50 lines from v5.1)
