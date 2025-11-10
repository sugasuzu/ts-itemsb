#!/usr/bin/env python3
"""
上位ルール抽出スクリプト

集中度、mean値、サポート数などの指標でルールを抽出し、
詳細情報をCSVとして保存します。

使い方:
    python3 extract_top_rules.py [--top N] [--min-conc X]

    --top N: 上位N件を抽出（デフォルト: 50）
    --min-conc X: 最小集中度X%以上のルールのみ（デフォルト: なし）

出力:
    - top_rules.csv: 上位ルールの詳細CSV
    - コンソールに集中度順のランキング表示
"""

import os
import argparse
import pandas as pd
import numpy as np

def calculate_rule_stats(csv_path, min_support=30):
    """
    CSVファイルからルールの統計情報を計算

    Args:
        csv_path: verification CSVファイルのパス
        min_support: 最小サポート数

    Returns:
        dict: ルールの統計情報
    """
    df_csv = pd.read_csv(csv_path)

    if len(df_csv) < min_support:
        return None

    # X列を数値型に変換
    df_csv['X(t+1)'] = pd.to_numeric(df_csv['X(t+1)'], errors='coerce')
    df_csv['X(t+2)'] = pd.to_numeric(df_csv['X(t+2)'], errors='coerce')

    # 象限分布
    q1 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] > 0)).sum()
    q2 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] <= 0)).sum()
    q3 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] <= 0)).sum()
    q4 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] > 0)).sum()

    # 集中度
    max_q = max(q1, q2, q3, q4)
    concentration = (max_q / len(df_csv)) * 100

    # 支配象限
    if max_q == q1:
        dominant_quadrant = "Q1(++)"
    elif max_q == q2:
        dominant_quadrant = "Q2(+-)"
    elif max_q == q3:
        dominant_quadrant = "Q3(--)"
    else:
        dominant_quadrant = "Q4(-+)"

    # 統計量
    mean_t1 = df_csv['X(t+1)'].mean()
    mean_t2 = df_csv['X(t+2)'].mean()
    sigma_t1 = df_csv['X(t+1)'].std()
    sigma_t2 = df_csv['X(t+2)'].std()

    return {
        'support': len(df_csv),
        'concentration': concentration,
        'dominant_quadrant': dominant_quadrant,
        'q1': q1,
        'q2': q2,
        'q3': q3,
        'q4': q4,
        'mean_t1': mean_t1,
        'mean_t2': mean_t2,
        'sigma_t1': sigma_t1,
        'sigma_t2': sigma_t2,
        'abs_mean_t1': abs(mean_t1),
        'abs_mean_t2': abs(mean_t2)
    }

def main():
    parser = argparse.ArgumentParser(description='上位ルール抽出スクリプト')
    parser.add_argument('--top', type=int, default=50, help='上位N件を抽出（デフォルト: 50）')
    parser.add_argument('--min-conc', type=float, default=0.0, help='最小集中度（デフォルト: 0）')
    args = parser.parse_args()

    # verification ディレクトリ
    verification_dir = "1-deta-enginnering/crypto_data_hourly/output/BTC/verification/"

    if not os.path.exists(verification_dir):
        print(f"Error: {verification_dir} が見つかりません")
        return

    print(f"ルール抽出中... (最小集中度: {args.min_conc}%)")

    # 全ルールを処理
    results = []
    for filename in os.listdir(verification_dir):
        if filename.startswith('rule_') and filename.endswith('.csv'):
            rule_id = int(filename.replace('rule_', '').replace('.csv', ''))
            csv_path = os.path.join(verification_dir, filename)

            stats = calculate_rule_stats(csv_path, min_support=30)
            if stats is not None:
                stats['rule_id'] = rule_id
                results.append(stats)

    if len(results) == 0:
        print("Error: 有効なルールが見つかりません")
        return

    # DataFrameに変換
    df_results = pd.DataFrame(results)

    # フィルタリング
    if args.min_conc > 0:
        df_results = df_results[df_results['concentration'] >= args.min_conc]
        print(f"集中度 >= {args.min_conc}%: {len(df_results)}件")

    if len(df_results) == 0:
        print("フィルタ後のルールが0件です")
        return

    # 集中度でソート
    df_sorted = df_results.sort_values('concentration', ascending=False)

    # 上位N件
    df_top = df_sorted.head(args.top)

    # コンソール表示
    print()
    print("=" * 90)
    print(f"  集中度上位{min(args.top, len(df_top))}件のルール")
    print("=" * 90)
    print()
    print(f"{'Rank':<6} {'RuleID':<8} {'Conc%':<8} {'Support':<8} {'Mean(t+1)':<12} "
          f"{'Mean(t+2)':<12} {'Dominant':<12}")
    print("-" * 90)

    for idx, (_, row) in enumerate(df_top.iterrows(), 1):
        print(f"{idx:<6} {int(row['rule_id']):<8} {row['concentration']:>5.1f}%   "
              f"{int(row['support']):<8} {row['mean_t1']:>+10.3f}%  "
              f"{row['mean_t2']:>+10.3f}%  {row['dominant_quadrant']:<12}")

    print("=" * 90)
    print()

    # CSV保存
    output_file = 'top_rules.csv'
    df_top.to_csv(output_file, index=False)
    print(f"✓ 保存: {output_file}")

    # 統計サマリー
    print()
    print("【統計サマリー】")
    print(f"  平均集中度: {df_top['concentration'].mean():.1f}%")
    print(f"  最大集中度: {df_top['concentration'].max():.1f}%")
    print(f"  平均サポート: {df_top['support'].mean():.1f}")
    print(f"  平均|mean(t+1)|: {df_top['abs_mean_t1'].mean():.3f}%")
    print(f"  平均|mean(t+2)|: {df_top['abs_mean_t2'].mean():.3f}%")

    # 象限分布
    q_counts = df_top['dominant_quadrant'].value_counts()
    print()
    print("【象限分布】")
    for q, count in q_counts.items():
        print(f"  {q}: {count}件 ({count/len(df_top)*100:.1f}%)")

if __name__ == "__main__":
    main()
