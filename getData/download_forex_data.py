#!/usr/bin/env python3
"""
為替データダウンロードスクリプト
Yahoo FinanceからG7通貨ペア + 主要通貨ペアのデータを取得
"""

import yfinance as yf
import pandas as pd
from datetime import datetime, timedelta
import os

# 為替ペアリスト（Yahoo Financeのティッカーシンボル）
FOREX_PAIRS = {
    # 対円ペア
    'USDJPY': 'USDJPY=X',  # ドル/円
    'EURJPY': 'EURJPY=X',  # ユーロ/円
    'GBPJPY': 'GBPJPY=X',  # ポンド/円
    'AUDJPY': 'AUDJPY=X',  # 豪ドル/円
    'NZDJPY': 'NZDJPY=X',  # NZドル/円
    'CADJPY': 'CADJPY=X',  # カナダドル/円
    'CHFJPY': 'CHFJPY=X',  # スイスフラン/円

    # 主要クロスペア
    'EURUSD': 'EURUSD=X',  # ユーロ/ドル
    'GBPUSD': 'GBPUSD=X',  # ポンド/ドル
    'AUDUSD': 'AUDUSD=X',  # 豪ドル/ドル
    'NZDUSD': 'NZDUSD=X',  # NZドル/ドル
    'USDCAD': 'USDCAD=X',  # ドル/カナダドル
    'USDCHF': 'USDCHF=X',  # ドル/スイスフラン

    # その他の主要ペア
    'EURGBP': 'EURGBP=X',  # ユーロ/ポンド
    'EURAUD': 'EURAUD=X',  # ユーロ/豪ドル
    'EURCHF': 'EURCHF=X',  # ユーロ/スイスフラン
    'GBPAUD': 'GBPAUD=X',  # ポンド/豪ドル
    'GBPCAD': 'GBPCAD=X',  # ポンド/カナダドル
    'AUDCAD': 'AUDCAD=X',  # 豪ドル/カナダドル
    'AUDNZD': 'AUDNZD=X',  # 豪ドル/NZドル
}

def download_forex_data(start_date='2010-01-01', end_date=None):
    """
    為替データをダウンロード

    Parameters:
    -----------
    start_date : str
        開始日（YYYY-MM-DD形式）
    end_date : str
        終了日（YYYY-MM-DD形式）。Noneの場合は今日まで

    Returns:
    --------
    pd.DataFrame
        タイムスタンプと各通貨ペアの終値を含むDataFrame
    """
    if end_date is None:
        end_date = datetime.now().strftime('%Y-%m-%d')

    print(f"=== Downloading Forex Data ===")
    print(f"Period: {start_date} to {end_date}")
    print(f"Pairs: {len(FOREX_PAIRS)}")
    print()

    # データを格納する辞書
    data_dict = {}
    failed_pairs = []

    for pair_name, ticker_symbol in FOREX_PAIRS.items():
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

    # 欠損値を前方埋め（為替は土日が休み）
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

    # 出力ディレクトリ作成
    os.makedirs('forex_data', exist_ok=True)

    # データダウンロード
    df = download_forex_data(start_date='2010-01-01')

    # CSVに保存
    output_file = 'forex_data/forex_raw.csv'
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
