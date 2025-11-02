#!/usr/bin/env python3
"""
GNMinerフォーマット変換スクリプト

生の価格データ（株価または為替）をGNMinerで使用できる形式に変換します。

変換手順:
1. 日次の変化率（%）を計算
2. 変化率をUp/Stay/Downに分類
   - Up: 変化率 > 0
   - Stay: 変化率 == 0 (完全一致)
   - Down: 変化率 < 0
3. バイナリ（0/1）形式に変換
4. 個別ファイルに分割（オプション）
"""

import pandas as pd
import numpy as np
import os
from pathlib import Path
import argparse

class GNMinerDataConverter:
    """GNMinerフォーマット変換クラス"""

    def __init__(self, stay_threshold=0.0):
        """
        Parameters:
        -----------
        stay_threshold : float
            Stay判定の閾値（デフォルト: 0.0 = 完全一致のみ）
        """
        self.stay_threshold = abs(stay_threshold)
        print(f"Stay threshold: ±{self.stay_threshold}%")

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

    def classify_movements(self, returns_df, price_columns):
        """
        変化率をUp/Stay/Downに分類

        Parameters:
        -----------
        returns_df : pd.DataFrame
            変化率データ
        price_columns : list
            価格カラム名のリスト

        Returns:
        --------
        pd.DataFrame
            分類データ（各カラムに Up/Stay/Down）
        """
        classified_df = pd.DataFrame()
        classified_df['T'] = returns_df['T']

        for col in price_columns:
            if col not in returns_df.columns:
                continue

            returns = returns_df[col]

            # Up/Stay/Downに分類
            if self.stay_threshold == 0.0:
                # 完全一致のみStay
                classified_df[f'{col}_Up'] = (returns > 0).astype(int)
                classified_df[f'{col}_Stay'] = (returns == 0).astype(int)
                classified_df[f'{col}_Down'] = (returns < 0).astype(int)
            else:
                # 閾値内をStay
                classified_df[f'{col}_Up'] = (returns > self.stay_threshold).astype(int)
                classified_df[f'{col}_Stay'] = (
                    (returns >= -self.stay_threshold) &
                    (returns <= self.stay_threshold)
                ).astype(int)
                classified_df[f'{col}_Down'] = (returns < -self.stay_threshold).astype(int)

        return classified_df

    def add_target_variable(self, classified_df, price_columns, prediction_span=1):
        """
        予測対象変数 X を追加

        NOTE: このメソッドはcreate_individual_files内で個別に呼ばれるため、
        ここでは変化率データを保持するだけです。

        Parameters:
        -----------
        classified_df : pd.DataFrame
            分類データ
        price_columns : list
            価格カラム名のリスト
        prediction_span : int
            予測期間（デフォルト: 1日先）

        Returns:
        --------
        pd.DataFrame
            元のデータ（変化率は個別ファイル作成時に追加）
        """
        # 変化率データを保持（個別ファイル作成時に使用）
        self.returns_data = {}
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

        # 変化率データを保持（個別ファイル作成時に使用）
        self.returns_df = returns_df.copy()
        self.price_columns = price_columns

        # Up/Stay/Downに分類
        print("Classifying movements...")
        classified_df = self.classify_movements(returns_df, price_columns)

        # NaN を除去（最初の行）
        classified_df = classified_df.dropna()

        print(f"Final records: {len(classified_df)}")

        # 保存
        if output_file is None:
            # 入力ファイルがforex_data/内なら、出力もforex_data/内に
            if 'forex_data' in input_file:
                output_file = input_file.replace('.csv', '_gnminer.csv')
            else:
                # それ以外は同じディレクトリに
                output_file = input_file.replace('.csv', '_gnminer.csv')

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

            # このペアの変化率をXとして追加（小数第2位に丸める）
            if base_name in self.returns_df.columns:
                # その時点の変化率をXとする（シフトなし）
                individual_df['X'] = self.returns_df[base_name].round(2)
            else:
                # データがない場合は0で埋める
                individual_df['X'] = 0.0

            # NaNを除去（最後の行）
            individual_df = individual_df.dropna()

            # 最初の行をスキップ（pct_change()で全て0になるため）
            if len(individual_df) > 0:
                individual_df = individual_df.iloc[1:].reset_index(drop=True)

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

def main():
    """メイン処理"""
    parser = argparse.ArgumentParser(
        description='Convert price data to GNMiner format'
    )
    parser.add_argument(
        'input_file',
        help='Input CSV file (e.g., forex_raw.csv or nikkei225_raw.csv)'
    )
    parser.add_argument(
        '-o', '--output',
        help='Output file path (default: input_file_gnminer.csv)'
    )
    parser.add_argument(
        '--stay-threshold',
        type=float,
        default=0.0,
        help='Stay threshold (default: 0.0 = exact match only)'
    )
    parser.add_argument(
        '--split',
        action='store_true',
        help='Create individual files for each instrument'
    )
    parser.add_argument(
        '--split-dir',
        default='forex_data/gnminer_individual',
        help='Directory for individual files (default: forex_data/gnminer_individual)'
    )

    args = parser.parse_args()

    # 変換実行
    converter = GNMinerDataConverter(stay_threshold=args.stay_threshold)
    classified_df = converter.convert(args.input_file, args.output)

    # 個別ファイル作成
    if args.split:
        converter.create_individual_files(classified_df, args.split_dir)

    print()
    print("✓ Conversion complete!")

if __name__ == '__main__':
    main()
