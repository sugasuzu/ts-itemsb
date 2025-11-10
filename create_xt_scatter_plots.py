#!/usr/bin/env python3
"""
X(t+1) vs Time 散布図作成スクリプト

目的:
- ベースライン結果の時系列依存性を可視化
- X(t+1)とTimestampの関係を散布図で表示
- 各通貨の上位3ルール（象限集中度）について作成
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from pathlib import Path
from datetime import datetime
import sys

# 基準ディレクトリ
BASE_DIR = Path("/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/1-deta-enginnering/crypto_data_hourly")
OUTPUT_DIR = BASE_DIR / "output_baseline_20251110"

# 対象通貨
SYMBOLS = ["AAVE", "ADA", "ALGO", "ATOM", "AVAX", "BNB", "BTC", "DOGE", "DOT", "ETC", "ETH"]

def load_verification_data(symbol, rule_id):
    """verificationファイルを読み込む"""
    # ファイル名形式: rule_001.csv (3桁ゼロパディング)
    verification_file = OUTPUT_DIR / symbol / "verification" / f"rule_{rule_id:03d}.csv"

    if not verification_file.exists():
        return None

    try:
        df = pd.read_csv(verification_file)

        # Timestampを日時型に変換
        if 'Timestamp' in df.columns:
            df['Timestamp'] = pd.to_datetime(df['Timestamp'])

        return df
    except Exception as e:
        print(f"Error loading verification {symbol} rule {rule_id}: {e}", file=sys.stderr)
        return None

def load_background_data(symbol):
    """背景データ（全データ）を読み込む"""
    data_file = BASE_DIR / f"{symbol}.txt"

    if not data_file.exists():
        return None

    try:
        df = pd.read_csv(data_file)

        # T列を日時型に変換
        if 'T' in df.columns:
            df['T'] = pd.to_datetime(df['T'])

        if 'X' not in df.columns:
            return None

        x_values = df['X'].values
        t_values = df['T'].values

        # t+1の値を作成
        x_t1_all = x_values[1:]
        t_all = t_values[:-1]  # t+1に対応する時刻はtの値

        return t_all, x_t1_all
    except Exception as e:
        print(f"Error loading background data for {symbol}: {e}", file=sys.stderr)
        return None, None

def calculate_quadrant_concentration(x_t1, x_t2):
    """象限集中度を計算"""
    if len(x_t1) == 0:
        return 0.0, "none", [0, 0, 0, 0]

    # 4象限に分類
    q1 = np.sum((x_t1 > 0) & (x_t2 > 0))  # ++
    q2 = np.sum((x_t1 < 0) & (x_t2 > 0))  # -+
    q3 = np.sum((x_t1 < 0) & (x_t2 < 0))  # --
    q4 = np.sum((x_t1 > 0) & (x_t2 < 0))  # +-

    quadrants = [q1, q2, q3, q4]
    max_count = max(quadrants)
    total = sum(quadrants)

    if total == 0:
        return 0.0, "none", quadrants

    concentration = max_count / total

    # 最大象限を特定
    quadrant_names = ["Q1(++)", "Q2(-+)", "Q3(--)", "Q4(+-)"]
    max_quadrant = quadrant_names[quadrants.index(max_count)]

    return concentration, max_quadrant, quadrants

def analyze_rules_for_xt1_quality(symbol):
    """通貨ごとに全ルールのX(t+1)品質を計算（X(t+2)を使わない）"""
    pool_file = OUTPUT_DIR / symbol / "pool" / "zrp01a.txt"

    if not pool_file.exists():
        return []

    try:
        df_pool = pd.read_csv(pool_file, sep='\t')
    except Exception as e:
        print(f"Error loading pool for {symbol}: {e}", file=sys.stderr)
        return []

    results = []

    for idx, row in df_pool.iterrows():
        # ルールIDは行番号 + 1
        rule_id = idx + 1

        # verificationデータを読み込む
        df_ver = load_verification_data(symbol, rule_id)

        if df_ver is None or len(df_ver) == 0:
            continue

        # X(t+1)のみを抽出
        if 'X(t+1)' not in df_ver.columns:
            continue

        x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values

        # NaNを除去
        valid_mask = ~np.isnan(x_t1)
        x_t1 = x_t1[valid_mask]

        if len(x_t1) == 0:
            continue

        # 統計情報を取得
        mean_t1 = row.get('X(t+1)_mean', 0.0)
        sigma_t1 = row.get('X(t+1)_sigma', 0.0)

        # 実際のマッチ数（verificationファイルの行数）
        actual_matches = len(x_t1)

        # X(t+1)のみで品質スコアを計算
        # 品質 = |mean| / sigma の比率（高いほど良い）
        if sigma_t1 > 0:
            quality_score = abs(mean_t1) / sigma_t1
        else:
            quality_score = 0.0

        results.append({
            'rule_id': rule_id,
            'quality_score': quality_score,
            'mean_t1': mean_t1,
            'sigma_t1': sigma_t1,
            'matches': actual_matches,
        })

    return results

def plot_rule_xt_scatter(symbol, rule_info, t_bg, x_t1_bg, output_path):
    """X(t+1) vs Time 散布図を作成"""
    rule_id = rule_info['rule_id']

    # verificationデータを読み込む
    df_ver = load_verification_data(symbol, rule_id)
    if df_ver is None:
        return

    if 'X(t+1)' not in df_ver.columns or 'Timestamp' not in df_ver.columns:
        return

    x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values
    timestamps = df_ver['Timestamp'].values

    # NaNを除去
    valid_mask = ~np.isnan(x_t1)
    x_t1 = x_t1[valid_mask]
    timestamps = timestamps[valid_mask]

    if len(x_t1) == 0 or len(timestamps) == 0:
        return

    # プロット作成
    fig, ax = plt.subplots(figsize=(14, 8))

    # 背景データ（全データ）
    if t_bg is not None and x_t1_bg is not None:
        ax.scatter(t_bg, x_t1_bg,
                   c='lightgray', alpha=0.15, s=15,
                   label=f'All Data (n={len(x_t1_bg):,})',
                   zorder=1, edgecolors='none')

    # ルールマッチ（赤色）
    ax.scatter(timestamps, x_t1,
               c='red', alpha=0.85, s=140,
               edgecolors='darkred', linewidths=1.8,
               label=f'Rule Matches (n={len(x_t1)})',
               zorder=4)

    # ゼロライン
    ax.axhline(y=0, color='black', linewidth=0.8, linestyle='-', alpha=0.3, zorder=2)

    # タイトル
    quality_score = rule_info['quality_score']

    title = f"{symbol}: Rule {rule_id} - X(t+1) vs Time\n"
    title += f"μ(t+1)={rule_info['mean_t1']:.3f}%, σ(t+1)={rule_info['sigma_t1']:.3f}%, "
    title += f"Quality Score={quality_score:.3f}\n"
    title += f"Matches: {rule_info['matches']}"

    ax.set_title(title, fontsize=10, fontweight='bold', pad=15)

    # 軸ラベル
    ax.set_xlabel('Time', fontsize=11, fontweight='bold')
    ax.set_ylabel('X(t+1) [%]', fontsize=11, fontweight='bold')

    # Y軸範囲
    ax.set_ylim(-3, 3)

    # 日付フォーマット
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
    ax.xaxis.set_major_locator(mdates.MonthLocator(interval=2))
    plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, ha='right')

    # グリッド
    ax.grid(True, alpha=0.2, linestyle='--', linewidth=0.5, zorder=0)

    # 凡例
    legend = ax.legend(loc='upper left', fontsize=10, framealpha=0.95,
                       edgecolor='black', fancybox=True, shadow=True)
    legend.set_zorder(10)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"  Created: {output_path.name}")

def main():
    print("=" * 80)
    print("各通貨のX(t+1)品質上位3ルールの X(t+1) vs Time 散布図作成")
    print("=" * 80)
    print()

    # 出力ディレクトリ
    output_base = OUTPUT_DIR / "top3_xt_scatter"
    output_base.mkdir(parents=True, exist_ok=True)

    all_summary = []

    for symbol in SYMBOLS:
        print(f"\n{symbol}:")
        print("-" * 40)

        # ルールのX(t+1)品質を計算
        results = analyze_rules_for_xt1_quality(symbol)

        if not results:
            print(f"  No rules found")
            continue

        # 品質スコアでソート
        results_sorted = sorted(results, key=lambda x: x['quality_score'], reverse=True)

        # 上位3つを選択
        top3 = results_sorted[:3]

        # 背景データを読み込む
        t_bg, x_t1_bg = load_background_data(symbol)

        # 散布図を作成
        for rank, rule_info in enumerate(top3, 1):
            print(f"  Rank {rank}: Rule {rule_info['rule_id']:04d} - "
                  f"Quality Score={rule_info['quality_score']:.3f} "
                  f"(μ={rule_info['mean_t1']:.3f}%, σ={rule_info['sigma_t1']:.3f}%)")

            output_path = output_base / f"{symbol}_rank{rank}_rule{rule_info['rule_id']:04d}_xt.png"
            plot_rule_xt_scatter(symbol, rule_info, t_bg, x_t1_bg, output_path)

            # サマリーに追加
            all_summary.append({
                'symbol': symbol,
                'rank': rank,
                'rule_id': rule_info['rule_id'],
                'quality_score': rule_info['quality_score'],
                'mean_t1': rule_info['mean_t1'],
                'sigma_t1': rule_info['sigma_t1'],
                'matches': rule_info['matches'],
            })

    # サマリーを保存
    df_summary = pd.DataFrame(all_summary)
    summary_file = output_base / "top3_xt_summary.csv"
    df_summary.to_csv(summary_file, index=False)

    print("\n" + "=" * 80)
    print(f"完了: {output_base}")
    print(f"サマリー: {summary_file}")
    print("=" * 80)

if __name__ == "__main__":
    main()
