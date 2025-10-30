#!/usr/bin/env python3
"""
トレードシミュレーションモジュール v2（最適化版）

改善点:
1. トランザクションコスト考慮（スプレッド・手数料・スリッページ）
2. 現実的な損益計算
"""

import pandas as pd


class TradeSimulatorV2:
    """最適化版トレードシミュレーションクラス"""

    def __init__(self, pair, spread=0.0002, commission=0.0001, slippage=0.0001):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: USDJPY)
        spread : float
            スプレッド（デフォルト: 0.02%）
        commission : float
            手数料（デフォルト: 0.01%）
        slippage : float
            スリッページ（デフォルト: 0.01%）
        """
        self.pair = pair
        self.spread = spread
        self.commission = commission
        self.slippage = slippage
        self.total_cost = spread + commission + slippage
        self.trades = []

    def simulate(self, signals, signal_generator):
        """
        シグナルに基づいてトレードを実行（コスト考慮）

        Parameters
        ----------
        signals : list of dict
            シグナルリスト
        signal_generator : SignalGeneratorV2
            データアクセス用

        Returns
        -------
        list of dict
            トレード結果リスト
        """
        print(f"\n{'='*80}")
        print(f"Trade Simulation (Optimized v2)")
        print(f"{'='*80}")
        print(f"Total signals: {len(signals)}")
        print(f"Transaction costs:")
        print(f"  Spread:     {self.spread*100:.3f}%")
        print(f"  Commission: {self.commission*100:.3f}%")
        print(f"  Slippage:   {self.slippage*100:.3f}%")
        print(f"  Total:      {self.total_cost*100:.3f}%")
        print()

        self.trades = []

        for sig in signals:
            t = sig['t']
            signal_type = sig['signal']
            rule_id = sig['rule_id']
            expected_x = sig['expected_x']

            # t+1の実際のX値を取得
            actual_x = signal_generator.get_actual_x(t)

            if actual_x is None:
                continue  # データが存在しない場合はスキップ

            # 損益計算（トランザクションコスト考慮）
            if signal_type == 'BUY':
                # ロング: X > 0 で利益、ただしコストを引く
                profit_before_cost = actual_x
                profit = profit_before_cost - self.total_cost
            else:  # SELL
                # ショート: X < 0 で利益（符号反転）、ただしコストを引く
                profit_before_cost = -actual_x
                profit = profit_before_cost - self.total_cost

            # トレード記録
            trade = {
                'entry_t': t,
                'exit_t': t + 1,
                'entry_date': signal_generator.get_timestamp(t),
                'signal': signal_type,
                'rule_id': rule_id,
                'rule_text': sig['rule_text'],
                'direction': sig['direction'],
                'support_count': sig.get('support_count', 0),
                'expected_x': expected_x,
                'actual_x': actual_x,
                'profit_before_cost': profit_before_cost,
                'transaction_cost': self.total_cost,
                'profit': profit,
                'win': 1 if profit > 0 else 0
            }

            self.trades.append(trade)

        print(f"✓ Executed {len(self.trades)} trades")
        print(f"{'='*80}\n")

        return self.trades

    def get_trades_df(self):
        """
        トレード結果をDataFrameとして取得

        Returns
        -------
        pd.DataFrame
            トレード結果
        """
        if not self.trades:
            return pd.DataFrame()

        df = pd.DataFrame(self.trades)

        # 累積リターンを計算
        df['cumulative_return'] = df['profit'].cumsum()

        return df

    def save_trades(self, output_path):
        """
        トレード結果をCSVに保存

        Parameters
        ----------
        output_path : str or Path
            出力ファイルパス
        """
        df = self.get_trades_df()

        if df.empty:
            print("No trades to save.")
            return

        df.to_csv(output_path, index=False)
        print(f"✓ Trades saved to: {output_path}")

    def get_basic_stats(self):
        """
        基本統計を取得

        Returns
        -------
        dict
            統計情報
        """
        if not self.trades:
            return {}

        df = self.get_trades_df()

        buy_trades = df[df['signal'] == 'BUY']
        sell_trades = df[df['signal'] == 'SELL']

        stats = {
            'total_trades': len(df),
            'buy_trades': len(buy_trades),
            'sell_trades': len(sell_trades),
            'total_wins': df['win'].sum(),
            'total_losses': len(df) - df['win'].sum(),
            'win_rate': df['win'].mean() * 100,
            'total_return': df['profit'].sum(),
            'total_return_before_cost': df['profit_before_cost'].sum(),
            'total_cost_paid': df['transaction_cost'].sum() * 100,  # パーセント表記
            'avg_profit': df['profit'].mean(),
            'avg_profit_before_cost': df['profit_before_cost'].mean(),
            'avg_win': df[df['win'] == 1]['profit'].mean() if df['win'].sum() > 0 else 0,
            'avg_loss': df[df['win'] == 0]['profit'].mean() if (len(df) - df['win'].sum()) > 0 else 0,
            'max_win': df['profit'].max(),
            'max_loss': df['profit'].min(),
            'final_cumulative_return': df['cumulative_return'].iloc[-1] if len(df) > 0 else 0,
        }

        # BUY/SELL別統計
        if len(buy_trades) > 0:
            stats['buy_win_rate'] = buy_trades['win'].mean() * 100
            stats['buy_avg_profit'] = buy_trades['profit'].mean()
            stats['buy_total_return'] = buy_trades['profit'].sum()
        else:
            stats['buy_win_rate'] = 0
            stats['buy_avg_profit'] = 0
            stats['buy_total_return'] = 0

        if len(sell_trades) > 0:
            stats['sell_win_rate'] = sell_trades['win'].mean() * 100
            stats['sell_avg_profit'] = sell_trades['profit'].mean()
            stats['sell_total_return'] = sell_trades['profit'].sum()
        else:
            stats['sell_win_rate'] = 0
            stats['sell_avg_profit'] = 0
            stats['sell_total_return'] = 0

        return stats

    def print_stats(self):
        """統計情報を表示"""
        stats = self.get_basic_stats()

        if not stats:
            print("No trades available.")
            return

        print(f"\n{'='*80}")
        print(f"Trade Statistics: {self.pair} (Optimized v2)")
        print(f"{'='*80}")
        print(f"Total Trades:         {stats['total_trades']:6d}")
        print(f"  BUY trades:         {stats['buy_trades']:6d}")
        print(f"  SELL trades:        {stats['sell_trades']:6d}")
        print()
        print(f"Wins / Losses:        {stats['total_wins']:6d} / {stats['total_losses']:6d}")
        print(f"Win Rate:             {stats['win_rate']:6.2f}%")
        print(f"  BUY win rate:       {stats['buy_win_rate']:6.2f}%")
        print(f"  SELL win rate:      {stats['sell_win_rate']:6.2f}%")
        print()
        print(f"Transaction Costs:")
        print(f"  Cost per trade:     {self.total_cost*100:6.3f}%")
        print(f"  Total cost paid:    {stats['total_cost_paid']:6.1f}%")
        print()
        print(f"Return (before cost): {stats['total_return_before_cost']:+7.3f}%")
        print(f"Return (after cost):  {stats['total_return']:+7.3f}%")
        print(f"  Cost impact:        {stats['total_return_before_cost'] - stats['total_return']:+7.3f}%")
        print()
        print(f"  BUY return:         {stats['buy_total_return']:+7.3f}%")
        print(f"  SELL return:        {stats['sell_total_return']:+7.3f}%")
        print()
        print(f"Avg Profit (before):  {stats['avg_profit_before_cost']:+7.4f}%")
        print(f"Avg Profit (after):   {stats['avg_profit']:+7.4f}%")
        print(f"  BUY avg:            {stats['buy_avg_profit']:+7.4f}%")
        print(f"  SELL avg:           {stats['sell_avg_profit']:+7.4f}%")
        print()
        print(f"Average Win:          {stats['avg_win']:+7.4f}%")
        print(f"Average Loss:         {stats['avg_loss']:+7.4f}%")
        print(f"Max Win:              {stats['max_win']:+7.4f}%")
        print(f"Max Loss:             {stats['max_loss']:+7.4f}%")
        print(f"{'='*80}\n")


if __name__ == '__main__':
    # テスト実行
    from rule_loader import RuleLoader
    from signal_generator_v2 import SignalGeneratorV2

    # ルール読み込み
    loader = RuleLoader('GBPAUD')
    pos_rules, neg_rules = loader.load_all_rules(top_n=10, sort_by='support')

    # シグナル生成（重複排除あり）
    generator = SignalGeneratorV2('GBPAUD', test_start_date='2021-01-01')
    generator.load_data()
    signals = generator.generate_signals(pos_rules, neg_rules, deduplicate=True)

    # トレードシミュレーション（コスト考慮）
    simulator = TradeSimulatorV2('GBPAUD')
    trades = simulator.simulate(signals, generator)

    # 統計表示
    simulator.print_stats()

    # トレード結果の一部表示
    df = simulator.get_trades_df()
    print("\n=== Sample Trades (first 10) ===")
    print(df[['entry_date', 'signal', 'rule_id', 'support_count', 'actual_x', 'profit_before_cost', 'profit', 'cumulative_return']].head(10).to_string())
