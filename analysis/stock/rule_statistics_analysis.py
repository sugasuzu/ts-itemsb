#!/usr/bin/env python3
"""
ルール統計分析スクリプト

発見されたルールの統計的特性を分析します。
- t統計量の分布
- 有意性率
- X_mean vs X_sigma の関係
- サポート数と統計的有意性の関係

使用方法:
    python analysis/stock/rule_statistics_analysis.py [stock_code]
"""

import pandas as pd
import numpy as np
from pathlib import Path
import sys
import warnings
warnings.filterwarnings('ignore')


class RuleStatisticsAnalyzer:
    """ルール統計分析クラス"""

    def __init__(self, stock_code):
        """
        Parameters
        ----------
        stock_code : str
            銘柄コード (例: 7203)
        """
        self.stock_code = stock_code
        self.rule_file = Path(f"output/{stock_code}/pool/zrp01a.txt")

        if not self.rule_file.exists():
            raise FileNotFoundError(f"Rule file not found: {self.rule_file}")

        print(f"\n{'='*80}")
        print(f"Rule Statistics Analysis")
        print(f"{'='*80}")
        print(f"Stock Code: {stock_code}")
        print(f"Rule File:  {self.rule_file}")
        print(f"{'='*80}\n")

    def load_rules(self):
        """ルールファイルを読み込み"""
        print("Loading rules...")
        df = pd.read_csv(self.rule_file, sep='\t')

        # 必須カラムの確認
        required_cols = ['X_mean', 'X_sigma', 'support_count', 't_statistic']
        for col in required_cols:
            if col not in df.columns:
                raise ValueError(f"Required column '{col}' not found in rule file")

        print(f"✓ Loaded {len(df)} rules\n")
        return df

    def analyze_basic_statistics(self, df):
        """基本統計量の分析"""
        print("="*80)
        print("BASIC STATISTICS")
        print("="*80)

        print(f"\nTotal Rules: {len(df)}")
        print(f"\n{'Metric':<20} {'Mean':>12} {'Std':>12} {'Min':>12} {'Max':>12}")
        print("-"*80)

        metrics = {
            'X_mean': 'X_mean',
            'X_sigma': 'X_sigma',
            'support_count': 'support_count',
            't_statistic': 't_statistic'
        }

        for label, col in metrics.items():
            mean = df[col].mean()
            std = df[col].std()
            min_val = df[col].min()
            max_val = df[col].max()
            print(f"{label:<20} {mean:12.4f} {std:12.4f} {min_val:12.4f} {max_val:12.4f}")

        print()

    def analyze_significance(self, df):
        """統計的有意性の分析"""
        print("="*80)
        print("STATISTICAL SIGNIFICANCE ANALYSIS")
        print("="*80)

        # 有意性の分類
        df['abs_t'] = df['t_statistic'].abs()
        df['significance_level'] = pd.cut(
            df['abs_t'],
            bins=[0, 1.0, 1.96, 2.0, 2.58, 3.0, np.inf],
            labels=['Not Significant (<1.0)',
                    'Weak (1.0-1.96)',
                    'Marginal (1.96-2.0)',
                    'Significant (2.0-2.58)',
                    'Highly Sig. (2.58-3.0)',
                    'Very Highly Sig. (>3.0)']
        )

        print("\nSignificance Level Distribution:")
        print("-"*80)
        sig_counts = df['significance_level'].value_counts().sort_index()
        for level, count in sig_counts.items():
            percentage = 100 * count / len(df)
            print(f"{level:<30} {count:>6} ({percentage:5.1f}%)")

        # 有意なルール（|t| >= 2.0）
        significant = df[df['abs_t'] >= 2.0]
        print(f"\n{'Total Significant Rules (|t|>=2.0):':<40} {len(significant):>6} ({100*len(significant)/len(df):5.1f}%)")

        # 正と負の有意なルール
        positive_sig = df[(df['t_statistic'] >= 2.0)]
        negative_sig = df[(df['t_statistic'] <= -2.0)]

        print(f"{'  - Positive significant (t>=2.0):':<40} {len(positive_sig):>6} ({100*len(positive_sig)/len(df):5.1f}%)")
        print(f"{'  - Negative significant (t<=-2.0):':<40} {len(negative_sig):>6} ({100*len(negative_sig)/len(df):5.1f}%)")
        print()

    def analyze_x_mean_distribution(self, df):
        """X_meanの分布分析"""
        print("="*80)
        print("X_MEAN DISTRIBUTION ANALYSIS")
        print("="*80)

        print(f"\nX_mean Statistics:")
        print("-"*80)
        print(f"{'Mean:':<30} {df['X_mean'].mean():>12.6f}")
        print(f"{'Median:':<30} {df['X_mean'].median():>12.6f}")
        print(f"{'Std Dev:':<30} {df['X_mean'].std():>12.6f}")

        # パーセンタイル
        percentiles = [1, 5, 10, 25, 50, 75, 90, 95, 99]
        print(f"\nPercentiles:")
        print("-"*80)
        for p in percentiles:
            val = df['X_mean'].quantile(p/100)
            print(f"{p:>3}th percentile: {val:12.6f}")

        # 正負の分布
        positive = df[df['X_mean'] > 0]
        negative = df[df['X_mean'] < 0]
        zero = df[df['X_mean'] == 0]

        print(f"\nSign Distribution:")
        print("-"*80)
        print(f"{'Positive X_mean:':<30} {len(positive):>6} ({100*len(positive)/len(df):5.1f}%)")
        print(f"{'Negative X_mean:':<30} {len(negative):>6} ({100*len(negative)/len(df):5.1f}%)")
        print(f"{'Zero X_mean:':<30} {len(zero):>6} ({100*len(zero)/len(df):5.1f}%)")
        print()

    def analyze_x_mean_vs_significance(self, df):
        """X_meanの大きさと統計的有意性の関係"""
        print("="*80)
        print("X_MEAN vs STATISTICAL SIGNIFICANCE")
        print("="*80)

        # 有意なルールのX_mean
        significant = df[df['abs_t'] >= 2.0]
        non_significant = df[df['abs_t'] < 2.0]

        print(f"\nX_mean comparison:")
        print("-"*80)
        print(f"{'Group':<30} {'Count':>8} {'Mean X_mean':>15} {'Std X_mean':>15}")
        print("-"*80)
        print(f"{'Significant (|t|>=2.0)':<30} {len(significant):>8} {significant['X_mean'].mean():>15.6f} {significant['X_mean'].std():>15.6f}")
        print(f"{'Not Significant (|t|<2.0)':<30} {len(non_significant):>8} {non_significant['X_mean'].mean():>15.6f} {non_significant['X_mean'].std():>15.6f}")
        print()

        # X_meanの絶対値で分析
        print(f"Absolute X_mean comparison:")
        print("-"*80)
        sig_abs_mean = significant['X_mean'].abs().mean()
        nonsig_abs_mean = non_significant['X_mean'].abs().mean()
        print(f"{'Significant (|t|>=2.0)':<30} {len(significant):>8} {sig_abs_mean:>15.6f}")
        print(f"{'Not Significant (|t|<2.0)':<30} {len(non_significant):>8} {nonsig_abs_mean:>15.6f}")
        print()

    def analyze_support_vs_significance(self, df):
        """サポート数と統計的有意性の関係"""
        print("="*80)
        print("SUPPORT COUNT vs STATISTICAL SIGNIFICANCE")
        print("="*80)

        # サポート数で分類
        support_bins = [0, 50, 100, 200, 500, 1000, np.inf]
        support_labels = ['<50', '50-100', '100-200', '200-500', '500-1000', '>1000']
        df['support_category'] = pd.cut(df['support_count'], bins=support_bins, labels=support_labels)

        print(f"\nSignificance rate by support count:")
        print("-"*80)
        print(f"{'Support Count':<20} {'Total':>10} {'Significant':>12} {'Rate':>10}")
        print("-"*80)

        for cat in support_labels:
            subset = df[df['support_category'] == cat]
            if len(subset) > 0:
                sig_count = len(subset[subset['abs_t'] >= 2.0])
                sig_rate = 100 * sig_count / len(subset)
                print(f"{cat:<20} {len(subset):>10} {sig_count:>12} {sig_rate:>9.1f}%")

        print()

    def analyze_x_sigma_distribution(self, df):
        """X_sigmaの分布分析"""
        print("="*80)
        print("X_SIGMA (VOLATILITY) DISTRIBUTION")
        print("="*80)

        print(f"\nX_sigma Statistics:")
        print("-"*80)
        print(f"{'Mean:':<30} {df['X_sigma'].mean():>12.6f}")
        print(f"{'Median:':<30} {df['X_sigma'].median():>12.6f}")
        print(f"{'Std Dev:':<30} {df['X_sigma'].std():>12.6f}")
        print(f"{'Min:':<30} {df['X_sigma'].min():>12.6f}")
        print(f"{'Max:':<30} {df['X_sigma'].max():>12.6f}")

        # X_sigmaと有意性の関係
        significant = df[df['abs_t'] >= 2.0]
        non_significant = df[df['abs_t'] < 2.0]

        print(f"\nX_sigma by significance:")
        print("-"*80)
        print(f"{'Significant (|t|>=2.0):':<30} {significant['X_sigma'].mean():>12.6f}")
        print(f"{'Not Significant (|t|<2.0):':<30} {non_significant['X_sigma'].mean():>12.6f}")
        print()

    def analyze_signal_to_noise(self, df):
        """シグナル対ノイズ比の分析"""
        print("="*80)
        print("SIGNAL-TO-NOISE RATIO ANALYSIS")
        print("="*80)

        # SNR = |X_mean| / X_sigma
        df['snr'] = df['X_mean'].abs() / df['X_sigma']

        print(f"\nSignal-to-Noise Ratio (|X_mean| / X_sigma):")
        print("-"*80)
        print(f"{'Mean SNR:':<30} {df['snr'].mean():>12.6f}")
        print(f"{'Median SNR:':<30} {df['snr'].median():>12.6f}")
        print(f"{'Max SNR:':<30} {df['snr'].max():>12.6f}")

        # SNRと有意性の関係
        significant = df[df['abs_t'] >= 2.0]
        non_significant = df[df['abs_t'] < 2.0]

        print(f"\nSNR by significance:")
        print("-"*80)
        print(f"{'Significant (|t|>=2.0):':<30} {significant['snr'].mean():>12.6f}")
        print(f"{'Not Significant (|t|<2.0):':<30} {non_significant['snr'].mean():>12.6f}")

        # 高SNRルールの数
        high_snr = df[df['snr'] > 0.1]
        print(f"\nHigh SNR Rules (SNR > 0.1):")
        print("-"*80)
        print(f"{'Count:':<30} {len(high_snr):>6} ({100*len(high_snr)/len(df):5.1f}%)")
        print(f"{'Of these, significant:':<30} {len(high_snr[high_snr['abs_t'] >= 2.0]):>6} ({100*len(high_snr[high_snr['abs_t'] >= 2.0])/len(high_snr):5.1f}%)")
        print()

    def analyze_composite_scores(self, df):
        """複合スコア分析（統計的有意性 × 実用性）"""
        print("="*80)
        print("COMPOSITE SCORE ANALYSIS")
        print("="*80)

        # 3つの複合スコアを計算

        # スコア1: 両方の閾値を満たすルール（最も保守的）
        high_quality = df[
            (df['abs_t'] >= 2.0) &  # 95%信頼水準
            (df['support_count'] >= 50)  # 最小サポート数
        ]

        # スコア2: t値 × log(support) （バランス型）
        df['composite_score'] = df['abs_t'] * np.log1p(df['support_count'])

        # スコア3: 期待値ベース（実用重視）
        df['expected_value'] = (
            df['X_mean'].abs() *
            df['support_count'] *
            (df['abs_t'] / 2.0).clip(upper=1.0)  # t値を0-1に正規化
        )

        print(f"\nComposite Score Formulas:")
        print("-"*80)
        print(f"1. High Quality Filter: |t|>=2.0 AND support>=50")
        print(f"2. Balanced Score:      |t| × log(1 + support)")
        print(f"3. Expected Value:      |X_mean| × support × min(|t|/2.0, 1.0)")
        print()

        print(f"\nHigh Quality Rules (|t|>=2.0 AND support>=50):")
        print("-"*80)
        print(f"{'Count:':<30} {len(high_quality):>6} ({100*len(high_quality)/len(df):5.1f}%)")
        if len(high_quality) > 0:
            print(f"{'Mean X_mean:':<30} {high_quality['X_mean'].mean():>12.6f}")
            print(f"{'Mean support:':<30} {high_quality['support_count'].mean():>12.1f}")
            print(f"{'Mean |t|:':<30} {high_quality['abs_t'].mean():>12.3f}")
        print()

    def find_top_significant_rules(self, df, n=10):
        """最も有意なルールを抽出"""
        print("="*80)
        print(f"TOP {n} MOST SIGNIFICANT RULES (by |t|)")
        print("="*80)

        # |t|でソート
        top_rules = df.nlargest(n, 'abs_t')

        print(f"\n{'Rank':<6} {'t-stat':>8} {'X_mean':>10} {'X_sigma':>10} {'n':>6} {'Attrs':>6}")
        print("-"*80)

        for idx, (i, row) in enumerate(top_rules.iterrows(), 1):
            print(f"{idx:<6} {row['t_statistic']:>8.3f} {row['X_mean']:>10.4f} {row['X_sigma']:>10.4f} {int(row['support_count']):>6} {int(row['NumAttr']):>6}")

        print()

    def find_most_valuable_rules(self, df, n=10):
        """最も価値のあるルールを複合スコアで抽出"""
        print("="*80)
        print(f"TOP {n} MOST VALUABLE RULES (by Composite Score)")
        print("="*80)

        print(f"\nRanking Method: |t| × log(1 + support_count)")
        print(f"  - Balances statistical significance with practical applicability")
        print(f"  - Higher t-statistic = more reliable prediction")
        print(f"  - Higher support = more opportunities to use the rule")
        print()

        # composite_scoreでソート
        top_rules = df.nlargest(n, 'composite_score')

        print(f"{'Rank':<6} {'Score':>10} {'t-stat':>8} {'Support':>8} {'X_mean':>10} {'X_sigma':>10} {'Attrs':>6}")
        print("-"*80)

        for idx, (i, row) in enumerate(top_rules.iterrows(), 1):
            print(f"{idx:<6} {row['composite_score']:>10.2f} {row['t_statistic']:>8.3f} {int(row['support_count']):>8} "
                  f"{row['X_mean']:>10.4f} {row['X_sigma']:>10.4f} {int(row['NumAttr']):>6}")

        print()

        return top_rules

    def export_significant_rules(self, df, output_file=None):
        """有意なルールをCSVにエクスポート"""
        if output_file is None:
            # 専用ディレクトリに保存
            output_file = f"analysis/stock/analysis_results/{self.stock_code}/significant_rules.csv"

        output_path = Path(output_file)
        output_path.parent.mkdir(parents=True, exist_ok=True)

        # 有意なルールのみ抽出（|t| >= 2.0）
        significant = df[df['abs_t'] >= 2.0].copy()

        # 複合スコアでソート（t値ではなく）
        significant = significant.sort_values('composite_score', ascending=False)

        # 保存
        significant.to_csv(output_path, index=False)

        print(f"✓ Exported {len(significant)} significant rules to: {output_path}")
        print(f"  Sorted by: composite_score (|t| × log(1 + support))")
        print(f"  File: {output_path}\n")

    def run_full_analysis(self):
        """全分析を実行"""
        # ルール読み込み
        df = self.load_rules()

        # 各種分析
        self.analyze_basic_statistics(df)
        self.analyze_significance(df)
        self.analyze_x_mean_distribution(df)
        self.analyze_x_mean_vs_significance(df)
        self.analyze_support_vs_significance(df)
        self.analyze_x_sigma_distribution(df)
        self.analyze_signal_to_noise(df)

        # 複合スコア分析（NEW!）
        self.analyze_composite_scores(df)

        self.find_top_significant_rules(df, n=10)

        # 最も価値のあるルールを複合スコアで抽出（NEW!）
        self.find_most_valuable_rules(df, n=10)

        # 有意なルールをエクスポート
        self.export_significant_rules(df)

        print("="*80)
        print("ANALYSIS COMPLETE")
        print("="*80)
        print()


def main():
    """メイン処理"""
    if len(sys.argv) < 2:
        # 引数がない場合は全銘柄を処理
        print("No stock code specified. Analyzing all available stocks...\n")

        output_dir = Path("output")
        stock_codes = []

        if output_dir.exists():
            for stock_dir in output_dir.iterdir():
                if stock_dir.is_dir() and (stock_dir / "pool/zrp01a.txt").exists():
                    stock_codes.append(stock_dir.name)

        if not stock_codes:
            print("Error: No stocks found in output directory")
            return

        stock_codes.sort()
        print(f"Found {len(stock_codes)} stocks with rule data\n")

        # 各銘柄を分析
        for stock_code in stock_codes:
            try:
                analyzer = RuleStatisticsAnalyzer(stock_code)
                analyzer.run_full_analysis()
            except Exception as e:
                print(f"Error analyzing {stock_code}: {e}\n")
                continue
    else:
        # 指定された銘柄を分析
        stock_code = sys.argv[1]
        analyzer = RuleStatisticsAnalyzer(stock_code)
        analyzer.run_full_analysis()


if __name__ == "__main__":
    main()
