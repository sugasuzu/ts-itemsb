#!/usr/bin/env python3
"""
GBPJPYルールプール分析スクリプト
ルールスコア = (support * concentration) / deviation
"""

import pandas as pd
import numpy as np
import sys

def calculate_quadrant_concentration(row):
    """
    各ルールの象限集中度を計算（0ベース判定）

    t+1, t+2の平均値から支配象限を推定
    ※実際の個別データポイントの集中度ではなく、平均値からの推定
    """
    mean_t1 = row['X(t+1)_mean']
    mean_t2 = row['X(t+2)_mean']

    # 平均値から支配象限を推定
    if mean_t1 >= 0 and mean_t2 >= 0:
        quadrant = 'Q1(++)'
    elif mean_t1 < 0 and mean_t2 >= 0:
        quadrant = 'Q2(-+)'
    elif mean_t1 < 0 and mean_t2 < 0:
        quadrant = 'Q3(--)'
    else:  # mean_t1 >= 0 and mean_t2 < 0
        quadrant = 'Q4(+-)'

    # 集中度の推定：分散が小さいほど集中度が高いと仮定
    # σが小さい = データが平均周辺に集中 = 象限集中度が高い
    sigma_t1 = row['X(t+1)_sigma']
    sigma_t2 = row['X(t+2)_sigma']

    # 平均分散を計算
    avg_sigma = (sigma_t1 + sigma_t2) / 2

    # 集中度の推定値（分散の逆数を基準に0-1の範囲に正規化）
    # σが0.3未満 → 高集中度（~0.8以上）
    # σが0.5前後 → 中程度（~0.5-0.7）
    # σが1.0以上 → 低集中度（~0.4以下）
    if avg_sigma < 0.01:  # ゼロ除算回避
        concentration_estimate = 0.90
    else:
        # 経験的な変換式: concentration ≈ 1 / (1 + σ)
        concentration_estimate = 1.0 / (1.0 + avg_sigma * 2)

    return quadrant, concentration_estimate, avg_sigma

def calculate_deviation_metric(row):
    """
    逸脱メトリックの計算

    deviation = max(|min|, |max|) の平均値
    ※大きいほど外れ値が存在
    """
    min_t1 = row['X(t+1)_min']
    max_t1 = row['X(t+1)_max']
    min_t2 = row['X(t+2)_min']
    max_t2 = row['X(t+2)_max']

    # 各時点での最大逸脱
    dev_t1 = max(abs(min_t1), abs(max_t1))
    dev_t2 = max(abs(min_t2), abs(max_t2))

    # 平均逸脱
    avg_deviation = (dev_t1 + dev_t2) / 2

    return avg_deviation

def main():
    # ファイル読み込み
    file_path = '/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/1-deta-enginnering/forex_data_daily/output/GBPJPY/pool/zrp01a.txt'

    print("="*80)
    print("GBPJPY ルールプール分析")
    print("="*80)
    print(f"\nファイル: {file_path}")

    # データ読み込み
    df = pd.read_csv(file_path, sep='\t')
    total_rules = len(df)
    print(f"総ルール数: {total_rules}")

    # 象限判定と集中度推定
    df[['Quadrant', 'Concentration_Est', 'Avg_Sigma']] = df.apply(
        lambda row: pd.Series(calculate_quadrant_concentration(row)), axis=1
    )

    # 逸脱メトリック計算
    df['Deviation'] = df.apply(calculate_deviation_metric, axis=1)

    # ルールスコア計算: (support * concentration) / deviation
    # ゼロ除算回避: deviation < 0.1 の場合は 0.1 を使用
    df['Deviation_Safe'] = df['Deviation'].clip(lower=0.1)
    df['Rule_Score'] = (df['support_rate'] * df['Concentration_Est']) / df['Deviation_Safe']

    # ルールIDを追加（1始まり）
    df['Rule_ID'] = range(1, len(df) + 1)

    # ============================================================================
    # 基本統計
    # ============================================================================
    print("\n" + "="*80)
    print("基本統計")
    print("="*80)

    print(f"\nサポート率:")
    print(f"  平均: {df['support_rate'].mean():.4f}")
    print(f"  最小: {df['support_rate'].min():.4f}")
    print(f"  最大: {df['support_rate'].max():.4f}")

    print(f"\n集中度（推定値）:")
    print(f"  平均: {df['Concentration_Est'].mean():.3f}")
    print(f"  最小: {df['Concentration_Est'].min():.3f}")
    print(f"  最大: {df['Concentration_Est'].max():.3f}")

    print(f"\n逸脱メトリック:")
    print(f"  平均: {df['Deviation'].mean():.2f}")
    print(f"  最小: {df['Deviation'].min():.2f}")
    print(f"  最大: {df['Deviation'].max():.2f}")

    print(f"\nルールスコア:")
    print(f"  平均: {df['Rule_Score'].mean():.6f}")
    print(f"  最小: {df['Rule_Score'].min():.6f}")
    print(f"  最大: {df['Rule_Score'].max():.6f}")

    # ============================================================================
    # 象限分布
    # ============================================================================
    print("\n" + "="*80)
    print("象限分布")
    print("="*80)

    quadrant_dist = df['Quadrant'].value_counts().sort_index()
    print("\n象限別ルール数:")
    for q, count in quadrant_dist.items():
        pct = count / total_rules * 100
        print(f"  {q}: {count:4d} ({pct:5.1f}%)")

    # ============================================================================
    # 属性数分布
    # ============================================================================
    print("\n" + "="*80)
    print("属性数分布")
    print("="*80)

    attr_dist = df['NumAttr'].value_counts().sort_index()
    print("\n属性数別ルール数:")
    for num_attr, count in attr_dist.items():
        pct = count / total_rules * 100
        print(f"  {num_attr}属性: {count:4d} ({pct:5.1f}%)")

    # ============================================================================
    # Top 20ルール（スコア順）
    # ============================================================================
    print("\n" + "="*80)
    print("Top 20 ルール（スコア順）")
    print("="*80)

    top_20 = df.nlargest(20, 'Rule_Score')

    print(f"\n{'ID':<5} {'Score':<10} {'Support':<8} {'Conc':<6} {'Dev':<6} {'Quad':<8} {'Attr':<5} {'Mean(t+1)':<10} {'Mean(t+2)':<10} {'Attributes':<60}")
    print("-"*150)

    for idx, row in top_20.iterrows():
        rule_id = row['Rule_ID']
        score = row['Rule_Score']
        support = row['support_rate']
        conc = row['Concentration_Est']
        dev = row['Deviation']
        quad = row['Quadrant']
        num_attr = int(row['NumAttr'])
        mean_t1 = row['X(t+1)_mean']
        mean_t2 = row['X(t+2)_mean']

        # 属性リストを抽出
        attrs = []
        for i in range(1, 9):
            attr_col = f'Attr{i}'
            attr_val = row[attr_col]
            if pd.notna(attr_val) and attr_val != '0' and attr_val != 0:
                attrs.append(str(attr_val))

        attrs_str = ', '.join(attrs[:3])  # 最初の3属性のみ表示
        if len(attrs) > 3:
            attrs_str += f", ... (+{len(attrs)-3})"

        print(f"{rule_id:<5} {score:<10.6f} {support:<8.4f} {conc:<6.3f} {dev:<6.2f} {quad:<8} {num_attr:<5} {mean_t1:<10.3f} {mean_t2:<10.3f} {attrs_str:<60}")

    # ============================================================================
    # 高サポートルール（support_rate >= 0.005）
    # ============================================================================
    print("\n" + "="*80)
    print("高サポートルール（support_rate >= 0.005）")
    print("="*80)

    high_support = df[df['support_rate'] >= 0.005].sort_values('Rule_Score', ascending=False)
    print(f"\n総数: {len(high_support)} ルール")

    if len(high_support) > 0:
        print(f"\n{'ID':<5} {'Score':<10} {'Support':<8} {'Conc':<6} {'Dev':<6} {'Quad':<8} {'Attr':<5} {'Mean(t+1)':<10} {'Mean(t+2)':<10}")
        print("-"*100)

        for idx, row in high_support.head(10).iterrows():
            rule_id = row['Rule_ID']
            score = row['Rule_Score']
            support = row['support_rate']
            conc = row['Concentration_Est']
            dev = row['Deviation']
            quad = row['Quadrant']
            num_attr = int(row['NumAttr'])
            mean_t1 = row['X(t+1)_mean']
            mean_t2 = row['X(t+2)_mean']

            print(f"{rule_id:<5} {score:<10.6f} {support:<8.4f} {conc:<6.3f} {dev:<6.2f} {quad:<8} {num_attr:<5} {mean_t1:<10.3f} {mean_t2:<10.3f}")

    # ============================================================================
    # 低逸脱ルール（deviation <= 5.0）
    # ============================================================================
    print("\n" + "="*80)
    print("低逸脱ルール（deviation <= 5.0）")
    print("="*80)

    low_dev = df[df['Deviation'] <= 5.0].sort_values('Rule_Score', ascending=False)
    print(f"\n総数: {len(low_dev)} ルール")

    if len(low_dev) > 0:
        print(f"\n{'ID':<5} {'Score':<10} {'Support':<8} {'Conc':<6} {'Dev':<6} {'Quad':<8} {'Attr':<5} {'Mean(t+1)':<10} {'Mean(t+2)':<10}")
        print("-"*100)

        for idx, row in low_dev.head(10).iterrows():
            rule_id = row['Rule_ID']
            score = row['Rule_Score']
            support = row['support_rate']
            conc = row['Concentration_Est']
            dev = row['Deviation']
            quad = row['Quadrant']
            num_attr = int(row['NumAttr'])
            mean_t1 = row['X(t+1)_mean']
            mean_t2 = row['X(t+2)_mean']

            print(f"{rule_id:<5} {score:<10.6f} {support:<8.4f} {conc:<6.3f} {dev:<6.2f} {quad:<8} {num_attr:<5} {mean_t1:<10.3f} {mean_t2:<10.3f}")

    # ============================================================================
    # CSVファイルへの出力
    # ============================================================================
    output_file = '/Users/suzukiyasuhiro/Desktop/study/ts-itemsbs/gbpjpy_rule_analysis.csv'

    # 出力用カラムを選択
    output_cols = [
        'Rule_ID', 'Rule_Score', 'support_rate', 'support_count',
        'Concentration_Est', 'Avg_Sigma', 'Deviation', 'Quadrant',
        'NumAttr', 'X(t+1)_mean', 'X(t+1)_sigma', 'X(t+2)_mean', 'X(t+2)_sigma',
        'Attr1', 'Attr2', 'Attr3', 'Attr4', 'Attr5', 'Attr6', 'Attr7', 'Attr8'
    ]

    df_output = df[output_cols].sort_values('Rule_Score', ascending=False)
    df_output.to_csv(output_file, index=False)

    print("\n" + "="*80)
    print(f"分析結果を保存: {output_file}")
    print("="*80)

    return 0

if __name__ == '__main__':
    sys.exit(main())
