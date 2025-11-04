#!/usr/bin/env python3
"""
Binance APIからデータをダウンロードして、GNMiner形式に変換
各銘柄ごとに個別ファイルを作成
"""

from binance.client import Client
import pandas as pd
from datetime import datetime, timedelta
import os
import time

# Binanceの仮想通貨ペアリスト（2020年1月1日から継続的にデータがある28通貨）
CRYPTO_PAIRS = {
    # メジャー通貨
    'BTC': 'BTCUSDT',      'ETH': 'ETHUSDT',      'BNB': 'BNBUSDT',
    'XRP': 'XRPUSDT',      'ADA': 'ADAUSDT',      'DOGE': 'DOGEUSDT',
    'TRX': 'TRXUSDT',      'LTC': 'LTCUSDT',      'BCH': 'BCHUSDT',
    'LINK': 'LINKUSDT',
    # アルトコイン
    'MATIC': 'MATICUSDT',  'XLM': 'XLMUSDT',      'ATOM': 'ATOMUSDT',
    'ETC': 'ETCUSDT',      'VET': 'VETUSDT',      'STX': 'STXUSDT',
    'HBAR': 'HBARUSDT',    'EOS': 'EOSUSDT',      'NEO': 'NEOUSDT',
    'DASH': 'DASHUSDT',    'ZEC': 'ZECUSDT',      'QTUM': 'QTUMUSDT',
    'ZIL': 'ZILUSDT',      'ONT': 'ONTUSDT',      'BAT': 'BATUSDT',
    'IOTA': 'IOTAUSDT',    'ALGO': 'ALGOUSDT',    'ENJ': 'ENJUSDT',
}

def download_binance_klines(client, symbol, interval, start_date, end_date):
    """Binance APIから時系列データを取得"""
    klines = []
    current_start = start_date

    while current_start < end_date:
        try:
            batch = client.get_historical_klines(
                symbol,
                interval,
                start_str=current_start.strftime('%Y-%m-%d'),
                end_str=min(current_start + timedelta(days=40), end_date).strftime('%Y-%m-%d')
            )

            if not batch:
                break

            klines.extend(batch)
            last_timestamp = batch[-1][0]
            current_start = datetime.fromtimestamp(last_timestamp / 1000) + timedelta(hours=4)
            time.sleep(0.1)

        except Exception as e:
            print(f" Error: {e}")
            break

    return klines

def download_all_pairs(start_date='2020-01-01', end_date='2025-01-01', interval='4h'):
    """全ペアのデータをダウンロード"""

    start_dt = datetime.strptime(start_date, '%Y-%m-%d')
    end_dt = datetime.strptime(end_date, '%Y-%m-%d')

    print(f"=== Downloading Cryptocurrency Data from Binance ===")
    print(f"Period: {start_date} to {end_date}")
    print(f"Interval: {interval}")
    print(f"Pairs: {len(CRYPTO_PAIRS)}")
    print()

    client = Client()
    interval_map = {
        '1h': Client.KLINE_INTERVAL_1HOUR,
        '4h': Client.KLINE_INTERVAL_4HOUR,
        '1d': Client.KLINE_INTERVAL_1DAY,
    }
    binance_interval = interval_map[interval]

    data_dict = {}
    total_pairs = len(CRYPTO_PAIRS)

    for idx, (pair_name, binance_symbol) in enumerate(CRYPTO_PAIRS.items(), 1):
        print(f"[{idx}/{total_pairs}] {pair_name}...", end=' ', flush=True)

        try:
            klines = download_binance_klines(client, binance_symbol, binance_interval, start_dt, end_dt)

            if not klines:
                print(f"⚠ No data")
                continue

            df = pd.DataFrame(klines, columns=[
                'timestamp', 'open', 'high', 'low', 'close', 'volume',
                'close_time', 'quote_asset_volume', 'number_of_trades',
                'taker_buy_base_asset_volume', 'taker_buy_quote_asset_volume', 'ignore'
            ])

            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='ms')
            df['close'] = df['close'].astype(float)
            close_series = df.set_index('timestamp')['close']
            data_dict[pair_name] = close_series

            print(f"✓ {len(df)} records")

        except Exception as e:
            print(f"✗ {e}")

    combined_df = pd.DataFrame(data_dict).ffill().reset_index()
    combined_df = combined_df.rename(columns={'index': 'timestamp'})

    print(f"\n✓ Downloaded {len(combined_df)} records for {len(data_dict)} pairs")
    return combined_df

def calculate_returns(df, price_columns):
    """変化率を計算"""
    returns_df = pd.DataFrame()

    datetime_series = pd.to_datetime(df['timestamp'], utc=True)
    returns_df['T'] = datetime_series.dt.strftime('%Y-%m-%d')

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

    # 1. データダウンロード
    print("=" * 70)
    print("  Step 1: Download Raw Data from Binance")
    print("=" * 70)

    df = download_all_pairs(start_date='2020-01-01', end_date='2025-01-01', interval='4h')

    # RAWデータを保存
    raw_output_dir = '../crypto_data_binance'
    os.makedirs(raw_output_dir, exist_ok=True)
    raw_file = os.path.join(raw_output_dir, 'crypto_raw.csv')
    df.to_csv(raw_file, index=False)
    print(f"\n✓ Raw data saved to: {raw_file}")

    # 2. 変化率計算
    print("\n" + "=" * 70)
    print("  Step 2: Calculate Returns")
    print("=" * 70)

    price_columns = [col for col in df.columns if col not in ['timestamp', 'T']]
    returns_df = calculate_returns(df, price_columns)
    print(f"✓ Calculated returns for {len(price_columns)} pairs")

    # 3. Up/Stay/Down分類
    print("\n" + "=" * 70)
    print("  Step 3: Classify Movements")
    print("=" * 70)

    classified_df = classify_movements(returns_df, price_columns)
    classified_df = classified_df.dropna()
    print(f"✓ Classified {len(classified_df)} records")

    # 4. 個別ファイル作成
    print("\n" + "=" * 70)
    print("  Step 4: Create Individual Files")
    print("=" * 70)

    create_individual_files(classified_df, returns_df, raw_output_dir)

    print("\n" + "=" * 70)
    print("  Complete!")
    print("=" * 70)
    print(f"Raw data: {raw_file}")
    print(f"Individual files: {raw_output_dir}/")
    print(f"Files created: {len(price_columns)}")
    print(f"Records per file: ~{len(classified_df)}")
    print(f"Attributes per file: {len(price_columns)} * 3 = {len(price_columns) * 3}")

if __name__ == '__main__':
    main()
