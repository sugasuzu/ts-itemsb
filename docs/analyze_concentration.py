#!/usr/bin/env python3
"""
象限集中度分析スクリプト

verification/ 内の全CSVファイルを読み込み、各ルールの象限集中度を計算します。

使い方:
    python3 analyze_concentration.py

出力:
    - 平均集中度、最大集中度
    - 集中度分布（60%+, 55-59%, 50-54%, 45-49%, <45%）
    - トップ20ルールの詳細
"""

import os
import pandas as pd
import numpy as np

def calculate_concentration(csv_path, min_support=30):
    """
    CSVファイルから象限集中度を計算

    Args:
        csv_path: verification CSVファイルのパス
        min_support: 最小サポート数（これ以上のルールのみ処理）

    Returns:
        dict: ルール情報（concentration, support, quadrant counts等）
    """
    df_csv = pd.read_csv(csv_path)

    if len(df_csv) < min_support:
        return None

    # X列を数値型に変換
    df_csv['X(t+1)'] = pd.to_numeric(df_csv['X(t+1)'], errors='coerce')
    df_csv['X(t+2)'] = pd.to_numeric(df_csv['X(t+2)'], errors='coerce')

    # 象限分布を計算
    q1 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] > 0)).sum()
    q2 = ((df_csv['X(t+1)'] > 0) & (df_csv['X(t+2)'] <= 0)).sum()
    q3 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] <= 0)).sum()
    q4 = ((df_csv['X(t+1)'] <= 0) & (df_csv['X(t+2)'] > 0)).sum()

    total = len(df_csv)
    max_q = max(q1, q2, q3, q4)
    concentration = (max_q / total) * 100

    return {
        'concentration': concentration,
        'support': total,
        'q1': q1,
        'q2': q2,
        'q3': q3,
        'q4': q4
    }

def main():
    # verification ディレクトリのパス
    verification_dir = "1-deta-enginnering/crypto_data_hourly/output/BTC/verification/"

    if not os.path.exists(verification_dir):
        print(f"Error: {verification_dir} が見つかりません")
        return

    print("=" * 80)
    print("  象限集中度分析（verification CSVベース）")
    print("=" * 80)
    print()

    # 全ルールを処理
    results = []
    for filename in os.listdir(verification_dir):
        if filename.startswith('rule_') and filename.endswith('.csv'):
            rule_id = int(filename.replace('rule_', '').replace('.csv', ''))
            csv_path = os.path.join(verification_dir, filename)

            result = calculate_concentration(csv_path, min_support=30)
            if result is not None:
                result['rule_id'] = rule_id
                results.append(result)

    if len(results) == 0:
        print("Error: 有効なルールが見つかりません")
        return

    # 統計計算
    concentrations = [r['concentration'] for r in results]
    avg_conc = np.mean(concentrations)
    max_conc = np.max(concentrations)

    print(f"総ルール数: {len(results)}")
    print(f"サポート >= 30のルール数: {len(results)}")
    print()
    print(f"平均集中度: {avg_conc:.1f}%")
    print(f"最大集中度: {max_conc:.1f}%")
    print()

    # 集中度分布
    conc_60plus = sum(1 for c in concentrations if c >= 60.0)
    conc_55_59 = sum(1 for c in concentrations if 55.0 <= c < 60.0)
    conc_50_54 = sum(1 for c in concentrations if 50.0 <= c < 55.0)
    conc_45_49 = sum(1 for c in concentrations if 45.0 <= c < 50.0)
    conc_below_45 = sum(1 for c in concentrations if c < 45.0)

    print("【集中度分布】")
    print(f"  60%+      : {conc_60plus:5d} ルール")
    print(f"  55-59%    : {conc_55_59:5d} ルール")
    print(f"  50-54%    : {conc_50_54:5d} ルール")
    print(f"  45-49%    : {conc_45_49:5d} ルール")
    print(f"  < 45%     : {conc_below_45:5d} ルール")
    print()

    # トップ20ルール
    results_sorted = sorted(results, key=lambda x: x['concentration'], reverse=True)
    top20 = results_sorted[:20]

    print("【トップ20ルール - 高集中度順】")
    print(f"{'Rank':<6} {'RuleID':<8} {'Conc%':<8} {'Support':<8} {'Q1':<6} {'Q2':<6} {'Q3':<6} {'Q4':<6}")
    print("-" * 70)

    for rank, r in enumerate(top20, 1):
        print(f"{rank:<6} {r['rule_id']:<8} {r['concentration']:>5.1f}%   "
              f"{r['support']:<8} {r['q1']:<6} {r['q2']:<6} {r['q3']:<6} {r['q4']:<6}")

    print("=" * 80)

if __name__ == "__main__":
    main()
