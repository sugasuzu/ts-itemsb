#!/usr/bin/env python3
"""
ベースライン結果の X(t+1) vs X(t+2) 散布図作成

目的:
- output_baseline_20251110の結果を可視化
- 各通貨でverificationデータから象限集中度を計算
- 上位3ルールを選定
- 散布図を作成（背景データ + ルールマッチ）
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
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
        return df
    except Exception as e:
        print(f"Error loading verification {symbol} rule {rule_id}: {e}", file=sys.stderr)
        return None

def load_background_data(symbol):
    """背景データ（全データ）を読み込む"""
    data_file = BASE_DIR / f"{symbol}.txt"

    if not data_file.exists():
        return None, None

    try:
        df = pd.read_csv(data_file)
        if 'X' not in df.columns:
            return None, None

        x_values = df['X'].values
        x_t1_all = x_values[1:]  # t+1
        x_t2_all = x_values[2:]  # t+2
        min_len = min(len(x_t1_all), len(x_t2_all))

        return x_t1_all[:min_len], x_t2_all[:min_len]
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

def analyze_rules_for_concentration(symbol):
    """通貨ごとに全ルールの集中度を計算"""
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
        # 推定されるルールID（行番号 + 1）
        rule_id = idx + 1

        # verificationデータを読み込む
        df_ver = load_verification_data(symbol, rule_id)

        if df_ver is None or len(df_ver) == 0:
            continue

        # X(t+1), X(t+2)を抽出
        if 'X(t+1)' not in df_ver.columns or 'X(t+2)' not in df_ver.columns:
            continue

        x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values
        x_t2 = pd.to_numeric(df_ver['X(t+2)'], errors='coerce').values

        # NaNを除去
        valid_mask = ~(np.isnan(x_t1) | np.isnan(x_t2))
        x_t1 = x_t1[valid_mask]
        x_t2 = x_t2[valid_mask]

        if len(x_t1) == 0 or len(x_t2) == 0:
            continue

        # 集中度を計算
        concentration, max_quadrant, quadrants = calculate_quadrant_concentration(x_t1, x_t2)

        # 統計情報を取得
        mean_t1 = row.get('X(t+1)_mean', 0.0)
        mean_t2 = row.get('X(t+2)_mean', 0.0)
        sigma_t1 = row.get('X(t+1)_sigma', 0.0)
        sigma_t2 = row.get('X(t+2)_sigma', 0.0)
        support_count = row.get('support_count', 0)

        results.append({
            'rule_id': rule_id,
            'concentration': concentration,
            'max_quadrant': max_quadrant,
            'q1': quadrants[0],
            'q2': quadrants[1],
            'q3': quadrants[2],
            'q4': quadrants[3],
            'total_matches': sum(quadrants),
            'mean_t1': mean_t1,
            'mean_t2': mean_t2,
            'sigma_t1': sigma_t1,
            'sigma_t2': sigma_t2,
            'support_count': support_count,
        })

    return results

def plot_rule_scatter(symbol, rule_info, x_t1_bg, x_t2_bg, output_path):
    """散布図を作成"""
    rule_id = rule_info['rule_id']

    # verificationデータを読み込む
    df_ver = load_verification_data(symbol, rule_id)
    if df_ver is None:
        return

    if 'X(t+1)' not in df_ver.columns or 'X(t+2)' not in df_ver.columns:
        return

    x_t1 = pd.to_numeric(df_ver['X(t+1)'], errors='coerce').values
    x_t2 = pd.to_numeric(df_ver['X(t+2)'], errors='coerce').values

    # NaNを除去
    valid_mask = ~(np.isnan(x_t1) | np.isnan(x_t2))
    x_t1 = x_t1[valid_mask]
    x_t2 = x_t2[valid_mask]

    if len(x_t1) == 0 or len(x_t2) == 0:
        return

    # プロット作成
    fig, ax = plt.subplots(figsize=(10, 9))

    zoom_range = 3  # ±3%

    # 背景データ
    if x_t1_bg is not None and x_t2_bg is not None:
        mask = (np.abs(x_t1_bg) <= zoom_range) & (np.abs(x_t2_bg) <= zoom_range)
        ax.scatter(x_t1_bg[mask], x_t2_bg[mask],
                   c='lightgray', alpha=0.2, s=25,
                   label=f'All Data (n={np.sum(mask):,})',
                   zorder=1, edgecolors='none')

    # ルールマッチ（赤色統一）
    ax.scatter(x_t1, x_t2,
               c='red', alpha=0.85, s=140,
               edgecolors='darkred', linewidths=1.8,
               label=f'Rule Matches (n={len(x_t1)})',
               zorder=4)

    # 軸線
    ax.axhline(y=0, color='black', linewidth=0.8, linestyle='-', alpha=0.3, zorder=2)
    ax.axvline(x=0, color='black', linewidth=0.8, linestyle='-', alpha=0.3, zorder=2)

    # タイトル
    concentration = rule_info['concentration'] * 100
    max_q = rule_info['max_quadrant']
    q1, q2, q3, q4 = rule_info['q1'], rule_info['q2'], rule_info['q3'], rule_info['q4']

    title = f"{symbol}: Rule {rule_id} - Concentration: {concentration:.1f}% in {max_q}\n"
    title += f"Q1={q1}, Q2={q2}, Q3={q3}, Q4={q4}\n"
    title += f"μ(t+1)={rule_info['mean_t1']:.3f}%, σ(t+1)={rule_info['sigma_t1']:.3f}%, "
    title += f"μ(t+2)={rule_info['mean_t2']:.3f}%, σ(t+2)={rule_info['sigma_t2']:.3f}%\n"
    title += f"Support: {rule_info['support_count']} matches"

    ax.set_title(title, fontsize=10, fontweight='bold', pad=15)

    # 軸ラベル
    ax.set_xlabel('X(t+1) [%]', fontsize=11, fontweight='bold')
    ax.set_ylabel('X(t+2) [%]', fontsize=11, fontweight='bold')

    # 軸範囲
    ax.set_xlim(-zoom_range, zoom_range)
    ax.set_ylim(-zoom_range, zoom_range)

    # グリッド
    ax.grid(True, alpha=0.2, linestyle='--', linewidth=0.5, zorder=0)

    # 凡例
    legend = ax.legend(loc='lower left', fontsize=10, framealpha=0.95,
                       edgecolor='black', fancybox=True, shadow=True)
    legend.set_zorder(10)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()

    print(f"  Created: {output_path.name}")

def main():
    print("=" * 80)
    print("ベースライン: 各通貨の象限集中度上位3ルールの散布図作成")
    print("=" * 80)
    print()

    # 出力ディレクトリ
    output_base = OUTPUT_DIR / "top3_concentration_scatter"
    output_base.mkdir(exist_ok=True)

    all_summary = []

    for symbol in SYMBOLS:
        print(f"\n{symbol}:")
        print("-" * 40)

        # ルールの集中度を計算
        results = analyze_rules_for_concentration(symbol)

        if not results:
            print(f"  No rules found")
            continue

        # 集中度でソート
        results_sorted = sorted(results, key=lambda x: x['concentration'], reverse=True)

        # 上位3つを選択
        top3 = results_sorted[:3]

        # 背景データを読み込む
        x_t1_bg, x_t2_bg = load_background_data(symbol)

        # 散布図を作成
        for rank, rule_info in enumerate(top3, 1):
            print(f"  Rank {rank}: Rule {rule_info['rule_id']:04d} - "
                  f"Concentration={rule_info['concentration']*100:.1f}% in {rule_info['max_quadrant']}")

            output_path = output_base / f"{symbol}_rank{rank}_rule{rule_info['rule_id']:04d}.png"
            plot_rule_scatter(symbol, rule_info, x_t1_bg, x_t2_bg, output_path)

            # サマリーに追加
            all_summary.append({
                'symbol': symbol,
                'rank': rank,
                'rule_id': rule_info['rule_id'],
                'concentration': rule_info['concentration'],
                'max_quadrant': rule_info['max_quadrant'],
                'total_matches': rule_info['total_matches'],
                'mean_t1': rule_info['mean_t1'],
                'mean_t2': rule_info['mean_t2'],
            })

    # サマリーを保存
    df_summary = pd.DataFrame(all_summary)
    summary_file = output_base / "top3_summary.csv"
    df_summary.to_csv(summary_file, index=False)

    print("\n" + "=" * 80)
    print(f"完了: {output_base}")
    print(f"サマリー: {summary_file}")
    print("=" * 80)

if __name__ == "__main__":
    main()
