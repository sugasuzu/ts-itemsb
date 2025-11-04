#!/usr/bin/env python3
"""
crypto_data_hourly/crypto_raw_hourly.csv をGNMiner形式に変換
各通貨ごとに個別ファイルを作成
"""

import pandas as pd
import os

def calculate_returns(df, price_columns):
    """変化率を計算"""
    returns_df = pd.DataFrame()

    datetime_series = pd.to_datetime(df['timestamp'], utc=True)
    returns_df['T'] = datetime_series.dt.strftime('%Y-%m-%d %H:%M:%S')

    for col in price_columns:
        if col in df.columns:
            returns_df[col] = df[col].pct_change() * 100

    return returns_df

def classify_movements(returns_df, price_columns):
    """Up/Stay/Downに分類"""
    classified_df = pd.DataFrame()
    classified_df['T'] = returns_df['T']

    for col in price_columns:
        if col not in returns_df.columns:
            continue

        returns = returns_df[col]
        classified_df[f'{col}_Up'] = (returns > 0).astype(int)
        classified_df[f'{col}_Stay'] = (returns == 0).astype(int)
        classified_df[f'{col}_Down'] = (returns < 0).astype(int)

    return classified_df

def create_individual_files(classified_df, returns_df, output_dir):
    """個別ファイルに分割"""

    os.makedirs(output_dir, exist_ok=True)
    print(f"\n=== Creating individual files in {output_dir} ===")

    all_columns = [col for col in classified_df.columns if col not in ['T', 'timestamp']]

    base_names = set()
    for col in all_columns:
        if col.endswith('_Up') or col.endswith('_Stay') or col.endswith('_Down'):
            base_name = col.rsplit('_', 1)[0]
            base_names.add(base_name)

    print(f"Creating {len(base_names)} files...")

    for idx, base_name in enumerate(sorted(base_names), 1):
        related_cols = [col for col in all_columns if col.startswith(f'{base_name}_')]
        other_cols = [col for col in all_columns if col not in related_cols]
        all_cols = related_cols + other_cols

        individual_df = classified_df[all_cols + ['T']].copy()

        if base_name in returns_df.columns:
            individual_df['X'] = returns_df[base_name].round(2)
        else:
            individual_df['X'] = 0.0

        individual_df = individual_df.dropna()

        if len(individual_df) > 0:
            individual_df = individual_df.iloc[1:].reset_index(drop=True)

        cols = [col for col in individual_df.columns if col not in ['T', 'X']]
        cols.append('X')
        cols.append('T')
        individual_df = individual_df[cols]

        output_file = os.path.join(output_dir, f'{base_name}.txt')
        individual_df.to_csv(output_file, index=False)

        if idx <= 5 or idx % 5 == 0:
            print(f"  [{idx}/{len(base_names)}] {base_name}.txt ({len(individual_df)} records)")

    print(f"✓ Created {len(base_names)} files")

def main():
    """メイン処理"""

    print("=" * 70)
    print("  Convert Hourly Data to GNMiner Format")
    print("=" * 70)

    # 1. データ読み込み
    input_file = '../crypto_data_hourly/crypto_raw_hourly.csv'
    print(f"Input: {input_file}")

    df = pd.read_csv(input_file)
    print(f"✓ Loaded {len(df)} records")

    # 2. 変化率計算
    print("\n" + "=" * 70)
    print("  Step 1: Calculate Returns")
    print("=" * 70)

    price_columns = [col for col in df.columns if col not in ['timestamp', 'T']]
    print(f"Price columns: {len(price_columns)} ({', '.join(price_columns[:5])}...)")

    returns_df = calculate_returns(df, price_columns)
    print(f"✓ Calculated returns for {len(price_columns)} pairs")

    # 3. Up/Stay/Down分類
    print("\n" + "=" * 70)
    print("  Step 2: Classify Movements")
    print("=" * 70)

    classified_df = classify_movements(returns_df, price_columns)
    classified_df = classified_df.dropna()
    print(f"✓ Classified {len(classified_df)} records")

    # 4. 個別ファイル作成
    print("\n" + "=" * 70)
    print("  Step 3: Create Individual Files")
    print("=" * 70)

    output_dir = '../crypto_data_hourly'
    create_individual_files(classified_df, returns_df, output_dir)

    print("\n" + "=" * 70)
    print("  Complete!")
    print("=" * 70)
    print(f"Input: {input_file}")
    print(f"Output directory: {output_dir}/")
    print(f"Files created: {len(price_columns)}")
    print(f"Records per file: ~{len(classified_df)}")
    print(f"Attributes per file: {len(price_columns)} * 3 = {len(price_columns) * 3}")

if __name__ == '__main__':
    main()
