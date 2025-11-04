#!/usr/bin/env python3
"""
X-T散布図可視化スクリプト

発見されたルールの上位3つについて、それぞれ個別の散布図を作成します。
- ベースレイヤー: 全データポイント（灰色、透明）
- オーバーレイ: 各ルールのマッチポイント（色付き）

各ルールごとに1つのPNG画像を生成します。
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import argparse
import os
import sys
from pathlib import Path


def calculate_rule_score(row):
    """
    ルールスコアを計算

    スコア = support_rate / (σ_t+1 * σ_t+2 * σ_t+3)

    Args:
        row: DataFrameの1行（ルール情報）

    Returns:
        float: ルールスコア（ゼロ除算の場合は0.0）
    """
    sigma_t1 = row['X(t+1)_sigma']
    sigma_t2 = row['X(t+2)_sigma']
    sigma_t3 = row['X(t+3)_sigma']

    # ゼロ除算チェック
    sigma_product = sigma_t1 * sigma_t2 * sigma_t3

    if sigma_product == 0 or sigma_product < 1e-10:
        return 0.0

    return row['support_rate'] / sigma_product


def load_rule_pool(symbol, base_dir="../../crypto_data/output"):
    """
    ルールプールファイルを読み込み、スコアを計算

    Args:
        symbol: 銘柄コード（例: AAVE）
        base_dir: 出力ディレクトリのベースパス

    Returns:
        DataFrame: スコア付きルール一覧（スコア降順）
    """
    pool_file = f"{base_dir}/{symbol}/pool/zrp01a.txt"

    if not os.path.exists(pool_file):
        raise FileNotFoundError(f"Pool file not found: {pool_file}")

    # タブ区切りで読み込み
    df = pd.read_csv(pool_file, sep='\t')

    # ルールスコアを計算
    df['rule_score'] = df.apply(calculate_rule_score, axis=1)

    # スコア降順でソート
    df = df.sort_values('rule_score', ascending=False).reset_index(drop=True)

    # ルールIDを追加（1-indexed）
    df['rule_id'] = range(1, len(df) + 1)

    return df


def load_all_data(symbol, base_dir="../../crypto_data/gnminer_individual"):
    """
    元データ全体を読み込み

    Args:
        symbol: 銘柄コード
        base_dir: データディレクトリ

    Returns:
        tuple: (X値のSeries, タイムスタンプのSeries)
    """
    data_file = f"{base_dir}/{symbol}.txt"

    if not os.path.exists(data_file):
        raise FileNotFoundError(f"Data file not found: {data_file}")

    df = pd.read_csv(data_file)

    # X列とT列を抽出
    x_values = df['X']
    timestamps = pd.to_datetime(df['T'])

    return x_values, timestamps


def load_rule_matches(rule_id, symbol, base_dir="../../crypto_data/output"):
    """
    特定ルールのマッチデータを読み込み

    Args:
        rule_id: ルールID（1-indexed）
        symbol: 銘柄コード
        base_dir: 出力ディレクトリ

    Returns:
        DataFrame: マッチデータ
    """
    verification_file = f"{base_dir}/{symbol}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        raise FileNotFoundError(f"Verification file not found: {verification_file}")

    df = pd.read_csv(verification_file)

    return df


def plot_single_rule_scatter(all_x, all_t, rule_x, rule_t, rule_info,
                              output_path, symbol):
    """
    単一ルールの散布図を作成

    Args:
        all_x: 全データのX値
        all_t: 全データのタイムスタンプ
        rule_x: ルールマッチ時のX値
        rule_t: ルールマッチ時のタイムスタンプ
        rule_info: ルール情報（Series）
        output_path: 出力ファイルパス
        symbol: 銘柄コード
    """
    fig, ax = plt.subplots(figsize=(14, 8))

    # 1. 全データポイント（ベースレイヤー）
    ax.scatter(all_t, all_x,
               c='lightgray', alpha=0.3, s=20,
               label=f'All Data (n={len(all_x)})',
               zorder=1)

    # 2. ルールマッチポイント（オーバーレイ）
    rule_id = rule_info['rule_id']
    rule_score = rule_info['rule_score']
    support_count = rule_info['support_count']
    support_rate = rule_info['support_rate']
    num_attrs = rule_info['NumAttr']

    # 平均σを計算
    avg_sigma = (rule_info['X(t+1)_sigma'] +
                 rule_info['X(t+2)_sigma'] +
                 rule_info['X(t+3)_sigma']) / 3.0

    ax.scatter(rule_t, rule_x,
               c='red', alpha=0.9, s=150, marker='o',
               edgecolors='darkred', linewidths=2,
               label=f'Rule {rule_id} Matches (n={support_count})',
               zorder=2)

    # タイトルと軸ラベル
    ax.set_xlabel('Time', fontsize=14, fontweight='bold')
    ax.set_ylabel('X (% Change)', fontsize=14, fontweight='bold')

    title = f'{symbol}: X-T Scatter Plot - Rule {rule_id}\n'
    title += f'Score={rule_score:.4f} | Support={support_rate:.4f} | '
    title += f'Attrs={num_attrs} | Avg σ={avg_sigma:.2f}%'
    ax.set_title(title, fontsize=15, fontweight='bold', pad=20)

    # 凡例
    ax.legend(loc='upper left', fontsize=11, framealpha=0.9)

    # グリッド
    ax.grid(True, alpha=0.3, linestyle='--')

    # 日付フォーマット
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m'))
    ax.xaxis.set_major_locator(mdates.MonthLocator(interval=3))
    plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, ha='right')

    # Y軸の範囲を調整（外れ値対策）
    y_mean = all_x.mean()
    y_std = all_x.std()
    ax.set_ylim(y_mean - 4*y_std, y_mean + 4*y_std)

    # レイアウト調整
    plt.tight_layout()

    # 保存
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")

    plt.close()


def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Generate X-T scatter plots for top 3 rules'
    )
    parser.add_argument('--symbol', '-s', type=str, required=True,
                        help='Symbol code (e.g., AAVE, BTC, ETH)')
    parser.add_argument('--top-n', '-n', type=int, default=3,
                        help='Number of top rules to plot (default: 3)')
    parser.add_argument('--output-dir', '-o', type=str,
                        default='output',
                        help='Output directory (default: output)')

    args = parser.parse_args()

    symbol = args.symbol
    top_n = args.top_n
    output_dir = args.output_dir

    print("=" * 60)
    print(f"  X-T Scatter Plot Generator")
    print("=" * 60)
    print(f"Symbol: {symbol}")
    print(f"Top N Rules: {top_n}")
    print(f"Output Directory: {output_dir}")
    print("=" * 60)

    # 出力ディレクトリを作成
    os.makedirs(output_dir, exist_ok=True)

    try:
        # 1. ルールプールを読み込み
        print(f"\n[1/4] Loading rule pool...")
        rules_df = load_rule_pool(symbol)
        print(f"  ✓ Loaded {len(rules_df)} rules")

        # 上位N件を選択
        top_rules = rules_df.head(top_n)
        print(f"  ✓ Selected top {top_n} rules")

        # スコア一覧を表示
        print("\n  Top Rules:")
        for idx, row in top_rules.iterrows():
            print(f"    Rule {row['rule_id']:3d}: "
                  f"Score={row['rule_score']:8.4f} | "
                  f"Support={row['support_count']:3d} | "
                  f"Rate={row['support_rate']:.4f}")

        # 2. 全データを読み込み
        print(f"\n[2/4] Loading all data...")
        all_x, all_t = load_all_data(symbol)
        print(f"  ✓ Loaded {len(all_x)} data points")

        # 3. 各ルールについて散布図を作成
        print(f"\n[3/4] Generating scatter plots...")

        for idx, rule_info in top_rules.iterrows():
            rule_id = rule_info['rule_id']

            # ルールマッチデータを読み込み
            rule_matches = load_rule_matches(rule_id, symbol)

            # RowIndexから元データのX, Tを取得
            row_indices = rule_matches['RowIndex'].values

            # インデックス範囲チェック
            valid_indices = row_indices[row_indices < len(all_x)]

            if len(valid_indices) == 0:
                print(f"  ⚠ Rule {rule_id}: No valid matches (skipping)")
                continue

            rule_x = all_x.iloc[valid_indices]
            rule_t = all_t.iloc[valid_indices]

            # 散布図を作成
            output_path = f"{output_dir}/{symbol}_rule_{rule_id:03d}_scatter.png"
            plot_single_rule_scatter(all_x, all_t, rule_x, rule_t,
                                     rule_info, output_path, symbol)

        # 4. 完了
        print(f"\n[4/4] Complete!")
        print(f"  ✓ Generated {top_n} scatter plots")
        print(f"  ✓ Output directory: {output_dir}")

    except FileNotFoundError as e:
        print(f"\n❌ ERROR: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
