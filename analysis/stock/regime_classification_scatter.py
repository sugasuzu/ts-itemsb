#!/usr/bin/env python3
"""
X-T散布図作成スクリプト with T_interval_std（株価データ版）

既存のX-T散布図に時間的規則性（T_interval_std）の情報を追加して可視化します。

使用方法:
    python analysis/stock/regime_classification_scatter.py
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import re
from pathlib import Path
import warnings
warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'Hiragino Sans', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False


class ActualDataScatterPlotterWithInterval:
    """X-T散布図 + 時間間隔情報の可視化クラス（株価版）"""

    def __init__(self, stock_code):
        """
        Parameters
        ----------
        stock_code : str
            銘柄コード (例: 7203)
        """
        self.stock_code = stock_code
        self.base_dir = Path("analysis/stock/visualizations_regime")
        self.output_dir = self.base_dir / stock_code
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # ファイルパス
        self.rule_file = Path(f"output/{stock_code}/pool/zrp01a.txt")
        self.full_data_file = Path(f"nikkei225_data/gnminer_individual/{stock_code}.txt")

        print(f"\n{'='*80}")
        print(f"Rule-Based X-T Scatter Plot with T_interval_std (Stock Data)")
        print(f"{'='*80}")
        print(f"Stock Code:    {stock_code}")
        print(f"Rule file:     {self.rule_file}")
        print(f"Full data:     {self.full_data_file}")
        print(f"Output:        {self.output_dir}")
        print(f"{'='*80}\n")

    def parse_rule_file(self, max_rules=None, sort_by='composite'):
        """
        zrp01a.txtからルールを解析

        Parameters
        ----------
        max_rules : int, optional
            読み込む最大ルール数
        sort_by : str, optional
            ソート基準 ('composite', 'support', 'discovery')

        Returns
        -------
        list of dict
            解析されたルールのリスト
        """
        print(f"Parsing rule file: {self.rule_file}")

        if not self.rule_file.exists():
            raise FileNotFoundError(f"Rule file not found: {self.rule_file}")

        df = pd.read_csv(self.rule_file, sep='\t')

        # 複合スコアを計算（全ルールに対して）
        # スコア = support_rate / (X_sigma × T_interval_sigma)
        # support_rate大きい、X_sigmaとT_interval_sigma小さい → 高スコア
        # 「頻繁に起こり（割合）、かつ安定的なルール」を優先

        # T_interval_stdをT_interval_sigmaとして扱う（名称統一）
        df['T_interval_sigma'] = df['T_interval_std']

        # 複合スコア計算（epsilon=0.01で0除算を防止）
        epsilon = 0.01
        df['composite_score'] = df['support_rate'] / (df['X_sigma'] * df['T_interval_sigma'] + epsilon)

        # ソート処理
        if sort_by == 'composite':
            df = df.sort_values('composite_score', ascending=False)
            print(f"✓ Sorted by composite_score (support_rate / (X_sigma × T_interval_sigma), descending)")
        elif sort_by == 'support':
            df = df.sort_values('support_count', ascending=False)
            print(f"✓ Sorted by support_count (descending)")
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
                'composite_score': row['composite_score'],
                't_interval_mean': row.get('T_interval_mean', 0.0),
                't_interval_sigma': row.get('T_interval_std', 0.0),  # T_interval_sigmaとして扱う
                'time_span_days': row.get('TimeSpan', 0)
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

                # ilocを使って位置ベースでアクセス
                if data_df.iloc[check_idx][attr_name] != 1:
                    all_match = False
                    break

            if all_match:
                # t+1のレコードを取得
                matched_indices.append(t + 1)

        if len(matched_indices) == 0:
            return pd.DataFrame(columns=['X', 'T', 'T_datetime'])

        # マッチしたレコードを返す
        matched_df = data_df.iloc[matched_indices][['X', 'T', 'T_datetime']].copy()
        matched_df = matched_df.reset_index(drop=True)

        return matched_df

    def create_xt_scatter_plot(self, full_df, matched_df, rule, output_path):
        """
        X-T散布図を作成（全体+ルール適用+時間情報）

        Parameters
        ----------
        full_df : pd.DataFrame
            全データ
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

        # 1. 全体データ（中間の灰色、中サイズ）
        ax.scatter(full_df['T_datetime'], full_df['X'],
                  alpha=0.3, s=30, c='gray',
                  label=f'All data (n={len(full_df)})', zorder=1)

        # 2. ルール適用データ（赤色、大きい点）
        ax.scatter(matched_df['T_datetime'], matched_df['X'],
                  alpha=0.8, s=80, c='red', edgecolors='darkred', linewidth=1,
                  label=f'Rule matched (n={len(matched_df)})', zorder=2)

        # Y軸の範囲を固定
        ax.set_ylim(-10, 10)

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
            f"Stock {self.stock_code} - Rule {rule['rule_idx']}: {rule['rule_text']}"
        )
        ax.set_title(title_text, fontsize=12, fontweight='bold', pad=20)

        # 凡例
        ax.legend(loc='upper left', fontsize=11, framealpha=0.9)

        # グリッド
        ax.grid(True, alpha=0.3, linestyle='--')

        # 統計情報ボックス
        composite_score = rule['composite_score']
        x_sigma = rule['x_sigma']
        support_count = rule['support_count']
        support_rate = rule['support_rate']

        # 時間情報
        t_interval_mean = rule['t_interval_mean']
        t_interval_sigma = rule['t_interval_sigma']
        time_span_days = rule['time_span_days']
        time_span_years = time_span_days / 365.25 if time_span_days > 0 else 0

        # 積の計算（スコアの分母部分）
        sigma_product = x_sigma * t_interval_sigma

        stats_text = (
            f"Localization Effect:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"All data:     {len(full_df):5d} records\n"
            f"Rule matched: {len(matched_df):5d} records ({100*len(matched_df)/len(full_df):.1f}%)\n"
            f"\n"
            f"Matched Statistics:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"X_mean:  {mean_X:7.3f}\n"
            f"X_sigma: {std_X:7.3f}\n"
            f"Min:     {min_X:7.3f}\n"
            f"Max:     {max_X:7.3f}\n"
            f"Range:   {max_X - min_X:7.3f}\n"
            f"\n"
            f"Rule Quality Metrics:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Composite Score: {composite_score:8.4f}\n"
            f"= support_rate / (X_σ × T_σ)\n"
            f"= {support_rate:.4f} / {sigma_product:.2f}\n"
            f"Support: {support_count} ({support_rate*100:.2f}%)\n"
            f"\n"
            f"Time Regularity:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"TimeSpan:         {time_span_days:4d} days ({time_span_years:.1f} yrs)\n"
            f"T_interval_mean:  {t_interval_mean:6.1f} days\n"
            f"T_interval_sigma: {t_interval_sigma:6.1f} days"
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

    def process_all_rules(self, max_rules=10, min_samples=1, sort_by='composite'):
        """
        全ルールを処理

        Parameters
        ----------
        max_rules : int
            処理する最大ルール数（上位10ルールを推奨）
        min_samples : int
            可視化に必要な最小サンプル数（1に設定して全てのマッチを可視化）
        sort_by : str
            ソート基準（デフォルト: composite = 複合スコア降順）
            'composite', 'support', 'discovery'から選択
        """
        # ルール解析（上位max_rules個を取得）
        rules = self.parse_rule_file(max_rules=max_rules, sort_by=sort_by)

        # データ読み込み
        full_df = self.load_full_data()

        print(f"\nProcessing top {len(rules)} rules (sorted by {sort_by})...")
        print(f"Minimum samples required: {min_samples}")
        print(f"All matched records will be visualized (no sampling)")
        print(f"{'='*80}\n")

        successful_count = 0
        skipped_count = 0

        # 各ルールを処理
        for rule in rules:
            # マッチング（全データに対して、全レコードをチェック）
            matched_df = self.match_rule_to_data(full_df, rule)

            # 最小サンプル数チェック
            if len(matched_df) < min_samples:
                print(f"  Skipping rule {rule['rule_idx']}: {len(matched_df)} matches < {min_samples} minimum")
                skipped_count += 1
                continue

            # X-T散布図作成
            output_path = self.output_dir / f"rule_{rule['rule_idx']:04d}_xt_scatter.png"
            self.create_xt_scatter_plot(full_df, matched_df, rule, output_path)
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
    """メイン処理 - 全銘柄・全ルールを自動処理"""
    # スクリプトの位置から base_dir を特定
    script_dir = Path(__file__).resolve().parent  # analysis/stock/
    base_dir = script_dir.parent.parent  # ts-itemsbs/
    output_dir = base_dir / "output"

    print(f"Base directory: {base_dir}")
    print(f"Output directory: {output_dir}")

    # 存在する銘柄を自動検出
    stock_codes = []
    if output_dir.exists():
        for stock_dir in output_dir.iterdir():
            if stock_dir.is_dir() and (stock_dir / "pool/zrp01a.txt").exists():
                stock_codes.append(stock_dir.name)

    if not stock_codes:
        print("Error: No stocks found in output directory")
        return

    stock_codes.sort()  # 数値順にソート

    print(f"\n{'#'*70}")
    print(f"# Auto-detected {len(stock_codes)} stocks:")
    print(f"# {', '.join(stock_codes)}")
    print(f"{'#'*70}\n")

    for stock_code in stock_codes:
        print(f"\n{'#'*70}")
        print(f"# Processing: {stock_code}")
        print(f"{'#'*70}\n")

        try:
            plotter = ActualDataScatterPlotterWithInterval(stock_code)

            # 上位10ルールを処理（複合スコア降順でソート）
            plotter.process_all_rules(max_rules=10, min_samples=1, sort_by='composite')

            print(f"\n✓ Completed: {stock_code}\n")
        except Exception as e:
            print(f"Error processing {stock_code}: {e}")
            import traceback
            traceback.print_exc()
            continue

    print(f"\n{'='*70}")
    print(f"✓ All visualizations completed!")
    print(f"  Processed {len(stock_codes)} stocks")
    print(f"{'='*70}\n")


if __name__ == "__main__":
    main()
