#!/usr/bin/env python3
"""
パフォーマンス評価モジュール

トレード結果の詳細分析と可視化を行います。
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

# 日本語フォント設定
plt.rcParams['font.sans-serif'] = ['Arial Unicode MS', 'Hiragino Sans', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False


class PerformanceEvaluator:
    """パフォーマンス評価クラス"""

    def __init__(self, pair):
        """
        Parameters
        ----------
        pair : str
            通貨ペア (例: USDJPY)
        """
        self.pair = pair
        self.trades_df = None

    def set_trades(self, trades_df):
        """
        トレードデータをセット

        Parameters
        ----------
        trades_df : pd.DataFrame
            トレード結果DataFrame
        """
        self.trades_df = trades_df

    def calculate_drawdown(self):
        """
        ドローダウンを計算

        Returns
        -------
        pd.Series
            各時点のドローダウン
        """
        if self.trades_df is None or self.trades_df.empty:
            return pd.Series()

        cumulative = self.trades_df['cumulative_return']
        running_max = cumulative.cummax()
        drawdown = cumulative - running_max

        return drawdown

    def get_max_drawdown(self):
        """
        最大ドローダウンを取得

        Returns
        -------
        float
            最大ドローダウン
        """
        drawdown = self.calculate_drawdown()
        if drawdown.empty:
            return 0
        return drawdown.min()

    def get_rule_performance(self):
        """
        ルール別パフォーマンスを取得

        Returns
        -------
        pd.DataFrame
            ルール別統計
        """
        if self.trades_df is None or self.trades_df.empty:
            return pd.DataFrame()

        # ルールごとにグループ化
        rule_stats = self.trades_df.groupby(['rule_id', 'signal', 'direction']).agg({
            'profit': ['count', 'sum', 'mean'],
            'win': 'sum'
        }).reset_index()

        # カラム名を平坦化
        rule_stats.columns = ['rule_id', 'signal', 'direction', 'trade_count', 'total_return', 'avg_profit', 'wins']

        # 勝率を計算
        rule_stats['win_rate'] = (rule_stats['wins'] / rule_stats['trade_count'] * 100)

        # ソート（総利益の降順）
        rule_stats = rule_stats.sort_values('total_return', ascending=False)

        return rule_stats

    def print_rule_performance(self, top_n=10):
        """
        ルール別パフォーマンスを表示

        Parameters
        ----------
        top_n : int
            表示する上位ルール数
        """
        rule_stats = self.get_rule_performance()

        if rule_stats.empty:
            print("No rule performance data available.")
            return

        print(f"\n{'='*100}")
        print(f"Rule Performance Analysis: {self.pair}")
        print(f"{'='*100}")
        print(f"{'Rule ID':<10} {'Signal':<6} {'Direction':<10} {'Trades':<8} {'Win Rate':<10} {'Avg Profit':<12} {'Total Return':<12}")
        print(f"{'-'*100}")

        for idx, row in rule_stats.head(top_n).iterrows():
            print(f"{row['rule_id']:<10d} {row['signal']:<6} {row['direction']:<10} {row['trade_count']:<8d} "
                  f"{row['win_rate']:>7.2f}%  {row['avg_profit']:>+10.4f}%  {row['total_return']:>+10.3f}%")

        print(f"{'='*100}\n")

    def create_cumulative_return_plot(self, output_path):
        """
        累積リターンチャートを作成

        Parameters
        ----------
        output_path : str or Path
            出力ファイルパス
        """
        if self.trades_df is None or self.trades_df.empty:
            print("No trades to plot.")
            return

        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 10))

        # 上段: 累積リターン
        ax1.plot(range(len(self.trades_df)), self.trades_df['cumulative_return'],
                linewidth=2, color='blue', label='Cumulative Return')
        ax1.axhline(y=0, color='gray', linestyle='--', linewidth=1, alpha=0.5)
        ax1.set_xlabel('Trade Number', fontsize=12, fontweight='bold')
        ax1.set_ylabel('Cumulative Return (%)', fontsize=12, fontweight='bold')
        ax1.set_title(f'{self.pair} - Cumulative Return', fontsize=14, fontweight='bold')
        ax1.legend(loc='upper left', fontsize=11)
        ax1.grid(True, alpha=0.3)

        # 下段: ドローダウン
        drawdown = self.calculate_drawdown()
        ax2.fill_between(range(len(drawdown)), 0, drawdown, color='red', alpha=0.3, label='Drawdown')
        ax2.plot(range(len(drawdown)), drawdown, linewidth=2, color='darkred')
        ax2.set_xlabel('Trade Number', fontsize=12, fontweight='bold')
        ax2.set_ylabel('Drawdown (%)', fontsize=12, fontweight='bold')
        ax2.set_title(f'{self.pair} - Drawdown', fontsize=14, fontweight='bold')
        ax2.legend(loc='lower left', fontsize=11)
        ax2.grid(True, alpha=0.3)

        # 統計ボックス
        max_dd = self.get_max_drawdown()
        final_return = self.trades_df['cumulative_return'].iloc[-1]
        total_trades = len(self.trades_df)
        win_rate = self.trades_df['win'].mean() * 100

        stats_text = (
            f"Performance Summary:\n"
            f"━━━━━━━━━━━━━━━━━━━━\n"
            f"Total Trades:    {total_trades:5d}\n"
            f"Win Rate:        {win_rate:6.2f}%\n"
            f"Total Return:    {final_return:+7.3f}%\n"
            f"Max Drawdown:    {max_dd:+7.3f}%\n"
        )

        ax1.text(0.98, 0.02, stats_text, transform=ax1.transAxes,
                fontsize=10, family='monospace',
                verticalalignment='bottom', horizontalalignment='right',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

        plt.tight_layout()
        plt.savefig(output_path, dpi=100, bbox_inches='tight')
        plt.close()

        print(f"✓ Cumulative return plot saved to: {output_path}")

    def create_rule_performance_plot(self, output_path, top_n=20):
        """
        ルール別パフォーマンスチャートを作成

        Parameters
        ----------
        output_path : str or Path
            出力ファイルパス
        top_n : int
            表示する上位ルール数
        """
        rule_stats = self.get_rule_performance()

        if rule_stats.empty:
            print("No rule performance data to plot.")
            return

        # 上位N件
        top_rules = rule_stats.head(top_n)

        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(18, 8))

        # 左: 総リターン
        colors = ['green' if s == 'BUY' else 'red' for s in top_rules['signal']]
        bars1 = ax1.barh(range(len(top_rules)), top_rules['total_return'], color=colors, alpha=0.7)
        ax1.set_yticks(range(len(top_rules)))
        ax1.set_yticklabels([f"Rule {r} ({s})" for r, s in zip(top_rules['rule_id'], top_rules['signal'])],
                           fontsize=9)
        ax1.set_xlabel('Total Return (%)', fontsize=12, fontweight='bold')
        ax1.set_title('Top Rules by Total Return', fontsize=14, fontweight='bold')
        ax1.axvline(x=0, color='black', linewidth=1)
        ax1.grid(True, alpha=0.3, axis='x')

        # 右: 勝率
        bars2 = ax2.barh(range(len(top_rules)), top_rules['win_rate'], color=colors, alpha=0.7)
        ax2.set_yticks(range(len(top_rules)))
        ax2.set_yticklabels([f"Rule {r} ({s})" for r, s in zip(top_rules['rule_id'], top_rules['signal'])],
                           fontsize=9)
        ax2.set_xlabel('Win Rate (%)', fontsize=12, fontweight='bold')
        ax2.set_title('Top Rules by Win Rate', fontsize=14, fontweight='bold')
        ax2.axvline(x=50, color='gray', linestyle='--', linewidth=1, alpha=0.5)
        ax2.grid(True, alpha=0.3, axis='x')
        ax2.set_xlim(0, 100)

        plt.tight_layout()
        plt.savefig(output_path, dpi=100, bbox_inches='tight')
        plt.close()

        print(f"✓ Rule performance plot saved to: {output_path}")

    def generate_report(self, output_dir):
        """
        完全なレポートを生成

        Parameters
        ----------
        output_dir : str or Path
            出力ディレクトリ
        """
        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        print(f"\n{'='*80}")
        print(f"Generating Performance Report")
        print(f"{'='*80}")

        # 累積リターンチャート
        self.create_cumulative_return_plot(output_dir / f"{self.pair}_cumulative_return.png")

        # ルールパフォーマンスチャート
        self.create_rule_performance_plot(output_dir / f"{self.pair}_rule_performance.png")

        # ルール別統計CSV
        rule_stats = self.get_rule_performance()
        if not rule_stats.empty:
            rule_stats.to_csv(output_dir / f"{self.pair}_rule_stats.csv", index=False)
            print(f"✓ Rule statistics saved to: {output_dir / f'{self.pair}_rule_stats.csv'}")

        print(f"{'='*80}\n")


if __name__ == '__main__':
    # テスト実行
    from rule_loader import RuleLoader
    from signal_generator import SignalGenerator
    from trade_simulator import TradeSimulator

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
    trades_df = simulator.get_trades_df()

    # パフォーマンス評価
    evaluator = PerformanceEvaluator('USDJPY')
    evaluator.set_trades(trades_df)

    # レポート生成
    evaluator.print_rule_performance(top_n=20)
    evaluator.generate_report('simulation/results')
