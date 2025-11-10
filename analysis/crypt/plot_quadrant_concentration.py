#!/usr/bin/env python3
"""
象限集中度可視化スクリプト

目的: X(t+1) vs X(t+2) 散布図で、特定の象限にデータが集中していることを示す

各ルールについて:
- 個別マッチポイントをプロット
- 象限ごとの集中度を計算
- 最も集中している象限を強調表示
- 集中度メトリクスを図に表示
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import argparse
import os
import sys
from pathlib import Path


def calculate_quadrant_concentration(x_t1, x_t2):
    """
    象限集中度を計算

    Q1 (++): x>0, y>0
    Q2 (-+): x<0, y>0
    Q3 (--): x<0, y<0
    Q4 (+-): x>0, y<0

    Returns:
        dict: 各象限の割合と最大集中度
    """
    total = len(x_t1)
    if total == 0:
        return None

    q1 = np.sum((x_t1 > 0) & (x_t2 > 0))
    q2 = np.sum((x_t1 < 0) & (x_t2 > 0))
    q3 = np.sum((x_t1 < 0) & (x_t2 < 0))
    q4 = np.sum((x_t1 > 0) & (x_t2 < 0))

    quadrants = {
        'Q1(++)': q1 / total,
        'Q2(-+)': q2 / total,
        'Q3(--)': q3 / total,
        'Q4(+-)': q4 / total
    }

    max_quadrant = max(quadrants, key=quadrants.get)
    max_concentration = quadrants[max_quadrant]

    return {
        'quadrants': quadrants,
        'max_quadrant': max_quadrant,
        'max_concentration': max_concentration,
        'counts': {'Q1': q1, 'Q2': q2, 'Q3': q3, 'Q4': q4},
        'total': total
    }


def load_rule_pool(pool_file):
    """ルールプールファイルを読み込み"""
    if not os.path.exists(pool_file):
        raise FileNotFoundError(f"Pool file not found: {pool_file}")

    df = pd.read_csv(pool_file, sep='\t')
    return df


def load_verification_data(verification_file):
    """検証データを読み込み"""
    if not os.path.exists(verification_file):
        return None

    df = pd.read_csv(verification_file)
    return df


def load_all_data_for_background(data_file):
    """背景用に全データを読み込み"""
    if not os.path.exists(data_file):
        return None, None

    df = pd.read_csv(data_file)

    if 'X' not in df.columns:
        return None, None

    # t+1, t+2の値を計算
    x_values = df['X'].values
    x_t1_all = x_values[1:]  # t+1
    x_t2_all = x_values[2:]  # t+2

    # 両方の長さを揃える
    min_len = min(len(x_t1_all), len(x_t2_all))

    return x_t1_all[:min_len], x_t2_all[:min_len]


def plot_quadrant_scatter(x_t1, x_t2, concentration_info, rule_info, output_path, symbol,
                          x_t1_all=None, x_t2_all=None):
    """
    象限集中度を強調した散布図を作成
    """
    fig, ax = plt.subplots(figsize=(13, 12))

    # 象限の背景色を設定（最大集中象限を強調）
    max_q = concentration_info['max_quadrant']
    alpha_bg = 0.08
    zoom_range = 3  # ズーム範囲を3%に変更

    # Q1 (右上)
    if max_q == 'Q1(++)':
        rect = Rectangle((0, 0), zoom_range, zoom_range, alpha=alpha_bg*2, facecolor='green', zorder=0)
    else:
        rect = Rectangle((0, 0), zoom_range, zoom_range, alpha=alpha_bg, facecolor='lightgray', zorder=0)
    ax.add_patch(rect)

    # Q2 (左上)
    if max_q == 'Q2(-+)':
        rect = Rectangle((-zoom_range, 0), zoom_range, zoom_range, alpha=alpha_bg*2, facecolor='blue', zorder=0)
    else:
        rect = Rectangle((-zoom_range, 0), zoom_range, zoom_range, alpha=alpha_bg, facecolor='lightgray', zorder=0)
    ax.add_patch(rect)

    # Q3 (左下)
    if max_q == 'Q3(--)':
        rect = Rectangle((-zoom_range, -zoom_range), zoom_range, zoom_range, alpha=alpha_bg*2, facecolor='red', zorder=0)
    else:
        rect = Rectangle((-zoom_range, -zoom_range), zoom_range, zoom_range, alpha=alpha_bg, facecolor='lightgray', zorder=0)
    ax.add_patch(rect)

    # Q4 (右下)
    if max_q == 'Q4(+-)':
        rect = Rectangle((0, -zoom_range), zoom_range, zoom_range, alpha=alpha_bg*2, facecolor='orange', zorder=0)
    else:
        rect = Rectangle((0, -zoom_range), zoom_range, zoom_range, alpha=alpha_bg, facecolor='lightgray', zorder=0)
    ax.add_patch(rect)

    # ゼロライン（太線）
    ax.axhline(0, color='black', linestyle='-', linewidth=2, alpha=0.7, zorder=2)
    ax.axvline(0, color='black', linestyle='-', linewidth=2, alpha=0.7, zorder=2)

    # 背景データ（全データポイント）を灰色で表示
    if x_t1_all is not None and x_t2_all is not None:
        # ズーム範囲内のデータのみ表示
        mask = (np.abs(x_t1_all) <= zoom_range) & (np.abs(x_t2_all) <= zoom_range)
        ax.scatter(x_t1_all[mask], x_t2_all[mask],
                   c='lightgray', alpha=0.2, s=25,
                   label=f'All Data (n={np.sum(mask):,})',
                   zorder=1, edgecolors='none')

    # 散布図プロット（ルールマッチポイント）- 全て赤色に統一
    ax.scatter(x_t1, x_t2,
               c='red', alpha=0.85, s=140,
               edgecolors='darkred', linewidths=1.8,
               label=f'Rule Matches (n={len(x_t1)})',
               zorder=4)

    # 平均値を表示
    mean_t1 = x_t1.mean()
    mean_t2 = x_t2.mean()
    sigma_t1 = x_t1.std()
    sigma_t2 = x_t2.std()

    ax.scatter([mean_t1], [mean_t2],
               c='darkviolet', s=500, marker='X',
               edgecolors='white', linewidths=3,
               label=f'Mean (μ₁={mean_t1:.3f}%, μ₂={mean_t2:.3f}%)',
               zorder=4)

    # 軸ラベルとタイトル
    ax.set_xlabel('X(t+1): Next Period Change (%)', fontsize=14, fontweight='bold')
    ax.set_ylabel('X(t+2): Two Periods Later Change (%)', fontsize=14, fontweight='bold')

    rule_id = rule_info.get('rule_id', '?')
    support_count = rule_info.get('support_count', 0)
    support_rate = rule_info.get('support_rate', 0)

    # ルールの条件部を取得（全属性）
    rule_attrs = []
    for i in range(1, 9):
        attr = rule_info.get(f'Attr{i}', '0')
        if str(attr) != '0':
            rule_attrs.append(str(attr))

    rule_str = ' AND '.join(rule_attrs)  # 全属性を表示

    title = f'{symbol}: Rule {rule_id} - Max Concentration: {max_q} = {concentration_info["max_concentration"]*100:.1f}%\n'
    title += f'IF {rule_str}\n'
    title += f'THEN (μ_X+1={mean_t1:.3f}%, σ_X+1={sigma_t1:.3f}%, μ_X+2={mean_t2:.3f}%, σ_X+2={sigma_t2:.3f}%)\n'
    title += f'Support: {support_count} / {rule_info.get("Negative", 0)} = {support_rate*100:.2f}%'
    ax.set_title(title, fontsize=11, fontweight='bold', pad=20)

    # 象限統計テキストボックス
    stats_text = "Quadrant Distribution:\n"
    for q_name, q_ratio in concentration_info['quadrants'].items():
        count = concentration_info['counts'][q_name.split('(')[0]]
        marker = "★" if q_name == max_q else " "
        stats_text += f"{marker} {q_name}: {q_ratio*100:5.1f}% (n={count})\n"

    # テキストボックスを配置
    props = dict(boxstyle='round', facecolor='wheat', alpha=0.9)
    ax.text(0.02, 0.98, stats_text,
            transform=ax.transAxes,
            fontsize=12,
            verticalalignment='top',
            bbox=props,
            family='monospace')

    # 凡例（見やすく配置）
    legend = ax.legend(loc='lower left', fontsize=11, framealpha=0.95,
                       edgecolor='black', fancybox=True, shadow=True)
    legend.set_zorder(10)  # 凡例を最前面に

    # グリッド
    ax.grid(True, alpha=0.4, linestyle=':', linewidth=0.8, zorder=2)

    # 軸の範囲（ズームイン）
    ax.set_xlim(-zoom_range, zoom_range)
    ax.set_ylim(-zoom_range, zoom_range)
    ax.set_aspect('equal', adjustable='box')

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")
    plt.close()


def plot_concentration_comparison(rules_data, output_path, symbol):
    """
    複数ルールの集中度を比較する棒グラフ
    """
    fig, ax = plt.subplots(figsize=(14, 8))

    rule_ids = [r['rule_id'] for r in rules_data]
    concentrations = [r['concentration']*100 for r in rules_data]
    quadrants = [r['max_quadrant'] for r in rules_data]

    # 色分け
    colors = []
    for q in quadrants:
        if 'Q1' in q:
            colors.append('green')
        elif 'Q2' in q:
            colors.append('blue')
        elif 'Q3' in q:
            colors.append('red')
        else:
            colors.append('orange')

    bars = ax.bar(range(len(rule_ids)), concentrations, color=colors, alpha=0.7, edgecolor='black')

    # 閾値ライン
    ax.axhline(40, color='red', linestyle='--', linewidth=2, label='MIN_CONCENTRATION (40%)')
    ax.axhline(50, color='orange', linestyle='--', linewidth=2, label='Target (50%)')
    ax.axhline(60, color='green', linestyle='--', linewidth=2, label='Ideal (60%)')

    ax.set_xlabel('Rule ID', fontsize=14, fontweight='bold')
    ax.set_ylabel('Maximum Quadrant Concentration (%)', fontsize=14, fontweight='bold')
    ax.set_title(f'{symbol}: Quadrant Concentration Comparison (Top Rules)', fontsize=16, fontweight='bold')
    ax.set_xticks(range(len(rule_ids)))
    ax.set_xticklabels([f'R{rid}' for rid in rule_ids], rotation=45)
    ax.legend(fontsize=11)
    ax.grid(True, alpha=0.3, axis='y')
    ax.set_ylim(0, 100)

    plt.tight_layout()
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"  ✓ Saved: {output_path}")
    plt.close()


def main():
    parser = argparse.ArgumentParser(
        description='Visualize quadrant concentration for rules (X(t+1) vs X(t+2))'
    )
    parser.add_argument('--pool', '-p', type=str, required=True,
                        help='Path to pool file (e.g., output/BTC/pool/zrp01a.txt)')
    parser.add_argument('--verification-dir', '-v', type=str, required=True,
                        help='Path to verification directory (e.g., output/BTC/verification)')
    parser.add_argument('--symbol', '-s', type=str, default='BTC',
                        help='Symbol name for titles')
    parser.add_argument('--top-n', '-n', type=int, default=10,
                        help='Number of top rules to visualize (default: 10)')
    parser.add_argument('--output-dir', '-o', type=str, default='output/quadrant_analysis',
                        help='Output directory')
    parser.add_argument('--min-concentration', '-m', type=float, default=0.40,
                        help='Minimum concentration threshold (default: 0.40)')
    parser.add_argument('--data-file', '-d', type=str, default=None,
                        help='Original data file for background (optional)')

    args = parser.parse_args()

    print("=" * 70)
    print("  Quadrant Concentration Visualizer (Zoomed)")
    print("=" * 70)
    print(f"Pool file: {args.pool}")
    print(f"Verification dir: {args.verification_dir}")
    print(f"Symbol: {args.symbol}")
    print(f"Top N: {args.top_n}")
    print(f"Min concentration: {args.min_concentration*100:.1f}%")
    if args.data_file:
        print(f"Background data: {args.data_file}")
    print("=" * 70)

    # 出力ディレクトリ作成
    os.makedirs(args.output_dir, exist_ok=True)

    try:
        # 背景データ読み込み（オプション）
        x_t1_all, x_t2_all = None, None
        if args.data_file:
            print("\n[1/4] Loading background data...")
            x_t1_all, x_t2_all = load_all_data_for_background(args.data_file)
            if x_t1_all is not None:
                print(f"  ✓ Loaded {len(x_t1_all):,} background points")
            else:
                print("  ⚠ Could not load background data")

        # ルールプール読み込み
        print("\n[2/4] Loading rule pool...")
        rules_df = load_rule_pool(args.pool)
        print(f"  ✓ Loaded {len(rules_df)} rules")

        # 上位N件を処理
        rules_to_plot = []

        for idx in range(min(args.top_n, len(rules_df))):
            rule_info = rules_df.iloc[idx].to_dict()
            rule_id = idx + 1
            rule_info['rule_id'] = rule_id

            # 検証データ読み込み
            verification_file = f"{args.verification_dir}/rule_{rule_id:03d}.csv"
            verify_df = load_verification_data(verification_file)

            if verify_df is None or len(verify_df) == 0:
                print(f"  ⚠ Rule {rule_id}: No verification data (skipping)")
                continue

            x_t1 = verify_df['X(t+1)'].values
            x_t2 = verify_df['X(t+2)'].values

            # 象限集中度計算
            concentration_info = calculate_quadrant_concentration(x_t1, x_t2)

            if concentration_info['max_concentration'] >= args.min_concentration:
                rules_to_plot.append({
                    'rule_id': rule_id,
                    'rule_info': rule_info,
                    'x_t1': x_t1,
                    'x_t2': x_t2,
                    'concentration_info': concentration_info,
                    'concentration': concentration_info['max_concentration'],
                    'max_quadrant': concentration_info['max_quadrant']
                })
                print(f"  ✓ Rule {rule_id}: {concentration_info['max_quadrant']} = "
                      f"{concentration_info['max_concentration']*100:.1f}%")
            else:
                print(f"  ⊘ Rule {rule_id}: Max concentration "
                      f"{concentration_info['max_concentration']*100:.1f}% < threshold")

        print(f"\n  → {len(rules_to_plot)} rules pass concentration threshold")

        # 個別散布図を作成
        print("\n[3/4] Generating individual scatter plots (zoomed to ±3%)...")
        for rule_data in rules_to_plot:
            output_file = f"{args.output_dir}/{args.symbol}_rule_{rule_data['rule_id']:03d}_quadrant_zoomed.png"
            plot_quadrant_scatter(
                rule_data['x_t1'],
                rule_data['x_t2'],
                rule_data['concentration_info'],
                rule_data['rule_info'],
                output_file,
                args.symbol,
                x_t1_all,
                x_t2_all
            )

        # 比較図を作成
        if len(rules_to_plot) > 0:
            print("\n[4/4] Generating concentration comparison chart...")
            comparison_file = f"{args.output_dir}/{args.symbol}_concentration_comparison.png"
            plot_concentration_comparison(rules_to_plot, comparison_file, args.symbol)

        print("\n" + "=" * 70)
        print("  Complete!")
        print(f"  Generated {len(rules_to_plot)} scatter plots")
        print(f"  Output directory: {args.output_dir}")
        print("=" * 70)

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
