#!/usr/bin/env python3
"""
Binanceで2020年1月1日から継続的にデータがある通貨ペアを調査
"""

from binance.client import Client
from datetime import datetime, timedelta
import time

# 調査対象の候補通貨（時価総額上位 + 流動性高い通貨）
CANDIDATE_PAIRS = [
    # メジャー通貨
    'BTCUSDT', 'ETHUSDT', 'BNBUSDT', 'XRPUSDT', 'ADAUSDT',
    'DOGEUSDT', 'SOLUSDT', 'TRXUSDT', 'MATICUSDT', 'DOTUSDT',
    'LTCUSDT', 'AVAXUSDT', 'LINKUSDT', 'BCHUSDT', 'XLMUSDT',
    'ATOMUSDT', 'UNIUSDT', 'ETCUSDT', 'FILUSDT', 'VETUSDT',

    # アルトコイン
    'SHIBUSDT', 'TONUSDT', 'NEARUSDT', 'ICPUSDT', 'APTUSDT',
    'STXUSDT', 'ARBUSDT', 'OPUSDT', 'HBARUSDT', 'TAOUSDT',
    'EOSUSDT', 'NEOUSDT', 'DASHUSDT', 'ZECUSDT', 'QTUMUSDT',
    'ZILUSDT', 'ONTUSDT', 'BATUSDT', 'IOTAUSDT', 'WAVESUSDT',
    'COMPUSDT', 'AAVEUSDT', 'MKRUSDT', 'SUSHIUSDT', 'YFIUSDT',
    'ALGOUSDT', 'ENJUSDT', 'MANAUSDT', 'SANDUSDT', 'AXSUSDT',
]

def check_pair_availability(client, symbol, start_date='2020-01-01', min_records=10000):
    """
    ペアが指定期間から継続的にデータを持つか確認

    Args:
        client: Binanceクライアント
        symbol: ペアシンボル
        start_date: 開始日
        min_records: 最小レコード数（4時間足で約5年分は約10,950）

    Returns:
        tuple: (利用可能か, レコード数, 最初の日付, 最後の日付)
    """
    try:
        # 最初の10日分を取得してみる
        klines = client.get_historical_klines(
            symbol,
            Client.KLINE_INTERVAL_4HOUR,
            start_str=start_date,
            end_str='2020-01-15'
        )

        if not klines or len(klines) == 0:
            return False, 0, None, None

        # 全期間のデータ数を確認
        all_klines = client.get_historical_klines(
            symbol,
            Client.KLINE_INTERVAL_4HOUR,
            start_str=start_date,
            end_str='2025-01-01'
        )

        if not all_klines:
            return False, 0, None, None

        first_date = datetime.fromtimestamp(all_klines[0][0] / 1000)
        last_date = datetime.fromtimestamp(all_klines[-1][0] / 1000)
        record_count = len(all_klines)

        # 2020年1月15日より前にデータがあるかチェック
        if first_date > datetime(2020, 1, 15):
            return False, record_count, first_date, last_date

        # 最小レコード数チェック
        if record_count < min_records:
            return False, record_count, first_date, last_date

        return True, record_count, first_date, last_date

    except Exception as e:
        return False, 0, None, None

def main():
    """メイン処理"""

    print("=" * 80)
    print("  Binance Pairs Availability Check (2020-01-01 ~ 2025-01-01, 4h interval)")
    print("=" * 80)
    print(f"Checking {len(CANDIDATE_PAIRS)} pairs...")
    print()

    # Binanceクライアント作成
    client = Client()

    available_pairs = []
    unavailable_pairs = []

    for idx, symbol in enumerate(CANDIDATE_PAIRS, 1):
        print(f"[{idx}/{len(CANDIDATE_PAIRS)}] Checking {symbol}...", end=' ', flush=True)

        is_available, count, first_date, last_date = check_pair_availability(client, symbol)

        if is_available:
            print(f"✓ Available ({count} records, from {first_date.strftime('%Y-%m-%d')})")
            available_pairs.append({
                'symbol': symbol,
                'records': count,
                'first_date': first_date,
                'last_date': last_date
            })
        else:
            if count > 0:
                print(f"✗ Insufficient (only {count} records, from {first_date.strftime('%Y-%m-%d') if first_date else 'N/A'})")
            else:
                print(f"✗ No data")
            unavailable_pairs.append(symbol)

        # レート制限対策
        time.sleep(0.2)

    print()
    print("=" * 80)
    print("  Results")
    print("=" * 80)
    print(f"Available pairs: {len(available_pairs)}")
    print(f"Unavailable pairs: {len(unavailable_pairs)}")
    print()

    if available_pairs:
        print("=" * 80)
        print("  Available Pairs (Ready to use)")
        print("=" * 80)
        print()
        print("CRYPTO_PAIRS = {")
        for pair in available_pairs:
            symbol = pair['symbol']
            name = symbol.replace('USDT', '')
            records = pair['records']
            first = pair['first_date'].strftime('%Y-%m-%d')
            print(f"    '{name}': '{symbol}',  # {records} records from {first}")
        print("}")
        print()

    if unavailable_pairs:
        print("=" * 80)
        print("  Unavailable Pairs (Cannot use for 2020-2025 period)")
        print("=" * 80)
        for symbol in unavailable_pairs:
            print(f"  - {symbol}")

if __name__ == '__main__':
    main()
