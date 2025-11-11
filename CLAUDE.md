# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Project Overview

This is a **time-series data mining system** that uses **Genetic Network Programming (GNP)** to discover temporal patterns in cryptocurrency markets. The system identifies **consistent patterns within specific quadrants** where all matched data points exhibit coherent directional behavior.

---

## Research Objective (Updated: 2025-11-11)

### Core Goal: Discovery of Quadrant-Consistent Patterns

**Discover patterns where ALL matched points fall within a single quadrant, ensuring consistent directional behavior.**

### What We Are Looking For

**Four types of quadrant-consistent patterns:**

1. **Q1 (++): Both Up** - All points: t+1 ≥ +0.05% AND t+2 ≥ +0.05%
   - Interpretation: Strong upward trend continuation
   - Trading strategy: Momentum buying

2. **Q2 (-+): Rebound** - All points: t+1 ≤ -0.05% AND t+2 ≥ +0.05%
   - Interpretation: Bounce after decline
   - Trading strategy: Counter-trend buying (dip buying)

3. **Q3 (--): Both Down** - All points: t+1 ≤ -0.05% AND t+2 ≤ -0.05%
   - Interpretation: Strong downward trend continuation
   - Trading strategy: Momentum selling

4. **Q4 (+-): Reversal** - All points: t+1 ≥ +0.05% AND t+2 ≤ -0.05%
   - Interpretation: Pullback after rise
   - Trading strategy: Counter-trend selling (profit-taking)

### Definition of "Quadrant Consistency"

```
Unconditional distribution (全データ):
  μ = 0.0077%
  σ = 0.5121%

Target conditional distribution (発見したいルール):
  ALL matched points satisfy quadrant constraints (Min/Max thresholds)

Example Q1 pattern:
  min(X_t+1) = +0.12%  (≥ +0.05% ✓)
  min(X_t+2) = +0.15%  (≥ +0.05% ✓)
  → All points stay in Q1 (++), no crossing into other quadrants
```

### Key Design Principle: Min/Max Filtering, Not Mean Filtering

**IMPORTANT CHANGE**: The system uses **Min/Max values** (not Mean) as the primary filter.

```c
// OLD APPROACH (rejected):
mean >= 0.5%  // Filters by average, allows inconsistent points

// NEW APPROACH (adopted):
min(X) >= 0.05%  // ALL points must satisfy threshold
max(X) <= -0.05% // Ensures quadrant consistency
```

**Why Min/Max is superior:**
- **Mean**: Can pass with inconsistent points (e.g., [+0.8%, -0.2%, +0.5%] → mean +0.37%)
- **Min/Max**: Guarantees ALL points are consistent (no quadrant crossing)

**Mean is still calculated**: For reference and output, but NOT used as a filter threshold.

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
2. **Time-series pattern matching** (t-2 to t-0 → predict t+1, t+2)
3. **Quadrant-based filtering** (Min/Max constraints for 4 quadrants)
4. **Verification data output** (CSV files with all match points)

### Data Structure

**Input:** `1-deta-enginnering/crypto_data_hourly/BTC.txt`
- 17,482 hourly records (Nov 2023 - Nov 2025)
- 60 binary attributes (20 cryptos × 3 states: Up/Stay/Down)
- X column: hourly return (%)
- T column: timestamp

**Output:**
- `output/pool/zrp01a.txt`: Rule pool with statistics (Quadrant, Min, Max added)
- `output/verification/rule_XXXX.csv`: Match-by-match verification data

---

## Current Threshold Settings (main.c Implementation)

### Active Quality Thresholds

```c
// main.c lines 33-39 (current implementation)
#define Minsup 0.001              // 0.1% support rate
#define Maxsigx 0.5               // Maximum sigma: 0.5%
#define MIN_ATTRIBUTES 1          // Minimum attributes per rule
#define Minmean 0.2               // 0.2% (NOTE: Being replaced by quadrant filter)
#define MIN_CONCENTRATION 0.3     // Quadrant concentration (NOTE: Will be removed)
#define MIN_SUPPORT_COUNT 15      // Minimum 15 matches
```

### Target Threshold Settings (minx_implementation_policy.md)

**Simplified 3-threshold system:**

```c
// Recommended settings (to be implemented)
#define Minsup 0.001                    // 0.1% support
#define QUADRANT_THRESHOLD 0.0005       // 0.05% for quadrant determination
#define Maxsigx 0.001                   // 0.1% sigma (tight cluster)
```

**Filter sequence:**
```
1. Support filter (0.1%) - Fast, rejects most candidates
2. Quadrant filter (0.05% Min/Max) - NEW, ensures consistency
3. Sigma filter (0.1%) - Tight variance control
```

**What changed:**
- **Removed**: Minmean filter (0.2%), concentration bonuses, significance bonuses
- **Added**: Min/Max quadrant determination (4 quadrants supported)
- **Mean**: Calculated for output only, NOT used in filtering

---

## Implementation Policy (Based on minx_implementation_policy.md)

### Core Philosophy

1. **Quadrant consistency over high mean**: A pattern with min=+0.12% (all positive) is better than mean=+0.5% with mixed signs
2. **Simplicity**: 3 thresholds (Support, Quadrant, Sigma) replace complex bonus system
3. **Bidirectional**: All 4 quadrants are equally valuable (not just Q1)
4. **Filter/Fitness separation**: Filters determine acceptance, fitness guides evolution

### Filter Logic (New System)

```c
// check_rule_quality() function logic:

// Step 1: Support filter
if (support < Minsup || matched_count < MIN_SUPPORT_COUNT) {
    return false;
}

// Step 2: Quadrant filter (NEW - replaces mean filter)
double min_t1 = calculate_minimum_return(matched_count, matched_indices, 1);
double max_t1 = calculate_maximum_return(matched_count, matched_indices, 1);
double min_t2 = calculate_minimum_return(matched_count, matched_indices, 2);
double max_t2 = calculate_maximum_return(matched_count, matched_indices, 2);

int quadrant = determine_quadrant(min_t1, max_t1, min_t2, max_t2);

if (quadrant == 0) {
    return false;  // Does not fit any quadrant
}

// Step 3: Sigma filter
if (sigma_t1 > Maxsigx || sigma_t2 > Maxsigx) {
    return false;
}

// Mean is calculated for output, but NOT filtered
```

### Fitness Function (Simplified, Original Code Compliant)

```c
// Based on backups/既存コードの修正たち/original_code.c

// New rule:
fitness = num_attributes +                      // Attribute count
          support × 10 +                         // Support weight
          2 / (sigma_t1 + 0.001) +              // t+1 variance penalty
          2 / (sigma_t2 + 0.001) +              // t+2 variance penalty
          20;                                    // New rule bonus

// Duplicate rule (no bonus):
fitness = num_attributes +
          support × 10 +
          2 / (sigma_t1 + 0.001) +
          2 / (sigma_t2 + 0.001);
```

**Removed complexities:**
- ❌ Concentration bonuses (0-6000 points)
- ❌ Statistical significance bonuses (0-6000 points)
- ❌ Small cluster bonuses (0-1000 points)
- ✓ Simple 4-component fitness (original code style)

---

## Expected Results

### Before Quadrant Filter (Current: Minmean=0.2%)

With current settings:
- Discovered: ~hundreds to thousands of rules
- Problem: Some rules cross quadrants (inconsistent direction)
- Mean: 0.2-0.5% average, but individual points may vary widely

### After Quadrant Filter (Target: QUADRANT_THRESHOLD=0.05%)

**Expected with 0.05% threshold:**
- Total rules: **500-1,500 (all quadrants combined)**
- Quadrant breakdown:
  - Q1 (++): 200-600 rules (upward trends)
  - Q2 (-+): 100-300 rules (rebound patterns)
  - Q3 (--): 200-600 rules (downward trends)
  - Q4 (+-): 100-300 rules (reversal patterns)

**Quality improvement:**
- ✓ All matched points within single quadrant
- ✓ No directional inconsistency
- ✓ Clear interpretation (4 distinct pattern types)
- ✓ Tight variance (Maxsigx = 0.1%)

### Pattern Quality Comparison

```
Before (mean filter, Minmean=0.2%):
  Rule A: mean=+0.25%, points=[+0.8%, -0.15%, +0.5%, +0.3%]
  → Crosses quadrants (inconsistent)

After (quadrant filter, Q1):
  Rule B: min=+0.12%, max=+0.68%, mean=+0.35%
  → All points positive, Q1 consistent ✓

After (quadrant filter, Q2):
  Rule C: max_t1=-0.11%, min_t2=+0.12%
  → All t+1 negative, all t+2 positive, Q2 consistent ✓
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

### Quadrant Threshold Interpretation

```
QUADRANT_THRESHOLD = 0.0005 (0.05%)
→ Approximately μ + 0.08σ (very sensitive to direction)

Traditional "statistical significance":
  μ + 1σ = 0.52%  → 1σ significance
  μ + 2σ = 1.03%  → 2σ significance

Quadrant approach (0.05%):
  → Lower threshold, but GUARANTEES consistency
  → All points have same sign (directional certainty)
```

**Key difference:**
- **High mean threshold (0.5%)**: Fewer rules, high average, but may include inconsistent points
- **Quadrant threshold (0.05%)**: More rules, lower average, but ALL points consistent

---

## Output Format Changes

### Rule Pool Output (pool/zrp01a.txt)

**OLD format:**
```
Rule_ID,Support,Mean_t1,Sigma_t1,Mean_t2,Sigma_t2,Attributes
1,0.0234,0.000735,0.003521,0.000842,0.003158,"BTC_Up USDT_Up"
```

**NEW format (with quadrant info):**
```
Rule_ID,Support,Quadrant,Mean_t1,Sigma_t1,Min_t1,Max_t1,Mean_t2,Sigma_t2,Min_t2,Max_t2,Attributes
1,0.0234,Q1,0.000735,0.003521,0.001200,0.002500,0.000842,0.003158,0.001050,0.002800,"BTC_Up USDT_Up"
```

**Added columns:**
- `Quadrant`: Q1, Q2, Q3, or Q4
- `Min_t1`, `Max_t1`: t+1 min/max values
- `Min_t2`, `Max_t2`: t+2 min/max values

### Rule Output (text format)

```
=== Rule 42 ===
Support: 0.0234 (2.34%, 180 matches)
Quadrant: Q1 (Both Up)
Avg[1]= 0.000735 (0.0735%), Sig[1]= 0.003521 (0.3521%)
  Min[1]= 0.001200 (0.1200%), Max[1]= 0.002500 (0.2500%)
Avg[2]= 0.000842 (0.0842%), Sig[2]= 0.003158 (0.3158%)
  Min[2]= 0.001050 (0.1050%), Max[2]= 0.002800 (0.2800%)
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
- Min/Max boundaries (new)
- Mean lines and statistics
- Visual confirmation of quadrant consistency

---

## Important Notes

- **Character encoding**: UTF-8 for new files, legacy files may be Shift-JIS
- **Mathematical dependencies**: Link with `-lm` flag
- **Memory limits**: `Nrulemax = 2002` rules maximum
- **Time delays**: System uses t-2, t-1, t-0 for pattern matching (adaptive delay)
- **Prediction span**: Forecasts t+1 and t+2

---

## Research Value Checklist (Updated)

A discovered rule has research value if:

- [ ] **Quadrant consistent**: min/max values satisfy single quadrant constraints
- [ ] **Support count >= 15**: Statistical reliability (current setting)
- [ ] **Low variance**: sigma <= 0.1% (tight cluster)
- [ ] **Pattern is interpretable**: Meaningful attribute combinations
- [ ] **Verified on out-of-sample data**: Not overfitted

**Note**: Mean value is no longer a primary criterion. Quadrant consistency is the key quality measure.

---

## Implementation Roadmap

### Phase 1: Core Quadrant System (Priority: High)

1. Add `calculate_minimum_return()` function (main.c:~1760)
2. Add `calculate_maximum_return()` function (main.c:~1775)
3. Add `determine_quadrant()` function (main.c:~1790)
4. Add `get_quadrant_name()` helper function (main.c:~1815)
5. Modify `check_rule_quality()` to use quadrant filter (main.c:~1833)
6. Update `rule_save()` to output Min/Max/Quadrant (main.c:~3187)

### Phase 2: Simplification (Priority: Medium)

1. Remove complex bonus macros (lines 86-112)
2. Remove bonus calculation functions:
   - `calculate_significance_bonus()`
   - `calculate_small_cluster_bonus()`
   - `calculate_concentration_fitness_bonus()`
3. Simplify fitness calculation (return to original 4-component formula)

### Phase 3: Testing and Validation (Priority: High)

1. Test with QUADRANT_THRESHOLD = 0.0005 (0.05%)
2. Verify quadrant distribution (expect all 4 quadrants)
3. Validate output format (CSV and text)
4. Check scatter plots for visual confirmation

**Estimated implementation time**: 2-3 hours

---

## Key Differences from Previous Approach

| Aspect | OLD (Mean-based) | NEW (Quadrant-based) |
|--------|------------------|---------------------|
| **Primary filter** | mean >= 0.05% | min/max quadrant check |
| **Mean usage** | Filter threshold | Output only (reference) |
| **Directionality** | Average-based | ALL points consistent |
| **Pattern types** | Mainly Q1 (positive) | All 4 quadrants |
| **Thresholds** | Multiple (3-5) | Simplified (3) |
| **Fitness bonuses** | Complex (9 levels) | Simple (4 components) |
| **Expected rules** | 100-1000 | 500-1500 (all quadrants) |

---

## Reference Documents

- **Implementation details**: `/docs/minx_implementation_policy.md` (v2.0, 2025-11-11)
- **Algorithm overview**: `/docs/rule_discovery_algorithm.md`
- **Support definition**: `/docs/support_definition.md`
- **Original code reference**: `backups/既存コードの修正たち/original_code.c`

---

**Document version**: 2.0 (Quadrant-based filtering)
**Last updated**: 2025-11-11
**Status**: Implementation in progress
