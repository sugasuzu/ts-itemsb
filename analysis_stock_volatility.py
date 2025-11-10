#!/usr/bin/env python3
"""
Calculate volatility (standard deviation of X) for all Nikkei 225 stocks
to identify high-volatility candidates for GNP rule discovery.
"""

import os
import pandas as pd
import numpy as np
from pathlib import Path

def calculate_stock_volatility(data_dir):
    """Calculate volatility for all stocks in the directory."""

    results = []

    # Get all .txt files
    stock_files = sorted(Path(data_dir).glob("*.txt"))

    print(f"Analyzing {len(stock_files)} stocks...")

    for filepath in stock_files:
        stock_code = filepath.stem

        try:
            # Read file
            df = pd.read_csv(filepath, header=None)

            # X is second-to-last column, T is last column
            x_values = pd.to_numeric(df.iloc[:, -2], errors='coerce')

            # Remove NaN values
            x_values = x_values.dropna()

            if len(x_values) < 100:  # Skip if too few data points
                continue

            # Calculate statistics
            mean_x = x_values.mean()
            std_x = x_values.std()
            abs_mean_x = x_values.abs().mean()
            min_x = x_values.min()
            max_x = x_values.max()
            count = len(x_values)

            results.append({
                'stock_code': stock_code,
                'volatility_std': std_x,
                'mean_x': mean_x,
                'abs_mean_x': abs_mean_x,
                'min_x': min_x,
                'max_x': max_x,
                'range': max_x - min_x,
                'count': count
            })

        except Exception as e:
            print(f"Error processing {stock_code}: {e}")
            continue

    # Create DataFrame and sort by volatility
    df_results = pd.DataFrame(results)
    df_results = df_results.sort_values('volatility_std', ascending=False)

    return df_results


def main():
    data_dir = "/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/nikkei225_data/gnminer_individual"

    print("="*70)
    print("Nikkei 225 Stock Volatility Analysis")
    print("="*70)
    print()

    # Calculate volatility
    df = calculate_stock_volatility(data_dir)

    # Overall statistics
    print(f"Total stocks analyzed: {len(df)}")
    print(f"Mean volatility: {df['volatility_std'].mean():.4f}%")
    print(f"Median volatility: {df['volatility_std'].median():.4f}%")
    print(f"Max volatility: {df['volatility_std'].max():.4f}%")
    print()

    # Top 20 high-volatility stocks
    print("="*70)
    print("TOP 20 HIGH-VOLATILITY STOCKS (Recommended for GNP)")
    print("="*70)
    print()
    print(df.head(20).to_string(index=False))
    print()

    # Bottom 20 low-volatility stocks (avoid these)
    print("="*70)
    print("BOTTOM 20 LOW-VOLATILITY STOCKS (Not recommended)")
    print("="*70)
    print()
    print(df.tail(20).to_string(index=False))
    print()

    # Save full results
    output_file = "/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/nikkei225_volatility_ranking.csv"
    df.to_csv(output_file, index=False)
    print(f"Full results saved to: {output_file}")
    print()

    # Comparison with BTC
    btc_std = 0.5121  # From your BTC analysis
    print("="*70)
    print("COMPARISON WITH CRYPTOCURRENCY (BTC)")
    print("="*70)
    print(f"BTC hourly volatility: {btc_std:.4f}%")
    print(f"Highest stock volatility: {df['volatility_std'].iloc[0]:.4f}% ({df['stock_code'].iloc[0]})")
    print(f"Median stock volatility: {df['volatility_std'].median():.4f}%")
    print()
    print(f"BTC is {btc_std / df['volatility_std'].iloc[0]:.2f}x more volatile than highest stock")
    print(f"BTC is {btc_std / df['volatility_std'].median():.2f}x more volatile than median stock")
    print()


if __name__ == "__main__":
    main()
