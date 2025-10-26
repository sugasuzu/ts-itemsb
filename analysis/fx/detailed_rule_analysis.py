"""
詳細なルール分析スクリプト

CSVファイルから上位ルールの統計を読み込み、以下の詳細分析を実施：
1. 通貨ペア間のルール特性比較
2. X_mean/X_sigmaの統計的分布分析
3. 時系列パターン（月・四半期・曜日）の傾向分析
4. Support rateとX値の相関分析
5. 異常値・外れ値の検出
6. クラスタリング分析
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from scipy import stats
from sklearn.preprocessing import StandardScaler
from sklearn.cluster import KMeans
from sklearn.decomposition import PCA
import warnings
warnings.filterwarnings('ignore')

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

# Seabornスタイル設定
sns.set_style("whitegrid")
sns.set_palette("husl")


class DetailedRuleAnalyzer:
    """詳細なルール分析クラス"""

    def __init__(self, base_dir=None):
        """
        Parameters
        ----------
        base_dir : str or Path
            プロジェクトのベースディレクトリ
        """
        if base_dir is None:
            script_dir = Path(__file__).parent
            base_dir = script_dir.parent.parent

        self.base_dir = Path(base_dir).resolve()
        self.fx_dir = self.base_dir / "analysis" / "fx"
        self.output_dir = self.fx_dir / "detailed_analysis"
        self.output_dir.mkdir(parents=True, exist_ok=True)

        # データ読み込み
        self.load_all_rules()

    def load_all_rules(self):
        """全通貨ペアのルールサマリーを読み込む"""
        forex_pairs = ['USDJPY', 'EURAUD', 'GBPCAD', 'AUDNZD']
        all_data = []

        for pair in forex_pairs:
            csv_path = self.fx_dir / pair / "top_10_rules_summary.csv"

            if not csv_path.exists():
                print(f"Warning: {csv_path} not found, skipping {pair}")
                continue

            df = pd.read_csv(csv_path)
            df['forex_pair'] = pair
            all_data.append(df)

            print(f"Loaded {len(df)} rules from {pair}")

        if not all_data:
            raise FileNotFoundError("No rule summary files found")

        self.all_rules = pd.concat(all_data, ignore_index=True)
        print(f"\nTotal rules loaded: {len(self.all_rules)}")
        print(f"Forex pairs: {self.all_rules['forex_pair'].unique()}")

    def analyze_basic_statistics(self):
        """基本統計量を計算・表示"""
        print("\n" + "="*60)
        print("基本統計量")
        print("="*60)

        # 全体の統計
        print("\n【全体統計】")
        numeric_cols = ['n_matches', 'support_rate', 'X_mean_rule', 'X_sigma_rule', 'num_attributes']
        print(self.all_rules[numeric_cols].describe())

        # 通貨ペア別の統計
        print("\n【通貨ペア別統計】")
        grouped = self.all_rules.groupby('forex_pair')[numeric_cols].mean()
        print(grouped)

        # 統計サマリーを保存
        summary_path = self.output_dir / "basic_statistics.csv"
        grouped.to_csv(summary_path)
        print(f"\nSaved to {summary_path}")

        return grouped

    def plot_distribution_analysis(self):
        """分布分析のプロット"""
        fig, axes = plt.subplots(3, 2, figsize=(16, 18))
        fig.suptitle('Rule Distribution Analysis Across Forex Pairs',
                     fontsize=16, fontweight='bold', y=0.995)

        # 1. Support rate分布
        ax1 = axes[0, 0]
        for pair in self.all_rules['forex_pair'].unique():
            data = self.all_rules[self.all_rules['forex_pair'] == pair]['support_rate']
            ax1.hist(data, alpha=0.5, label=pair, bins=15, edgecolor='black')
        ax1.set_xlabel('Support Rate')
        ax1.set_ylabel('Frequency')
        ax1.set_title('Support Rate Distribution by Forex Pair')
        ax1.legend()
        ax1.grid(True, alpha=0.3)

        # 2. X_mean分布
        ax2 = axes[0, 1]
        for pair in self.all_rules['forex_pair'].unique():
            data = self.all_rules[self.all_rules['forex_pair'] == pair]['X_mean_rule']
            ax2.hist(data, alpha=0.5, label=pair, bins=15, edgecolor='black')
        ax2.set_xlabel('X Mean (Predicted)')
        ax2.set_ylabel('Frequency')
        ax2.set_title('X Mean Distribution by Forex Pair')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        # 3. X_sigma分布（Box plot）
        ax3 = axes[1, 0]
        self.all_rules.boxplot(column='X_sigma_rule', by='forex_pair', ax=ax3)
        ax3.set_xlabel('Forex Pair')
        ax3.set_ylabel('X Sigma (Uncertainty)')
        ax3.set_title('X Sigma Distribution by Forex Pair')
        plt.sca(ax3)
        plt.xticks(rotation=45)

        # 4. Support rate vs X_mean散布図
        ax4 = axes[1, 1]
        for pair in self.all_rules['forex_pair'].unique():
            data = self.all_rules[self.all_rules['forex_pair'] == pair]
            ax4.scatter(data['support_rate'], data['X_mean_rule'],
                       label=pair, alpha=0.6, s=100)
        ax4.set_xlabel('Support Rate')
        ax4.set_ylabel('X Mean (Predicted)')
        ax4.set_title('Support Rate vs X Mean')
        ax4.legend()
        ax4.grid(True, alpha=0.3)

        # 5. n_matches分布
        ax5 = axes[2, 0]
        self.all_rules.boxplot(column='n_matches', by='forex_pair', ax=ax5)
        ax5.set_xlabel('Forex Pair')
        ax5.set_ylabel('Number of Matches')
        ax5.set_title('Match Count Distribution by Forex Pair')
        plt.sca(ax5)
        plt.xticks(rotation=45)

        # 6. 相関ヒートマップ
        ax6 = axes[2, 1]
        numeric_cols = ['support_rate', 'X_mean_rule', 'X_sigma_rule', 'n_matches', 'num_attributes']
        corr_matrix = self.all_rules[numeric_cols].corr()
        sns.heatmap(corr_matrix, annot=True, fmt='.3f', cmap='coolwarm',
                   center=0, ax=ax6, square=True, cbar_kws={'shrink': 0.8})
        ax6.set_title('Correlation Matrix of Rule Features')

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / "distribution_analysis.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"\nSaved distribution analysis to {output_path}")

    def analyze_temporal_patterns(self):
        """時系列パターン分析"""
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle('Temporal Pattern Analysis',
                     fontsize=16, fontweight='bold')

        # 1. 月別分布
        ax1 = axes[0, 0]
        month_counts = self.all_rules.groupby(['forex_pair', 'dominant_month']).size().unstack(fill_value=0)
        month_counts.T.plot(kind='bar', ax=ax1, width=0.8)
        ax1.set_xlabel('Month')
        ax1.set_ylabel('Frequency')
        ax1.set_title('Dominant Month Distribution')
        ax1.legend(title='Forex Pair', bbox_to_anchor=(1.05, 1), loc='upper left')
        ax1.grid(True, alpha=0.3, axis='y')
        plt.sca(ax1)
        plt.xticks(rotation=0)

        # 2. 四半期別分布
        ax2 = axes[0, 1]
        quarter_counts = self.all_rules.groupby(['forex_pair', 'dominant_quarter']).size().unstack(fill_value=0)
        quarter_counts.T.plot(kind='bar', ax=ax2, width=0.8)
        ax2.set_xlabel('Quarter')
        ax2.set_ylabel('Frequency')
        ax2.set_title('Dominant Quarter Distribution')
        ax2.legend(title='Forex Pair', bbox_to_anchor=(1.05, 1), loc='upper left')
        ax2.grid(True, alpha=0.3, axis='y')
        plt.sca(ax2)
        plt.xticks(rotation=0)

        # 3. 曜日別分布
        ax3 = axes[1, 0]
        dow_counts = self.all_rules.groupby(['forex_pair', 'dominant_day']).size().unstack(fill_value=0)
        dow_labels = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
        dow_counts.T.plot(kind='bar', ax=ax3, width=0.8)
        ax3.set_xlabel('Day of Week')
        ax3.set_ylabel('Frequency')
        ax3.set_title('Dominant Day of Week Distribution')
        ax3.set_xticklabels(dow_labels[:len(dow_counts.columns)], rotation=0)
        ax3.legend(title='Forex Pair', bbox_to_anchor=(1.05, 1), loc='upper left')
        ax3.grid(True, alpha=0.3, axis='y')

        # 4. 月別のX_mean平均値
        ax4 = axes[1, 1]
        month_xmean = self.all_rules.groupby(['forex_pair', 'dominant_month'])['X_mean_rule'].mean().unstack(fill_value=0)
        month_xmean.T.plot(kind='line', ax=ax4, marker='o', linewidth=2)
        ax4.set_xlabel('Month')
        ax4.set_ylabel('Average X Mean')
        ax4.set_title('Average X Mean by Month')
        ax4.legend(title='Forex Pair', bbox_to_anchor=(1.05, 1), loc='upper left')
        ax4.grid(True, alpha=0.3)
        ax4.axhline(y=0, color='red', linestyle='--', linewidth=1, alpha=0.5)

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / "temporal_pattern_analysis.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"Saved temporal pattern analysis to {output_path}")

    def perform_clustering_analysis(self, n_clusters=3):
        """クラスタリング分析"""
        print(f"\n{'='*60}")
        print(f"Clustering Analysis (k={n_clusters})")
        print(f"{'='*60}")

        # 特徴量を選択
        features = ['support_rate', 'X_mean_rule', 'X_sigma_rule', 'n_matches']
        X = self.all_rules[features].values

        # 標準化
        scaler = StandardScaler()
        X_scaled = scaler.fit_transform(X)

        # K-meansクラスタリング
        kmeans = KMeans(n_clusters=n_clusters, random_state=42, n_init=10)
        self.all_rules['cluster'] = kmeans.fit_predict(X_scaled)

        # PCAで2次元に削減
        pca = PCA(n_components=2)
        X_pca = pca.fit_transform(X_scaled)
        self.all_rules['pca1'] = X_pca[:, 0]
        self.all_rules['pca2'] = X_pca[:, 1]

        print(f"\nPCA explained variance ratio: {pca.explained_variance_ratio_}")
        print(f"Total explained variance: {pca.explained_variance_ratio_.sum():.3f}")

        # クラスタごとの統計
        print("\n【クラスタ別統計】")
        cluster_stats = self.all_rules.groupby('cluster')[features].mean()
        print(cluster_stats)

        # プロット
        fig, axes = plt.subplots(1, 2, figsize=(16, 6))
        fig.suptitle(f'Clustering Analysis (k={n_clusters})',
                     fontsize=16, fontweight='bold')

        # 1. PCA散布図（クラスタ別色分け）
        ax1 = axes[0]
        scatter = ax1.scatter(self.all_rules['pca1'], self.all_rules['pca2'],
                             c=self.all_rules['cluster'], cmap='viridis',
                             s=100, alpha=0.6, edgecolors='black', linewidth=0.5)
        ax1.set_xlabel(f'PC1 ({pca.explained_variance_ratio_[0]:.2%} variance)')
        ax1.set_ylabel(f'PC2 ({pca.explained_variance_ratio_[1]:.2%} variance)')
        ax1.set_title('PCA Clustering Visualization')
        ax1.grid(True, alpha=0.3)
        plt.colorbar(scatter, ax=ax1, label='Cluster')

        # クラスタ中心をプロット
        centers_pca = pca.transform(kmeans.cluster_centers_)
        ax1.scatter(centers_pca[:, 0], centers_pca[:, 1],
                   c='red', s=300, alpha=0.8, marker='X',
                   edgecolors='black', linewidth=2, label='Centroids')
        ax1.legend()

        # 2. PCA散布図（通貨ペア別色分け）
        ax2 = axes[1]
        for pair in self.all_rules['forex_pair'].unique():
            data = self.all_rules[self.all_rules['forex_pair'] == pair]
            ax2.scatter(data['pca1'], data['pca2'],
                       label=pair, s=100, alpha=0.6, edgecolors='black', linewidth=0.5)
        ax2.set_xlabel(f'PC1 ({pca.explained_variance_ratio_[0]:.2%} variance)')
        ax2.set_ylabel(f'PC2 ({pca.explained_variance_ratio_[1]:.2%} variance)')
        ax2.set_title('PCA by Forex Pair')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / f"clustering_analysis_k{n_clusters}.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"\nSaved clustering analysis to {output_path}")

        # クラスタ統計を保存
        cluster_path = self.output_dir / f"cluster_statistics_k{n_clusters}.csv"
        cluster_stats.to_csv(cluster_path)
        print(f"Saved cluster statistics to {cluster_path}")

        return cluster_stats

    def detect_outliers(self):
        """外れ値検出"""
        print(f"\n{'='*60}")
        print("Outlier Detection")
        print(f"{'='*60}")

        features = ['support_rate', 'X_mean_rule', 'X_sigma_rule', 'n_matches']

        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle('Outlier Detection Analysis',
                     fontsize=16, fontweight='bold')

        outlier_summary = []

        for idx, feature in enumerate(features):
            ax = axes[idx // 2, idx % 2]

            # Z-scoreで外れ値検出
            z_scores = np.abs(stats.zscore(self.all_rules[feature]))
            outliers = z_scores > 3

            # 通常点と外れ値をプロット
            normal_data = self.all_rules[~outliers]
            outlier_data = self.all_rules[outliers]

            # 通貨ペア別に色分け
            for pair in self.all_rules['forex_pair'].unique():
                pair_normal = normal_data[normal_data['forex_pair'] == pair]
                pair_outlier = outlier_data[outlier_data['forex_pair'] == pair]

                ax.scatter(range(len(pair_normal)), pair_normal[feature],
                          label=f'{pair} (normal)', alpha=0.5, s=50)
                if len(pair_outlier) > 0:
                    ax.scatter(range(len(normal_data), len(normal_data) + len(pair_outlier)),
                             pair_outlier[feature],
                             label=f'{pair} (outlier)', marker='x', s=100, linewidths=2)

            ax.set_xlabel('Sample Index')
            ax.set_ylabel(feature)
            ax.set_title(f'{feature} - Outliers: {outliers.sum()}')
            ax.legend(fontsize=8)
            ax.grid(True, alpha=0.3)

            # 外れ値情報を収集
            outlier_summary.append({
                'feature': feature,
                'n_outliers': outliers.sum(),
                'outlier_percentage': outliers.sum() / len(self.all_rules) * 100,
                'mean': self.all_rules[feature].mean(),
                'std': self.all_rules[feature].std(),
            })

            print(f"\n{feature}:")
            print(f"  Outliers: {outliers.sum()} ({outliers.sum()/len(self.all_rules)*100:.1f}%)")
            if outliers.sum() > 0:
                print(f"  Outlier values: {self.all_rules[outliers][feature].values}")

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / "outlier_detection.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"\nSaved outlier detection to {output_path}")

        # サマリー保存
        outlier_df = pd.DataFrame(outlier_summary)
        summary_path = self.output_dir / "outlier_summary.csv"
        outlier_df.to_csv(summary_path, index=False)
        print(f"Saved outlier summary to {summary_path}")

    def analyze_prediction_accuracy(self):
        """予測精度分析（X_mean vs X_sigma）"""
        print(f"\n{'='*60}")
        print("Prediction Accuracy Analysis")
        print(f"{'='*60}")

        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle('Prediction Accuracy Analysis (X_mean vs X_sigma)',
                     fontsize=16, fontweight='bold')

        # 1. X_mean vs X_sigma散布図
        ax1 = axes[0, 0]
        for pair in self.all_rules['forex_pair'].unique():
            data = self.all_rules[self.all_rules['forex_pair'] == pair]
            ax1.scatter(data['X_mean_rule'], data['X_sigma_rule'],
                       label=pair, alpha=0.6, s=100)
        ax1.set_xlabel('X Mean (Predicted Change)')
        ax1.set_ylabel('X Sigma (Uncertainty)')
        ax1.set_title('Prediction vs Uncertainty')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        ax1.axvline(x=0, color='red', linestyle='--', alpha=0.5)

        # 2. Signal-to-Noise Ratio (SNR = |X_mean| / X_sigma)
        ax2 = axes[0, 1]
        self.all_rules['SNR'] = np.abs(self.all_rules['X_mean_rule']) / self.all_rules['X_sigma_rule']

        for pair in self.all_rules['forex_pair'].unique():
            data = self.all_rules[self.all_rules['forex_pair'] == pair]['SNR']
            ax2.hist(data, alpha=0.5, label=pair, bins=15, edgecolor='black')

        ax2.set_xlabel('Signal-to-Noise Ratio (|X_mean| / X_sigma)')
        ax2.set_ylabel('Frequency')
        ax2.set_title('SNR Distribution')
        ax2.legend()
        ax2.grid(True, alpha=0.3, axis='y')

        print(f"\nAverage SNR by Forex Pair:")
        print(self.all_rules.groupby('forex_pair')['SNR'].mean())

        # 3. High confidence rules (low X_sigma)
        ax3 = axes[1, 0]
        threshold = self.all_rules['X_sigma_rule'].quantile(0.25)  # 下位25%
        high_conf = self.all_rules[self.all_rules['X_sigma_rule'] <= threshold]

        high_conf_counts = high_conf.groupby('forex_pair').size()
        ax3.bar(high_conf_counts.index, high_conf_counts.values,
               color='steelblue', alpha=0.7, edgecolor='black')
        ax3.set_xlabel('Forex Pair')
        ax3.set_ylabel('Count')
        ax3.set_title(f'High Confidence Rules (X_sigma ≤ {threshold:.3f})')
        ax3.grid(True, alpha=0.3, axis='y')
        plt.sca(ax3)
        plt.xticks(rotation=45)

        # 4. 予測方向の分布（正/負）
        ax4 = axes[1, 1]
        self.all_rules['direction'] = np.where(self.all_rules['X_mean_rule'] > 0, 'Positive', 'Negative')
        direction_counts = self.all_rules.groupby(['forex_pair', 'direction']).size().unstack(fill_value=0)

        direction_counts.plot(kind='bar', ax=ax4, width=0.8, color=['red', 'green'], alpha=0.7)
        ax4.set_xlabel('Forex Pair')
        ax4.set_ylabel('Count')
        ax4.set_title('Prediction Direction Distribution')
        ax4.legend(title='Direction')
        ax4.grid(True, alpha=0.3, axis='y')
        plt.sca(ax4)
        plt.xticks(rotation=45)

        plt.tight_layout()

        # 保存
        output_path = self.output_dir / "prediction_accuracy_analysis.png"
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

        print(f"\nSaved prediction accuracy analysis to {output_path}")

    def generate_comprehensive_report(self):
        """包括的なレポート生成"""
        print(f"\n{'='*60}")
        print("Generating Comprehensive Report")
        print(f"{'='*60}")

        report_path = self.output_dir / "comprehensive_report.txt"

        with open(report_path, 'w', encoding='utf-8') as f:
            f.write("="*80 + "\n")
            f.write("Forex Rule Mining - Comprehensive Analysis Report\n")
            f.write("="*80 + "\n\n")

            # 基本情報
            f.write("【データセット情報】\n")
            f.write(f"Total Rules: {len(self.all_rules)}\n")
            f.write(f"Forex Pairs: {', '.join(self.all_rules['forex_pair'].unique())}\n")
            f.write(f"Rules per Pair: {len(self.all_rules) / len(self.all_rules['forex_pair'].unique()):.1f}\n\n")

            # 統計サマリー
            f.write("【統計サマリー】\n")
            f.write(self.all_rules[['support_rate', 'X_mean_rule', 'X_sigma_rule', 'n_matches']].describe().to_string())
            f.write("\n\n")

            # 通貨ペア別統計
            f.write("【通貨ペア別統計】\n")
            pair_stats = self.all_rules.groupby('forex_pair')[['support_rate', 'X_mean_rule', 'X_sigma_rule']].agg(['mean', 'std'])
            f.write(pair_stats.to_string())
            f.write("\n\n")

            # 時系列パターン
            f.write("【時系列パターン】\n")
            f.write("Dominant Month:\n")
            month_dist = self.all_rules.groupby('dominant_month').size()
            f.write(month_dist.to_string())
            f.write("\n\nDominant Quarter:\n")
            quarter_dist = self.all_rules.groupby('dominant_quarter').size()
            f.write(quarter_dist.to_string())
            f.write("\n\nDominant Day of Week:\n")
            dow_dist = self.all_rules.groupby('dominant_day').size()
            f.write(dow_dist.to_string())
            f.write("\n\n")

            # 予測統計
            f.write("【予測統計】\n")
            f.write(f"Average SNR: {self.all_rules['SNR'].mean():.3f}\n")
            f.write(f"Positive predictions: {(self.all_rules['X_mean_rule'] > 0).sum()} ({(self.all_rules['X_mean_rule'] > 0).sum()/len(self.all_rules)*100:.1f}%)\n")
            f.write(f"Negative predictions: {(self.all_rules['X_mean_rule'] < 0).sum()} ({(self.all_rules['X_mean_rule'] < 0).sum()/len(self.all_rules)*100:.1f}%)\n")
            f.write(f"Neutral predictions: {(self.all_rules['X_mean_rule'] == 0).sum()} ({(self.all_rules['X_mean_rule'] == 0).sum()/len(self.all_rules)*100:.1f}%)\n\n")

            # トップルール
            f.write("【トップ5ルール（Support Rate順）】\n")
            top5 = self.all_rules.nlargest(5, 'support_rate')[['forex_pair', 'rule_idx', 'support_rate', 'X_mean_rule', 'X_sigma_rule', 'n_matches']]
            f.write(top5.to_string(index=False))
            f.write("\n\n")

            f.write("="*80 + "\n")
            f.write("Report generation completed\n")
            f.write("="*80 + "\n")

        print(f"\nSaved comprehensive report to {report_path}")


def main():
    """メイン実行関数"""

    print("="*60)
    print("Detailed Rule Analysis")
    print("="*60)

    # Analyzerを初期化
    analyzer = DetailedRuleAnalyzer()

    # 1. 基本統計量
    print("\n[1/6] Analyzing basic statistics...")
    analyzer.analyze_basic_statistics()

    # 2. 分布分析
    print("\n[2/6] Creating distribution plots...")
    analyzer.plot_distribution_analysis()

    # 3. 時系列パターン分析
    print("\n[3/6] Analyzing temporal patterns...")
    analyzer.analyze_temporal_patterns()

    # 4. クラスタリング分析
    print("\n[4/6] Performing clustering analysis...")
    analyzer.perform_clustering_analysis(n_clusters=3)

    # 5. 外れ値検出
    print("\n[5/6] Detecting outliers...")
    analyzer.detect_outliers()

    # 6. 予測精度分析
    print("\n[6/6] Analyzing prediction accuracy...")
    analyzer.analyze_prediction_accuracy()

    # 包括的レポート生成
    print("\n[Final] Generating comprehensive report...")
    analyzer.generate_comprehensive_report()

    print("\n" + "="*60)
    print("All detailed analysis completed!")
    print(f"Results saved to: {analyzer.output_dir}")
    print("="*60)


if __name__ == "__main__":
    main()
