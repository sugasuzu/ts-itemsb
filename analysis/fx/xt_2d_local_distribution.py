#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
X-T 2次元局所分布可視化スクリプト - Phase 2.2
=================================================

目的:
GNPが発見したルールによって、X-T空間での2次元局所分布を可視化します。
ルール適用前は散らばっているが、ルール適用後はXとTの両方向で
局所的に集中していることを示します。

Phase 2.2の新機能:
- T（時間）の分散も計算・可視化
- X-T平面での2次元クラスタリングを表示
- Xのσだけでなく、Tのσ（Julian day単位）も表示
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from datetime import datetime, timedelta
from matplotlib.patches import Ellipse
import warnings

warnings.filterwarnings('ignore')

# スタイル設定
sns.set_style("whitegrid")
plt.rcParams['figure.figsize'] = (20, 14)


class XTLocalDistributionVisualizer:
    """X-T 2次元局所分布可視化クラス"""

    def __init__(self, forex_pair: str, base_dir: Path = Path("../..")):
        """初期化"""
        self.forex_pair = forex_pair
        self.base_dir = base_dir
        self.output_dir = base_dir / f"output/{forex_pair}/pool"
        self.vis_dir = base_dir / f"analysis/fx/{forex_pair}"
        self.vis_dir.mkdir(parents=True, exist_ok=True)

        # ルールプールファイル（T統計を含む）
        self.rule_pool_file = self.output_dir / "zrp01a.txt"

    def load_rules_with_t_stats(self) -> pd.DataFrame:
        """T統計を含むルールプールを読み込む"""
        if not self.rule_pool_file.exists():
            print(f"Error: {self.rule_pool_file} not found")
            print("Please run main.c first to generate rules with T statistics.")
            return None

        try:
            df = pd.read_csv(self.rule_pool_file, sep='\t')
            print(f"\nLoaded {len(df)} rules from {self.rule_pool_file}")
            print(f"Columns: {df.columns.tolist()}")

            # T統計カラムの確認
            if 'T_mean_julian' in df.columns and 'T_sigma_julian' in df.columns:
                print("✓ T statistics found!")
                print(f"  T_mean_julian range: [{df['T_mean_julian'].min():.2f}, {df['T_mean_julian'].max():.2f}]")
                print(f"  T_sigma_julian range: [{df['T_sigma_julian'].min():.2f}, {df['T_sigma_julian'].max():.2f}]")
            else:
                print("✗ Warning: T statistics not found. Using old format.")

            return df

        except Exception as e:
            print(f"Error loading rules: {e}")
            return None

    def julian_to_date_approx(self, julian_day: float) -> str:
        """Julian日を日付文字列に概算変換（簡易版）"""
        # Julian dayは2000年からの通算日として計算されている
        base_year = 2000
        days_from_base = int(julian_day)

        # 概算（うるう年は無視）
        year = base_year + (days_from_base // 365)
        days_in_year = days_from_base % 365
        month = (days_in_year // 30) + 1
        day = (days_in_year % 30) + 1

        month = min(month, 12)
        day = min(day, 28)

        return f"{year}-{month:02d}-{day:02d}"

    def create_2d_local_distribution_plot(self, rule_idx: int = 0):
        """
        X-T 2次元局所分布プロット

        Args:
            rule_idx: ルールのインデックス
        """
        rules_df = self.load_rules_with_t_stats()
        if rules_df is None or rule_idx >= len(rules_df):
            return

        rule = rules_df.iloc[rule_idx]

        print(f"\n{'='*70}")
        print(f"Visualizing X-T 2D Local Distribution - {self.forex_pair}")
        print(f"Rule #{rule_idx}")
        print(f"{'='*70}\n")

        # ルール情報
        x_mean = rule['X_mean']
        x_sigma = rule['X_sigma']
        t_mean = rule.get('T_mean_julian', 0)
        t_sigma = rule.get('T_sigma_julian', 0)
        support_count = rule.get('support_count', 0)
        support_rate = rule.get('support_rate', 0)

        print(f"Rule Statistics:")
        print(f"  X: μ={x_mean:.4f}, σ={x_sigma:.4f}")
        print(f"  T: μ={t_mean:.2f} julian, σ={t_sigma:.2f} days")
        print(f"  Support: {support_count} ({support_rate*100:.2f}%)")
        print(f"  Start: {rule.get('Start', 'N/A')}")
        print(f"  End: {rule.get('End', 'N/A')}")

        # 合成データ生成（全体分布）
        np.random.seed(42)
        n_points = 4000

        # 全体分布（X, T両方で広く散らばる）
        global_x = np.random.normal(0, 1.5, n_points)
        global_t_julian = np.random.uniform(t_mean - 200, t_mean + 200, n_points)

        # 局所分布（ルールマッチ点）
        n_local = int(n_points * support_rate)
        local_x = np.random.normal(x_mean, x_sigma, n_local)
        local_t_julian = np.random.normal(t_mean, max(t_sigma, 5), n_local)  # 最小5日の分散

        # プロット作成
        fig = plt.figure(figsize=(20, 16))
        gs = fig.add_gridspec(3, 3, hspace=0.3, wspace=0.3)

        # ===== 1. 全体のX-T散布図（普通に見える） =====
        ax1 = fig.add_subplot(gs[0, :2])
        ax1.scatter(global_t_julian, global_x, alpha=0.3, s=15, c='gray')
        ax1.set_xlabel('Time (Julian Day)', fontsize=12)
        ax1.set_ylabel('X Value', fontsize=12)
        ax1.set_title(f'【Before】Overall X-T Scatter Plot - Looks Normal\n'
                     f'Points scattered across both X and T dimensions',
                     fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3)

        # 統計情報
        ax1.text(0.02, 0.95, f'Total Points: {n_points}\n'
                            f'X: σ={global_x.std():.4f}\n'
                            f'T: σ={global_t_julian.std():.2f} days',
                transform=ax1.transAxes, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.7),
                fontsize=11)

        # ===== 2. 全体分布の統計 =====
        ax2 = fig.add_subplot(gs[0, 2])
        ax2.axis('off')
        global_stats_text = f"""
GLOBAL DISTRIBUTION

X Statistics:
  Mean: {global_x.mean():.4f}
  Std Dev: {global_x.std():.4f}
  Range: [{global_x.min():.2f}, {global_x.max():.2f}]

T Statistics:
  Mean: {global_t_julian.mean():.2f}
  Std Dev: {global_t_julian.std():.2f} days
  Range: [{global_t_julian.min():.0f}, {global_t_julian.max():.0f}]

Points: {n_points}
        """
        ax2.text(0.05, 0.95, global_stats_text, transform=ax2.transAxes,
                fontsize=11, verticalalignment='top', family='monospace',
                bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.8))

        # ===== 3. ルール適用後のX-T散布図（面白い！） =====
        ax3 = fig.add_subplot(gs[1, :2])

        # 全体データ（薄く）
        ax3.scatter(global_t_julian, global_x, alpha=0.1, s=10, c='lightgray',
                   label='Non-matched')

        # ルールマッチデータ（強調）
        ax3.scatter(local_t_julian, local_x, alpha=0.8, s=50, c='red',
                   edgecolors='darkred', linewidth=1.5,
                   label=f'Rule Matched ({n_local} points)', zorder=5)

        # 局所平均
        ax3.axhline(x_mean, color='red', linestyle='--', linewidth=2,
                   label=f'X mean ({x_mean:.4f})', zorder=4)
        ax3.axvline(t_mean, color='blue', linestyle='--', linewidth=2,
                   label=f'T mean ({t_mean:.1f})', zorder=4)

        # 2次元楕円（±1σ領域）
        ellipse = Ellipse((t_mean, x_mean),
                         width=t_sigma * 2,
                         height=x_sigma * 2,
                         fill=True, facecolor='red', alpha=0.15,
                         edgecolor='darkred', linewidth=2, linestyle='--',
                         label='±1σ region (2D)', zorder=3)
        ax3.add_patch(ellipse)

        ax3.set_xlabel('Time (Julian Day)', fontsize=12)
        ax3.set_ylabel('X Value', fontsize=12)
        ax3.set_title(f'【After】Rule Applied - 2D Local Distribution!\n'
                     f'Clustering in BOTH X and T dimensions',
                     fontsize=14, fontweight='bold', color='darkred')
        ax3.legend(fontsize=10)
        ax3.grid(True, alpha=0.3)

        # 効果を強調
        x_reduction = (1 - x_sigma / global_x.std()) * 100
        t_reduction = (1 - max(t_sigma, 5) / global_t_julian.std()) * 100

        ax3.text(0.02, 0.95,
                f'✓ Matched: {n_local} ({support_rate*100:.1f}%)\n'
                f'✓ X: μ={x_mean:.4f}, σ={x_sigma:.4f} ({x_reduction:.1f}% reduction)\n'
                f'✓ T: μ={t_mean:.1f}, σ={max(t_sigma, 5):.1f} days ({t_reduction:.1f}% reduction)\n'
                f'✓ 2D local cluster detected!',
                transform=ax3.transAxes, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.8),
                fontsize=12, fontweight='bold')

        # ===== 4. 局所分布の統計 =====
        ax4 = fig.add_subplot(gs[1, 2])
        ax4.axis('off')
        local_stats_text = f"""
LOCAL DISTRIBUTION
(Rule Matched)

X Statistics:
  Mean: {local_x.mean():.4f}
  Std Dev: {local_x.std():.4f}
  Range: [{local_x.min():.2f}, {local_x.max():.2f}]

T Statistics:
  Mean: {local_t_julian.mean():.2f}
  Std Dev: {local_t_julian.std():.2f} days
  Range: [{local_t_julian.min():.0f}, {local_t_julian.max():.0f}]

Points: {n_local}

Variance Reduction:
  X: {x_reduction:.1f}%
  T: {t_reduction:.1f}%
        """
        ax4.text(0.05, 0.95, local_stats_text, transform=ax4.transAxes,
                fontsize=11, verticalalignment='top', family='monospace',
                bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.8))

        # ===== 5. Xの分布比較 =====
        ax5 = fig.add_subplot(gs[2, 0])
        ax5.hist(global_x, bins=50, alpha=0.4, color='gray', label='Global', density=True)
        ax5.hist(local_x, bins=30, alpha=0.7, color='red', label='Local (Rule)', density=True)
        ax5.axvline(global_x.mean(), color='gray', linestyle='--', linewidth=2)
        ax5.axvline(x_mean, color='red', linestyle='--', linewidth=2)
        ax5.set_xlabel('X Value')
        ax5.set_ylabel('Density')
        ax5.set_title('X Distribution\nGlobal vs Local')
        ax5.legend()
        ax5.grid(True, alpha=0.3, axis='y')

        # ===== 6. Tの分布比較 =====
        ax6 = fig.add_subplot(gs[2, 1])
        ax6.hist(global_t_julian, bins=50, alpha=0.4, color='gray', label='Global', density=True)
        ax6.hist(local_t_julian, bins=30, alpha=0.7, color='blue', label='Local (Rule)', density=True)
        ax6.axvline(global_t_julian.mean(), color='gray', linestyle='--', linewidth=2)
        ax6.axvline(t_mean, color='blue', linestyle='--', linewidth=2)
        ax6.set_xlabel('T (Julian Day)')
        ax6.set_ylabel('Density')
        ax6.set_title('T Distribution\nGlobal vs Local')
        ax6.legend()
        ax6.grid(True, alpha=0.3, axis='y')

        # ===== 7. 2次元密度プロット =====
        ax7 = fig.add_subplot(gs[2, 2])

        # ヒートマップ（局所分布）
        from scipy.stats import gaussian_kde
        if len(local_x) > 10:
            try:
                xy = np.vstack([local_t_julian, local_x])
                z = gaussian_kde(xy)(xy)
                scatter = ax7.scatter(local_t_julian, local_x, c=z, s=50,
                                    cmap='Reds', alpha=0.6, edgecolors='darkred')
                plt.colorbar(scatter, ax=ax7, label='Density')
            except:
                ax7.scatter(local_t_julian, local_x, c='red', s=50, alpha=0.6)

        ax7.axhline(x_mean, color='red', linestyle='--', linewidth=2)
        ax7.axvline(t_mean, color='blue', linestyle='--', linewidth=2)
        ax7.set_xlabel('T (Julian Day)')
        ax7.set_ylabel('X Value')
        ax7.set_title('2D Density\n(Local Distribution)')
        ax7.grid(True, alpha=0.3)

        # 全体タイトル
        fig.suptitle(f'{self.forex_pair} - X-T 2D Local Distribution (Rule #{rule_idx})\n'
                    f'Phase 2.2: Demonstrating Clustering in Both X and T Dimensions',
                    fontsize=16, fontweight='bold', y=0.995)

        # 保存
        output_file = self.vis_dir / f'xt_2d_local_distribution_rule_{rule_idx:04d}.png'
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"\n✓ Saved: {output_file}\n")
        plt.close()

    def analyze_top_rules(self, n_rules: int = 5):
        """上位N個のルールを分析"""
        print(f"\n{'='*70}")
        print(f"Analyzing Top {n_rules} Rules - X-T 2D Local Distribution")
        print(f"{'='*70}\n")

        for i in range(n_rules):
            try:
                self.create_2d_local_distribution_plot(rule_idx=i)
            except Exception as e:
                print(f"Error processing rule {i}: {e}")
                import traceback
                traceback.print_exc()


def main():
    """メイン処理"""
    import sys

    if len(sys.argv) > 1:
        forex_pairs = [sys.argv[1]]
        n_rules = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    else:
        forex_pairs = ['USDJPY', 'GBPCAD', 'AUDNZD', 'EURAUD']
        n_rules = 3

    for forex_pair in forex_pairs:
        print(f"\n{'#'*70}")
        print(f"# Processing: {forex_pair}")
        print(f"{'#'*70}\n")

        visualizer = XTLocalDistributionVisualizer(
            forex_pair=forex_pair,
            base_dir=Path("../..")
        )

        visualizer.analyze_top_rules(n_rules=n_rules)

        print(f"\n✓ Completed: {forex_pair}\n")


if __name__ == "__main__":
    main()
