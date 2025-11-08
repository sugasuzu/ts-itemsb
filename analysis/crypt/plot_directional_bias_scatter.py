#!/usr/bin/env python3
"""
方向性バイアスルール散布図可視化スクリプト

発見された正/負バイアスルールについて、勝率・損益比・期待値を可視化します。
- X軸: 勝率 (Win Rate)
- Y軸: 損益比 (Profit Factor)
- 色: 平均値 (正=緑、負=赤)
- サイズ: サポート数

正バイアスと負バイアスの両方のルールを1つの散布図に表示します。
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import argparse
import os
import sys
from pathlib import Path


def load_rule_pool(symbol, base_dir=None):
    """
    ルールプールファイルを読み込み

    Args:
        symbol: 銘柄コード（例: BTC）
        base_dir: 出力ディレクトリのベースパス（Noneの場合は自動検出）

    Returns:
        DataFrame: ルール一覧
    """
    if base_dir is None:
        # スクリプトファイルの場所を基準にパスを解決
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../1-deta-enginnering/crypto_data_hourly/output"
        base_dir = base_dir.resolve()

    pool_file = f"{base_dir}/{symbol}/pool/zrp01a.txt"

    if not os.path.exists(pool_file):
        raise FileNotFoundError(f"Pool file not found: {pool_file}")

    df = pd.read_csv(pool_file, sep='\t')

    return df


def classify_rules(df, min_abs_mean=0.1, min_win_positive=0.52, max_win_negative=0.48,
                   min_pf=1.1, max_pf=0.9, min_sigma=0.5):
    """
    ルールを正バイアス・負バイアスに分類

    Args:
        df: ルールDataFrame
        min_abs_mean: 最小絶対平均値
        min_win_positive: 正バイアスの最小勝率
        max_win_negative: 負バイアスの最大勝率
        min_pf: 正バイアスの最小損益比
        max_pf: 負バイアスの最大損益比
        min_sigma: 最小σ（高ボラティリティルールのみ）

    Returns:
        tuple: (positive_df, negative_df, neutral_df)
    """
    mean_t1 = df['X(t+1)_mean']
    sigma_t1 = df['X(t+1)_sigma']
    win_rate = df['X(t+1)_win_rate']
    pf = df['X(t+1)_profit_factor']

    # 高ボラティリティルール（σ > min_sigma）
    high_volatility = sigma_t1 > min_sigma

    # 正バイアス条件
    positive_condition = (
        (mean_t1 > min_abs_mean) &
        (win_rate >= min_win_positive) &
        (pf >= min_pf) &
        high_volatility
    )

    # 負バイアス条件
    negative_condition = (
        (mean_t1 < -min_abs_mean) &
        (win_rate <= max_win_negative) &
        (pf <= max_pf) &
        high_volatility
    )

    positive_df = df[positive_condition].copy()
    negative_df = df[negative_condition].copy()
    neutral_df = df[~(positive_condition | negative_condition)].copy()

    return positive_df, negative_df, neutral_df


def plot_directional_bias_scatter(positive_df, negative_df, neutral_df,
                                  output_path, symbol):
    """
    方向性バイアス散布図を作成

    Args:
        positive_df: 正バイアスルール
        negative_df: 負バイアスルール
        neutral_df: 中立ルール
        output_path: 出力ファイルパス
        symbol: 銘柄コード
    """
    fig, ax = plt.subplots(figsize=(14, 10))

    # 1. 背景レイヤー: 中立ルール（薄い灰色）
    if len(neutral_df) > 0:
        ax.scatter(neutral_df['X(t+1)_win_rate'],
                  neutral_df['X(t+1)_profit_factor'],
                  c='lightgray', alpha=0.15, s=30,
                  label=f'Neutral Rules (n={len(neutral_df)})',
                  zorder=1)

    # 2. 正バイアスルール（緑色、サイズ=サポート数）
    if len(positive_df) > 0:
        sizes_pos = positive_df['support_count'] * 3  # サポート数に比例
        colors_pos = positive_df['X(t+1)_mean']  # 平均値で色分け

        scatter_pos = ax.scatter(
            positive_df['X(t+1)_win_rate'],
            positive_df['X(t+1)_profit_factor'],
            c=colors_pos, cmap='Greens', alpha=0.7,
            s=sizes_pos, marker='o',
            edgecolors='darkgreen', linewidths=1.5,
            label=f'Positive Bias (n={len(positive_df)})',
            vmin=0, vmax=0.5,
            zorder=3
        )

        # カラーバー（正バイアス）
        cbar_pos = plt.colorbar(scatter_pos, ax=ax, pad=0.02, aspect=30)
        cbar_pos.set_label('Mean X(t+1) [%] (Positive)', fontsize=11, fontweight='bold')

    # 3. 負バイアスルール（赤色、サイズ=サポート数）
    if len(negative_df) > 0:
        sizes_neg = negative_df['support_count'] * 3
        colors_neg = -negative_df['X(t+1)_mean']  # 絶対値で色分け

        scatter_neg = ax.scatter(
            negative_df['X(t+1)_win_rate'],
            negative_df['X(t+1)_profit_factor'],
            c=colors_neg, cmap='Reds', alpha=0.7,
            s=sizes_neg, marker='s',
            edgecolors='darkred', linewidths=1.5,
            label=f'Negative Bias (n={len(negative_df)})',
            vmin=0, vmax=0.5,
            zorder=3
        )

    # 4. 基準線（勝率50%、損益比1.0）
    ax.axhline(1.0, color='black', linestyle='--', alpha=0.5, linewidth=1.5,
              label='Break-even (PF=1.0)')
    ax.axvline(0.5, color='black', linestyle='--', alpha=0.5, linewidth=1.5,
              label='50% Win Rate')

    # 5. 軸ラベルとタイトル
    ax.set_xlabel('Win Rate (X(t+1))', fontsize=14, fontweight='bold')
    ax.set_ylabel('Profit Factor (X(t+1))', fontsize=14, fontweight='bold')

    title = f'{symbol}: Directional Bias Rules Distribution\n'
    title += f'Positive Bias: {len(positive_df)} rules | '
    title += f'Negative Bias: {len(negative_df)} rules | '
    title += f'Total: {len(positive_df) + len(negative_df)} high-volatility directional rules'
    ax.set_title(title, fontsize=15, fontweight='bold', pad=20)

    # 6. 凡例（マーカーサイズを小さく）
    ax.legend(loc='upper left', fontsize=10, framealpha=0.95, markerscale=0.5)

    # 7. グリッド
    ax.grid(True, alpha=0.3, linestyle=':', linewidth=0.8)

    # 8. 軸の範囲
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 6)

    # 9. 注釈（散布図の説明）
    textstr = 'Circle = Positive Bias (Upward)\nSquare = Negative Bias (Downward)\nSize = Support Count'
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.8)
    ax.text(0.98, 0.02, textstr, transform=ax.transAxes, fontsize=10,
            verticalalignment='bottom', horizontalalignment='right', bbox=props)

    # レイアウト調整
    plt.tight_layout()

    # 保存
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")

    plt.close()


def plot_mean_sigma_scatter(positive_df, negative_df, neutral_df,
                            output_path, symbol):
    """
    平均値 vs σ の散布図を作成

    Args:
        positive_df: 正バイアスルール
        negative_df: 負バイアスルール
        neutral_df: 中立ルール
        output_path: 出力ファイルパス
        symbol: 銘柄コード
    """
    fig, ax = plt.subplots(figsize=(14, 10))

    # 1. 背景レイヤー: 中立ルール（薄い灰色）
    if len(neutral_df) > 0:
        ax.scatter(neutral_df['X(t+1)_mean'],
                  neutral_df['X(t+1)_sigma'],
                  c='lightgray', alpha=0.15, s=30,
                  label=f'Neutral Rules (n={len(neutral_df)})',
                  zorder=1)

    # 2. 正バイアスルール（緑色）
    if len(positive_df) > 0:
        sizes_pos = positive_df['support_count'] * 3

        ax.scatter(
            positive_df['X(t+1)_mean'],
            positive_df['X(t+1)_sigma'],
            c='green', alpha=0.7,
            s=sizes_pos, marker='o',
            edgecolors='darkgreen', linewidths=1.5,
            label=f'Positive Bias (n={len(positive_df)})',
            zorder=3
        )

    # 3. 負バイアスルール（赤色）
    if len(negative_df) > 0:
        sizes_neg = negative_df['support_count'] * 3

        ax.scatter(
            negative_df['X(t+1)_mean'],
            negative_df['X(t+1)_sigma'],
            c='red', alpha=0.7,
            s=sizes_neg, marker='s',
            edgecolors='darkred', linewidths=1.5,
            label=f'Negative Bias (n={len(negative_df)})',
            zorder=3
        )

    # 4. 基準線（σ=0.5の従来制約）
    ax.axhline(0.5, color='blue', linestyle='--', alpha=0.6, linewidth=2,
              label='Traditional σ limit (0.5)')
    ax.axvline(0, color='black', linestyle='-', alpha=0.4, linewidth=1)

    # 5. 軸ラベルとタイトル
    ax.set_xlabel('Mean X(t+1) [%]', fontsize=14, fontweight='bold')
    ax.set_ylabel('σ X(t+1) [%]', fontsize=14, fontweight='bold')

    title = f'{symbol}: Mean vs σ Distribution\n'
    title += f'High-volatility directional rules (σ > 0.5)'
    ax.set_title(title, fontsize=15, fontweight='bold', pad=20)

    # 6. 凡例（マーカーサイズを小さく）
    ax.legend(loc='upper right', fontsize=10, framealpha=0.95, markerscale=0.5)

    # 7. グリッド
    ax.grid(True, alpha=0.3, linestyle=':', linewidth=0.8)

    # 8. 軸の範囲
    ax.set_xlim(-0.5, 0.5)
    ax.set_ylim(0, 2)

    # レイアウト調整
    plt.tight_layout()

    # 保存
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")

    plt.close()


def process_single_symbol(symbol, output_dir):
    """
    単一銘柄の処理

    Args:
        symbol: 銘柄コード
        output_dir: 出力ディレクトリ

    Returns:
        bool: 成功したかどうか
    """
    try:
        print(f"\n{'='*70}")
        print(f"  Processing: {symbol}")
        print(f"{'='*70}")

        # 1. ルールプールを読み込み
        print(f"[1/4] Loading rule pool...")
        df = load_rule_pool(symbol)
        print(f"  ✓ Loaded {len(df)} rules")

        # 2. ルールを分類
        print(f"[2/4] Classifying rules...")
        positive_df, negative_df, neutral_df = classify_rules(df)

        print(f"  ✓ Positive bias rules: {len(positive_df)}")
        print(f"  ✓ Negative bias rules: {len(negative_df)}")
        print(f"  ✓ Neutral rules: {len(neutral_df)}")

        if len(positive_df) == 0 and len(negative_df) == 0:
            print(f"  ⚠ No directional bias rules found (skipping)")
            return False

        # 統計情報を表示
        print(f"\n  Positive Bias Statistics:")
        if len(positive_df) > 0:
            print(f"    Mean range: {positive_df['X(t+1)_mean'].min():.3f}% ~ {positive_df['X(t+1)_mean'].max():.3f}%")
            print(f"    σ range: {positive_df['X(t+1)_sigma'].min():.3f} ~ {positive_df['X(t+1)_sigma'].max():.3f}")
            print(f"    Win rate range: {positive_df['X(t+1)_win_rate'].min():.3f} ~ {positive_df['X(t+1)_win_rate'].max():.3f}")
            print(f"    PF range: {positive_df['X(t+1)_profit_factor'].min():.2f} ~ {positive_df['X(t+1)_profit_factor'].max():.2f}")

        print(f"\n  Negative Bias Statistics:")
        if len(negative_df) > 0:
            print(f"    Mean range: {negative_df['X(t+1)_mean'].min():.3f}% ~ {negative_df['X(t+1)_mean'].max():.3f}%")
            print(f"    σ range: {negative_df['X(t+1)_sigma'].min():.3f} ~ {negative_df['X(t+1)_sigma'].max():.3f}")
            print(f"    Win rate range: {negative_df['X(t+1)_win_rate'].min():.3f} ~ {negative_df['X(t+1)_win_rate'].max():.3f}")
            print(f"    PF range: {negative_df['X(t+1)_profit_factor'].min():.2f} ~ {negative_df['X(t+1)_profit_factor'].max():.2f}")

        # 3. 散布図を作成
        print(f"\n[3/4] Generating scatter plots...")

        # 銘柄ごとのディレクトリを作成
        symbol_dir = os.path.join(output_dir, symbol)
        os.makedirs(symbol_dir, exist_ok=True)

        # Win Rate vs Profit Factor散布図
        output_path_1 = f"{symbol_dir}/{symbol}_directional_bias_winrate_pf.png"
        plot_directional_bias_scatter(positive_df, negative_df, neutral_df,
                                      output_path_1, symbol)

        # Mean vs σ散布図
        output_path_2 = f"{symbol_dir}/{symbol}_directional_bias_mean_sigma.png"
        plot_mean_sigma_scatter(positive_df, negative_df, neutral_df,
                               output_path_2, symbol)

        # 4. 完了
        print(f"\n[4/4] Complete!")
        print(f"  ✓ Generated 2 scatter plots for {symbol}")

        return True

    except FileNotFoundError as e:
        print(f"\n  ⚠ {symbol}: {e}")
        return False
    except Exception as e:
        print(f"\n  ❌ {symbol}: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return False


def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Generate directional bias scatter plots (Win Rate vs PF, Mean vs σ)'
    )
    parser.add_argument('--symbol', '-s', type=str, default='BTC',
                       help='Symbol code (default: BTC)')
    parser.add_argument('--output-dir', '-o', type=str,
                       default='analysis/crypt/output',
                       help='Output directory (default: analysis/crypt/output)')

    args = parser.parse_args()

    symbol = args.symbol
    output_dir = args.output_dir

    print("=" * 70)
    print(f"  Directional Bias Scatter Plot Generator")
    print("=" * 70)
    print(f"Symbol: {symbol}")
    print(f"Output Directory: {output_dir}")
    print("=" * 70)

    # 出力ディレクトリを作成
    os.makedirs(output_dir, exist_ok=True)

    # 処理実行
    result = process_single_symbol(symbol, output_dir)

    # 最終サマリー
    print("\n" + "=" * 70)
    print(f"  Summary")
    print("=" * 70)
    print(f"Status: {'Success' if result else 'Failed'}")
    print(f"Output directory: {output_dir}")
    print("=" * 70)


if __name__ == "__main__":
    main()
