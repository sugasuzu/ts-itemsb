#!/usr/bin/env python3
"""
仮想通貨データダウンロードスクリプト（Binance API使用）
Binance APIから主要な仮想通貨ペアの時間足データを取得
過去データの制限なし
"""

from binance.client import Client
import pandas as pd
from datetime import datetime, timedelta
import os
import time

# Binanceの仮想通貨ペアリスト（2020年1月1日から継続的にデータがある28通貨）
# 注意: BinanceではペアのシンボルがYahooと異なる（例: BTC/USDTではなくBTCUSDT）
# check_binance_pairs.pyで確認済み（各通貨約10,962レコード、4時間足で5年分）
CRYPTO_PAIRS = {
    # メジャー通貨
    'BTC': 'BTCUSDT',      # Bitcoin
    'ETH': 'ETHUSDT',      # Ethereum
    'BNB': 'BNBUSDT',      # Binance Coin
    'XRP': 'XRPUSDT',      # Ripple
    'ADA': 'ADAUSDT',      # Cardano
    'DOGE': 'DOGEUSDT',    # Dogecoin
    'TRX': 'TRXUSDT',      # Tron
    'LTC': 'LTCUSDT',      # Litecoin
    'BCH': 'BCHUSDT',      # Bitcoin Cash
    'LINK': 'LINKUSDT',    # Chainlink

    # アルトコイン
    'MATIC': 'MATICUSDT',  # Polygon
    'XLM': 'XLMUSDT',      # Stellar
    'ATOM': 'ATOMUSDT',    # Cosmos
    'ETC': 'ETCUSDT',      # Ethereum Classic
    'VET': 'VETUSDT',      # VeChain
    'STX': 'STXUSDT',      # Stacks
    'HBAR': 'HBARUSDT',    # Hedera
    'EOS': 'EOSUSDT',      # EOS
    'NEO': 'NEOUSDT',      # NEO
    'DASH': 'DASHUSDT',    # Dash
    'ZEC': 'ZECUSDT',      # Zcash
    'QTUM': 'QTUMUSDT',    # Qtum
    'ZIL': 'ZILUSDT',      # Zilliqa
    'ONT': 'ONTUSDT',      # Ontology
    'BAT': 'BATUSDT',      # Basic Attention Token
    'IOTA': 'IOTAUSDT',    # IOTA
    'ALGO': 'ALGOUSDT',    # Algorand
    'ENJ': 'ENJUSDT',      # Enjin Coin
}

def download_binance_klines(client, symbol, interval, start_date, end_date, show_progress=True):
    """
    Binance APIから時系列データ（klines）を取得

    Args:
        client: Binanceクライアント
        symbol: ペアシンボル（例: BTCUSDT）
        interval: 時間間隔（例: Client.KLINE_INTERVAL_1HOUR）
        start_date: 開始日（datetime）
        end_date: 終了日（datetime）
        show_progress: 進捗を表示するか

    Returns:
        list: klineデータのリスト
    """
    klines = []
    current_start = start_date
    total_days = (end_date - start_date).days

    # Binance APIは1回のリクエストで最大1000本まで取得可能
    # 期間が長い場合は複数回に分けて取得
    batch_count = 0
    while current_start < end_date:
        try:
            # データ取得
            batch = client.get_historical_klines(
                symbol,
                interval,
                start_str=current_start.strftime('%Y-%m-%d'),
                end_str=min(current_start + timedelta(days=40), end_date).strftime('%Y-%m-%d')
            )

            if not batch:
                break

            klines.extend(batch)
            batch_count += 1

            # 次の開始日を設定
            last_timestamp = batch[-1][0]
            current_start = datetime.fromtimestamp(last_timestamp / 1000) + timedelta(hours=1)

            # 進捗表示
            if show_progress and batch_count % 10 == 0:
                progress = min(100, int((current_start - start_date).days / total_days * 100))
                print(f".", end='', flush=True)

            # レート制限対策
            time.sleep(0.1)

        except Exception as e:
            if show_progress:
                print(f"\n    Error: {e}")
            break

    return klines

def download_crypto_data_binance(start_date='2020-01-01', end_date=None, interval='1h'):
    """
    Binance APIから仮想通貨データをダウンロード

    Args:
        start_date: 開始日（YYYY-MM-DD形式）
        end_date: 終了日（YYYY-MM-DD形式）。Noneの場合は今日まで
        interval: データ間隔（'1h', '4h', '1d'など）

    Returns:
        pd.DataFrame: タイムスタンプと各仮想通貨ペアの終値
    """
    if end_date is None:
        end_date = datetime.now().strftime('%Y-%m-%d')

    start_dt = datetime.strptime(start_date, '%Y-%m-%d')
    end_dt = datetime.strptime(end_date, '%Y-%m-%d')

    print(f"=== Downloading Cryptocurrency Data from Binance ===")
    print(f"Period: {start_date} to {end_date}")
    print(f"Total days: {(end_dt - start_dt).days}")
    print(f"Interval: {interval}")
    print(f"Pairs: {len(CRYPTO_PAIRS)}")
    print()

    # Binanceクライアント作成（APIキー不要、パブリックデータのみ）
    client = Client()

    # インターバル設定
    interval_map = {
        '1m': Client.KLINE_INTERVAL_1MINUTE,
        '5m': Client.KLINE_INTERVAL_5MINUTE,
        '15m': Client.KLINE_INTERVAL_15MINUTE,
        '30m': Client.KLINE_INTERVAL_30MINUTE,
        '1h': Client.KLINE_INTERVAL_1HOUR,
        '2h': Client.KLINE_INTERVAL_2HOUR,
        '4h': Client.KLINE_INTERVAL_4HOUR,
        '1d': Client.KLINE_INTERVAL_1DAY,
    }

    if interval not in interval_map:
        raise ValueError(f"Invalid interval: {interval}. Choose from {list(interval_map.keys())}")

    binance_interval = interval_map[interval]

    # データを格納する辞書
    data_dict = {}
    failed_pairs = []

    total_pairs = len(CRYPTO_PAIRS)
    for idx, (pair_name, binance_symbol) in enumerate(CRYPTO_PAIRS.items(), 1):
        try:
            print(f"[{idx}/{total_pairs}] Downloading {pair_name} ({binance_symbol})...", end=' ', flush=True)

            # klineデータを取得
            klines = download_binance_klines(
                client, binance_symbol, binance_interval, start_dt, end_dt
            )

            if not klines:
                print(f" ⚠ No data")
                failed_pairs.append(pair_name)
                continue

            # DataFrameに変換
            df = pd.DataFrame(klines, columns=[
                'timestamp', 'open', 'high', 'low', 'close', 'volume',
                'close_time', 'quote_asset_volume', 'number_of_trades',
                'taker_buy_base_asset_volume', 'taker_buy_quote_asset_volume', 'ignore'
            ])

            # タイムスタンプを変換
            df['timestamp'] = pd.to_datetime(df['timestamp'], unit='ms')

            # 終値を数値に変換
            df['close'] = df['close'].astype(float)

            # 終値データのみ抽出
            close_series = df.set_index('timestamp')['close']
            data_dict[pair_name] = close_series

            print(f" ✓ {len(df)} records")

        except Exception as e:
            print(f" ✗ Error: {e}")
            failed_pairs.append(pair_name)

    if not data_dict:
        raise ValueError("No data downloaded. Please check your internet connection.")

    # DataFrameに統合
    combined_df = pd.DataFrame(data_dict)

    # 欠損値を前方埋め
    combined_df = combined_df.ffill()

    # タイムスタンプをリセット
    combined_df = combined_df.reset_index()
    combined_df = combined_df.rename(columns={'index': 'timestamp'})

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

    # 出力ディレクトリ作成（Binanceデータ専用）
    os.makedirs('../crypto_data_binance', exist_ok=True)

    # 期間指定: 2020/1/1 ～ 2025/1/1（5年間）
    start_date_str = '2020-01-01'
    end_date_str = '2025-01-01'

    print(f"Target period: {start_date_str} to {end_date_str}")
    print()

    # データダウンロード（1時間足）
    df = download_crypto_data_binance(start_date=start_date_str, end_date=end_date_str, interval='4h')

    # CSVに保存
    output_file = '../crypto_data_binance/crypto_raw_hourly.csv'
    df.to_csv(output_file, index=False)

    print()
    print(f"✓ Data saved to: {output_file}")
    print(f"  Shape: {df.shape}")
    print(f"  Columns: {list(df.columns)}")
    print(f"  Records per day: ~24 (hourly)")
    print(f"  Total days: ~{len(df) / 24:.0f}")
    print(f"  Total hours: {len(df)}")

    # サンプルデータを表示
    print()
    print("=== Sample Data (first 5 rows) ===")
    print(df.head())

    print()
    print("=== Sample Data (last 5 rows) ===")
    print(df.tail())

if __name__ == '__main__':
    main()
