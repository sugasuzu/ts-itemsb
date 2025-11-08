#!/usr/bin/env python3
"""
個別ルール詳細散布図スクリプト

特定のルールがマッチした時点のX(t+1)分布を可視化します。
- 正方向（X>0）と負方向（X<0）を色分け
- 各マッチポイントをタイムスタンプ順に表示
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


def plot_rule_detail_scatter(rule_info, verification_df, output_path, symbol, rule_id):
    """
    個別ルールの詳細散布図を作成

    Args:
        rule_info: ルール情報
        verification_df: 検証データ
        output_path: 出力パス
        symbol: 銘柄コード
        rule_id: ルールID
    """
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 12))

    # データを分類
    positive_mask = verification_df['X(t+1)'] > 0
    negative_mask = verification_df['X(t+1)'] < 0

    positive_df = verification_df[positive_mask]
    negative_df = verification_df[negative_mask]

    # 統計値
    mean_t1 = rule_info['X(t+1)_mean']
    sigma_t1 = rule_info['X(t+1)_sigma']
    win_rate = rule_info['X(t+1)_win_rate']
    pf = rule_info['X(t+1)_profit_factor']
    pos_mean = rule_info['X(t+1)_pos_mean']
    neg_mean = rule_info['X(t+1)_neg_mean']
    pos_cnt = rule_info['X(t+1)_pos_cnt']
    neg_cnt = rule_info['X(t+1)_neg_cnt']

    # === 上段: X(t+1)のヒストグラム ===
    bins = np.linspace(-2, 2, 40)

    ax1.hist(negative_df['X(t+1)'], bins=bins, color='red', alpha=0.6,
             label=f'Negative (n={len(negative_df)}, mean={neg_mean:.3f}%)', edgecolor='darkred')
    ax1.hist(positive_df['X(t+1)'], bins=bins, color='green', alpha=0.6,
             label=f'Positive (n={len(positive_df)}, mean={pos_mean:.3f}%)', edgecolor='darkgreen')

    # 平均線
    ax1.axvline(pos_mean, color='darkgreen', linestyle='--', linewidth=2, label=f'Positive Mean')
    ax1.axvline(neg_mean, color='darkred', linestyle='--', linewidth=2, label=f'Negative Mean')
    ax1.axvline(0, color='black', linestyle='-', linewidth=1, alpha=0.5)

    ax1.set_xlabel('X(t+1) Change [%]', fontsize=12, fontweight='bold')
    ax1.set_ylabel('Frequency', fontsize=12, fontweight='bold')
    ax1.set_title(f'X(t+1) Distribution: Rule {rule_id}\n'
                  f'Win Rate: {win_rate*100:.1f}% | Profit Factor: {pf:.2f} | '
                  f'Mean: {mean_t1:.3f}% ± {sigma_t1:.3f}%',
                  fontsize=13, fontweight='bold')
    ax1.legend(fontsize=10)
    ax1.grid(True, alpha=0.3)

    # === 下段: 時系列プロット ===
    # インデックスを時系列として使用
    indices = range(len(verification_df))

    # 正・負を色分けしてプロット
    ax2.scatter(indices, verification_df['X(t+1)'],
                c=['green' if x > 0 else 'red' for x in verification_df['X(t+1)']],
                alpha=0.7, s=80, edgecolors='black', linewidths=0.5)

    # 平均線
    ax2.axhline(mean_t1, color='blue', linestyle='--', linewidth=2,
                label=f'Overall Mean ({mean_t1:.3f}%)')
    ax2.axhline(0, color='black', linestyle='-', linewidth=1, alpha=0.5)

    # ±1σの範囲
    ax2.axhline(mean_t1 + sigma_t1, color='gray', linestyle=':', linewidth=1, alpha=0.5)
    ax2.axhline(mean_t1 - sigma_t1, color='gray', linestyle=':', linewidth=1, alpha=0.5)
    ax2.fill_between(indices, mean_t1 - sigma_t1, mean_t1 + sigma_t1,
                      color='blue', alpha=0.1, label='±1σ Range')

    ax2.set_xlabel('Match Index (Time Order)', fontsize=12, fontweight='bold')
    ax2.set_ylabel('X(t+1) Change [%]', fontsize=12, fontweight='bold')
    ax2.set_title('X(t+1) Time Series', fontsize=13, fontweight='bold')
    ax2.legend(fontsize=10)
    ax2.grid(True, alpha=0.3)

    # ルール情報をテキストボックスで表示
    # 属性情報を取得
    attrs = []
    for i in range(1, 9):
        attr_col = f'Attr{i}'
        if attr_col in rule_info.index and rule_info[attr_col] and str(rule_info[attr_col]) != '0':
            attrs.append(str(rule_info[attr_col]))

    rule_text = 'Rule: ' + ' ∧ '.join(attrs[:5])  # 最初の5属性
    if len(attrs) > 5:
        rule_text += '\n      ' + ' ∧ '.join(attrs[5:])

    rule_text += f'\n\nStatistics:'
    rule_text += f'\n  Support: {int(rule_info["support_count"])} matches'
    rule_text += f'\n  Positive: {pos_cnt} times (mean: +{pos_mean:.3f}%)'
    rule_text += f'\n  Negative: {neg_cnt} times (mean: {neg_mean:.3f}%)'
    rule_text += f'\n  Win Rate: {win_rate*100:.1f}%'
    rule_text += f'\n  Profit Factor: {pf:.2f}'
    rule_text += f'\n  Expected Value: {win_rate*pos_mean - (1-win_rate)*abs(neg_mean):.4f}%'

    props = dict(boxstyle='round', facecolor='wheat', alpha=0.9)
    fig.text(0.98, 0.5, rule_text, transform=fig.transFigure,
             fontsize=9, verticalalignment='center', horizontalalignment='right',
             bbox=props, family='monospace')

    plt.tight_layout(rect=[0, 0, 0.85, 1])  # テキストボックスのスペースを確保

    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")

    plt.close()


def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Generate detailed scatter plot for a single rule'
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
    print(f"  Single Rule Detail Scatter Plot Generator")
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
        print(f"\n[3/3] Generating detail scatter plot...")
        symbol_dir = os.path.join(output_dir, symbol)
        os.makedirs(symbol_dir, exist_ok=True)

        output_path = f"{symbol_dir}/{symbol}_rule_{rule_id:04d}_detail.png"
        plot_rule_detail_scatter(rule_info, verification_df, output_path, symbol, rule_id)

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
