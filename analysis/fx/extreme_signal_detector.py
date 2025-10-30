"""
極値シグナル検出器

既存のルールプールから、0付近ではなく大きな変化率を示す
「極値シグナルルール」を検出・ランキングする。

【コンセプト】
通常の変化率分布は0付近に集中するが、特定のルール条件下では
0から遠い極値を示す局所的な分布が現れる。
これをトレーディングシグナルとして活用する。

【主要指標】
1. Signal Strength: |X_mean| - 変化の大きさ
2. SNR (Signal-to-Noise Ratio): |X_mean| / X_sigma - 確実性
3. Extremeness: |X_mean| / global_std(X) - 全体からの乖離度
4. Statistical Significance: t検定によるp値
5. Tail Event Rate: 極端な変動が起こる確率
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
import warnings
warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False
sns.set_style("whitegrid")


class ExtremeSignalDetector:
    """極値シグナルルール検出器"""

    def __init__(self, forex_pair, base_dir=None):
        """
        Parameters
        ----------
        forex_pair : str
            通貨ペア名（例: "USDJPY"）
        base_dir : str or Path
            プロジェクトのベースディレクトリ
        """
        if base_dir is None:
            script_dir = Path(__file__).parent
            base_dir = script_dir.parent.parent

        self.forex_pair = forex_pair
        self.base_dir = Path(base_dir).resolve()
        self.output_dir = self.base_dir / "analysis" / "fx" / forex_pair / "extreme_signals"
        self.output_dir.mkdir(parents=True, exist_ok=True)

        print(f"\n{'='*60}")
        print(f"Extreme Signal Detector - {forex_pair}")
        print(f"{'='*60}\n")

        # データ読み込み
        self.load_data()
        self.load_rules()
        self.global_stats = self.calculate_global_statistics()

    def load_data(self):
        """個別データファイルを読み込む"""
        data_path = self.base_dir / "forex_data" / "gnminer_individual" / f"{self.forex_pair}.txt"

        if not data_path.exists():
            raise FileNotFoundError(f"Data file not found: {data_path}")

        # CSVを読み込み
        self.data = pd.read_csv(data_path)

        # 【極値データフィルタリング】方向性を持つ極値データのみを残す
        # C言語側のフィルタリングと一致させる
        original_count = len(self.data)

        # 正と負の極値をカウント
        positive_count = (self.data['X'] >= 1.0).sum()
        negative_count = (self.data['X'] <= -1.0).sum()

        # 多い方を自動選択
        if positive_count > negative_count:
            self.extreme_direction = 1  # POSITIVE
            self.data = self.data[self.data['X'] >= 1.0].reset_index(drop=True)
            direction_label = "POSITIVE (X >= 1.0)"
        else:
            self.extreme_direction = -1  # NEGATIVE
            self.data = self.data[self.data['X'] <= -1.0].reset_index(drop=True)
            direction_label = "NEGATIVE (X <= -1.0)"

        filtered_count = len(self.data)

        print(f"\n========== Filtering Directional Extreme Data ==========")
        print(f"Original records: {original_count}")
        print(f"Positive extreme count (X >= 1.0): {positive_count}")
        print(f"Negative extreme count (X <= -1.0): {negative_count}")
        print(f"Auto-selected direction: {direction_label}")
        print(f"Filtered records: {filtered_count} ({100.0 * filtered_count / original_count:.2f}% of original)")
        print(f"Removed records: {original_count - filtered_count}")

        # Tカラムをdatetimeに変換
        self.data['T'] = pd.to_datetime(self.data['T'])

        # 属性カラムのリストを取得
        self.attribute_columns = [col for col in self.data.columns if col not in ['X', 'T']]

        print(f"\nLoaded DIRECTIONAL EXTREME data: {len(self.data)} records")
        print(f"Direction: {'UP (POSITIVE)' if self.extreme_direction == 1 else 'DOWN (NEGATIVE)'}")
        print(f"Date range: {self.data['T'].min()} to {self.data['T'].max()}")
        print(f"X range: {self.data['X'].min():.3f} to {self.data['X'].max():.3f}")
        print(f"X mean: {self.data['X'].mean():.3f}, std: {self.data['X'].std():.3f}")

    def load_rules(self):
        """Pool/からルールを読み込む"""
        pool_path = self.base_dir / "output" / self.forex_pair / "pool" / "zrp01a.txt"
        scores_path = self.base_dir / "output" / self.forex_pair / "pool" / "extreme_scores.csv"

        if not pool_path.exists():
            raise FileNotFoundError(f"Pool file not found: {pool_path}")

        # TSVとして読み込み
        self.rules = pd.read_csv(pool_path, sep='\t')

        # 属性カラムを抽出
        self.attr_cols = [col for col in self.rules.columns if col.startswith('Attr')]

        print(f"Loaded {len(self.rules)} rules from pool")

        # 極値スコアCSVを読み込んでマージ
        if scores_path.exists():
            print(f"Loading pre-computed extreme scores from CSV...")
            scores_df = pd.read_csv(scores_path)

            # rule_idxをインデックスとして使用
            scores_df = scores_df.set_index('rule_idx')

            # スコアカラムをrulesにマージ（方向性情報を追加）
            score_cols = ['extreme_direction', 'signal_strength', 'SNR', 'extremeness', 't_statistic',
                         'p_value', 'tail_event_rate', 'extreme_signal_score', 'matched_count',
                         'directional_bias', 'non_zero_rate', 'positive_rate',
                         'very_extreme_rate', 'mean_abs_ratio', 'spread_ratio',
                         'p75_abs', 'p90_abs', 'p95_abs']

            for col in score_cols:
                if col in scores_df.columns:
                    self.rules[col] = scores_df[col]

            # matched_count_verified として使用
            if 'matched_count' in scores_df.columns:
                self.rules['matched_count_verified'] = scores_df['matched_count']

            print(f"Loaded extreme scores for {len(scores_df)} rules")
            self.scores_loaded = True
        else:
            print(f"Warning: extreme_scores.csv not found at {scores_path}")
            print("You'll need to run calculate_signal_scores() manually")
            self.scores_loaded = False

    def calculate_global_statistics(self):
        """全体データの統計量を計算"""
        stats_dict = {
            'X_mean': self.data['X'].mean(),
            'X_std': self.data['X'].std(),
            'X_median': self.data['X'].median(),
            'X_q25': self.data['X'].quantile(0.25),
            'X_q75': self.data['X'].quantile(0.75),
            'X_min': self.data['X'].min(),
            'X_max': self.data['X'].max(),
        }

        print(f"\nGlobal X statistics:")
        for key, value in stats_dict.items():
            print(f"  {key}: {value:.4f}")

        return stats_dict

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

    def check_rule_match(self, row_idx, rule_attrs):
        """
        特定のデータ行が特定のルールにマッチするかチェック

        Parameters
        ----------
        row_idx : int
            データ行のインデックス
        rule_attrs : list
            ルールの属性リスト

        Returns
        -------
        bool
            マッチする場合True
        """
        for attr_str in rule_attrs:
            parsed = self.parse_attribute(attr_str)

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
        np.array
            マッチしたインデックスの配列
        """
        rule = self.rules.iloc[rule_idx]
        rule_attrs = [rule[col] for col in self.attr_cols]

        # 最大遅延を取得
        max_delay = 0
        for attr_str in rule_attrs:
            parsed = self.parse_attribute(attr_str)
            if parsed:
                _, delay = parsed
                max_delay = max(max_delay, delay)

        # マッチング
        matched = []
        for i in range(max_delay, len(self.data)):
            if self.check_rule_match(i, rule_attrs):
                matched.append(i)

        return np.array(matched)

    def calculate_signal_scores(self, sample_size=1000):
        """
        各ルールのシグナルスコアを計算（サンプリング版）

        Parameters
        ----------
        sample_size : int
            処理するルール数（ランダムサンプリング）
        """
        print(f"\n{'='*60}")
        print(f"Calculating signal scores (sampling {sample_size} rules)...")
        print(f"{'='*60}\n")

        # 新しいカラムを初期化
        self.rules['signal_strength'] = 0.0
        self.rules['SNR'] = 0.0
        self.rules['extremeness'] = 0.0
        self.rules['t_statistic'] = 0.0
        self.rules['p_value'] = 1.0
        self.rules['tail_event_rate'] = 0.0
        self.rules['extreme_signal_score'] = 0.0
        self.rules['matched_count_verified'] = 0

        global_std = self.global_stats['X_std']
        tail_threshold = 2 * global_std  # ±2σを極端イベントとする

        # ランダムサンプリング（再現性のためseed設定）
        np.random.seed(42)
        total_rules = len(self.rules)

        if total_rules <= sample_size:
            # ルール数がサンプルサイズ以下なら全て処理
            sample_indices = list(range(total_rules))
            print(f"Processing all {total_rules} rules...")
        else:
            # ランダムにサンプリング
            sample_indices = np.random.choice(total_rules, size=sample_size, replace=False)
            print(f"Randomly sampled {sample_size} rules from {total_rules} total rules")

        # サンプリングしたルールのみ処理
        for i, idx in enumerate(sample_indices):
            if i % 100 == 0:
                print(f"Processing rule {i}/{len(sample_indices)}... (original index: {idx})")

            rule = self.rules.iloc[idx]

            # マッチしたインデックスを取得
            matched_indices = self.get_matched_indices(idx)

            if len(matched_indices) == 0:
                continue

            # マッチしたX値を取得
            matched_X = self.data.iloc[matched_indices]['X'].values

            # 検証用のマッチ数を保存
            self.rules.at[idx, 'matched_count_verified'] = len(matched_indices)

            # 1. Signal Strength（変化の大きさ）
            X_mean_actual = np.mean(matched_X)
            self.rules.at[idx, 'signal_strength'] = abs(X_mean_actual)

            # 2. SNR（シグナル対ノイズ比）
            X_std_actual = np.std(matched_X, ddof=1) if len(matched_X) > 1 else 0.0
            self.rules.at[idx, 'SNR'] = abs(X_mean_actual) / (X_std_actual + 1e-6)

            # 3. Extremeness（全体分布からの乖離度）
            self.rules.at[idx, 'extremeness'] = abs(X_mean_actual) / global_std

            # 4. 統計的有意性（t検定）
            if len(matched_X) > 1:
                t_stat, p_val = stats.ttest_1samp(matched_X, 0.0)
                self.rules.at[idx, 't_statistic'] = t_stat
                self.rules.at[idx, 'p_value'] = p_val
            else:
                self.rules.at[idx, 't_statistic'] = 0.0
                self.rules.at[idx, 'p_value'] = 1.0

            # 5. Tail Event Rate（極端な変動の確率）
            tail_events = np.sum(np.abs(matched_X) > tail_threshold)
            self.rules.at[idx, 'tail_event_rate'] = tail_events / len(matched_X)

            # 総合スコア（重み付き和）
            self.rules.at[idx, 'extreme_signal_score'] = (
                self.rules.at[idx, 'signal_strength'] * 10.0 +
                self.rules.at[idx, 'SNR'] * 5.0 +
                self.rules.at[idx, 'extremeness'] * 3.0 +
                (1.0 - self.rules.at[idx, 'p_value']) * 2.0 +
                self.rules.at[idx, 'tail_event_rate'] * 5.0
            )

        print(f"\nSignal score calculation completed!")
        print(f"Sampled rules: {len(sample_indices)}")
        print(f"Rules with matches: {(self.rules['matched_count_verified'] > 0).sum()}")

    def rank_extreme_rules(self, min_signal_strength=0.3, min_snr=0.3,
                           min_support=30, max_p_value=0.05, top_n=20):
        """
        極値シグナルルールをランキング

        Parameters
        ----------
        min_signal_strength : float
            最小シグナル強度 (|X_mean|の絶対値)
        min_snr : float
            最小SNR
        min_support : int
            最小サポート数
        max_p_value : float
            最大p値（統計的有意性）
        top_n : int
            上位何件を返すか

        Returns
        -------
        pd.DataFrame
            ランキングされた極値ルール
        """
        print(f"\n{'='*60}")
        print("Ranking Extreme Signal Rules")
        print(f"{'='*60}")
        print(f"Filtering criteria:")
        print(f"  Signal Strength >= {min_signal_strength}")
        print(f"  SNR >= {min_snr}")
        print(f"  Support Count >= {min_support}")
        print(f"  p-value < {max_p_value}")

        # フィルタリング
        filtered = self.rules[
            (self.rules['signal_strength'] >= min_signal_strength) &
            (self.rules['SNR'] >= min_snr) &
            (self.rules['matched_count_verified'] >= min_support) &
            (self.rules['p_value'] < max_p_value)
        ].copy()

        print(f"\nFiltered rules: {len(filtered)} / {len(self.rules)}")

        if len(filtered) == 0:
            print("⚠️  No rules meet the criteria. Try relaxing the filters.")
            return pd.DataFrame()

        # スコア順にソート
        ranked = filtered.sort_values('extreme_signal_score', ascending=False)

        # 統計表示
        print(f"\nTop {min(top_n, len(ranked))} Extreme Signal Rules:")
        print(f"{'='*60}")

        display_cols = ['extremeness', 'tail_event_rate', 'very_extreme_rate',
                       'mean_abs_ratio', 'p75_abs', 'matched_count_verified', 'extreme_signal_score']

        print(ranked[display_cols].head(top_n).to_string(index=True))

        return ranked.head(top_n)

    def visualize_extreme_rule(self, rule_idx):
        """
        極値ルールの局所分布を可視化
        X軸のみの1次元分布で、極値領域（±1～±3）の集団を強調

        Parameters
        ----------
        rule_idx : int
            ルールのインデックス
        """
        rule = self.rules.iloc[rule_idx]
        matched_indices = self.get_matched_indices(rule_idx)

        if len(matched_indices) == 0:
            print(f"Rule {rule_idx}: No matches found")
            return

        # マッチしたデータを取得
        matched_X = self.data.iloc[matched_indices]['X'].values

        # 統計情報
        signal_strength = rule['signal_strength']
        snr = rule['SNR']
        p_value = rule['p_value']
        tail_rate = rule['tail_event_rate']
        score = rule['extreme_signal_score']
        X_mean = matched_X.mean()
        X_std = matched_X.std()

        # プロット作成（2段）
        fig, axes = plt.subplots(2, 1, figsize=(16, 10))

        # === 上段：1次元散布図（Strip plot） ===
        ax1 = axes[0]

        # 全体データ（薄いグレー、小さい）
        y_all = np.random.uniform(-0.3, 0.3, len(self.data))  # Y軸はランダムジッター
        ax1.scatter(self.data['X'], y_all,
                   alpha=0.05, s=10, c='gray', label='All data', edgecolors='none')

        # ルールマッチ点（赤、大きく、Y=0付近に配置）
        y_matched = np.random.uniform(-0.15, 0.15, len(matched_X))
        ax1.scatter(matched_X, y_matched,
                   alpha=0.8, s=200, c='red', label=f'Rule matched (n={len(matched_X)})',
                   edgecolors='black', linewidth=1.5, zorder=5)

        # 重要な領域を強調
        ax1.axvspan(-3, -1, alpha=0.1, color='orange', label='Target zone (negative)')
        ax1.axvspan(1, 3, alpha=0.1, color='orange', label='Target zone (positive)')
        ax1.axvspan(-1, 1, alpha=0.05, color='blue', label='Zero zone (not useful)')

        # 垂直線（0と平均値）
        ax1.axvline(x=0, color='blue', linestyle='--', linewidth=2, alpha=0.5)
        ax1.axvline(x=X_mean, color='darkred', linestyle='-', linewidth=2.5,
                   label=f'Rule mean={X_mean:.3f}', zorder=6)

        # 装飾
        ax1.set_xlabel('Change Rate X (%)', fontsize=13, fontweight='bold')
        ax1.set_ylabel('Random Jitter (for visibility)', fontsize=11)
        ax1.set_ylim(-0.5, 0.5)
        ax1.set_xlim(-5, 5)
        ax1.set_title(f'Extreme Signal Rule #{rule_idx} - 1D Distribution (Strip Plot)\n'
                     f'Signal Strength={signal_strength:.3f}, SNR={snr:.2f}, Score={score:.2f}',
                     fontsize=14, fontweight='bold')
        ax1.legend(fontsize=10, loc='upper right')
        ax1.grid(True, alpha=0.3, axis='x')

        # === 下段：ヒストグラム比較（密度プロット） ===
        ax2 = axes[1]

        # 全体分布（グレー、半透明）
        ax2.hist(self.data['X'], bins=100, alpha=0.3, color='gray',
                label=f'All data (n={len(self.data)})', density=True, edgecolor='none')

        # ルールマッチ分布（赤、濃い）
        ax2.hist(matched_X, bins=50, alpha=0.8, color='red',
                label=f'Rule matched (n={len(matched_X)})', density=True, edgecolor='black', linewidth=1.5)

        # 重要な領域を強調
        ax2.axvspan(-3, -1, alpha=0.1, color='orange')
        ax2.axvspan(1, 3, alpha=0.1, color='orange')
        ax2.axvspan(-1, 1, alpha=0.05, color='blue')

        # 統計線
        ax2.axvline(x=0, color='blue', linestyle='--', linewidth=2, alpha=0.5, label='X=0')
        ax2.axvline(x=X_mean, color='darkred', linestyle='-', linewidth=2.5,
                   label=f'Rule mean={X_mean:.3f}')
        ax2.axvline(x=X_mean + X_std, color='darkred', linestyle=':', linewidth=1.5, alpha=0.7,
                   label=f'±1σ={X_std:.3f}')
        ax2.axvline(x=X_mean - X_std, color='darkred', linestyle=':', linewidth=1.5, alpha=0.7)

        # 装飾
        ax2.set_xlabel('Change Rate X (%)', fontsize=13, fontweight='bold')
        ax2.set_ylabel('Density', fontsize=12)
        ax2.set_xlim(-5, 5)
        ax2.set_title(f'Distribution Comparison\n'
                     f'p-value={p_value:.6f}, Tail Event Rate={tail_rate:.3f}, '
                     f'Mean±Std={X_mean:.3f}±{X_std:.3f}',
                     fontsize=13, fontweight='bold')
        ax2.legend(fontsize=10, loc='upper right')
        ax2.grid(True, alpha=0.3)

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / f"extreme_rule_{rule_idx:04d}_1d_distribution.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"Saved 1D distribution visualization to {output_path}")

    def generate_report(self, top_rules):
        """
        極値シグナルルールのレポートを生成

        Parameters
        ----------
        top_rules : pd.DataFrame
            上位ルールのデータフレーム
        """
        report_path = self.output_dir / "extreme_signals_report.txt"

        with open(report_path, 'w', encoding='utf-8') as f:
            f.write("="*80 + "\n")
            f.write("Extreme Signal Rules Report\n")
            f.write(f"Forex Pair: {self.forex_pair}\n")
            f.write("="*80 + "\n\n")

            # グローバル統計
            f.write("【Global Statistics】\n")
            for key, value in self.global_stats.items():
                f.write(f"  {key}: {value:.4f}\n")
            f.write("\n")

            # フィルタリング結果
            f.write("【Filtering Results】\n")
            f.write(f"  Total rules in pool: {len(self.rules)}\n")
            f.write(f"  Rules with matches: {(self.rules['matched_count_verified'] > 0).sum()}\n")
            f.write(f"  Extreme signal rules: {len(top_rules)}\n\n")

            # トップルールの詳細
            f.write("【Top Extreme Signal Rules】\n")
            f.write("="*80 + "\n\n")

            for idx, (rule_idx, rule) in enumerate(top_rules.iterrows(), 1):
                f.write(f"Rank {idx}: Rule #{rule_idx}\n")
                f.write("-"*80 + "\n")

                # 属性表示
                f.write("Attributes: ")
                attrs = []
                for col in self.attr_cols:
                    attr_val = rule[col]
                    if pd.notna(attr_val) and attr_val != '0':
                        attrs.append(str(attr_val))
                f.write(", ".join(attrs) + "\n")

                # 統計情報
                f.write(f"  Signal Strength: {rule['signal_strength']:.4f}\n")
                f.write(f"  SNR: {rule['SNR']:.4f}\n")
                f.write(f"  Extremeness: {rule['extremeness']:.4f}\n")
                f.write(f"  p-value: {rule['p_value']:.6f}\n")
                f.write(f"  Tail Event Rate: {rule['tail_event_rate']:.4f}\n")
                f.write(f"  Match Count: {rule['matched_count_verified']}\n")
                f.write(f"  Extreme Signal Score: {rule['extreme_signal_score']:.4f}\n")
                f.write(f"  Original X_mean: {rule['X_mean']:.4f}\n")
                f.write(f"  Original X_sigma: {rule['X_sigma']:.4f}\n")
                f.write("\n")

            f.write("="*80 + "\n")
            f.write("Report generation completed\n")
            f.write("="*80 + "\n")

        print(f"\nSaved report to {report_path}")

        # CSV出力
        csv_path = self.output_dir / "extreme_signals_ranked.csv"
        top_rules.to_csv(csv_path, index=True)
        print(f"Saved ranked rules to {csv_path}")


def main():
    """メイン実行関数"""
    import sys

    print("="*60)
    print("Extreme Signal Detector")
    print("="*60)

    # 通貨ペアリスト
    if len(sys.argv) > 1 and sys.argv[1] == '--all':
        # 全通貨ペア
        forex_pairs = ['AUDCAD', 'AUDJPY', 'AUDNZD', 'AUDUSD', 'CADJPY',
                      'CHFJPY', 'EURAUD', 'EURCHF', 'EURGBP', 'EURJPY',
                      'EURUSD', 'GBPAUD', 'GBPCAD', 'GBPJPY', 'GBPUSD',
                      'NZDJPY', 'NZDUSD', 'USDCAD', 'USDCHF', 'USDJPY']
    elif len(sys.argv) > 1:
        # コマンドライン引数で指定された通貨ペア
        forex_pairs = [sys.argv[1]]
    else:
        # デフォルトはGBPCAD
        forex_pairs = ['GBPCAD']

    for pair in forex_pairs:
        print(f"\n{'#'*60}")
        print(f"# Processing: {pair}")
        print(f"{'#'*60}\n")

        try:
            # Detectorを初期化
            detector = ExtremeSignalDetector(pair)

            # シグナルスコアを計算（CSVがあればスキップ）
            if not detector.scores_loaded:
                print("\n[1/4] Calculating signal scores...")
                detector.calculate_signal_scores(sample_size=1000)
            else:
                print("\n[1/4] Using pre-computed extreme scores from C (skipping calculation)")

            # 極値ルールをランキング（方向性の偏りを重視）
            print("\n[2/4] Ranking extreme signal rules...")

            # Extremenessを読み込む（新指標: p75_abs / global_std）
            print(f"\nExtremeness (75th percentile / global_std) statistics:")
            print(f"  Max: {detector.rules['extremeness'].max():.4f}")
            print(f"  95th percentile: {detector.rules['extremeness'].quantile(0.95):.4f}")
            print(f"  Median: {detector.rules['extremeness'].median():.4f}")
            print(f"  This measures how far the 75th percentile is from zero")

            top_rules = detector.rank_extreme_rules(
                min_signal_strength=0.0,   # 制限なし
                min_snr=0.0,               # 制限なし
                min_support=10,            # 最低10サンプル（極値データのみなので閾値を下げる）
                max_p_value=1.0,           # 制限なし
                top_n=20
            )

            # extreme_signal_scoreでソート（降順）- 既にランキング済み
            # extremenessは既に新指標（p75_abs / global_std）が使われている
            if len(top_rules) > 0:
                top_rules = top_rules.sort_values('extreme_signal_score', ascending=False)

            if len(top_rules) == 0:
                print(f"\n⚠️  No extreme signal rules found for {pair}. Skipping visualization.")
                continue

            # 上位3ルールを可視化
            print("\n[3/4] Visualizing top extreme rules...")
            for i, rule_idx in enumerate(top_rules.index[:3]):
                print(f"\n  Visualizing rule #{rule_idx} (Rank {i+1})...")
                detector.visualize_extreme_rule(rule_idx)

            # レポート生成
            print("\n[4/4] Generating report...")
            detector.generate_report(top_rules)

            print(f"\n✅ {pair} processing completed!")
            print(f"   Output directory: {detector.output_dir}")

        except FileNotFoundError as e:
            print(f"\n⚠️  Skipping {pair}: {e}")
            continue
        except Exception as e:
            print(f"\n❌ Error processing {pair}: {e}")
            import traceback
            traceback.print_exc()
            continue

    print("\n" + "="*60)
    print("All extreme signal detection completed!")
    print("="*60)


if __name__ == "__main__":
    main()
