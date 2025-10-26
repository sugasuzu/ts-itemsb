#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
局所分布分析スクリプト - Local Distribution Analysis
============================================

目的:
一見普通に見えるX,Tの散布図において、特定のルール（条件）を適用すると
統計的に有意な局所分布（低分散）が現れることを示す。

GNPアルゴリズムがMaxsigx=10.0の制約下で発見したルールが、
全体分布と比較してどれだけ分散を減少させているかを可視化・定量化する。
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
from typing import List, Dict, Tuple
import warnings

warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial']
plt.rcParams['axes.unicode_minus'] = False

# スタイル設定
sns.set_style("whitegrid")
sns.set_context("talk")


class LocalDistributionAnalyzer:
    """局所分布分析クラス"""

    def __init__(self, forex_pair: str, base_dir: Path = Path(".")):
        """
        初期化

        Args:
            forex_pair: 通貨ペア名 (e.g., "USDJPY")
            base_dir: プロジェクトのベースディレクトリ
        """
        self.forex_pair = forex_pair
        self.base_dir = base_dir

        # パス設定
        self.data_file = base_dir / f"forex_data/gnminer_individual/{forex_pair}.txt"
        self.rule_pool_file = base_dir / f"output_{forex_pair}/pool/zrp01a.txt"
        self.output_dir = base_dir / f"analysis/fx/{forex_pair}"
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # データ保持用
        self.data_df = None
        self.rules_df = None
        self.attribute_names = []

    def load_data(self) -> bool:
        """データとルールプールを読み込む"""
        try:
            # データファイル読み込み
            print(f"Loading data: {self.data_file}")
            self.data_df = pd.read_csv(self.data_file, sep=',')

            # カラム名取得
            self.attribute_names = [col for col in self.data_df.columns
                                   if col not in ['T', 'X']]

            print(f"  Records: {len(self.data_df)}")
            print(f"  Attributes: {len(self.attribute_names)}")
            print(f"  X range: [{self.data_df['X'].min():.2f}, {self.data_df['X'].max():.2f}]")
            print(f"  X mean: {self.data_df['X'].mean():.4f}")
            print(f"  X std: {self.data_df['X'].std():.4f}")

            # ルールプール読み込み
            print(f"\nLoading rules: {self.rule_pool_file}")
            if not self.rule_pool_file.exists():
                print(f"  Warning: Rule pool not found")
                return False

            self.rules_df = pd.read_csv(self.rule_pool_file, sep='\t')
            print(f"  Total rules: {len(self.rules_df)}")

            return True

        except Exception as e:
            print(f"Error loading data: {e}")
            return False

    def extract_rule_conditions(self, rule_idx: int) -> Dict:
        """
        ルールの条件を抽出

        Args:
            rule_idx: ルールのインデックス

        Returns:
            条件の辞書 {attribute_name: (value, delay), ...}
        """
        rule = self.rules_df.iloc[rule_idx]
        conditions = {}

        # Attr1-8カラムからルール条件を抽出
        for i in range(1, 9):
            attr_col = f'Attr{i}'
            if attr_col in rule and pd.notna(rule[attr_col]) and rule[attr_col] != '0':
                attr_str = str(rule[attr_col])
                if attr_str and attr_str != '0':
                    conditions[attr_col] = attr_str

        return conditions

    def apply_rule_filter(self, rule_idx: int) -> pd.DataFrame:
        """
        ルールの条件を満たすデータポイントを抽出

        Args:
            rule_idx: ルールのインデックス

        Returns:
            条件を満たすデータのDataFrame
        """
        conditions = self.extract_rule_conditions(rule_idx)

        # データのコピー
        filtered_df = self.data_df.copy()

        # 各条件を適用
        for attr_col, attr_value in conditions.items():
            # 属性名と時間遅延を解析
            # 例: "volume_high(t-2)" -> attribute="volume_high", delay=2
            parts = attr_value.split('(t-')
            if len(parts) == 2:
                attr_name = parts[0]
                delay = int(parts[1].rstrip(')'))

                # 属性名がカラムに存在する場合
                if attr_name in self.data_df.columns:
                    # 時間遅延を考慮してフィルタリング
                    # t-delayの値が1であるレコードを抽出
                    mask = filtered_df[attr_name].shift(delay) == 1
                    filtered_df = filtered_df[mask]

        return filtered_df

    def calculate_statistics(self, df: pd.DataFrame) -> Dict:
        """
        統計量を計算

        Args:
            df: データフレーム

        Returns:
            統計量の辞書
        """
        if len(df) == 0:
            return {
                'count': 0,
                'mean': np.nan,
                'std': np.nan,
                'min': np.nan,
                'max': np.nan,
                'q25': np.nan,
                'q50': np.nan,
                'q75': np.nan
            }

        return {
            'count': len(df),
            'mean': df['X'].mean(),
            'std': df['X'].std(),
            'min': df['X'].min(),
            'max': df['X'].max(),
            'q25': df['X'].quantile(0.25),
            'q50': df['X'].quantile(0.50),
            'q75': df['X'].quantile(0.75)
        }

    def variance_reduction_ratio(self, global_std: float, local_std: float) -> float:
        """
        分散減少率を計算

        Args:
            global_std: 全体の標準偏差
            local_std: 局所的な標準偏差

        Returns:
            減少率 (0-1, 1に近いほど大きな減少)
        """
        if global_std == 0:
            return 0
        return 1 - (local_std / global_std)

    def statistical_significance(self, global_data: pd.Series,
                                local_data: pd.Series) -> Dict:
        """
        統計的有意性を検定

        Args:
            global_data: 全体のデータ
            local_data: 局所的なデータ

        Returns:
            検定結果の辞書
        """
        if len(local_data) < 30:
            return {
                'test': 'insufficient_data',
                'p_value': np.nan,
                'significant': False
            }

        # Leveneの等分散性検定
        levene_stat, levene_p = stats.levene(global_data, local_data)

        # t検定（平均の差）
        t_stat, t_p = stats.ttest_ind(global_data, local_data, equal_var=False)

        # F検定（分散の比）
        f_stat = global_data.var() / local_data.var() if local_data.var() > 0 else np.inf

        return {
            'levene_stat': levene_stat,
            'levene_p': levene_p,
            't_stat': t_stat,
            't_p': t_p,
            'f_stat': f_stat,
            'variance_ratio': f_stat,
            'significant_variance_reduction': levene_p < 0.05 and f_stat > 1.5
        }

    def plot_comparison(self, rule_idx: int, save_path: Path = None):
        """
        全体分布と局所分布の比較プロット

        Args:
            rule_idx: ルールのインデックス
            save_path: 保存先パス
        """
        # ルール適用
        filtered_df = self.apply_rule_filter(rule_idx)

        if len(filtered_df) == 0:
            print(f"  Warning: No data points match rule {rule_idx}")
            return

        # 統計量計算
        global_stats = self.calculate_statistics(self.data_df)
        local_stats = self.calculate_statistics(filtered_df)

        # 統計的有意性
        sig_test = self.statistical_significance(
            self.data_df['X'], filtered_df['X']
        )

        # 分散減少率
        var_reduction = self.variance_reduction_ratio(
            global_stats['std'], local_stats['std']
        )

        # ルール情報
        rule = self.rules_df.iloc[rule_idx]

        # プロット作成
        fig = plt.figure(figsize=(20, 12))
        gs = fig.add_gridspec(3, 3, hspace=0.3, wspace=0.3)

        # ===== 1. 全体のX,T散布図 =====
        ax1 = fig.add_subplot(gs[0, 0])
        ax1.scatter(self.data_df.index, self.data_df['X'],
                   alpha=0.3, s=10, c='gray', label='All data')
        ax1.set_xlabel('Time Index (T)')
        ax1.set_ylabel('X Value')
        ax1.set_title(f'Overall Distribution\n(σ={global_stats["std"]:.4f})')
        ax1.legend()
        ax1.grid(True, alpha=0.3)

        # ===== 2. ルール適用後のX,T散布図 =====
        ax2 = fig.add_subplot(gs[0, 1])
        ax2.scatter(self.data_df.index, self.data_df['X'],
                   alpha=0.1, s=10, c='lightgray', label='All data')
        ax2.scatter(filtered_df.index, filtered_df['X'],
                   alpha=0.7, s=30, c='red', label='Rule matched', edgecolors='darkred')
        ax2.axhline(local_stats['mean'], color='red', linestyle='--',
                   linewidth=2, label=f'Local mean={local_stats["mean"]:.4f}')
        ax2.axhline(local_stats['mean'] + local_stats['std'],
                   color='red', linestyle=':', alpha=0.5)
        ax2.axhline(local_stats['mean'] - local_stats['std'],
                   color='red', linestyle=':', alpha=0.5)
        ax2.set_xlabel('Time Index (T)')
        ax2.set_ylabel('X Value')
        ax2.set_title(f'Rule Applied (Local Distribution)\n(σ={local_stats["std"]:.4f}, n={local_stats["count"]})')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        # ===== 3. 重ね合わせ（ズーム） =====
        ax3 = fig.add_subplot(gs[0, 2])
        if len(filtered_df) > 0:
            # 局所データ周辺にズーム
            y_center = local_stats['mean']
            y_range = max(global_stats['std'] * 2, local_stats['std'] * 4)

            # 全体データ（薄く）
            mask_zoom = (self.data_df['X'] >= y_center - y_range) & \
                       (self.data_df['X'] <= y_center + y_range)
            ax3.scatter(self.data_df[mask_zoom].index,
                       self.data_df[mask_zoom]['X'],
                       alpha=0.2, s=10, c='lightgray')

            # 局所データ（濃く）
            ax3.scatter(filtered_df.index, filtered_df['X'],
                       alpha=0.8, s=50, c='red', edgecolors='darkred', linewidth=1.5)
            ax3.axhline(local_stats['mean'], color='red', linestyle='--', linewidth=2)
            ax3.fill_between([filtered_df.index.min(), filtered_df.index.max()],
                           local_stats['mean'] - local_stats['std'],
                           local_stats['mean'] + local_stats['std'],
                           color='red', alpha=0.2, label='±1σ (local)')

        ax3.set_xlabel('Time Index (T)')
        ax3.set_ylabel('X Value')
        ax3.set_title('Zoomed View (Local Region)')
        ax3.legend()
        ax3.grid(True, alpha=0.3)

        # ===== 4. X分布のヒストグラム比較 =====
        ax4 = fig.add_subplot(gs[1, 0])
        ax4.hist(self.data_df['X'], bins=50, alpha=0.5,
                color='gray', label='Global', density=True)
        if len(filtered_df) > 0:
            ax4.hist(filtered_df['X'], bins=30, alpha=0.7,
                    color='red', label='Local (Rule applied)', density=True)
        ax4.axvline(global_stats['mean'], color='gray',
                   linestyle='--', linewidth=2, label='Global mean')
        ax4.axvline(local_stats['mean'], color='red',
                   linestyle='--', linewidth=2, label='Local mean')
        ax4.set_xlabel('X Value')
        ax4.set_ylabel('Density')
        ax4.set_title('Distribution Comparison')
        ax4.legend()
        ax4.grid(True, alpha=0.3)

        # ===== 5. 箱ひげ図比較 =====
        ax5 = fig.add_subplot(gs[1, 1])
        box_data = [self.data_df['X'], filtered_df['X']]
        bp = ax5.boxplot(box_data, labels=['Global', 'Local (Rule)'],
                        patch_artist=True, widths=0.6)
        bp['boxes'][0].set_facecolor('lightgray')
        bp['boxes'][1].set_facecolor('red')
        bp['boxes'][1].set_alpha(0.7)
        ax5.set_ylabel('X Value')
        ax5.set_title('Box Plot Comparison')
        ax5.grid(True, alpha=0.3, axis='y')

        # 統計値をテキストで表示
        y_pos = ax5.get_ylim()[1]
        ax5.text(1, y_pos * 0.95, f'σ_global = {global_stats["std"]:.4f}',
                ha='center', fontsize=10, bbox=dict(boxstyle='round', facecolor='wheat'))
        ax5.text(2, y_pos * 0.95, f'σ_local = {local_stats["std"]:.4f}',
                ha='center', fontsize=10, bbox=dict(boxstyle='round', facecolor='lightcoral'))

        # ===== 6. 分散減少の可視化 =====
        ax6 = fig.add_subplot(gs[1, 2])
        categories = ['Global\nVariance', 'Local\nVariance']
        variances = [global_stats['std']**2, local_stats['std']**2]
        colors_bar = ['gray', 'red']
        bars = ax6.bar(categories, variances, color=colors_bar, alpha=0.7, edgecolor='black')
        ax6.set_ylabel('Variance (σ²)')
        ax6.set_title(f'Variance Reduction\n({var_reduction*100:.1f}% reduction)')
        ax6.grid(True, alpha=0.3, axis='y')

        # 値をバーに表示
        for bar, var in zip(bars, variances):
            height = bar.get_height()
            ax6.text(bar.get_x() + bar.get_width()/2., height,
                    f'{var:.4f}',
                    ha='center', va='bottom', fontsize=11, fontweight='bold')

        # ===== 7. 統計サマリーテーブル =====
        ax7 = fig.add_subplot(gs[2, :2])
        ax7.axis('tight')
        ax7.axis('off')

        summary_data = [
            ['Metric', 'Global', 'Local (Rule)', 'Ratio/Diff'],
            ['Count', f"{global_stats['count']}",
             f"{local_stats['count']}",
             f"{local_stats['count']/global_stats['count']*100:.1f}%"],
            ['Mean (μ)', f"{global_stats['mean']:.4f}",
             f"{local_stats['mean']:.4f}",
             f"{abs(global_stats['mean']-local_stats['mean']):.4f}"],
            ['Std (σ)', f"{global_stats['std']:.4f}",
             f"{local_stats['std']:.4f}",
             f"{var_reduction*100:.1f}% ↓"],
            ['Variance (σ²)', f"{global_stats['std']**2:.6f}",
             f"{local_stats['std']**2:.6f}",
             f"{sig_test.get('variance_ratio', 0):.2f}x"],
            ['Min', f"{global_stats['min']:.4f}",
             f"{local_stats['min']:.4f}", '-'],
            ['Max', f"{global_stats['max']:.4f}",
             f"{local_stats['max']:.4f}", '-'],
            ['Range', f"{global_stats['max']-global_stats['min']:.4f}",
             f"{local_stats['max']-local_stats['min']:.4f}", '-'],
        ]

        table = ax7.table(cellText=summary_data, cellLoc='center',
                         loc='center', colWidths=[0.25, 0.25, 0.25, 0.25])
        table.auto_set_font_size(False)
        table.set_fontsize(10)
        table.scale(1, 2)

        # ヘッダー行のスタイル
        for i in range(4):
            table[(0, i)].set_facecolor('#40466e')
            table[(0, i)].set_text_props(weight='bold', color='white')

        # データ行のスタイル（交互に色付け）
        for i in range(1, len(summary_data)):
            for j in range(4):
                if i % 2 == 0:
                    table[(i, j)].set_facecolor('#f0f0f0')

        ax7.set_title('Statistical Summary', fontsize=14, fontweight='bold', pad=20)

        # ===== 8. ルール情報と統計検定結果 =====
        ax8 = fig.add_subplot(gs[2, 2])
        ax8.axis('off')

        # ルール条件を取得
        conditions = self.extract_rule_conditions(rule_idx)
        condition_text = '\n'.join([f"  • {cond}" for cond in conditions.values()])
        if not condition_text:
            condition_text = '  (No conditions)'

        info_text = f"""
RULE #{rule_idx}

Conditions:
{condition_text}

Statistical Test:
  Variance Ratio: {sig_test.get('variance_ratio', 0):.2f}x
  Levene's test p: {sig_test.get('levene_p', np.nan):.4f}
  t-test p: {sig_test.get('t_p', np.nan):.4f}

  Significant: {"✓ YES" if sig_test.get('significant_variance_reduction', False) else "✗ NO"}

Quality Metrics:
  Support: {rule.get('Support', 0):.4f}
  X_mean: {rule.get('X_mean', 0):.4f}
  X_sigma: {rule.get('X_sigma', 0):.4f}
  Actual_X_mean: {rule.get('Actual_X_mean', 0):.4f}
        """

        ax8.text(0.05, 0.95, info_text, transform=ax8.transAxes,
                fontsize=10, verticalalignment='top', family='monospace',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

        # 全体タイトル
        fig.suptitle(f'{self.forex_pair} - Local Distribution Analysis (Rule #{rule_idx})\n'
                    f'Demonstrating Variance Reduction: {var_reduction*100:.1f}%',
                    fontsize=16, fontweight='bold', y=0.98)

        # 保存
        if save_path is None:
            save_path = self.output_dir / f'local_distribution_rule_{rule_idx:04d}.png'

        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"  Saved: {save_path}")
        plt.close()

        return {
            'rule_idx': rule_idx,
            'global_stats': global_stats,
            'local_stats': local_stats,
            'variance_reduction': var_reduction,
            'statistical_test': sig_test
        }

    def analyze_top_rules(self, n_rules: int = 10):
        """
        上位N個のルールを分析

        Args:
            n_rules: 分析するルール数
        """
        print(f"\n{'='*60}")
        print(f"Analyzing Top {n_rules} Rules - Local Distribution")
        print(f"{'='*60}\n")

        results = []

        for i in range(min(n_rules, len(self.rules_df))):
            print(f"\n[{i+1}/{n_rules}] Rule #{i}")
            try:
                result = self.plot_comparison(i)
                if result:
                    results.append(result)
                    print(f"  ✓ Variance reduction: {result['variance_reduction']*100:.1f}%")
                    print(f"  ✓ Local std: {result['local_stats']['std']:.4f}")
                    print(f"  ✓ Sample size: {result['local_stats']['count']}")
            except Exception as e:
                print(f"  ✗ Error: {e}")

        # 結果サマリーを保存
        if results:
            summary_df = pd.DataFrame([
                {
                    'rule_idx': r['rule_idx'],
                    'global_mean': r['global_stats']['mean'],
                    'global_std': r['global_stats']['std'],
                    'local_mean': r['local_stats']['mean'],
                    'local_std': r['local_stats']['std'],
                    'local_count': r['local_stats']['count'],
                    'variance_reduction_%': r['variance_reduction'] * 100,
                    'variance_ratio': r['statistical_test'].get('variance_ratio', 0),
                    'levene_p': r['statistical_test'].get('levene_p', np.nan),
                    'significant': r['statistical_test'].get('significant_variance_reduction', False)
                }
                for r in results
            ])

            summary_file = self.output_dir / 'local_distribution_summary.csv'
            summary_df.to_csv(summary_file, index=False)
            print(f"\n✓ Summary saved: {summary_file}")

            # 最も分散減少が大きいルールを表示
            top_reduction = summary_df.nlargest(5, 'variance_reduction_%')
            print(f"\n{'='*60}")
            print("Top 5 Rules by Variance Reduction:")
            print(f"{'='*60}")
            print(top_reduction[['rule_idx', 'local_std', 'variance_reduction_%',
                               'local_count', 'significant']].to_string(index=False))

    def create_summary_visualization(self):
        """全ルールのサマリー可視化"""
        summary_file = self.output_dir / 'local_distribution_summary.csv'
        if not summary_file.exists():
            print("No summary file found. Run analyze_top_rules first.")
            return

        df = pd.read_csv(summary_file)

        fig, axes = plt.subplots(2, 2, figsize=(16, 12))

        # 1. 分散減少率ランキング
        ax1 = axes[0, 0]
        df_sorted = df.sort_values('variance_reduction_%', ascending=False).head(15)
        colors = ['red' if sig else 'gray' for sig in df_sorted['significant']]
        ax1.barh(df_sorted['rule_idx'].astype(str), df_sorted['variance_reduction_%'],
                color=colors, alpha=0.7)
        ax1.set_xlabel('Variance Reduction (%)')
        ax1.set_ylabel('Rule Index')
        ax1.set_title('Top Rules by Variance Reduction\n(Red = Statistically Significant)')
        ax1.grid(True, alpha=0.3, axis='x')

        # 2. 局所標準偏差 vs サンプル数
        ax2 = axes[0, 1]
        scatter = ax2.scatter(df['local_count'], df['local_std'],
                            c=df['variance_reduction_%'], cmap='RdYlGn_r',
                            s=100, alpha=0.7, edgecolors='black')
        ax2.set_xlabel('Sample Size (Local)')
        ax2.set_ylabel('Standard Deviation (Local)')
        ax2.set_title('Local Std vs Sample Size\n(Color = Variance Reduction %)')
        ax2.grid(True, alpha=0.3)
        plt.colorbar(scatter, ax=ax2, label='Var. Reduction %')

        # 3. 分散比の分布
        ax3 = axes[1, 0]
        ax3.hist(df['variance_ratio'], bins=20, color='blue', alpha=0.7, edgecolor='black')
        ax3.axvline(1.0, color='red', linestyle='--', linewidth=2,
                   label='No reduction (ratio=1)')
        ax3.set_xlabel('Variance Ratio (Global / Local)')
        ax3.set_ylabel('Frequency')
        ax3.set_title('Distribution of Variance Ratios\n(Higher = Better variance reduction)')
        ax3.legend()
        ax3.grid(True, alpha=0.3)

        # 4. 統計的有意性
        ax4 = axes[1, 1]
        sig_counts = df['significant'].value_counts()
        ax4.pie(sig_counts.values, labels=['Not Significant', 'Significant'],
               autopct='%1.1f%%', colors=['lightgray', 'red'], startangle=90)
        ax4.set_title(f'Statistical Significance\n({sig_counts.get(True, 0)}/{len(df)} rules significant)')

        plt.tight_layout()
        save_path = self.output_dir / 'local_distribution_summary_viz.png'
        plt.savefig(save_path, dpi=150, bbox_inches='tight')
        print(f"\n✓ Summary visualization saved: {save_path}")
        plt.close()


def main():
    """メイン処理"""
    import sys

    # コマンドライン引数から通貨ペアを取得
    if len(sys.argv) > 1:
        forex_pairs = [sys.argv[1]]
    else:
        # デフォルト: 分析済みの通貨ペアを検索
        forex_pairs = ['USDJPY', 'GBPCAD', 'AUDNZD', 'EURAUD']

    for forex_pair in forex_pairs:
        print(f"\n{'#'*70}")
        print(f"# Analyzing: {forex_pair}")
        print(f"{'#'*70}\n")

        # 分析器を初期化
        analyzer = LocalDistributionAnalyzer(
            forex_pair=forex_pair,
            base_dir=Path("../..")  # analysis/fx/ から見た相対パス
        )

        # データ読み込み
        if not analyzer.load_data():
            print(f"Skipping {forex_pair} (data not available)")
            continue

        # 上位10ルールを分析
        analyzer.analyze_top_rules(n_rules=10)

        # サマリー可視化
        analyzer.create_summary_visualization()

        print(f"\n✓ Completed analysis for {forex_pair}\n")


if __name__ == "__main__":
    main()
