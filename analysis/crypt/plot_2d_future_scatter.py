#!/usr/bin/env python3
"""
2次元未来予測散布図可視化スクリプト [X(t+1) vs X(t+2)]

発見されたルールの上位3つについて、未来予測値の2次元分布を可視化します。
- X軸: X(t+1) - 翌日の変化率
- Y軸: X(t+2) - 2日後の変化率
- 楕円: 2σの分散範囲

各ルールごとに1つのPNG画像を生成します。
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Ellipse
import argparse
import os
import sys
from pathlib import Path


def calculate_rule_score(row):
    """
    ルールスコアを計算（新定義）

    良いルール:
      1. マッチ数が多い → support_count が大きい
      2. σが小さい → 予測が安定
      3. σが均等 → 正方形/立方体（等方的）

    スコア = support_count / (sigma_avg × (1 + sigma_variance_penalty))

    Args:
        row: DataFrameの1行（ルール情報）

    Returns:
        float: ルールスコア
    """
    sigma_t1 = row['X(t+1)_sigma']
    sigma_t2 = row['X(t+2)_sigma']
    sigma_t3 = row['X(t+3)_sigma']
    support_count = row['support_count']

    # σの平均値（小さいほど良い）
    sigma_avg = (sigma_t1 + sigma_t2 + sigma_t3) / 3.0

    # σの分散（均等性のペナルティ）
    sigma_variance = np.var([sigma_t1, sigma_t2, sigma_t3])

    # ゼロ除算対策
    if sigma_avg < 1e-10:
        return 0.0

    # スコア計算
    # - マッチ数が多いほど高スコア
    # - σが小さいほど高スコア
    # - σが均等なほど高スコア（分散ペナルティが小さい）
    score = support_count / (sigma_avg * (1 + sigma_variance))

    return score


def load_rule_pool(symbol, base_dir=None):
    """
    ルールプールファイルを読み込み、スコアを計算

    Args:
        symbol: 銘柄コード（例: AAVE）
        base_dir: 出力ディレクトリのベースパス（Noneの場合は自動検出）

    Returns:
        DataFrame: スコア付きルール一覧（スコア降順）
    """
    if base_dir is None:
        # スクリプトファイルの場所を基準にパスを解決
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../crypto_data/output"
        base_dir = base_dir.resolve()

    pool_file = f"{base_dir}/{symbol}/pool/zrp01a.txt"

    if not os.path.exists(pool_file):
        raise FileNotFoundError(f"Pool file not found: {pool_file}")

    df = pd.read_csv(pool_file, sep='\t')
    df['rule_score'] = df.apply(calculate_rule_score, axis=1)
    df = df.sort_values('rule_score', ascending=False).reset_index(drop=True)
    df['rule_id'] = range(1, len(df) + 1)

    return df


def load_all_matches(rules_df, symbol, base_dir=None):
    """
    全ルールの全マッチポイントを読み込み

    Args:
        rules_df: ルール一覧DataFrame
        symbol: 銘柄コード
        base_dir: 出力ディレクトリ（Noneの場合は自動検出）

    Returns:
        tuple: (全X(t+1)のarray, 全X(t+2)のarray)
    """
    if base_dir is None:
        # スクリプトファイルの場所を基準にパスを解決
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../crypto_data/output"
        base_dir = base_dir.resolve()

    all_x_t1 = []
    all_x_t2 = []

    for idx, row in rules_df.iterrows():
        rule_id = row['rule_id']
        verification_file = f"{base_dir}/{symbol}/verification/rule_{rule_id:03d}.csv"

        if not os.path.exists(verification_file):
            continue

        df = pd.read_csv(verification_file)

        all_x_t1.extend(df['X(t+1)'].values)
        all_x_t2.extend(df['X(t+2)'].values)

    return np.array(all_x_t1), np.array(all_x_t2)


def load_rule_future_values(rule_id, symbol, base_dir=None):
    """
    特定ルールの未来予測値を読み込み

    Args:
        rule_id: ルールID（1-indexed）
        symbol: 銘柄コード
        base_dir: 出力ディレクトリ（Noneの場合は自動検出）

    Returns:
        DataFrame: マッチデータ
    """
    if base_dir is None:
        # スクリプトファイルの場所を基準にパスを解決
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../crypto_data/output"
        base_dir = base_dir.resolve()

    verification_file = f"{base_dir}/{symbol}/verification/rule_{rule_id:03d}.csv"

    if not os.path.exists(verification_file):
        raise FileNotFoundError(f"Verification file not found: {verification_file}")

    df = pd.read_csv(verification_file)

    return df


def plot_2d_scatter(all_x_t1, all_x_t2, rule_x_t1, rule_x_t2,
                    rule_info, output_path, symbol):
    """
    2次元散布図を作成 [X(t+1) vs X(t+2)]

    Args:
        all_x_t1: 全ルールのX(t+1)
        all_x_t2: 全ルールのX(t+2)
        rule_x_t1: 該当ルールのX(t+1)
        rule_x_t2: 該当ルールのX(t+2)
        rule_info: ルール情報（Series）
        output_path: 出力ファイルパス
        symbol: 銘柄コード
    """
    fig, ax = plt.subplots(figsize=(12, 10))

    # 1. ベースレイヤー: 全マッチポイント（薄い灰色）
    ax.scatter(all_x_t1, all_x_t2,
               c='lightgray', alpha=0.2, s=15,
               label='All Rules Matches',
               zorder=1)

    # 2. 該当ルールのマッチポイント（赤色）
    rule_id = rule_info['rule_id']
    rule_score = rule_info['rule_score']
    support_count = rule_info['support_count']
    support_rate = rule_info['support_rate']
    num_attrs = rule_info['NumAttr']

    ax.scatter(rule_x_t1, rule_x_t2,
               c='red', alpha=0.8, s=120, marker='o',
               edgecolors='darkred', linewidths=2,
               label=f'Rule {rule_id} Matches (n={support_count})',
               zorder=3)

    # 3. 統計値を計算
    mean_t1 = rule_x_t1.mean()
    mean_t2 = rule_x_t2.mean()
    sigma_t1 = rule_x_t1.std()
    sigma_t2 = rule_x_t2.std()

    # 4. 平均点を表示（大きなX印）
    ax.scatter([mean_t1], [mean_t2],
               c='darkred', s=400, marker='X',
               edgecolors='white', linewidths=3,
               label=f'Mean (μ_t+1={mean_t1:.2f}, μ_t+2={mean_t2:.2f})',
               zorder=4)

    # 5. 2σの楕円を描画
    if sigma_t1 > 0 and sigma_t2 > 0:
        ellipse = Ellipse(
            xy=(mean_t1, mean_t2),
            width=4 * sigma_t1,   # 2σ × 2（両側）
            height=4 * sigma_t2,
            alpha=0.25,
            facecolor='red',
            edgecolor='darkred',
            linewidth=2.5,
            linestyle='--',
            label=f'2σ Range (σ_t+1={sigma_t1:.2f}, σ_t+2={sigma_t2:.2f})',
            zorder=2
        )
        ax.add_patch(ellipse)

    # 6. ゼロライン（基準線）
    ax.axhline(0, color='black', linestyle='--', alpha=0.4, linewidth=1)
    ax.axvline(0, color='black', linestyle='--', alpha=0.4, linewidth=1)

    # 7. 軸ラベルとタイトル
    ax.set_xlabel('X(t+1): Next Day Change (%)', fontsize=14, fontweight='bold')
    ax.set_ylabel('X(t+2): 2 Days Later Change (%)', fontsize=14, fontweight='bold')

    title = f'{symbol}: 2D Future Distribution - Rule {rule_id}\n'
    title += f'Score={rule_score:.4f} | Support={support_rate:.4f} | '
    title += f'Attrs={num_attrs} | n={support_count}'
    ax.set_title(title, fontsize=15, fontweight='bold', pad=20)

    # 8. 凡例
    ax.legend(loc='upper left', fontsize=10, framealpha=0.95)

    # 9. グリッド
    ax.grid(True, alpha=0.3, linestyle=':', linewidth=0.8)

    # 10. 軸の範囲を固定（-10 ~ 10）
    ax.set_xlim(-10, 10)
    ax.set_ylim(-10, 10)

    # 11. アスペクト比を1:1に（正方形）
    ax.set_aspect('equal', adjustable='box')

    # レイアウト調整
    plt.tight_layout()

    # 保存
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")

    plt.close()


def process_single_symbol(symbol, top_n, output_dir):
    """
    単一銘柄の処理

    Args:
        symbol: 銘柄コード
        top_n: 上位何件のルールを処理するか
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
        rules_df = load_rule_pool(symbol)
        print(f"  ✓ Loaded {len(rules_df)} rules")

        if len(rules_df) == 0:
            print(f"  ⚠ No rules found for {symbol} (skipping)")
            return False

        # 上位N件を選択
        top_rules = rules_df.head(top_n)
        print(f"  ✓ Selected top {min(top_n, len(rules_df))} rules")

        # スコア一覧を表示
        print(f"\n  Top Rules:")
        for idx, row in top_rules.iterrows():
            print(f"    Rule {row['rule_id']:3d}: "
                  f"Score={row['rule_score']:8.4f} | "
                  f"Support={row['support_count']:3d} | "
                  f"σ_t+1={row['X(t+1)_sigma']:.2f} | "
                  f"σ_t+2={row['X(t+2)_sigma']:.2f}")

        # 2. 全ルールの全マッチポイントを読み込み
        print(f"\n[2/4] Loading all matches from all rules...")
        all_x_t1, all_x_t2 = load_all_matches(rules_df, symbol)
        print(f"  ✓ Loaded {len(all_x_t1)} total match points")

        # 統計情報を表示
        print(f"\n  All Matches Statistics:")
        print(f"    X(t+1): μ={all_x_t1.mean():.2f}, σ={all_x_t1.std():.2f}, "
              f"range=[{all_x_t1.min():.2f}, {all_x_t1.max():.2f}]")
        print(f"    X(t+2): μ={all_x_t2.mean():.2f}, σ={all_x_t2.std():.2f}, "
              f"range=[{all_x_t2.min():.2f}, {all_x_t2.max():.2f}]")

        # 3. 各ルールについて散布図を作成
        print(f"\n[3/4] Generating 2D scatter plots...")

        for idx, rule_info in top_rules.iterrows():
            rule_id = rule_info['rule_id']

            # ルールの未来予測値を読み込み
            rule_data = load_rule_future_values(rule_id, symbol)

            if len(rule_data) == 0:
                print(f"  ⚠ Rule {rule_id}: No matches (skipping)")
                continue

            rule_x_t1 = rule_data['X(t+1)'].values
            rule_x_t2 = rule_data['X(t+2)'].values

            # 銘柄ごとのディレクトリを作成
            symbol_dir = os.path.join(output_dir, symbol)
            os.makedirs(symbol_dir, exist_ok=True)

            # 散布図を作成
            output_path = f"{symbol_dir}/{symbol}_rule_{rule_id:03d}_2d_future.png"
            plot_2d_scatter(all_x_t1, all_x_t2, rule_x_t1, rule_x_t2,
                            rule_info, output_path, symbol)

        # 4. 完了
        print(f"\n[4/4] Complete!")
        print(f"  ✓ Generated {len(top_rules)} 2D scatter plots for {symbol}")

        return True

    except FileNotFoundError as e:
        print(f"\n  ⚠ {symbol}: {e}")
        return False
    except Exception as e:
        print(f"\n  ❌ {symbol}: Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return False


def get_all_symbols(base_dir=None):
    """
    利用可能な全銘柄を取得

    Returns:
        list: 銘柄コードのリスト
    """
    if base_dir is None:
        script_dir = Path(__file__).parent
        base_dir = script_dir / "../../crypto_data/output"
        base_dir = base_dir.resolve()

    symbols = []
    if os.path.exists(base_dir):
        for item in os.listdir(base_dir):
            item_path = os.path.join(base_dir, item)
            if os.path.isdir(item_path):
                symbols.append(item)

    return sorted(symbols)


def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Generate 2D future scatter plots [X(t+1) vs X(t+2)] for top 3 rules'
    )
    parser.add_argument('--symbol', '-s', type=str, default=None,
                        help='Symbol code (e.g., AAVE, BTC). If not specified, process all symbols.')
    parser.add_argument('--top-n', '-n', type=int, default=3,
                        help='Number of top rules to plot (default: 3)')
    parser.add_argument('--output-dir', '-o', type=str,
                        default='analysis/crypt/output',
                        help='Output directory (default: analysis/crypt/output)')

    args = parser.parse_args()

    top_n = args.top_n
    output_dir = args.output_dir

    print("=" * 70)
    print(f"  2D Future Scatter Plot Generator [X(t+1) vs X(t+2)]")
    print("=" * 70)
    print(f"Top N Rules: {top_n}")
    print(f"Output Directory: {output_dir}")
    print("=" * 70)

    # 出力ディレクトリを作成
    os.makedirs(output_dir, exist_ok=True)

    # 銘柄リストを決定
    if args.symbol:
        # 単一銘柄を指定された場合
        symbols = [args.symbol]
    else:
        # 全銘柄を処理
        symbols = get_all_symbols()
        print(f"\nFound {len(symbols)} symbols: {', '.join(symbols)}")

    # 各銘柄を処理
    success_count = 0
    fail_count = 0

    for symbol in symbols:
        result = process_single_symbol(symbol, top_n, output_dir)
        if result:
            success_count += 1
        else:
            fail_count += 1

    # 最終サマリー
    print("\n" + "=" * 70)
    print(f"  Summary")
    print("=" * 70)
    print(f"Total symbols: {len(symbols)}")
    print(f"Success: {success_count}")
    print(f"Failed: {fail_count}")
    print(f"Output directory: {output_dir}")
    print("=" * 70)


if __name__ == "__main__":
    main()
