#!/usr/bin/env python3
"""
トレードシミュレーションモジュール

シグナルに基づいてトレードを実行し、損益を計算します。
"""

import pandas as pd


class TradeSimulator:
    """トレードシミュレーションクラス"""

    def __init__(self, pair):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: USDJPY)
        """
        self.pair = pair
        self.trades = []

    def simulate(self, signals, signal_generator):
        """
        シグナルに基づいてトレードを実行

        Parameters
        ----------
        signals : list of dict
            シグナルリスト
        signal_generator : SignalGenerator
            データアクセス用

        Returns
        -------
        list of dict
            トレード結果リスト
        """
        print(f"\n{'='*80}")
        print(f"Trade Simulation")
        print(f"{'='*80}")
        print(f"Total signals: {len(signals)}")
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

            # 損益計算
            if signal_type == 'BUY':
                # ロング: X > 0 で利益
                profit = actual_x
            else:  # SELL
                # ショート: X < 0 で利益（符号反転）
                profit = -actual_x

            # トレード記録
            trade = {
                'entry_t': t,
                'exit_t': t + 1,
                'entry_date': signal_generator.get_timestamp(t),
                'signal': signal_type,
                'rule_id': rule_id,
                'rule_text': sig['rule_text'],
                'direction': sig['direction'],
                'expected_x': expected_x,
                'actual_x': actual_x,
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
            'avg_profit': df['profit'].mean(),
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
        print(f"Trade Statistics: {self.pair}")
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
        print(f"Total Return:         {stats['total_return']:+7.3f}%")
        print(f"  BUY return:         {stats['buy_total_return']:+7.3f}%")
        print(f"  SELL return:        {stats['sell_total_return']:+7.3f}%")
        print()
        print(f"Average Profit:       {stats['avg_profit']:+7.4f}%")
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
    from signal_generator import SignalGenerator

    # ルール読み込み
    loader = RuleLoader('USDJPY')
    pos_rules, neg_rules = loader.load_all_rules(top_n=10, sort_by='support')

    # シグナル生成
    generator = SignalGenerator('USDJPY')
    generator.load_data()
    signals = generator.generate_signals(pos_rules, neg_rules)

    # トレードシミュレーション
    simulator = TradeSimulator('USDJPY')
    trades = simulator.simulate(signals, generator)

    # 統計表示
    simulator.print_stats()

    # トレード結果の一部表示
    df = simulator.get_trades_df()
    print("\n=== Sample Trades (first 10) ===")
    print(df[['entry_date', 'signal', 'rule_id', 'expected_x', 'actual_x', 'profit', 'cumulative_return']].head(10).to_string())
