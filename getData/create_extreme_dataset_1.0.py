#!/usr/bin/env python3
"""
極値データセット作成スクリプト

既存のGNMiner個別ファイルから極値データのみを抽出し、
forex_data/extreme_gnminer/ ディレクトリに個別ファイルとして保存します。

処理フロー:
1. forex_data/gnminer_individual/ から各通貨ペアのデータを読み込み
2. |X| >= 1.0 の極値レコードのみを抽出
3. 正の極値と負の極値をカウントし、多い方を自動選択
4. forex_data/extreme_gnminer/ に個別ファイルとして保存

使用例:
    # 全通貨ペアを処理
    python3 getData/create_extreme_dataset.py

    # 特定の通貨ペアのみ処理
    python3 getData/create_extreme_dataset.py --pairs USDJPY EURJPY
"""

import pandas as pd
import os
from pathlib import Path
import argparse
from datetime import datetime

# 主要為替ペアリスト
FOREX_PAIRS = [
    # 対円ペア (7ペア)
    'USDJPY', 'EURJPY', 'GBPJPY', 'AUDJPY', 'NZDJPY', 'CADJPY', 'CHFJPY',
    # 主要クロスペア (6ペア)
    'EURUSD', 'GBPUSD', 'AUDUSD', 'NZDUSD', 'USDCAD', 'USDCHF',
    # その他の主要ペア (7ペア)
    'EURGBP', 'EURAUD', 'EURCHF', 'GBPAUD', 'GBPCAD', 'AUDCAD', 'AUDNZD',
]

class ExtremeDatasetCreator:
    """極値データセット作成クラス"""

    def __init__(self, threshold=1.0):
        """
        Parameters:
        -----------
        threshold : float
            極値判定の閾値（デフォルト: 1.0%）
        """
        self.threshold = threshold
        self.stats_list = []
        print(f"Extreme value threshold: ±{self.threshold}%")

    def create_extreme_files(self, input_file, output_dir, pair_name):
        """
        正・負の極値データファイルを個別に作成

        Parameters:
        -----------
        input_file : str
            入力ファイルパス
        output_dir : Path
            出力ディレクトリ
        pair_name : str
            通貨ペア名

        Returns:
        --------
        dict
            処理統計情報
        """
        try:
            # データ読み込み
            df = pd.read_csv(input_file)
        except FileNotFoundError:
            print(f"  ⚠ File not found: {input_file}")
            return None
        except Exception as e:
            print(f"  ✗ Error reading {input_file}: {e}")
            return None

        # Xカラムが存在するか確認
        if 'X' not in df.columns:
            print(f"  ⚠ 'X' column not found in {input_file}")
            return None

        original_count = len(df)

        # 正と負の極値を分離
        df_positive = df[df['X'] >= self.threshold].copy()
        df_negative = df[df['X'] <= -self.threshold].copy()

        positive_count = len(df_positive)
        negative_count = len(df_negative)

        # 統計情報を収集
        stats = {
            'pair': pair_name,
            'original_count': original_count,
            'positive_count': positive_count,
            'negative_count': negative_count,
            'total_extreme_count': positive_count + negative_count,
            'removed_count': original_count - positive_count - negative_count,
        }

        # 正の極値ファイルを保存
        if positive_count > 0:
            output_file_positive = output_dir / f"{pair_name}_positive.txt"
            df_positive.to_csv(output_file_positive, index=False)
            stats['positive_file'] = str(output_file_positive)
            stats['positive_X_min'] = df_positive['X'].min()
            stats['positive_X_max'] = df_positive['X'].max()
            stats['positive_X_mean'] = df_positive['X'].mean()
            stats['positive_date_min'] = df_positive['T'].min()
            stats['positive_date_max'] = df_positive['T'].max()

        # 負の極値ファイルを保存
        if negative_count > 0:
            output_file_negative = output_dir / f"{pair_name}_negative.txt"
            df_negative.to_csv(output_file_negative, index=False)
            stats['negative_file'] = str(output_file_negative)
            stats['negative_X_min'] = df_negative['X'].min()
            stats['negative_X_max'] = df_negative['X'].max()
            stats['negative_X_mean'] = df_negative['X'].mean()
            stats['negative_date_min'] = df_negative['T'].min()
            stats['negative_date_max'] = df_negative['T'].max()

        stats['success'] = (positive_count > 0 or negative_count > 0)

        return stats

    def create_all_extreme_files(self, input_dir, output_dir, pairs=None):
        """
        全通貨ペアの極値ファイルを作成

        Parameters:
        -----------
        input_dir : str
            入力ディレクトリ
        output_dir : str
            出力ディレクトリ
        pairs : list
            処理する通貨ペアのリスト（Noneの場合は全ペア）

        Returns:
        --------
        dict
            処理結果サマリー
        """
        input_dir = Path(input_dir)
        output_dir = Path(output_dir)

        # 出力ディレクトリを作成
        output_dir.mkdir(parents=True, exist_ok=True)

        # 処理する通貨ペアを決定
        if pairs is None:
            pairs = FOREX_PAIRS

        print(f"\n{'='*80}")
        print(f"Creating Extreme Value Dataset")
        print(f"{'='*80}")
        print(f"Input directory:  {input_dir}")
        print(f"Output directory: {output_dir}")
        print(f"Threshold:        ±{self.threshold}%")
        print(f"Total pairs:      {len(pairs)}")
        print()

        success_count = 0
        failed_pairs = []
        self.stats_list = []

        # 各通貨ペアを処理
        for idx, pair in enumerate(pairs, 1):
            input_file = input_dir / f"{pair}.txt"

            print(f"[{idx}/{len(pairs)}] Processing {pair}...")

            stats = self.create_extreme_files(str(input_file), output_dir, pair)

            if stats and stats['success']:
                self.stats_list.append(stats)
                success_count += 1
                print(f"  ✓ Success")
                print(f"    Original: {stats['original_count']:>4} records")
                print(f"    Positive: {stats['positive_count']:>3} records (X >= {self.threshold})")
                if stats['positive_count'] > 0:
                    print(f"      → {pair}_positive.txt")
                    print(f"      X: [{stats['positive_X_min']:.3f}, {stats['positive_X_max']:.3f}]")
                print(f"    Negative: {stats['negative_count']:>3} records (X <= -{self.threshold})")
                if stats['negative_count'] > 0:
                    print(f"      → {pair}_negative.txt")
                    print(f"      X: [{stats['negative_X_min']:.3f}, {stats['negative_X_max']:.3f}]")
                print(f"    Removed:  {stats['removed_count']:>3} records (|X| < {self.threshold})")
            else:
                failed_pairs.append(pair)
                print("  ✗ Failed")

        # サマリーを表示
        self._print_summary(success_count, failed_pairs)

        # 統計情報をCSVに保存
        self._save_statistics(output_dir)

        # READMEファイルを作成
        self._create_readme(output_dir)

        return {
            'success_count': success_count,
            'failed_pairs': failed_pairs,
            'stats_list': self.stats_list
        }

    def _print_summary(self, success_count, failed_pairs):
        """サマリーを表示"""
        print()
        print(f"{'='*80}")
        print("Processing Summary")
        print(f"{'='*80}")
        print(f"Total pairs processed: {success_count + len(failed_pairs)}")
        print(f"Success: {success_count}")
        print(f"Failed:  {len(failed_pairs)}")
        if failed_pairs:
            print(f"Failed pairs: {', '.join(failed_pairs)}")

        if self.stats_list:
            total_original = sum(s['original_count'] for s in self.stats_list)
            total_positive = sum(s['positive_count'] for s in self.stats_list)
            total_negative = sum(s['negative_count'] for s in self.stats_list)
            total_extreme = sum(s['total_extreme_count'] for s in self.stats_list)
            total_removed = sum(s['removed_count'] for s in self.stats_list)

            print()
            print(f"Total original records: {total_original:,}")
            print(f"Total extreme records:  {total_extreme:,}")
            print(f"Total removed records:  {total_removed:,}")
            print(f"Average retention rate: {100.0 * total_extreme / total_original:.2f}%")

            print()
            print("Extreme value distribution:")
            print(f"  Positive extremes (X >= {self.threshold}): {total_positive:,}")
            print(f"  Negative extremes (X <= -{self.threshold}): {total_negative:,}")
            print(f"  Total extremes: {total_extreme:,}")
            print(f"  Files created: {success_count * 2} ({success_count} pairs × 2 directions)")

    def _save_statistics(self, output_dir):
        """統計情報をCSVに保存"""
        if not self.stats_list:
            return

        stats_df = pd.DataFrame(self.stats_list)

        # 基本カラムのみ選択
        columns_order = [
            'pair', 'original_count', 'positive_count', 'negative_count',
            'total_extreme_count', 'removed_count'
        ]
        columns_order = [col for col in columns_order if col in stats_df.columns]
        stats_df = stats_df[columns_order]

        # CSVに保存
        stats_file = output_dir / 'extreme_dataset_stats.csv'
        stats_df.to_csv(stats_file, index=False)
        print(f"\n✓ Statistics saved to: {stats_file}")

    def _create_readme(self, output_dir):
        """READMEファイルを作成"""
        readme_path = output_dir / 'README.md'

        with open(readme_path, 'w', encoding='utf-8') as f:
            f.write("# Extreme Value Forex Dataset (Separated Directions)\n\n")
            f.write(f"Created: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")

            f.write("## Dataset Description\n\n")
            f.write("This dataset contains **directional extreme value data** for forex trading analysis.\n")
            f.write("**Each currency pair has TWO separate files** - one for positive extremes (BUY signals)\n")
            f.write("and one for negative extremes (SELL signals).\n\n")

            f.write("### Filtering Criteria\n\n")
            f.write(f"- **Threshold**: ±{self.threshold}%\n")
            f.write("- **Direction Separation**: SEPARATED into individual files\n")
            f.write(f"- **Positive Extreme (`{{PAIR}}_positive.txt`)**: X >= {self.threshold}% (upward movement, BUY signal)\n")
            f.write(f"- **Negative Extreme (`{{PAIR}}_negative.txt`)**: X <= -{self.threshold}% (downward movement, SELL signal)\n")
            f.write("- **Excluded**: -1.0% < X < 1.0% (normal fluctuations near zero)\n\n")

            f.write("### File Structure\n\n")
            f.write("```\n")
            f.write("forex_data/extreme_gnminer/\n")
            f.write("├── USDJPY_positive.txt   # Positive extremes (X >= 1.0%)\n")
            f.write("├── USDJPY_negative.txt   # Negative extremes (X <= -1.0%)\n")
            f.write("├── EURJPY_positive.txt\n")
            f.write("├── EURJPY_negative.txt\n")
            f.write("└── ...\n")
            f.write("```\n\n")

            f.write("### Data Format\n\n")
            f.write("Each file contains:\n")
            f.write("- **Attributes**: Binary features (Up/Stay/Down) for all 20 forex pairs\n")
            f.write("- **X**: Target variable (percentage change rate)\n")
            f.write("- **T**: Timestamp (YYYY-MM-DD format)\n\n")

            f.write("### Files\n\n")
            f.write(f"- Total forex pairs: {len(self.stats_list)}\n")
            f.write(f"- Total files created: {len(self.stats_list) * 2} ({len(self.stats_list)} pairs × 2 directions)\n")
            f.write("- `extreme_dataset_stats.csv`: Detailed statistics for each pair\n\n")

            if self.stats_list:
                f.write("### Summary Statistics\n\n")
                total_original = sum(s['original_count'] for s in self.stats_list)
                total_positive = sum(s['positive_count'] for s in self.stats_list)
                total_negative = sum(s['negative_count'] for s in self.stats_list)
                total_extreme = sum(s['total_extreme_count'] for s in self.stats_list)
                total_removed = sum(s['removed_count'] for s in self.stats_list)
                avg_retention_rate = 100.0 * total_extreme / total_original if total_original > 0 else 0.0

                f.write(f"- Original records: {total_original:,}\n")
                f.write(f"- Extreme records: {total_extreme:,}\n")
                f.write(f"- Removed records: {total_removed:,}\n")
                f.write(f"- Average retention rate: {avg_retention_rate:.2f}%\n\n")

                f.write("### Extreme Value Distribution\n\n")
                f.write(f"- Positive extremes (X >= {self.threshold}%): {total_positive:,} records\n")
                f.write(f"- Negative extremes (X <= -{self.threshold}%): {total_negative:,} records\n")
                f.write(f"- Total extreme records: {total_extreme:,}\n\n")

            f.write("### Usage\n\n")
            f.write("These files are designed for use with GNMiner Phase 2 for rule mining.\n\n")
            f.write("**Key Features**:\n")
            f.write("1. **Directional Clarity**: Each file contains only one direction of extreme movements\n")
            f.write("2. **Clear Trading Signals**: Positive files for BUY rules, negative files for SELL rules\n")
            f.write("3. **Independent Analysis**: Rules can be discovered independently for each direction\n\n")

            f.write("**Example Usage**:\n")
            f.write("```bash\n")
            f.write("# Discover BUY rules for USDJPY\n")
            f.write("./main USDJPY positive\n\n")
            f.write("# Discover SELL rules for USDJPY\n")
            f.write("./main USDJPY negative\n")
            f.write("```\n\n")

            f.write("### Note\n\n")
            f.write("- This dataset **separates directions** for clear BUY/SELL signal discovery\n")
            f.write("- Each direction is analyzed independently to avoid signal mixing\n")
            f.write("- Results will be stored in separate directories: `output/{PAIR}/positive/` and `output/{PAIR}/negative/`\n")

        print(f"✓ README created: {readme_path}")


def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Create extreme value dataset from GNMiner individual files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Process all forex pairs
  python3 getData/create_extreme_dataset.py

  # Process specific pairs
  python3 getData/create_extreme_dataset.py --pairs USDJPY EURJPY GBPJPY

  # Specify custom directories
  python3 getData/create_extreme_dataset.py --input forex_data/gnminer_individual --output forex_data/extreme_gnminer

  # Change threshold
  python3 getData/create_extreme_dataset.py --threshold 0.5
        """
    )
    parser.add_argument(
        '--input',
        default='forex_data/gnminer_individual',
        help='Input directory containing GNMiner format files (default: forex_data/gnminer_individual)'
    )
    parser.add_argument(
        '--output',
        default='forex_data/extreme_gnminer',
        help='Output directory for extreme value files (default: forex_data/extreme_gnminer)'
    )
    parser.add_argument(
        '--threshold',
        type=float,
        default=1.0,
        help='Extreme value threshold in percentage (default: 1.0)'
    )
    parser.add_argument(
        '--pairs',
        nargs='+',
        default=None,
        help='Specific forex pairs to process (default: all pairs)'
    )

    args = parser.parse_args()

    # データセット作成
    creator = ExtremeDatasetCreator(threshold=args.threshold)
    result = creator.create_all_extreme_files(
        input_dir=args.input,
        output_dir=args.output,
        pairs=args.pairs
    )

    print()
    print("✓ Extreme value dataset creation complete!")
    print(f"  Output directory: {args.output}")

if __name__ == '__main__':
    main()
