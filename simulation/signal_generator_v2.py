#!/usr/bin/env python3
"""
シグナル生成モジュール v2（最適化版）

改善点:
1. 時間分割バックテスト対応（train/test split）
2. シグナル重複排除（1時点1トレード、サポート数優先）
3. テスト期間のデータのみでシグナル生成
"""

import pandas as pd
from pathlib import Path


class SignalGeneratorV2:
    """最適化版シグナル生成クラス"""

    def __init__(self, pair, test_start_date='2021-01-01'):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: USDJPY)
        test_start_date : str
            テスト期間開始日（この日以降のデータでシミュレーション）
        """
        self.pair = pair
        self.test_start_date = test_start_date
        self.data = None
        self.test_data = None

    def load_data(self):
        """
        全履歴データを読み込み、テスト期間で分割

        Returns
        -------
        pd.DataFrame
            テスト期間のデータフレーム
        """
        data_file = Path(f"forex_data/gnminer_individual/{self.pair}.txt")

        if not data_file.exists():
            raise FileNotFoundError(f"Data file not found: {data_file}")

        print(f"Loading data: {data_file}")
        self.data = pd.read_csv(data_file)

        # テスト期間のデータのみ抽出
        self.test_data = self.data[self.data['T'] >= self.test_start_date].copy()
        self.test_data = self.test_data.reset_index(drop=True)

        print(f"  ✓ Loaded {len(self.data)} total records")
        print(f"  Full date range: {self.data['T'].min()} to {self.data['T'].max()}")
        print(f"  ✓ Test period: {self.test_data['T'].min()} to {self.test_data['T'].max()}")
        print(f"  Test records: {len(self.test_data)}")
        print(f"  X range: {self.test_data['X'].min():.3f} to {self.test_data['X'].max():.3f}")

        return self.test_data

    def match_rule(self, rule, t):
        """
        時点tでルール条件がマッチするかチェック

        Parameters
        ----------
        rule : dict
            ルール情報
        t : int
            現在時点のインデックス

        Returns
        -------
        bool
            マッチした場合True
        """
        # 全条件をチェック
        for condition in rule['conditions']:
            attr_name = condition['attr']
            delay = condition['delay']
            check_idx = t - delay

            # インデックス範囲チェック
            if check_idx < 0:
                return False

            # 属性が存在するかチェック
            if attr_name not in self.test_data.columns:
                return False

            # 属性値が1でなければマッチ失敗
            if self.test_data.loc[check_idx, attr_name] != 1:
                return False

        # 全条件クリア
        return True

    def generate_signals(self, positive_rules, negative_rules, deduplicate=True):
        """
        全時点・全ルールでシグナルを生成（重複排除オプション付き）

        Parameters
        ----------
        positive_rules : list of dict
            BUYルールリスト
        negative_rules : list of dict
            SELLルールリスト
        deduplicate : bool
            True: 1時点1トレードに制限（サポート数で優先順位）
            False: 従来の全シグナル生成

        Returns
        -------
        list of dict
            シグナルリスト
        """
        if self.test_data is None:
            raise ValueError("Data not loaded. Call load_data() first.")

        signals = []

        # 最大遅延を計算（全ルールから）
        all_rules = positive_rules + negative_rules
        max_delay = 0
        for rule in all_rules:
            for condition in rule['conditions']:
                if condition['delay'] > max_delay:
                    max_delay = condition['delay']

        # 評価可能範囲を設定
        start_idx = max_delay
        end_idx = len(self.test_data) - 1  # t+1が必要なので最後は除外

        print(f"\n{'='*80}")
        print(f"Generating Signals (Optimized v2)")
        print(f"{'='*80}")
        print(f"Test period: {self.test_start_date} onwards")
        print(f"Data range: {start_idx} to {end_idx} (total: {end_idx - start_idx} timepoints)")
        print(f"Positive rules: {len(positive_rules)}")
        print(f"Negative rules: {len(negative_rules)}")
        print(f"Deduplication: {'ON (support-based priority)' if deduplicate else 'OFF'}")
        print()

        # 各時点でルールをチェック
        for t in range(start_idx, end_idx):
            timepoint_signals = []

            # Positive rules (BUY シグナル)
            for rule in positive_rules:
                if self.match_rule(rule, t):
                    timepoint_signals.append({
                        't': t,
                        'signal': 'BUY',
                        'rule_id': rule['rule_id'],
                        'rule_text': rule['rule_text'],
                        'expected_x': rule['x_mean'],
                        'direction': 'positive',
                        'support_count': rule['support_count']
                    })

            # Negative rules (SELL シグナル)
            for rule in negative_rules:
                if self.match_rule(rule, t):
                    timepoint_signals.append({
                        't': t,
                        'signal': 'SELL',
                        'rule_id': rule['rule_id'],
                        'rule_text': rule['rule_text'],
                        'expected_x': rule['x_mean'],
                        'direction': 'negative',
                        'support_count': rule['support_count']
                    })

            # 重複排除処理
            if deduplicate and len(timepoint_signals) > 0:
                # サポート数で降順ソート
                timepoint_signals.sort(key=lambda x: x['support_count'], reverse=True)
                # 最もサポート数が高いシグナル1つのみ採用
                signals.append(timepoint_signals[0])
            else:
                # 重複排除なし（従来通り全シグナル追加）
                signals.extend(timepoint_signals)

        print(f"✓ Generated {len(signals)} signals")
        print(f"  BUY signals:  {sum(1 for s in signals if s['signal'] == 'BUY')}")
        print(f"  SELL signals: {sum(1 for s in signals if s['signal'] == 'SELL')}")
        if deduplicate:
            print(f"  ℹ️  Deduplication: 1 signal per timepoint (highest support)")
        print(f"{'='*80}\n")

        return signals

    def get_actual_x(self, t):
        """
        時点t+1の実際のX値を取得

        Parameters
        ----------
        t : int
            エントリー時点

        Returns
        -------
        float
            t+1のX値
        """
        if t + 1 >= len(self.test_data):
            return None
        return self.test_data.loc[t + 1, 'X']

    def get_timestamp(self, t):
        """
        時点tのタイムスタンプを取得

        Parameters
        ----------
        t : int
            時点インデックス

        Returns
        -------
        str
            タイムスタンプ
        """
        return self.test_data.loc[t, 'T']


if __name__ == '__main__':
    # テスト実行
    from rule_loader import RuleLoader

    # ルール読み込み
    loader = RuleLoader('GBPAUD')
    pos_rules, neg_rules = loader.load_all_rules(top_n=10, sort_by='support')

    # シグナル生成（重複排除あり）
    generator = SignalGeneratorV2('GBPAUD', test_start_date='2021-01-01')
    generator.load_data()
    signals = generator.generate_signals(pos_rules, neg_rules, deduplicate=True)

    # サンプル表示
    print("\n=== Signal Sample (first 10) ===")
    for i, sig in enumerate(signals[:10]):
        t = sig['t']
        actual_x = generator.get_actual_x(t)
        timestamp = generator.get_timestamp(t)
        print(f"{i+1}. t={t} ({timestamp}): {sig['signal']} | Rule {sig['rule_id']} (support={sig['support_count']}) | Expected: {sig['expected_x']:.3f} | Actual: {actual_x:.3f}")
