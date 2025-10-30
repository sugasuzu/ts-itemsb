#!/usr/bin/env python3
"""
ルールベースX-T散布図作成スクリプト

全体分布と各ルールの局所分布を重ねて可視化し、
ルール適用による局所化効果を示します。

使用方法:
    python analysis/fx/rule_scatter_plot_xt.py GBPAUD positive
    python analysis/fx/rule_scatter_plot_xt.py GBPAUD positive --top 10
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import re
import argparse
from pathlib import Path
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'Hiragino Sans', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False


class RuleScatterPlotterXT:
    """ルールベースX-T散布図作成クラス"""

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
        self.base_dir = Path("analysis/fx/visualizations_xt")
        self.output_dir = self.base_dir / f"{pair}_{direction}"
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # ファイルパス
        self.rule_file = Path(f"output/{pair}/{direction}/pool/zrp01a.txt")
        self.extreme_data_file = Path(f"forex_data/extreme_gnminer/{pair}_{direction}.txt")
        self.full_data_file = Path(f"forex_data/gnminer_individual/{pair}.txt")

        print(f"\n{'='*80}")
        print(f"Rule-Based X-T Scatter Plot Generator")
        print(f"{'='*80}")
        print(f"Pair:          {pair}")
        print(f"Direction:     {direction}")
        print(f"Rule file:     {self.rule_file}")
        print(f"Extreme data:  {self.extreme_data_file}")
        print(f"Full data:     {self.full_data_file}")
        print(f"Output:        {self.output_dir}")
        print(f"{'='*80}\n")

    def parse_rule_file(self, max_rules=None, sort_by='support'):
        """
        zrp01a.txtからルールを解析

        Parameters
        ----------
        max_rules : int, optional
            読み込む最大ルール数
        sort_by : str, optional
            ソート基準

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
        elif sort_by == 'discovery':
            print(f"✓ Using discovery order (file order)")

        if max_rules:
            df = df.head(max_rules)

        rules = []
        for idx, row in df.iterrows():
            conditions = []
            rule_text_parts = []

            for i in range(1, 9):
                attr_col = f'Attr{i}'
                attr_value = row[attr_col]

                if attr_value == 0 or pd.isna(attr_value):
                    break

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

    def load_full_data(self):
        """
        全データを読み込み（gnminer_individual）

        Returns
        -------
        pd.DataFrame
            全データフレーム
        """
        print(f"Loading full data: {self.full_data_file}")

        if not self.full_data_file.exists():
            raise FileNotFoundError(f"Full data file not found: {self.full_data_file}")

        df = pd.read_csv(self.full_data_file)

        # 日付をdatetime型に変換
        df['T_datetime'] = pd.to_datetime(df['T'])

        print(f"✓ Loaded {len(df)} records (full dataset)")
        print(f"  Date range: {df['T'].min()} to {df['T'].max()}")
        print(f"  X range: {df['X'].min():.3f} to {df['X'].max():.3f}")

        return df

    def load_extreme_data(self):
        """
        極値データを読み込み

        Returns
        -------
        pd.DataFrame
            極値データフレーム
        """
        print(f"Loading extreme data: {self.extreme_data_file}")

        if not self.extreme_data_file.exists():
            raise FileNotFoundError(f"Extreme data file not found: {self.extreme_data_file}")

        df = pd.read_csv(self.extreme_data_file)

        # 日付をdatetime型に変換
        df['T_datetime'] = pd.to_datetime(df['T'])

        print(f"✓ Loaded {len(df)} records (extreme dataset)")
        print(f"  Date range: {df['T'].min()} to {df['T'].max()}")
        print(f"  X range: {df['X'].min():.3f} to {df['X'].max():.3f}")

        return df

    def match_rule_to_data(self, data_df, rule):
        """
        ルール条件に合致するレコードのX値とT値を抽出

        Parameters
        ----------
        data_df : pd.DataFrame
            元データ
        rule : dict
            ルール情報

        Returns
        -------
        pd.DataFrame
            マッチしたレコード (X, T, T_datetime列を含む)
        """
        max_delay = max([c['delay'] for c in rule['conditions']])
        start_idx = max_delay
        end_idx = len(data_df) - 1

        matched_indices = []

        for t in range(start_idx, end_idx):
            all_match = True

            for condition in rule['conditions']:
                attr_name = condition['attr']
                delay = condition['delay']
                check_idx = t - delay

                if attr_name not in data_df.columns:
                    all_match = False
                    break

                if data_df.loc[check_idx, attr_name] != 1:
                    all_match = False
                    break

            if all_match:
                # t+1のレコードを取得
                matched_indices.append(t + 1)

        if len(matched_indices) == 0:
            return pd.DataFrame(columns=['X', 'T', 'T_datetime'])

        # マッチしたレコードを返す
        matched_df = data_df.loc[matched_indices, ['X', 'T', 'T_datetime']].copy()
        matched_df = matched_df.reset_index(drop=True)

        return matched_df

    def create_xt_scatter_plot(self, full_df, extreme_df, matched_df, rule, output_path):
        """
        X-T散布図を作成（全体+極値+ルール適用）

        Parameters
        ----------
        full_df : pd.DataFrame
            全データ
        extreme_df : pd.DataFrame
            極値データ
        matched_df : pd.DataFrame
            ルール適用後のデータ
        rule : dict
            ルール情報
        output_path : Path
            出力ファイルパス
        """
        if len(matched_df) == 0:
            print(f"  Skipping rule {rule['rule_idx']}: No matched records")
            return

        # 統計計算
        mean_X = matched_df['X'].mean()
        std_X = matched_df['X'].std()
        min_X = matched_df['X'].min()
        max_X = matched_df['X'].max()

        # 図の作成
        fig, ax = plt.subplots(figsize=(16, 10))

        # 1. 全体データ（最も薄い灰色、小さい点）
        ax.scatter(full_df['T_datetime'], full_df['X'],
                  alpha=0.15, s=10, c='lightgray',
                  label=f'All data (n={len(full_df)})', zorder=1)

        # 2. 極値データ（中間の灰色、中サイズ）
        ax.scatter(extreme_df['T_datetime'], extreme_df['X'],
                  alpha=0.3, s=30, c='gray',
                  label=f'Extreme data (n={len(extreme_df)}, |X|≥1.0)', zorder=2)

        # 3. ルール適用データ（赤色、大きい点）
        scatter = ax.scatter(matched_df['T_datetime'], matched_df['X'],
                           alpha=0.8, s=80, c='red', edgecolors='darkred', linewidth=1,
                           label=f'Rule matched (n={len(matched_df)})', zorder=3)

        # 平均値の水平線
        ax.axhline(y=mean_X, color='red', linewidth=2, linestyle='--',
                  label=f'Mean = {mean_X:.3f}', zorder=4)

        # ±1σ範囲
        ax.axhspan(mean_X - std_X, mean_X + std_X, alpha=0.2, color='red',
                  label=f'±1σ = {std_X:.3f}', zorder=0)

        # X軸（時間）のフォーマット
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y'))
        ax.xaxis.set_major_locator(mdates.YearLocator(2))
        plt.setp(ax.xaxis.get_majorticklabels(), rotation=45, ha='right')

        # ラベルとタイトル
        ax.set_xlabel('Time (Year)', fontsize=14, fontweight='bold')
        ax.set_ylabel('X (Change Rate %)', fontsize=14, fontweight='bold')

        title_text = (
            f"Rule {rule['rule_idx']}: {rule['rule_text']}\n"
            f"Support: {rule['support_count']} ({rule['support_rate']:.2%}) | "
            f"X_mean={rule['x_mean']:.3f} X_sigma={rule['x_sigma']:.3f} | "
            f"SNR={rule['SNR']:.2f}"
        )
        ax.set_title(title_text, fontsize=12, fontweight='bold', pad=20)

        # 凡例
        ax.legend(loc='upper left', fontsize=11, framealpha=0.9)

        # グリッド
        ax.grid(True, alpha=0.3, linestyle='--')

        # 統計情報ボックス
        stats_text = (
            f"Localization Effect:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"All data:     {len(full_df):5d} records\n"
            f"Extreme:      {len(extreme_df):5d} records ({100*len(extreme_df)/len(full_df):.1f}%)\n"
            f"Rule matched: {len(matched_df):5d} records ({100*len(matched_df)/len(full_df):.1f}%)\n"
            f"\n"
            f"Matched Statistics:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Mean:   {mean_X:7.3f}\n"
            f"Std:    {std_X:7.3f}\n"
            f"Min:    {min_X:7.3f}\n"
            f"Max:    {max_X:7.3f}\n"
            f"Range:  {max_X - min_X:7.3f}"
        )

        ax.text(0.98, 0.98, stats_text, transform=ax.transAxes,
               fontsize=10, family='monospace',
               verticalalignment='top', horizontalalignment='right',
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

        # 保存
        plt.tight_layout()
        plt.savefig(output_path, dpi=100, bbox_inches='tight')
        plt.close()

        print(f"  ✓ Rule {rule['rule_idx']:4d}: {len(matched_df):4d} records matched → {output_path.name}")

    def process_all_rules(self, max_rules=10, min_samples=5, sort_by='support'):
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
        full_df = self.load_full_data()
        extreme_df = self.load_extreme_data()

        print(f"\nProcessing {len(rules)} rules...")
        print(f"Minimum samples required: {min_samples}")
        print(f"{'='*80}\n")

        successful_count = 0
        skipped_count = 0

        # 各ルールを処理
        for rule in rules:
            # マッチング（極値データに対して）
            matched_df = self.match_rule_to_data(extreme_df, rule)

            # 最小サンプル数チェック
            if len(matched_df) < min_samples:
                skipped_count += 1
                continue

            # X-T散布図作成
            output_path = self.output_dir / f"rule_{rule['rule_idx']:04d}_xt_scatter.png"
            self.create_xt_scatter_plot(full_df, extreme_df, matched_df, rule, output_path)
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
        description='Create X-T scatter plots for discovered rules',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Process with default settings (support-based, top 10)
  python analysis/fx/rule_scatter_plot_xt.py GBPAUD positive

  # Process top 20 rules by SNR
  python analysis/fx/rule_scatter_plot_xt.py GBPAUD positive --top 20 --sort-by snr

  # Process with minimum 10 samples
  python analysis/fx/rule_scatter_plot_xt.py USDJPY negative --top 15 --min-samples 10
        """
    )

    parser.add_argument('pair', type=str, help='Currency pair (e.g., GBPAUD)')
    parser.add_argument('direction', type=str, choices=['positive', 'negative'],
                       help='Direction (positive/negative)')
    parser.add_argument('--top', type=int, default=10,
                       help='Number of top rules to process (default: 10)')
    parser.add_argument('--min-samples', type=int, default=5,
                       help='Minimum samples required for plotting (default: 5)')
    parser.add_argument('--sort-by', type=str, default='support',
                       choices=['support', 'extreme_score', 'snr', 'extremeness', 'discovery'],
                       help='Sort criterion (default: support)')

    args = parser.parse_args()

    # 処理実行
    plotter = RuleScatterPlotterXT(args.pair, args.direction)
    plotter.process_all_rules(max_rules=args.top, min_samples=args.min_samples, sort_by=args.sort_by)


if __name__ == '__main__':
    main()
