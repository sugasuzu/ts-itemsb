#!/usr/bin/env python3
"""
個別ルールX(t+1) vs X(t+2)散布図スクリプト

特定のルールがマッチした時点のX(t+1)とX(t+2)の関係を可視化します。
- 4象限に分類（両方正、両方負、t+1正/t+2負、t+1負/t+2正）
- 各マッチポイントを色分けして表示
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import argparse
import os
from pathlib import Path


def load_rule_info(rule_id, symbol, base_dir=None):
    """
    ルール情報を読み込み

    Args:
        rule_id: ルールID（1-indexed）
        symbol: 銘柄コード
        base_dir: ベースディレクトリ

    Returns:
        Series: ルール情報
    """
    if base_dir is None:
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../1-deta-enginnering/crypto_data_hourly/output"
        base_dir = base_dir.resolve()

    pool_file = f"{base_dir}/{symbol}/pool/zrp01a.txt"
    df = pd.read_csv(pool_file, sep='\t')

    # rule_idは1-indexed、DataFrameは0-indexed
    rule_info = df.iloc[rule_id - 1]

    return rule_info


def load_verification_data(rule_id, symbol, base_dir=None):
    """
    検証データを読み込み

    Args:
        rule_id: ルールID（1-indexed）
        symbol: 銘柄コード
        base_dir: ベースディレクトリ

    Returns:
        DataFrame: 検証データ
    """
    if base_dir is None:
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../1-deta-enginnering/crypto_data_hourly/output"
        base_dir = base_dir.resolve()

    verification_file = f"{base_dir}/{symbol}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        raise FileNotFoundError(f"Verification file not found: {verification_file}")

    df = pd.read_csv(verification_file)

    return df


def plot_2d_future_scatter(rule_info, verification_df, output_path, symbol, rule_id):
    """
    X(t+1) vs X(t+2)の2次元散布図を作成

    Args:
        rule_info: ルール情報
        verification_df: 検証データ
        output_path: 出力パス
        symbol: 銘柄コード
        rule_id: ルールID
    """
    fig, ax = plt.subplots(figsize=(12, 12))

    # データを4象限に分類
    q1_mask = (verification_df['X(t+1)'] > 0) & (verification_df['X(t+2)'] > 0)  # 両方正
    q2_mask = (verification_df['X(t+1)'] < 0) & (verification_df['X(t+2)'] > 0)  # t+1負、t+2正
    q3_mask = (verification_df['X(t+1)'] < 0) & (verification_df['X(t+2)'] < 0)  # 両方負
    q4_mask = (verification_df['X(t+1)'] > 0) & (verification_df['X(t+2)'] < 0)  # t+1正、t+2負

    q1_df = verification_df[q1_mask]
    q2_df = verification_df[q2_mask]
    q3_df = verification_df[q3_mask]
    q4_df = verification_df[q4_mask]

    # 各象限をプロット
    ax.scatter(q1_df['X(t+1)'], q1_df['X(t+2)'],
               color='green', alpha=0.7, s=100, edgecolors='darkgreen', linewidths=1.5,
               label=f'Q1: Both Positive (n={len(q1_df)})', marker='o')

    ax.scatter(q2_df['X(t+1)'], q2_df['X(t+2)'],
               color='orange', alpha=0.7, s=100, edgecolors='darkorange', linewidths=1.5,
               label=f'Q2: t+1 Neg, t+2 Pos (n={len(q2_df)})', marker='s')

    ax.scatter(q3_df['X(t+1)'], q3_df['X(t+2)'],
               color='red', alpha=0.7, s=100, edgecolors='darkred', linewidths=1.5,
               label=f'Q3: Both Negative (n={len(q3_df)})', marker='^')

    ax.scatter(q4_df['X(t+1)'], q4_df['X(t+2)'],
               color='blue', alpha=0.7, s=100, edgecolors='darkblue', linewidths=1.5,
               label=f'Q4: t+1 Pos, t+2 Neg (n={len(q4_df)})', marker='D')

    # 軸と基準線
    ax.axhline(0, color='black', linestyle='-', linewidth=1, alpha=0.5)
    ax.axvline(0, color='black', linestyle='-', linewidth=1, alpha=0.5)

    # 平均値の線
    mean_t1 = rule_info['X(t+1)_mean']
    mean_t2 = rule_info['X(t+2)_mean']
    ax.axvline(mean_t1, color='blue', linestyle='--', linewidth=2, alpha=0.7,
               label=f'Mean X(t+1): {mean_t1:.3f}%')
    ax.axhline(mean_t2, color='purple', linestyle='--', linewidth=2, alpha=0.7,
               label=f'Mean X(t+2): {mean_t2:.3f}%')

    # 統計値
    sigma_t1 = rule_info['X(t+1)_sigma']
    sigma_t2 = rule_info['X(t+2)_sigma']
    win_rate_t1 = rule_info['X(t+1)_win_rate']
    win_rate_t2 = rule_info['X(t+2)_win_rate']
    pf_t1 = rule_info['X(t+1)_profit_factor']
    pf_t2 = rule_info['X(t+2)_profit_factor']

    # 軸ラベルとタイトル
    ax.set_xlabel('X(t+1) Change [%]', fontsize=13, fontweight='bold')
    ax.set_ylabel('X(t+2) Change [%]', fontsize=13, fontweight='bold')
    ax.set_title(f'X(t+1) vs X(t+2) Scatter Plot: Rule {rule_id}\n'
                 f'Support: {int(rule_info["support_count"])} matches | '
                 f'X(t+1) Win Rate: {win_rate_t1*100:.1f}% | '
                 f'X(t+2) Win Rate: {win_rate_t2*100:.1f}%',
                 fontsize=14, fontweight='bold')

    # 凡例
    ax.legend(loc='upper left', fontsize=10, framealpha=0.95)
    ax.grid(True, alpha=0.3)

    # 軸の範囲を適切に設定
    all_x = verification_df['X(t+1)'].values
    all_y = verification_df['X(t+2)'].values
    x_range = max(abs(all_x.min()), abs(all_x.max())) * 1.1
    y_range = max(abs(all_y.min()), abs(all_y.max())) * 1.1
    max_range = max(x_range, y_range)
    ax.set_xlim(-max_range, max_range)
    ax.set_ylim(-max_range, max_range)

    # 等比線（45度線）を追加
    lims = [-max_range, max_range]
    ax.plot(lims, lims, 'k--', alpha=0.3, linewidth=1, label='X(t+1) = X(t+2)')

    # ルール情報をテキストボックスで表示
    attrs = []
    for i in range(1, 9):
        attr_col = f'Attr{i}'
        if attr_col in rule_info.index and rule_info[attr_col] and str(rule_info[attr_col]) != '0':
            attrs.append(str(rule_info[attr_col]))

    rule_text = 'Rule: ' + ' ∧ '.join(attrs[:5])
    if len(attrs) > 5:
        rule_text += '\n      ' + ' ∧ '.join(attrs[5:])

    rule_text += f'\n\nX(t+1) Statistics:'
    rule_text += f'\n  Mean: {mean_t1:.3f}% ± {sigma_t1:.3f}%'
    rule_text += f'\n  Win Rate: {win_rate_t1*100:.1f}%'
    rule_text += f'\n  Profit Factor: {pf_t1:.2f}'

    rule_text += f'\n\nX(t+2) Statistics:'
    rule_text += f'\n  Mean: {mean_t2:.3f}% ± {sigma_t2:.3f}%'
    rule_text += f'\n  Win Rate: {win_rate_t2*100:.1f}%'
    rule_text += f'\n  Profit Factor: {pf_t2:.2f}'

    rule_text += f'\n\nQuadrant Distribution:'
    rule_text += f'\n  Q1 (++): {len(q1_df)} ({len(q1_df)/len(verification_df)*100:.1f}%)'
    rule_text += f'\n  Q2 (-+): {len(q2_df)} ({len(q2_df)/len(verification_df)*100:.1f}%)'
    rule_text += f'\n  Q3 (--): {len(q3_df)} ({len(q3_df)/len(verification_df)*100:.1f}%)'
    rule_text += f'\n  Q4 (+-): {len(q4_df)} ({len(q4_df)/len(verification_df)*100:.1f}%)'

    props = dict(boxstyle='round', facecolor='wheat', alpha=0.9)
    fig.text(0.98, 0.5, rule_text, transform=fig.transFigure,
             fontsize=9, verticalalignment='center', horizontalalignment='right',
             bbox=props, family='monospace')

    plt.tight_layout(rect=[0, 0, 0.82, 1])  # テキストボックスのスペースを確保

    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")

    plt.close()


def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Generate X(t+1) vs X(t+2) scatter plot for a single rule'
    )
    parser.add_argument('--rule-id', '-r', type=int, required=True,
                       help='Rule ID (1-indexed)')
    parser.add_argument('--symbol', '-s', type=str, default='BTC',
                       help='Symbol code (default: BTC)')
    parser.add_argument('--output-dir', '-o', type=str,
                       default='analysis/crypt/output',
                       help='Output directory (default: analysis/crypt/output)')

    args = parser.parse_args()

    rule_id = args.rule_id
    symbol = args.symbol
    output_dir = args.output_dir

    print("=" * 70)
    print(f"  X(t+1) vs X(t+2) Scatter Plot Generator")
    print("=" * 70)
    print(f"Rule ID: {rule_id}")
    print(f"Symbol: {symbol}")
    print(f"Output Directory: {output_dir}")
    print("=" * 70)

    try:
        # 1. ルール情報を読み込み
        print(f"\n[1/3] Loading rule information...")
        rule_info = load_rule_info(rule_id, symbol)
        print(f"  ✓ Loaded rule {rule_id}")

        # 2. 検証データを読み込み
        print(f"\n[2/3] Loading verification data...")
        verification_df = load_verification_data(rule_id, symbol)
        print(f"  ✓ Loaded {len(verification_df)} match points")

        # 3. 散布図を作成
        print(f"\n[3/3] Generating 2D future scatter plot...")
        symbol_dir = os.path.join(output_dir, symbol)
        os.makedirs(symbol_dir, exist_ok=True)

        output_path = f"{symbol_dir}/{symbol}_rule_{rule_id:04d}_2d_future.png"
        plot_2d_future_scatter(rule_info, verification_df, output_path, symbol, rule_id)

        print(f"\n{'='*70}")
        print(f"  Success!")
        print(f"  Output: {output_path}")
        print(f"{'='*70}")

    except FileNotFoundError as e:
        print(f"\n  ❌ Error: {e}")
    except Exception as e:
        print(f"\n  ❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()
