#!/usr/bin/env python3
"""
ルールベース散布図作成スクリプト

発見されたルールに対して、条件に合致するデータポイントのX値分布を可視化します。

使用方法:
    python analysis/fx/rule_scatter_plot.py GBPAUD positive
    python analysis/fx/rule_scatter_plot.py GBPAUD positive --top 10
    python analysis/fx/rule_scatter_plot.py --all
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import re
import argparse
from pathlib import Path
import warnings
warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'Hiragino Sans', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False


class RuleScatterPlotter:
    """ルールベース散布図作成クラス"""

    def __init__(self, pair, direction):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: GBPAUD)
        direction : str
            方向 (positive/negative)
        """
        self.pair = pair
        self.direction = direction
        self.base_dir = Path("analysis/fx/visualizations")
        self.output_dir = self.base_dir / f"{pair}_{direction}"
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # ファイルパス
        self.rule_file = Path(f"output/{pair}/{direction}/pool/zrp01a.txt")
        self.data_file = Path(f"forex_data/extreme_gnminer/{pair}_{direction}.txt")

        print(f"\n{'='*80}")
        print(f"Rule-Based Scatter Plot Generator")
        print(f"{'='*80}")
        print(f"Pair:      {pair}")
        print(f"Direction: {direction}")
        print(f"Rule file: {self.rule_file}")
        print(f"Data file: {self.data_file}")
        print(f"Output:    {self.output_dir}")
        print(f"{'='*80}\n")

    def parse_rule_file(self, max_rules=None, sort_by='support'):
        """
        zrp01a.txtからルールを解析

        Parameters
        ----------
        max_rules : int, optional
            読み込む最大ルール数
        sort_by : str, optional
            ソート基準 ('support', 'extreme_score', 'snr', 'extremeness', 'discovery')
            デフォルト: 'support'

        Returns
        -------
        list of dict
            解析されたルールのリスト
        """
        print(f"Parsing rule file: {self.rule_file}")

        if not self.rule_file.exists():
            raise FileNotFoundError(f"Rule file not found: {self.rule_file}")

        df = pd.read_csv(self.rule_file, sep='\t')

        # ソート処理
        if sort_by == 'support':
            df = df.sort_values('support_count', ascending=False)
            print(f"✓ Sorted by support_count (descending)")
        elif sort_by == 'extreme_score':
            df = df.sort_values('ExtremeScore', ascending=False)
            print(f"✓ Sorted by ExtremeScore (descending)")
        elif sort_by == 'snr':
            df = df.sort_values('SNR', ascending=False)
            print(f"✓ Sorted by SNR (descending)")
        elif sort_by == 'extremeness':
            df = df.sort_values('Extremeness', ascending=False)
            print(f"✓ Sorted by Extremeness (descending)")
        elif sort_by == 'discovery':
            # 元の順序を保持
            print(f"✓ Using discovery order (file order)")
        else:
            print(f"Warning: Unknown sort_by '{sort_by}', using discovery order")

        if max_rules:
            df = df.head(max_rules)

        rules = []
        for idx, row in df.iterrows():
            # 属性列を抽出 (Attr1-Attr8)
            conditions = []
            rule_text_parts = []

            for i in range(1, 9):
                attr_col = f'Attr{i}'
                attr_value = row[attr_col]

                if attr_value == 0 or pd.isna(attr_value):
                    break

                # 属性名と遅延を解析
                # 例: "NZDJPY_Up(t-1)" → attr='NZDJPY_Up', delay=1
                match = re.match(r'(.+)\(t-(\d+)\)', str(attr_value))
                if match:
                    attr_name = match.group(1)
                    delay = int(match.group(2))
                    conditions.append({
                        'attr': attr_name,
                        'delay': delay
                    })
                    rule_text_parts.append(f"{attr_name}(t-{delay})")

            if not conditions:
                continue

            rule = {
                'rule_idx': idx,
                'conditions': conditions,
                'rule_text': ' AND '.join(rule_text_parts),
                'x_mean': row['X_mean'],
                'x_sigma': row['X_sigma'],
                'support_count': int(row['support_count']),
                'support_rate': row['support_rate'],
                'signal_strength': row.get('SignalStrength', 0),
                'SNR': row.get('SNR', 0),
                'extremeness': row.get('Extremeness', 0)
            }

            rules.append(rule)

        print(f"✓ Parsed {len(rules)} rules")
        return rules

    def load_data(self):
        """
        元データを読み込み

        Returns
        -------
        pd.DataFrame
            データフレーム
        """
        print(f"Loading data file: {self.data_file}")

        if not self.data_file.exists():
            raise FileNotFoundError(f"Data file not found: {self.data_file}")

        df = pd.read_csv(self.data_file)
        print(f"✓ Loaded {len(df)} records with {len(df.columns)} columns")
        return df

    def match_rule_to_data(self, data_df, rule):
        """
        ルール条件に合致するレコードのX値を抽出

        Parameters
        ----------
        data_df : pd.DataFrame
            元データ
        rule : dict
            ルール情報

        Returns
        -------
        tuple (np.ndarray, list)
            (マッチしたX値の配列, マッチした時点のインデックスリスト)
        """
        # 最大遅延を計算
        max_delay = max([c['delay'] for c in rule['conditions']])

        # 評価可能範囲
        start_idx = max_delay
        end_idx = len(data_df) - 1  # t+1を取得するため-1

        matched_X = []
        matched_indices = []

        # 各時点tを現在として評価
        for t in range(start_idx, end_idx):
            all_match = True

            # 全条件をチェック
            for condition in rule['conditions']:
                attr_name = condition['attr']
                delay = condition['delay']
                check_idx = t - delay

                # 属性が存在するか確認
                if attr_name not in data_df.columns:
                    print(f"  Warning: Attribute '{attr_name}' not found in data")
                    all_match = False
                    break

                # 条件チェック
                if data_df.loc[check_idx, attr_name] != 1:
                    all_match = False
                    break

            # 全条件一致 → t+1のXを取得
            if all_match:
                X_value = data_df.loc[t + 1, 'X']
                matched_X.append(X_value)
                matched_indices.append(t)

        return np.array(matched_X), matched_indices

    def create_scatter_plot(self, X_values, rule, output_path):
        """
        1次元散布図を作成

        Parameters
        ----------
        X_values : np.ndarray
            X値の配列
        rule : dict
            ルール情報
        output_path : Path
            出力ファイルパス
        """
        if len(X_values) == 0:
            print(f"  Skipping rule {rule['rule_idx']}: No matched records")
            return

        # 統計計算
        mean = np.mean(X_values)
        std = np.std(X_values)
        median = np.median(X_values)
        min_val = np.min(X_values)
        max_val = np.max(X_values)

        # 図の作成
        fig = plt.figure(figsize=(14, 8))
        gs = fig.add_gridspec(3, 2, height_ratios=[2, 1, 1], hspace=0.3, wspace=0.3)

        # 上段: 1次元散布図
        ax1 = fig.add_subplot(gs[0, :])

        # ジッターを追加して重なりを防ぐ
        y_jitter = np.random.randn(len(X_values)) * 0.05

        ax1.scatter(X_values, y_jitter, alpha=0.6, s=50, c='steelblue', edgecolors='navy', linewidth=0.5)
        ax1.axvline(x=mean, color='red', linewidth=2, label=f'Mean = {mean:.3f}', linestyle='--')
        ax1.axvline(x=median, color='orange', linewidth=2, label=f'Median = {median:.3f}', linestyle=':')
        ax1.axvspan(mean - std, mean + std, alpha=0.2, color='red', label=f'±1σ = {std:.3f}')

        ax1.set_xlabel('X (Change Rate %)', fontsize=12, fontweight='bold')
        ax1.set_ylabel('Random Jitter (for visibility)', fontsize=10)
        ax1.set_ylim(-0.3, 0.3)
        ax1.set_title(
            f"Rule {rule['rule_idx']}: {rule['rule_text']}\n"
            f"Matched: {len(X_values)} records | X_mean={rule['x_mean']:.3f} X_sigma={rule['x_sigma']:.3f}",
            fontsize=11, fontweight='bold', pad=15
        )
        ax1.legend(loc='upper right', fontsize=10)
        ax1.grid(True, alpha=0.3, axis='x')

        # 中段左: ヒストグラム
        ax2 = fig.add_subplot(gs[1, 0])
        n, bins, patches = ax2.hist(X_values, bins=20, alpha=0.7, color='steelblue', edgecolor='black')
        ax2.axvline(x=mean, color='red', linewidth=2, linestyle='--')
        ax2.axvline(x=median, color='orange', linewidth=2, linestyle=':')
        ax2.set_xlabel('X (Change Rate %)', fontsize=10)
        ax2.set_ylabel('Frequency', fontsize=10)
        ax2.set_title('Distribution Histogram', fontsize=10, fontweight='bold')
        ax2.grid(True, alpha=0.3, axis='y')

        # 中段右: 統計情報
        ax3 = fig.add_subplot(gs[1, 1])
        ax3.axis('off')

        stats_text = f"""
Statistics:
━━━━━━━━━━━━━━━━━━━━
Count:     {len(X_values)}
Mean:      {mean:.4f}
Median:    {median:.4f}
Std Dev:   {std:.4f}
Min:       {min_val:.4f}
Max:       {max_val:.4f}
Range:     {max_val - min_val:.4f}

Rule Metrics:
━━━━━━━━━━━━━━━━━━━━
Support:   {rule['support_count']} ({rule['support_rate']:.2%})
SNR:       {rule['SNR']:.3f}
Extremeness: {rule['extremeness']:.3f}
Signal:    {rule['signal_strength']:.3f}
"""
        ax3.text(0.1, 0.5, stats_text, fontsize=10, family='monospace',
                verticalalignment='center', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.3))

        # 下段: Box plot
        ax4 = fig.add_subplot(gs[2, :])
        bp = ax4.boxplot([X_values], vert=False, widths=0.5, patch_artist=True,
                         boxprops=dict(facecolor='lightblue', alpha=0.7),
                         medianprops=dict(color='red', linewidth=2),
                         whiskerprops=dict(linewidth=1.5),
                         capprops=dict(linewidth=1.5))
        ax4.set_xlabel('X (Change Rate %)', fontsize=10)
        ax4.set_title('Box Plot', fontsize=10, fontweight='bold')
        ax4.grid(True, alpha=0.3, axis='x')

        # 保存
        plt.savefig(output_path, dpi=100, bbox_inches='tight')
        plt.close()

        print(f"  ✓ Rule {rule['rule_idx']:4d}: {len(X_values):4d} records matched → {output_path.name}")

    def process_all_rules(self, max_rules=50, min_samples=5, sort_by='support'):
        """
        全ルールを処理

        Parameters
        ----------
        max_rules : int
            処理する最大ルール数
        min_samples : int
            可視化に必要な最小サンプル数
        sort_by : str
            ソート基準
        """
        # ルール解析
        rules = self.parse_rule_file(max_rules=max_rules, sort_by=sort_by)

        # データ読み込み
        data_df = self.load_data()

        print(f"\nProcessing {len(rules)} rules...")
        print(f"Minimum samples required: {min_samples}")
        print(f"{'='*80}\n")

        successful_count = 0
        skipped_count = 0

        # 各ルールを処理
        for rule in rules:
            # マッチング
            X_values, indices = self.match_rule_to_data(data_df, rule)

            # 最小サンプル数チェック
            if len(X_values) < min_samples:
                skipped_count += 1
                continue

            # 散布図作成
            output_path = self.output_dir / f"rule_{rule['rule_idx']:04d}_scatter.png"
            self.create_scatter_plot(X_values, rule, output_path)
            successful_count += 1

        print(f"\n{'='*80}")
        print(f"Processing Complete")
        print(f"{'='*80}")
        print(f"Total rules:       {len(rules)}")
        print(f"Plots created:     {successful_count}")
        print(f"Skipped (< {min_samples}):   {skipped_count}")
        print(f"Output directory:  {self.output_dir}")
        print(f"{'='*80}\n")


def main():
    parser = argparse.ArgumentParser(
        description='Create scatter plots for discovered rules',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Process single pair
  python analysis/fx/rule_scatter_plot.py GBPAUD positive

  # Process with top 20 rules only
  python analysis/fx/rule_scatter_plot.py GBPAUD positive --top 20

  # Process with minimum 10 samples
  python analysis/fx/rule_scatter_plot.py GBPAUD positive --min-samples 10
        """
    )

    parser.add_argument('pair', type=str, help='Currency pair (e.g., GBPAUD)')
    parser.add_argument('direction', type=str, choices=['positive', 'negative'],
                       help='Direction (positive/negative)')
    parser.add_argument('--top', type=int, default=50,
                       help='Number of top rules to process (default: 50)')
    parser.add_argument('--min-samples', type=int, default=5,
                       help='Minimum samples required for plotting (default: 5)')
    parser.add_argument('--sort-by', type=str, default='support',
                       choices=['support', 'extreme_score', 'snr', 'extremeness', 'discovery'],
                       help='Sort criterion (default: support)')

    args = parser.parse_args()

    # 処理実行
    plotter = RuleScatterPlotter(args.pair, args.direction)
    plotter.process_all_rules(max_rules=args.top, min_samples=args.min_samples, sort_by=args.sort_by)


if __name__ == '__main__':
    main()
