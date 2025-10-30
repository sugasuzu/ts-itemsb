#!/usr/bin/env python3
"""
GNMinerフォーマット変換スクリプト (標準化版)

生の価格データ（株価または為替）をGNMinerで使用できる形式に変換します。
変化率を標準化することで、局所的な分布を改善します。

変換手順:
1. 日次の変化率（%）を計算
2. 変化率を標準化 (Z-score, Rolling Z-score, Rank変換)
3. 標準化された値をUp/Stay/Downに分類
4. バイナリ（0/1）形式に変換
5. 個別ファイルに分割（オプション）
"""

import pandas as pd
import numpy as np
import os
from pathlib import Path
import argparse
from scipy.stats import rankdata

class GNMinerDataConverter:
    """GNMinerフォーマット変換クラス (標準化機能付き)"""

    def __init__(self, stay_threshold=0.0, standardize_method='z-score', rolling_window=20):
        """
        Parameters:
        -----------
        stay_threshold : float
            Stay判定の閾値（デフォルト: 0.0 = 完全一致のみ）
        standardize_method : str
            標準化手法 ('z-score', 'rolling-z-score', 'rank', 'none')
        rolling_window : int
            ローリング窓サイズ(rolling-z-scoreの場合)
        """
        self.stay_threshold = abs(stay_threshold)
        self.standardize_method = standardize_method
        self.rolling_window = rolling_window
        print(f"Stay threshold: ±{self.stay_threshold}")
        print(f"Standardization method: {standardize_method}", end="")
        if standardize_method == 'rolling-z-score':
            print(f" (window={rolling_window})")
        else:
            print()

    def standardize_series(self, series):
        """
        系列を標準化

        Parameters:
        -----------
        series : pd.Series
            変化率データ

        Returns:
        --------
        pd.Series
            標準化されたデータ
        """
        if self.standardize_method == 'none':
            return series

        elif self.standardize_method == 'z-score':
            # 標準Z-score: (X - mean) / std
            mean = series.mean()
            std = series.std()
            if std == 0:
                return series - mean  # 標準偏差が0の場合は平均を引くだけ
            return (series - mean) / std

        elif self.standardize_method == 'rolling-z-score':
            # ローリングZ-score: 時変的なボラティリティに対応
            rolling_mean = series.rolling(window=self.rolling_window, min_periods=1).mean()
            rolling_std = series.rolling(window=self.rolling_window, min_periods=1).std()
            # 標準偏差が0の場合の処理
            rolling_std = rolling_std.replace(0, np.nan)
            standardized = (series - rolling_mean) / rolling_std
            # NaNを0で埋める
            return standardized.fillna(0)

        elif self.standardize_method == 'rank':
            # パーセンタイルランク変換: [0, 1]の範囲に変換
            # NaNを除外してランク付け
            valid_mask = ~series.isna()
            result = pd.Series(np.nan, index=series.index)
            if valid_mask.sum() > 0:
                valid_values = series[valid_mask]
                ranks = rankdata(valid_values, method='average') / len(valid_values)
                # [0, 1] -> [-1, 1]に変換（中心を0にするため）
                ranks = (ranks - 0.5) * 2
                result[valid_mask] = ranks
            return result

        else:
            raise ValueError(f"Unknown standardization method: {self.standardize_method}")

    def calculate_returns(self, df, price_columns):
        """
        日次変化率を計算

        Parameters:
        -----------
        df : pd.DataFrame
            価格データ
        price_columns : list
            価格カラム名のリスト

        Returns:
        --------
        pd.DataFrame
            変化率データ
        """
        returns_df = pd.DataFrame()
        # timestampカラムをTとして保存（タイムゾーンを除去してYYYY-MM-DD形式に）
        if 'timestamp' in df.columns:
            # タイムゾーン付きの場合は日付部分のみ抽出
            datetime_series = pd.to_datetime(df['timestamp'], utc=True)
            returns_df['T'] = datetime_series.dt.strftime('%Y-%m-%d')
        elif 'T' in df.columns:
            # 既にTがある場合もタイムゾーンを除去
            datetime_series = pd.to_datetime(df['T'], utc=True)
            returns_df['T'] = datetime_series.dt.strftime('%Y-%m-%d')

        for col in price_columns:
            if col in df.columns:
                # 変化率（%）を計算
                returns_df[col] = df[col].pct_change() * 100

        return returns_df

    def standardize_returns(self, returns_df, price_columns):
        """
        変化率を標準化

        Parameters:
        -----------
        returns_df : pd.DataFrame
            変化率データ
        price_columns : list
            価格カラム名のリスト

        Returns:
        --------
        pd.DataFrame
            標準化された変化率データ
        """
        standardized_df = pd.DataFrame()
        standardized_df['T'] = returns_df['T']

        for col in price_columns:
            if col in returns_df.columns:
                standardized_df[col] = self.standardize_series(returns_df[col])

        return standardized_df

    def classify_movements(self, standardized_df, price_columns):
        """
        標準化された変化率をUp/Stay/Downに分類

        Parameters:
        -----------
        standardized_df : pd.DataFrame
            標準化された変化率データ
        price_columns : list
            価格カラム名のリスト

        Returns:
        --------
        pd.DataFrame
            分類データ（各カラムに Up/Stay/Down）
        """
        classified_df = pd.DataFrame()
        classified_df['T'] = standardized_df['T']

        for col in price_columns:
            if col not in standardized_df.columns:
                continue

            values = standardized_df[col]

            # Up/Stay/Downに分類
            if self.stay_threshold == 0.0:
                # 完全一致のみStay
                classified_df[f'{col}_Up'] = (values > 0).astype(int)
                classified_df[f'{col}_Stay'] = (values == 0).astype(int)
                classified_df[f'{col}_Down'] = (values < 0).astype(int)
            else:
                # 閾値内をStay
                classified_df[f'{col}_Up'] = (values > self.stay_threshold).astype(int)
                classified_df[f'{col}_Stay'] = (
                    (values >= -self.stay_threshold) &
                    (values <= self.stay_threshold)
                ).astype(int)
                classified_df[f'{col}_Down'] = (values < -self.stay_threshold).astype(int)

        return classified_df

    def convert(self, input_file, output_file=None, add_target=True):
        """
        データを変換

        Parameters:
        -----------
        input_file : str
            入力ファイルパス
        output_file : str
            出力ファイルパス（Noneの場合は自動生成）
        add_target : bool
            予測対象変数Xを追加するか

        Returns:
        --------
        pd.DataFrame
            変換後のデータ
        """
        print(f"=== Converting {input_file} ===")

        # データ読み込み
        df = pd.read_csv(input_file)
        print(f"Loaded {len(df)} records")

        # timestampまたはTカラムを確認
        if 'timestamp' not in df.columns and 'T' not in df.columns:
            raise ValueError("'timestamp' or 'T' column not found")

        # 価格カラムを抽出（timestampまたはT以外の全カラム）
        price_columns = [col for col in df.columns if col not in ['timestamp', 'T']]
        print(f"Found {len(price_columns)} price columns")

        # 変化率を計算
        print("Calculating returns...")
        returns_df = self.calculate_returns(df, price_columns)

        # 変化率を標準化
        print("Standardizing returns...")
        standardized_df = self.standardize_returns(returns_df, price_columns)

        # 元の変化率と標準化された変化率を保持（個別ファイル作成時に使用）
        self.returns_df = returns_df.copy()  # 元の変化率（予測対象Xで使用）
        self.standardized_df = standardized_df.copy()  # 標準化された変化率
        self.price_columns = price_columns

        # Up/Stay/Downに分類（標準化された値を使用）
        print("Classifying movements...")
        classified_df = self.classify_movements(standardized_df, price_columns)

        # NaN を除去（最初の行）
        classified_df = classified_df.dropna()

        print(f"Final records: {len(classified_df)}")

        # 保存
        if output_file is None:
            # 入力ファイルがforex_data/内なら、出力もforex_data/内に
            if 'forex_data' in input_file:
                output_file = input_file.replace('.csv', '_gnminer_std.csv')
            else:
                # それ以外は同じディレクトリに
                output_file = input_file.replace('.csv', '_gnminer_std.csv')

        classified_df.to_csv(output_file, index=False)
        print(f"✓ Saved to: {output_file}")

        # 統計情報を表示
        print()
        print("=== Statistics ===")
        for col in price_columns[:5]:  # 最初の5つだけ表示
            if f'{col}_Up' in classified_df.columns:
                up_count = classified_df[f'{col}_Up'].sum()
                stay_count = classified_df[f'{col}_Stay'].sum()
                down_count = classified_df[f'{col}_Down'].sum()
                total = len(classified_df)
                print(f"{col}:")
                print(f"  Up: {up_count} ({up_count/total*100:.1f}%)")
                print(f"  Stay: {stay_count} ({stay_count/total*100:.1f}%)")
                print(f"  Down: {down_count} ({down_count/total*100:.1f}%)")

        return classified_df

    def create_individual_files(self, classified_df, output_dir, target_column='X'):
        """
        個別ファイルに分割（各銘柄/通貨ペアごと）

        Parameters:
        -----------
        classified_df : pd.DataFrame
            変換済みデータ
        output_dir : str
            出力ディレクトリ
        target_column : str
            予測対象カラム名（デフォルト: 'X'）
        """
        os.makedirs(output_dir, exist_ok=True)
        print(f"\n=== Creating individual files in {output_dir} ===")

        # カラムからベース名を抽出（_Up, _Stay, _Down を除去）
        all_columns = [col for col in classified_df.columns
                      if col not in ['T', 'timestamp', target_column]]

        # ベース名を抽出（_Up, _Stay, _Downを除去）
        base_names = set()
        for col in all_columns:
            if col.endswith('_Up') or col.endswith('_Stay') or col.endswith('_Down'):
                base_name = col.rsplit('_', 1)[0]
                base_names.add(base_name)

        print(f"Found {len(base_names)} unique instruments")

        # 標準化された変化率の統計情報を計算（後で表示）
        standardization_stats = {}

        # 各銘柄/通貨ペアごとにファイル作成
        for idx, base_name in enumerate(sorted(base_names), 1):
            # このペアに関連する全カラムを取得（Tは除外、後で追加）
            related_cols = []
            for col in all_columns:
                if col.startswith(f'{base_name}_'):
                    related_cols.append(col)

            # 他の全ペアのカラムも含める（相互影響を学習するため）
            for col in all_columns:
                if col not in related_cols:
                    related_cols.append(col)

            # データを抽出（Tカラムも含める）
            individual_df = classified_df[related_cols + ['T']].copy()

            # このペアの標準化された変化率をXとして追加
            if base_name in self.standardized_df.columns:
                # 翌日の標準化された変化率を予測対象とする（1期先にシフト）
                individual_df['X'] = self.standardized_df[base_name].shift(-1)

                # 統計情報を記録
                x_values = individual_df['X'].dropna()
                standardization_stats[base_name] = {
                    'mean': x_values.mean(),
                    'std': x_values.std(),
                    'min': x_values.min(),
                    'max': x_values.max(),
                    'median': x_values.median()
                }
            else:
                # データがない場合は0で埋める
                individual_df['X'] = 0.0

            # NaNを除去（最後の行）
            individual_df = individual_df.dropna()

            # GNMiner形式に合わせてカラムを並び替え
            # 期待される形式: [属性1, 属性2, ..., X, T]
            # Tを最後に移動
            cols = [col for col in individual_df.columns if col not in ['T', 'X']]
            cols.append('X')
            cols.append('T')
            individual_df = individual_df[cols]

            # ファイル保存
            output_file = os.path.join(output_dir, f'{base_name}.txt')
            individual_df.to_csv(output_file, index=False)

            if idx <= 5 or idx % 10 == 0:  # 進捗表示
                print(f"  [{idx}/{len(base_names)}] Created {base_name}.txt ({len(individual_df)} records)")

        print(f"✓ Created {len(base_names)} individual files")

        # 標準化後のX統計情報を表示
        print()
        print("=== Standardized X Statistics (first 5 instruments) ===")
        for idx, (base_name, stats) in enumerate(list(standardization_stats.items())[:5]):
            print(f"{base_name}:")
            print(f"  Mean: {stats['mean']:.6f}")
            print(f"  Std:  {stats['std']:.6f}")
            print(f"  Min:  {stats['min']:.6f}")
            print(f"  Max:  {stats['max']:.6f}")
            print(f"  Median: {stats['median']:.6f}")

def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Convert price data to GNMiner format with standardization'
    )
    parser.add_argument(
        'input_file',
        help='Input CSV file (e.g., forex_raw.csv or nikkei225_raw.csv)'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output file path (default: input_file_gnminer_std.csv)'
    )
    parser.add_argument(
        '--stay-threshold',
        type=float,
        default=0.0,
        help='Stay threshold for standardized values (default: 0.0)'
    )
    parser.add_argument(
        '--standardize',
        type=str,
        default='z-score',
        choices=['z-score', 'rolling-z-score', 'rank', 'none'],
        help='Standardization method (default: z-score)'
    )
    parser.add_argument(
        '--rolling-window',
        type=int,
        default=20,
        help='Rolling window size for rolling-z-score (default: 20)'
    )
    parser.add_argument(
        '--split',
        action='store_true',
        help='Create individual files for each instrument'
    )
    parser.add_argument(
        '--split-dir',
        default='forex_data/gnminer_individual_std',
        help='Directory for individual files (default: forex_data/gnminer_individual_std)'
    )

    args = parser.parse_args()

    # 変換実行
    converter = GNMinerDataConverter(
        stay_threshold=args.stay_threshold,
        standardize_method=args.standardize,
        rolling_window=args.rolling_window
    )
    classified_df = converter.convert(args.input_file, args.output)

    # 個別ファイル作成
    if args.split:
        converter.create_individual_files(classified_df, args.split_dir)

    print()
    print("✓ Conversion complete!")

if __name__ == '__main__':
    main()
