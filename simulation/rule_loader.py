#!/usr/bin/env python3
"""
ルールローダーモジュール

zrp01a.txtファイルから取引ルールを読み込み、
positive (BUY) とnegative (SELL) のルールを統合管理します。
"""

import pandas as pd
import re
from pathlib import Path


class RuleLoader:
    """ルール読み込みクラス"""

    def __init__(self, pair):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: USDJPY)
        """
        self.pair = pair
        self.positive_rules = []
        self.negative_rules = []

    def load_rules(self, direction, top_n=10, sort_by='support'):
        """
        指定方向のルールを読み込み

        Parameters
        ----------
        direction : str
            'positive' or 'negative'
        top_n : int
            読み込む上位ルール数
        sort_by : str
            ソート基準 ('support', 'extreme_score', 'snr', 'extremeness', 'discovery')

        Returns
        -------
        list of dict
            解析されたルールのリスト
        """
        rule_file = Path(f"output/{self.pair}/{direction}/pool/zrp01a.txt")

        if not rule_file.exists():
            raise FileNotFoundError(f"Rule file not found: {rule_file}")

        print(f"Loading {direction} rules from: {rule_file}")

        df = pd.read_csv(rule_file, sep='\t')

        # ソート処理
        if sort_by == 'support':
            df = df.sort_values('support_count', ascending=False)
        elif sort_by == 'extreme_score':
            df = df.sort_values('ExtremeScore', ascending=False)
        elif sort_by == 'snr':
            df = df.sort_values('SNR', ascending=False)
        elif sort_by == 'extremeness':
            df = df.sort_values('Extremeness', ascending=False)

        if top_n:
            df = df.head(top_n)

        rules = []
        for idx, row in df.iterrows():
            conditions = []
            rule_text_parts = []

            # 属性解析（Attr1～Attr8）
            for i in range(1, 9):
                attr_col = f'Attr{i}'
                attr_value = row[attr_col]

                if attr_value == 0 or pd.isna(attr_value):
                    break

                # 属性名と遅延を解析: "NZDJPY_Up(t-1)" → attr="NZDJPY_Up", delay=1
                match = re.match(r'(.+)\(t-(\d+)\)', str(attr_value))
                if match:
                    attr_name = match.group(1)
                    delay = int(match.group(2))
                    conditions.append({
                        'attr': attr_name,
                        'delay': delay
                    })
                    rule_text_parts.append(f"{attr_name}(t-{delay})")

            if not conditions:
                continue

            rule = {
                'rule_id': idx,
                'direction': direction,
                'conditions': conditions,
                'rule_text': ' AND '.join(rule_text_parts),
                'x_mean': row['X_mean'],
                'x_sigma': row['X_sigma'],
                'support_count': int(row['support_count']),
                'support_rate': row['support_rate'],
                'signal_strength': row.get('SignalStrength', 0),
                'SNR': row.get('SNR', 0),
                'extremeness': row.get('Extremeness', 0)
            }

            rules.append(rule)

        print(f"  ✓ Loaded {len(rules)} {direction} rules")
        return rules

    def load_all_rules(self, top_n=10, sort_by='support'):
        """
        positive と negative の両方のルールを読み込み

        Parameters
        ----------
        top_n : int
            各方向の上位ルール数
        sort_by : str
            ソート基準

        Returns
        -------
        tuple of (positive_rules, negative_rules)
        """
        print(f"\n{'='*80}")
        print(f"Loading Rules for {self.pair}")
        print(f"{'='*80}")
        print(f"Top N per direction: {top_n}")
        print(f"Sort by: {sort_by}")
        print()

        self.positive_rules = self.load_rules('positive', top_n, sort_by)
        self.negative_rules = self.load_rules('negative', top_n, sort_by)

        total_rules = len(self.positive_rules) + len(self.negative_rules)
        print(f"\n✓ Total rules loaded: {total_rules} ({len(self.positive_rules)} BUY + {len(self.negative_rules)} SELL)")
        print(f"{'='*80}\n")

        return self.positive_rules, self.negative_rules

    def get_rule_summary(self):
        """
        ルールのサマリー情報を取得

        Returns
        -------
        dict
            統計情報
        """
        summary = {
            'pair': self.pair,
            'positive_count': len(self.positive_rules),
            'negative_count': len(self.negative_rules),
            'total_count': len(self.positive_rules) + len(self.negative_rules),
            'positive_avg_support': sum(r['support_count'] for r in self.positive_rules) / len(self.positive_rules) if self.positive_rules else 0,
            'negative_avg_support': sum(r['support_count'] for r in self.negative_rules) / len(self.negative_rules) if self.negative_rules else 0,
            'positive_avg_x_mean': sum(r['x_mean'] for r in self.positive_rules) / len(self.positive_rules) if self.positive_rules else 0,
            'negative_avg_x_mean': sum(r['x_mean'] for r in self.negative_rules) / len(self.negative_rules) if self.negative_rules else 0,
        }
        return summary


if __name__ == '__main__':
    # テスト実行
    loader = RuleLoader('USDJPY')
    pos_rules, neg_rules = loader.load_all_rules(top_n=10, sort_by='support')

    print("\n=== Positive Rules Sample ===")
    for i, rule in enumerate(pos_rules[:3]):
        print(f"{i+1}. Rule {rule['rule_id']}: {rule['rule_text']}")
        print(f"   Support: {rule['support_count']} ({rule['support_rate']:.2%})")
        print(f"   X_mean: {rule['x_mean']:.3f}, SNR: {rule['SNR']:.2f}")
        print()

    print("\n=== Negative Rules Sample ===")
    for i, rule in enumerate(neg_rules[:3]):
        print(f"{i+1}. Rule {rule['rule_id']}: {rule['rule_text']}")
        print(f"   Support: {rule['support_count']} ({rule['support_rate']:.2%})")
        print(f"   X_mean: {rule['x_mean']:.3f}, SNR: {rule['SNR']:.2f}")
        print()

    summary = loader.get_rule_summary()
    print("\n=== Summary ===")
    for key, value in summary.items():
        print(f"{key}: {value}")
