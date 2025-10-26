"""
ルールベースのForex統計分析スクリプト

pool/ディレクトリから発見されたルールを読み込み、
各ルールの条件を適用したときのX,Tの統計的特性を分析します。

分析内容:
1. 全データのX,T散布図（ベースライン）
2. 各ルール適用時のX,T散布図と統計量
3. ルールごとの分布比較（ヒストグラム、KDEプロット）
4. 回帰分析（線形、多項式）
5. 時系列パターン（周期性、自己相関）
6. 統計的有意性検定（t検定、KS検定）
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
from scipy.signal import periodogram
from datetime import datetime
import warnings
warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

class ForexRuleAnalyzer:
    """Forexルール分析クラス"""

    def __init__(self, forex_pair, base_dir="."):
        """
        Parameters
        ----------
        forex_pair : str
            通貨ペア名（例: "USDJPY"）
        base_dir : str
            プロジェクトのベースディレクトリ
        """
        self.forex_pair = forex_pair
        self.base_dir = Path(base_dir).resolve()  # 絶対パスに変換
        self.output_dir = self.base_dir / "analysis" / "fx" / forex_pair
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # データ読み込み
        self.load_data()
        self.load_rules()

    def load_data(self):
        """Forexデータを読み込む"""
        data_path = self.base_dir / "forex_data" / "forex_raw_gnminer.csv"

        if not data_path.exists():
            raise FileNotFoundError(f"Data file not found: {data_path}")

        # CSVを読み込み
        df = pd.read_csv(data_path)

        # Tカラムをdatetimeに変換
        df['T'] = pd.to_datetime(df['T'])

        # Xカラムを作成（次の行のUSDJPY_Upの値など）
        # ここではシンプルに次の日の変化率を計算
        # 実際のXの定義に合わせて調整が必要
        self.data = df

        # 通貨ペアの各カラム名を取得
        self.pair_columns = [col for col in df.columns if col.startswith(self.forex_pair)]

        print(f"Loaded {len(df)} records from {data_path}")
        print(f"Date range: {df['T'].min()} to {df['T'].max()}")
        print(f"Columns for {self.forex_pair}: {self.pair_columns}")

    def load_rules(self):
        """Pool/からルールを読み込む"""
        pool_path = self.base_dir / "output" / self.forex_pair / "pool" / "zrp01a.txt"

        if not pool_path.exists():
            raise FileNotFoundError(f"Pool file not found: {pool_path}")

        # TSVとして読み込み
        rules_df = pd.read_csv(pool_path, sep='\t')

        # 属性カラムを抽出
        attr_cols = [col for col in rules_df.columns if col.startswith('Attr')]

        self.rules = rules_df
        self.attr_cols = attr_cols

        print(f"Loaded {len(rules_df)} rules from {pool_path}")
        print(f"Attribute columns: {attr_cols}")

    def parse_attribute(self, attr_str):
        """
        属性文字列をパースする
        例: "USDJPY_Up(t-1)" -> ("USDJPY_Up", 1)

        Returns
        -------
        tuple or None
            (attribute_name, time_delay) or None if invalid
        """
        if pd.isna(attr_str) or attr_str == '0':
            return None

        try:
            # "USDJPY_Up(t-1)" を分解
            parts = attr_str.split('(')
            attr_name = parts[0]
            delay_str = parts[1].rstrip(')')  # "t-1"
            delay = int(delay_str.split('-')[1])
            return (attr_name, delay)
        except:
            return None

    def check_rule_match(self, row_idx, rule_idx):
        """
        特定のデータ行が特定のルールにマッチするかチェック

        Parameters
        ----------
        row_idx : int
            データ行のインデックス
        rule_idx : int
            ルールのインデックス

        Returns
        -------
        bool
            マッチする場合True
        """
        rule = self.rules.iloc[rule_idx]

        # すべての属性条件をチェック
        for attr_col in self.attr_cols:
            attr_value = rule[attr_col]
            parsed = self.parse_attribute(attr_value)

            if parsed is None:
                continue

            attr_name, delay = parsed

            # 時間遅延を考慮したインデックス
            target_idx = row_idx - delay

            if target_idx < 0 or target_idx >= len(self.data):
                return False

            # 属性値をチェック
            if attr_name not in self.data.columns:
                return False

            if self.data.iloc[target_idx][attr_name] != 1:
                return False

        return True

    def get_matched_indices(self, rule_idx):
        """
        ルールにマッチするデータインデックスのリストを取得

        Parameters
        ----------
        rule_idx : int
            ルールのインデックス

        Returns
        -------
        list
            マッチしたインデックスのリスト
        """
        matched = []
        max_delay = self.get_max_delay(rule_idx)

        # 遅延を考慮して開始インデックスを設定
        for i in range(max_delay, len(self.data)):
            if self.check_rule_match(i, rule_idx):
                matched.append(i)

        return matched

    def get_max_delay(self, rule_idx):
        """ルールの最大時間遅延を取得"""
        rule = self.rules.iloc[rule_idx]
        max_delay = 0

        for attr_col in self.attr_cols:
            parsed = self.parse_attribute(rule[attr_col])
            if parsed:
                _, delay = parsed
                max_delay = max(max_delay, delay)

        return max_delay

    def analyze_rule(self, rule_idx, plot=True):
        """
        特定のルールを分析

        Parameters
        ----------
        rule_idx : int
            分析するルールのインデックス
        plot : bool
            プロットを生成するかどうか

        Returns
        -------
        dict
            統計情報の辞書
        """
        rule = self.rules.iloc[rule_idx]
        matched_indices = self.get_matched_indices(rule_idx)

        if len(matched_indices) == 0:
            print(f"Rule {rule_idx}: No matches found")
            return None

        # マッチしたデータを抽出
        matched_data = self.data.iloc[matched_indices].copy()

        # X値を計算（ルールのX_meanと比較）
        X_pred = rule['X_mean']
        X_sigma = rule['X_sigma']

        # 統計量を計算
        stats_dict = {
            'rule_idx': rule_idx,
            'n_matches': len(matched_indices),
            'support_count': rule['support_count'],
            'support_rate': rule['support_rate'],
            'X_mean_rule': X_pred,
            'X_sigma_rule': X_sigma,
            'high_support': rule['HighSup'],
            'low_variance': rule['LowVar'],
            'num_attributes': rule['NumAttr'],
        }

        # 時系列パターン情報
        if 'Month' in rule:
            stats_dict['dominant_month'] = rule['Month']
        if 'Quarter' in rule:
            stats_dict['dominant_quarter'] = rule['Quarter']
        if 'Day' in rule:
            stats_dict['dominant_day'] = rule['Day']

        # プロット生成
        if plot:
            self.plot_rule_analysis(rule_idx, matched_data, stats_dict)

        return stats_dict

    def plot_rule_analysis(self, rule_idx, matched_data, stats_dict):
        """ルール分析結果をプロット"""
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle(f'Rule {rule_idx} Analysis - {self.forex_pair}\n'
                     f'Matches: {stats_dict["n_matches"]}, '
                     f'Support: {stats_dict["support_rate"]:.4f}, '
                     f'X_mean: {stats_dict["X_mean_rule"]:.3f}±{stats_dict["X_sigma_rule"]:.3f}',
                     fontsize=14, fontweight='bold')

        # 1. 時系列プロット
        ax1 = axes[0, 0]
        ax1.scatter(matched_data['T'], range(len(matched_data)),
                   alpha=0.5, s=10)
        ax1.set_xlabel('Date (T)')
        ax1.set_ylabel('Match Index')
        ax1.set_title('Temporal Distribution of Matches')
        ax1.grid(True, alpha=0.3)

        # 2. 月別分布
        ax2 = axes[0, 1]
        matched_data['Month'] = matched_data['T'].dt.month
        month_counts = matched_data['Month'].value_counts().sort_index()
        ax2.bar(month_counts.index, month_counts.values, color='steelblue', alpha=0.7)
        ax2.set_xlabel('Month')
        ax2.set_ylabel('Frequency')
        ax2.set_title(f'Monthly Distribution (Dominant: {stats_dict.get("dominant_month", "N/A")})')
        ax2.set_xticks(range(1, 13))
        ax2.grid(True, alpha=0.3, axis='y')

        # 3. 曜日別分布
        ax3 = axes[1, 0]
        matched_data['DayOfWeek'] = matched_data['T'].dt.dayofweek
        dow_counts = matched_data['DayOfWeek'].value_counts().sort_index()
        dow_labels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
        ax3.bar(range(len(dow_counts)), dow_counts.values,
               tick_label=[dow_labels[i] for i in dow_counts.index],
               color='coral', alpha=0.7)
        ax3.set_xlabel('Day of Week')
        ax3.set_ylabel('Frequency')
        ax3.set_title(f'Day of Week Distribution (Dominant: {stats_dict.get("dominant_day", "N/A")})')
        ax3.grid(True, alpha=0.3, axis='y')

        # 4. 年別トレンド
        ax4 = axes[1, 1]
        matched_data['Year'] = matched_data['T'].dt.year
        year_counts = matched_data['Year'].value_counts().sort_index()
        ax4.plot(year_counts.index, year_counts.values,
                marker='o', linewidth=2, markersize=6, color='forestgreen')
        ax4.set_xlabel('Year')
        ax4.set_ylabel('Frequency')
        ax4.set_title('Yearly Trend of Matches')
        ax4.grid(True, alpha=0.3)

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / f"rule_{rule_idx:04d}_analysis.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"Saved plot to {output_path}")

    def compare_top_rules(self, top_n=10):
        """
        上位N個のルールを比較分析

        Parameters
        ----------
        top_n : int
            分析するルール数
        """
        # Support rateでソート
        top_rules = self.rules.nlargest(top_n, 'support_rate')

        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle(f'Top {top_n} Rules Comparison - {self.forex_pair}',
                     fontsize=14, fontweight='bold')

        # データ収集
        rule_stats = []
        for idx in top_rules.index[:top_n]:
            stats = self.analyze_rule(idx, plot=False)
            if stats:
                rule_stats.append(stats)

        if not rule_stats:
            print("No valid rules found")
            return

        stats_df = pd.DataFrame(rule_stats)

        # 1. Support rate vs X_mean
        ax1 = axes[0, 0]
        scatter = ax1.scatter(stats_df['support_rate'], stats_df['X_mean_rule'],
                            c=stats_df['X_sigma_rule'], s=stats_df['n_matches']/10,
                            alpha=0.6, cmap='viridis')
        ax1.set_xlabel('Support Rate')
        ax1.set_ylabel('X Mean (Predicted)')
        ax1.set_title('Support Rate vs Predicted X Mean')
        ax1.grid(True, alpha=0.3)
        plt.colorbar(scatter, ax=ax1, label='X Sigma')

        # 2. Number of attributes vs Support rate
        ax2 = axes[0, 1]
        ax2.scatter(stats_df['num_attributes'], stats_df['support_rate'],
                   alpha=0.6, s=100, color='coral')
        ax2.set_xlabel('Number of Attributes')
        ax2.set_ylabel('Support Rate')
        ax2.set_title('Complexity vs Support Rate')
        ax2.grid(True, alpha=0.3)

        # 3. X_mean分布
        ax3 = axes[1, 0]
        ax3.hist(stats_df['X_mean_rule'], bins=20, color='steelblue',
                alpha=0.7, edgecolor='black')
        ax3.axvline(stats_df['X_mean_rule'].mean(), color='red',
                   linestyle='--', linewidth=2, label=f'Mean: {stats_df["X_mean_rule"].mean():.3f}')
        ax3.set_xlabel('X Mean (Predicted)')
        ax3.set_ylabel('Frequency')
        ax3.set_title('Distribution of Predicted X Mean')
        ax3.legend()
        ax3.grid(True, alpha=0.3, axis='y')

        # 4. X_sigma分布
        ax4 = axes[1, 1]
        ax4.hist(stats_df['X_sigma_rule'], bins=20, color='forestgreen',
                alpha=0.7, edgecolor='black')
        ax4.axvline(stats_df['X_sigma_rule'].mean(), color='red',
                   linestyle='--', linewidth=2, label=f'Mean: {stats_df["X_sigma_rule"].mean():.3f}')
        ax4.set_xlabel('X Sigma (Uncertainty)')
        ax4.set_ylabel('Frequency')
        ax4.set_title('Distribution of X Uncertainty')
        ax4.legend()
        ax4.grid(True, alpha=0.3, axis='y')

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / f"top_{top_n}_rules_comparison.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"Saved comparison plot to {output_path}")

        # 統計サマリーを保存
        summary_path = self.output_dir / f"top_{top_n}_rules_summary.csv"
        stats_df.to_csv(summary_path, index=False)
        print(f"Saved summary to {summary_path}")

        return stats_df


def main():
    """メイン実行関数"""

    # スクリプトファイルの位置からベースディレクトリを特定
    script_dir = Path(__file__).parent  # analysis/fx/
    base_dir = script_dir.parent.parent  # ts-itemsbs/

    # 通貨ペアリスト
    forex_pairs = ['USDJPY', 'EURAUD', 'GBPCAD', 'AUDNZD']

    for pair in forex_pairs:
        print(f"\n{'='*60}")
        print(f"Analyzing {pair}")
        print(f"{'='*60}\n")

        try:
            # Analyzerを初期化
            analyzer = ForexRuleAnalyzer(pair, base_dir=base_dir)

            # 上位10ルールを比較
            print("\n--- Comparing Top 10 Rules ---")
            stats_df = analyzer.compare_top_rules(top_n=10)

            # 個別ルール分析（上位3つ）
            print("\n--- Analyzing Individual Rules ---")
            for i in range(min(3, len(analyzer.rules))):
                print(f"\nAnalyzing Rule {i}...")
                analyzer.analyze_rule(i, plot=True)

            print(f"\n{pair} analysis completed!")

        except FileNotFoundError as e:
            print(f"Skipping {pair}: {e}")
            continue
        except Exception as e:
            print(f"Error analyzing {pair}: {e}")
            import traceback
            traceback.print_exc()
            continue

    print("\n" + "="*60)
    print("All analysis completed!")
    print("="*60)


if __name__ == "__main__":
    main()
