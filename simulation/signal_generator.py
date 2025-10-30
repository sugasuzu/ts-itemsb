#!/usr/bin/env python3
"""
シグナル生成モジュール

ルールの条件マッチングを行い、BUY/SELLシグナルを生成します。
"""

import pandas as pd
from pathlib import Path


class SignalGenerator:
    """シグナル生成クラス"""

    def __init__(self, pair):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: USDJPY)
        """
        self.pair = pair
        self.data = None

    def load_data(self):
        """
        全履歴データを読み込み

        Returns
        -------
        pd.DataFrame
            全データフレーム
        """
        data_file = Path(f"forex_data/gnminer_individual/{self.pair}.txt")

        if not data_file.exists():
            raise FileNotFoundError(f"Data file not found: {data_file}")

        print(f"Loading data: {data_file}")
        self.data = pd.read_csv(data_file)

        print(f"  ✓ Loaded {len(self.data)} records")
        print(f"  Date range: {self.data['T'].min()} to {self.data['T'].max()}")
        print(f"  X range: {self.data['X'].min():.3f} to {self.data['X'].max():.3f}")

        return self.data

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
            if attr_name not in self.data.columns:
                return False

            # 属性値が1でなければマッチ失敗
            if self.data.loc[check_idx, attr_name] != 1:
                return False

        # 全条件クリア
        return True

    def generate_signals(self, positive_rules, negative_rules):
        """
        全時点・全ルールでシグナルを生成

        Parameters
        ----------
        positive_rules : list of dict
            BUYルールリスト
        negative_rules : list of dict
            SELLルールリスト

        Returns
        -------
        list of dict
            シグナルリスト: [{
                't': 時点,
                'signal': 'BUY' or 'SELL',
                'rule_id': ルールID,
                'rule_text': ルールテキスト,
                'expected_x': 期待値,
                'direction': 'positive' or 'negative'
            }, ...]
        """
        if self.data is None:
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
        end_idx = len(self.data) - 1  # t+1が必要なので最後は除外

        print(f"\n{'='*80}")
        print(f"Generating Signals")
        print(f"{'='*80}")
        print(f"Data range: {start_idx} to {end_idx} (total: {end_idx - start_idx} timepoints)")
        print(f"Positive rules: {len(positive_rules)}")
        print(f"Negative rules: {len(negative_rules)}")
        print()

        # 各時点でルールをチェック
        for t in range(start_idx, end_idx):
            # Positive rules (BUY シグナル)
            for rule in positive_rules:
                if self.match_rule(rule, t):
                    signals.append({
                        't': t,
                        'signal': 'BUY',
                        'rule_id': rule['rule_id'],
                        'rule_text': rule['rule_text'],
                        'expected_x': rule['x_mean'],
                        'direction': 'positive'
                    })

            # Negative rules (SELL シグナル)
            for rule in negative_rules:
                if self.match_rule(rule, t):
                    signals.append({
                        't': t,
                        'signal': 'SELL',
                        'rule_id': rule['rule_id'],
                        'rule_text': rule['rule_text'],
                        'expected_x': rule['x_mean'],
                        'direction': 'negative'
                    })

        print(f"✓ Generated {len(signals)} signals")
        print(f"  BUY signals:  {sum(1 for s in signals if s['signal'] == 'BUY')}")
        print(f"  SELL signals: {sum(1 for s in signals if s['signal'] == 'SELL')}")
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
        if t + 1 >= len(self.data):
            return None
        return self.data.loc[t + 1, 'X']

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
        return self.data.loc[t, 'T']


if __name__ == '__main__':
    # テスト実行
    from rule_loader import RuleLoader

    # ルール読み込み
    loader = RuleLoader('USDJPY')
    pos_rules, neg_rules = loader.load_all_rules(top_n=10, sort_by='support')

    # シグナル生成
    generator = SignalGenerator('USDJPY')
    generator.load_data()
    signals = generator.generate_signals(pos_rules, neg_rules)

    # サンプル表示
    print("\n=== Signal Sample (first 10) ===")
    for i, sig in enumerate(signals[:10]):
        t = sig['t']
        actual_x = generator.get_actual_x(t)
        timestamp = generator.get_timestamp(t)
        print(f"{i+1}. t={t} ({timestamp}): {sig['signal']} | Rule {sig['rule_id']} | Expected: {sig['expected_x']:.3f} | Actual: {actual_x:.3f}")
