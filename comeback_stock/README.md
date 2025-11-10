# Comeback Stock - GNP-based Rule Discovery for Nikkei 225

## Overview

This directory contains the GNP (Genetic Network Programming) system adapted for **Nikkei 225 stock market analysis**. The system discovers temporal patterns in high-volatility stocks using evolutionary algorithms.

---

## Research Goal

**学術研究（論文発表）を目的とした、日経225高ボラティリティ株のルール発見**

### Key Features

1. **High-Volatility Target Stocks**: Top 10 stocks with σ = 2.93% - 6.62% (daily)
2. **Rich Attributes**: 660 attributes (220 stocks × 3 states: Up/Stay/Down)
3. **Long-term Data**: 21 years (2000-01-05 to 2020-12-30, 5,268 data points)
4. **Temporal Pattern Mining**: GNP-based evolutionary rule discovery

---

## Directory Structure

```
comeback_stock/
├── main.c              # GNP implementation for stock data
├── Makefile            # Build and experiment automation
├── TOP10_STOCKS.txt    # List of top 10 high-volatility stocks
├── README.md           # This file
└── output/             # Experimental results (created on run)
    └── {STOCK_CODE}/
        ├── pool/       # Discovered rules (zrp01a.txt)
        ├── verification/  # Match-by-match verification data
        ├── doc/        # Documentation and reports
        └── vis/        # Visualization data
```

---

## Top 10 High-Volatility Stocks

| Rank | Code | Volatility (σ) | Mean (μ) | Description |
|------|------|----------------|----------|-------------|
| 1    | 3436 | 6.62%          | 0.12%    | Highest volatility |
| 2    | 4755 | 3.40%          | 0.07%    | High volatility #2 |
| 3    | 9984 | 3.34%          | 0.06%    | SoftBank Group |
| 4    | 1808 | 3.29%          | 0.07%    | High volatility #4 |
| 5    | 5541 | 3.22%          | 0.06%    | High volatility #5 |
| 6    | 6976 | 3.13%          | 0.05%    | High volatility #6 |
| 7    | 5707 | 2.96%          | 0.06%    | High volatility #7 |
| 8    | 7003 | 2.94%          | 0.03%    | High volatility #8 |
| 9    | 6857 | 2.93%          | 0.04%    | High volatility #9 |
| 10   | 7202 | 2.93%          | 0.06%    | High volatility #10 |

**Comparison with Cryptocurrency:**
- BTC (hourly): σ = 0.51%
- Stock (daily): σ = 2.93% - 6.62%
- **→ Stocks have 5.7x - 13.0x higher volatility**

---

## Quick Start

### 1. Compile

```bash
make
```

### 2. Run Test Experiment (Stock 3436)

```bash
make test
```

### 3. Run on Specific Stock

```bash
make run-stock STOCK=9984
```

### 4. View Results

```bash
make show-results STOCK=3436
```

---

## Batch Experiments

### Run All Top 10 Stocks

```bash
make run-top10
```

### Run Top 3 Stocks (Quick Test)

```bash
make run-top3
```

---

## Output Files

### Rule Pool: `output/{STOCK}/pool/zrp01a.txt`

**Format:**
```
Attr1  Attr2  Attr3  ...  X(t+1)_mean  X(t+1)_sigma  X(t+2)_mean  X(t+2)_sigma  support_count  ...
```

**Example:**
```
9766_Up(t-1)  2432_Stay(t-1)  4755_Stay(t-0)  ...  0.278  0.954  0.171  0.853  23  0.0044  ...
```

### Verification Data: `output/{STOCK}/verification/rule_{ID}.csv`

**Format:**
```
Row,Time,X(t+1),X(t+2)
1234,2005-03-15,0.52,0.31
1567,2008-07-22,-0.83,-0.45
...
```

---

## Key Parameters

### GNP Evolution
- **Generations**: 201
- **Population**: 120 individuals
- **Mutation rate**: 1-6%

### Rule Quality Filters
- **Minimum support**: 0.001 (0.1%)
- **Maximum variance**: 1.0% (σ)
- **Minimum mean**: 0.1%
- **Minimum attributes**: 2

### Temporal Analysis
- **Delay range**: t-0 to t-3 (adaptive)
- **Prediction span**: t+1, t+2

---

## Data Format

### Input: `../nikkei225_data/gnminer_individual/{CODE}.txt`

**Structure:**
```csv
Stock1_Up,Stock1_Stay,Stock1_Down,...,Stock220_Up,Stock220_Stay,Stock220_Down,X,T
0,1,0,...,1,0,0,0.52,2000-01-05
1,0,0,...,0,1,0,-0.31,2000-01-06
...
```

- **Columns 1-660**: Binary attributes (stock states)
- **Column 661 (X)**: Target stock's daily return (%)
- **Column 662 (T)**: Date (YYYY-MM-DD)

---

## Comparison: Crypto vs. Stock

| Feature | Cryptocurrency (BTC) | Nikkei 225 Stocks |
|---------|---------------------|-------------------|
| **Data Granularity** | Hourly | Daily |
| **Volatility (σ)** | 0.51% | 2.93% - 6.62% |
| **Data Period** | 2 years (17,482 points) | 21 years (5,268 points) |
| **Attributes** | 60 (20 cryptos × 3) | 660 (220 stocks × 3) |
| **Match Frequency** | High (hourly) | Moderate (daily) |

**→ Stocks provide higher volatility and longer data history, ideal for academic research.**

---

## Experimental Results Example (Stock 3436)

```
Total Rules Discovered: 2000
High-Support Rules: 23
Execution Time: 6.95s

Top Rule Example:
  Attributes: 9766_Up(t-1), 2432_Stay(t-1), 4755_Stay(t-0), ...
  X(t+1) mean: 0.278%
  X(t+1) sigma: 0.954%
  Support: 23 matches (0.44%)
```

---

## Commands Reference

### Build
```bash
make            # Compile
make clean      # Remove build files
```

### Run Experiments
```bash
make test                      # Test with stock 3436
make run-stock STOCK=9984      # Run specific stock
make run-top3                  # Quick run: top 3 stocks
make run-top10                 # Full run: all top 10 stocks
```

### View Results
```bash
make list-results              # List completed experiments
make show-results STOCK=3436   # Show results for specific stock
```

### Cleanup
```bash
make clean-stock STOCK=3436    # Clean specific stock output
make clean-output              # Clean all outputs
make distclean                 # Full cleanup (build + outputs)
```

### Help
```bash
make help       # Show all commands
```

---

## Research Notes

### Advantages for Academic Publication

1. **High Volatility**: 5-13x higher than crypto (easier to discover significant patterns)
2. **Long-term Data**: 21 years → statistically reliable
3. **Rich Context**: 660 attributes → complex inter-stock relationships
4. **Real-world Application**: Stock market → high relevance to investors
5. **Novelty**: GNP for stock pattern mining is relatively unexplored

### Potential Research Questions

1. **Cross-stock dependencies**: How do other stocks predict high-volatility stock movements?
2. **Temporal patterns**: What are the optimal time delays for prediction?
3. **Volatility regimes**: Do patterns differ across low/high volatility periods?
4. **Sector effects**: Do sector movements predict individual stock returns?

---

## Data Source

- **Source Directory**: `/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/nikkei225_data/gnminer_individual/`
- **Format**: CSV (no header, 659 columns)
- **Period**: 2000-01-05 to 2020-12-30
- **Records per Stock**: 5,268 days (~21 years)

---

## Implementation Notes

### Changes from Crypto Version

1. **Data path**: `crypto_data_hourly/` → `../nikkei225_data/gnminer_individual/`
2. **Output path**: `crypto_data_hourly/output/` → `comeback_stock/output/`
3. **Default stock**: `BTC` → `3436`
4. **Attributes**: 60 → 660 (already supported by `MAX_ATTRS = 1000`)

### Key Files Modified

- `main.c` line 28: `DATANAME` path updated
- `main.c` line 161: Default `stock_code` changed to `"3436"`
- `main.c` line 3180: Data file path format
- `main.c` line 3184: Output directory format

---

## Future Work

1. **Phase 2 fitness function**: Implement advanced quality metrics
2. **Statistical significance testing**: Add t-tests, p-values
3. **Visualization**: Create scatter plots (X(t+1) vs X(t+2), X(t+1) vs Time)
4. **Comparison analysis**: Compare rules across different volatility stocks
5. **Out-of-sample validation**: Split data for training/testing

---

## Author & Date

- **Project**: Comeback Stock Analysis
- **Date**: 2025-01-10
- **Purpose**: Academic research (論文発表)
- **Target**: Nikkei 225 high-volatility stocks

---

## Quick Reference

**Test run:**
```bash
cd comeback_stock
make test
```

**Full experiment (all top 10):**
```bash
make run-top10
```

**View results:**
```bash
head -20 output/3436/pool/zrp01a.txt
```

---

## Contact & Support

For questions or issues, refer to:
- Main project: `/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/`
- Volatility ranking: `/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/nikkei225_volatility_ranking.csv`
- Original crypto system: `/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/main.c`
