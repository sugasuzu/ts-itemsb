# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C-based data mining/rule extraction system that implements a Genetic Network Programming (GNP) algorithm. The main program analyzes datasets to discover association rules using evolutionary computation techniques.

## Build Commands

```bash
# Compile the main program
gcc -o sample_02cvr sample_02cvr.c -lm

# Run the program
./sample_02cvr
```

## Core Architecture

### Main Components

1. **sample_02cvr.c** - Main GNP-based rule mining implementation
   - Implements evolutionary algorithm with 201 generations
   - Population size: 120 individuals
   - Uses processing nodes (10) and judgment nodes (100) per individual
   - Performs crossover and mutation operations for evolution

2. **Input Files**
   - `testdata1s.txt` - Training dataset (159 records × 95 attributes + 2 output values)
   - `testdic1.txt` - Attribute dictionary/names (Japanese encoded)

3. **Output Files**
   - `zrp01a.txt` - Rule pool with attribute names
   - `zrp01b.txt` - Rule pool with numbers
   - `zrd01.txt` - Documentation file
   - `zrmemo01.txt` - Memo/log file

### Key Parameters

- **Nrd**: 159 total records
- **Nzk**: 95 attributes for antecedent
- **Nrulemax**: 2002 maximum rules in pool
- **Minsup**: 0.04 minimum support threshold
- **Generation**: 201 generations for evolution
- **Nkotai**: 120 population size

### Algorithm Flow

1. Reads training data and attribute dictionary
2. Initializes GNP population with random nodes
3. Evolves population through selection, crossover, and mutation
4. Extracts association rules meeting support/confidence thresholds
5. Outputs discovered rules to result files

## Important Notes

- Character encoding: Files contain Shift-JIS encoded Japanese text
- Line endings: CRLF format (Windows-style)
- Mathematical dependencies: Requires linking with math library (-lm flag)