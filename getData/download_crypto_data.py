#!/usr/bin/env python3
"""
仮想通貨データダウンロードスクリプト
Yahoo Financeから主要な仮想通貨ペアのデータを取得
"""

import yfinance as yf
import pandas as pd
from datetime import datetime, timedelta
import os

# 仮想通貨ペアリスト（Yahoo Financeのティッカーシンボル）
CRYPTO_PAIRS = {
    # メジャー仮想通貨（時価総額TOP）
    'BTC-USD': 'BTC-USD',    # Bitcoin
    'ETH-USD': 'ETH-USD',    # Ethereum
    'BNB-USD': 'BNB-USD',    # Binance Coin
    'XRP-USD': 'XRP-USD',    # Ripple
    'ADA-USD': 'ADA-USD',    # Cardano
    'SOL-USD': 'SOL-USD',    # Solana
    'DOGE-USD': 'DOGE-USD',  # Dogecoin
    'DOT-USD': 'DOT-USD',    # Polkadot
    'MATIC-USD': 'MATIC-USD', # Polygon
    'AVAX-USD': 'AVAX-USD',  # Avalanche

    # アルトコイン（高ボラティリティ候補）
    'LINK-USD': 'LINK-USD',  # Chainlink
    'UNI-USD': 'UNI-USD',    # Uniswap
    'ATOM-USD': 'ATOM-USD',  # Cosmos
    'LTC-USD': 'LTC-USD',    # Litecoin
    'ETC-USD': 'ETC-USD',    # Ethereum Classic
    'XLM-USD': 'XLM-USD',    # Stellar
    'ALGO-USD': 'ALGO-USD',  # Algorand
    'VET-USD': 'VET-USD',    # VeChain
    'FIL-USD': 'FIL-USD',    # Filecoin
    'AAVE-USD': 'AAVE-USD',  # Aave
}

def download_crypto_data(start_date='2020-01-01', end_date=None):
    """
    仮想通貨データをダウンロード

    Parameters:
    -----------
    start_date : str
        開始日（YYYY-MM-DD形式）
    end_date : str
        終了日（YYYY-MM-DD形式）。Noneの場合は今日まで

    Returns:
    --------
    pd.DataFrame
        タイムスタンプと各仮想通貨ペアの終値を含むDataFrame
    """
    if end_date is None:
        end_date = datetime.now().strftime('%Y-%m-%d')

    print(f"=== Downloading Cryptocurrency Data ===")
    print(f"Period: {start_date} to {end_date}")
    print(f"Pairs: {len(CRYPTO_PAIRS)}")
    print()

    # データを格納する辞書
    data_dict = {}
    failed_pairs = []

    for pair_name, ticker_symbol in CRYPTO_PAIRS.items():
        try:
            print(f"Downloading {pair_name} ({ticker_symbol})...", end=' ')

            # データをダウンロード
            ticker = yf.Ticker(ticker_symbol)
            df = ticker.history(start=start_date, end=end_date, interval='1d')

            if df.empty:
                print(f"⚠ No data")
                failed_pairs.append(pair_name)
                continue

            # 終値データを抽出
            data_dict[pair_name] = df['Close']
            print(f"✓ {len(df)} records")

        except Exception as e:
            print(f"✗ Error: {e}")
            failed_pairs.append(pair_name)

    if not data_dict:
        raise ValueError("No data downloaded. Please check your internet connection.")

    # DataFrameに統合
    combined_df = pd.DataFrame(data_dict)

    # カラム名から -USD サフィックスを削除
    combined_df.columns = [col.replace('-USD', '') for col in combined_df.columns]

    # 欠損値を前方埋め（一部取引所の休止日対応）
    combined_df = combined_df.fillna(method='ffill')

    # タイムスタンプをリセット
    combined_df = combined_df.reset_index()
    combined_df = combined_df.rename(columns={'Date': 'timestamp'})

    print()
    print("=== Download Summary ===")
    print(f"Total records: {len(combined_df)}")
    print(f"Successful pairs: {len(data_dict)}")
    print(f"Failed pairs: {len(failed_pairs)}")
    if failed_pairs:
        print(f"Failed: {', '.join(failed_pairs)}")
    print(f"Date range: {combined_df['timestamp'].min()} to {combined_df['timestamp'].max()}")

    return combined_df

def main():
    """メイン処理"""

    # 出力ディレクトリ作成（ルートディレクトリ直下）
    os.makedirs('../crypto_data', exist_ok=True)

    # データダウンロード
    df = download_crypto_data(start_date='2020-01-01')

    # CSVに保存
    output_file = '../crypto_data/crypto_raw.csv'
    df.to_csv(output_file, index=False)

    print()
    print(f"✓ Data saved to: {output_file}")
    print(f"  Shape: {df.shape}")
    print(f"  Columns: {list(df.columns)}")

    # サンプルデータを表示
    print()
    print("=== Sample Data (first 5 rows) ===")
    print(df.head())

    print()
    print("=== Sample Data (last 5 rows) ===")
    print(df.tail())

if __name__ == '__main__':
    main()
