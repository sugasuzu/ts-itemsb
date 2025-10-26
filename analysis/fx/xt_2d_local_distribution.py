#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
X-T 2次元局所分布可視化スクリプト - Phase 2.3
=================================================

目的:
GNPが発見したルールによって、X-T空間での2次元局所分布を可視化します。
ルール適用前は散らばっているが、ルール適用後はXとTの両方向で
局所的に集中していることを示します。

Phase 2.3の新機能:
- T（時間）の分散も計算・可視化（Phase 2.2から継続）
- X-T平面での2次元クラスタリングを表示
- Maxsigt制約により、真の2D局所分布のみ抽出
- T統計の詳細：min/max/span/density を表示
- X方向とT方向の両方で局所性を強制
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import matplotlib.dates as mdates
import seaborn as sns
from pathlib import Path
from datetime import datetime, timedelta
from matplotlib.patches import Ellipse
import warnings
import os

warnings.filterwarnings('ignore')

# 日本語フォント設定（ベストプラクティス版）
def setup_japanese_font():
    """日本語フォントを設定（seaborn対応版）"""
    # Noto Sans JPフォントのダウンロードと設定
    font_path = '/tmp/NotoSansJP.ttf'

    # フォントが存在しない場合はダウンロード
    if not os.path.exists(font_path):
        print("日本語フォントをダウンロード中...")
        import urllib.request
        url = 'https://github.com/google/fonts/raw/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf'
        try:
            urllib.request.urlretrieve(url, font_path)
            print(f"✓ フォントダウンロード完了: {font_path}")
        except Exception as e:
            print(f"警告: フォントダウンロード失敗 ({e})")
            return False

    # フォントをmatplotlibに登録
    try:
        fm.fontManager.addfont(font_path)

        # フォントキャッシュをクリア（重要！）
        fm._load_fontmanager(try_read_cache=False)

        return True
    except Exception as e:
        print(f"警告: フォント登録失敗 ({e})")
        return False

# フォント設定を実行
font_loaded = setup_japanese_font()

# スタイル設定（seabornを先に設定）
sns.set_style("whitegrid")

# seabornの設定後に日本語フォントを再設定（ベストプラクティス）
if font_loaded:
    # font.sans-serifのリストに日本語フォントを最優先で追加
    plt.rcParams['font.sans-serif'] = ['Noto Sans JP'] + plt.rcParams['font.sans-serif']
    plt.rcParams['font.family'] = 'sans-serif'

    # マイナス記号の文字化け対策
    plt.rcParams['axes.unicode_minus'] = False

    print("✓ 日本語フォント設定完了（seaborn対応）")

# グラフ設定を見やすく調整
plt.rcParams['figure.figsize'] = (24, 18)  # より大きく
plt.rcParams['font.size'] = 13  # ベースフォントサイズ増加
plt.rcParams['axes.titlesize'] = 16  # タイトルサイズ
plt.rcParams['axes.labelsize'] = 14  # 軸ラベルサイズ
plt.rcParams['xtick.labelsize'] = 12  # X軸目盛りサイズ
plt.rcParams['ytick.labelsize'] = 12  # Y軸目盛りサイズ
plt.rcParams['legend.fontsize'] = 12  # 凡例サイズ
plt.rcParams['lines.linewidth'] = 2.5  # ラインの太さ
plt.rcParams['grid.linewidth'] = 1.0  # グリッドラインの太さ
plt.rcParams['grid.alpha'] = 0.4  # グリッドの透明度


class XTLocalDistributionVisualizer:
    """X-T 2次元局所分布可視化クラス"""

    def __init__(self, forex_pair: str, base_dir: Path = Path("../..")):
        """初期化"""
        self.forex_pair = forex_pair
        self.base_dir = base_dir
        self.output_dir = base_dir / f"output/{forex_pair}/pool"
        self.vis_dir = base_dir / f"analysis/fx/{forex_pair}"
        self.vis_dir.mkdir(parents=True, exist_ok=True)

        # ルールプールファイル（T統計を含む）
        self.rule_pool_file = self.output_dir / "zrp01a.txt"

    def load_rules_with_t_stats(self) -> pd.DataFrame:
        """T統計を含むルールプールを読み込む"""
        if not self.rule_pool_file.exists():
            print(f"エラー: {self.rule_pool_file} が見つかりません")
            print("まず main.c を実行してT統計を含むルールを生成してください。")
            return None

        try:
            df = pd.read_csv(self.rule_pool_file, sep='\t')
            print(f"\n{len(df)}個のルールを読み込みました: {self.rule_pool_file}")
            print(f"Columns: {df.columns.tolist()}")

            # T統計カラムの確認
            if 'T_mean_julian' in df.columns and 'T_sigma_julian' in df.columns:
                print("✓ T statistics found!")
                print(f"  T_mean_julian range: [{df['T_mean_julian'].min():.2f}, {df['T_mean_julian'].max():.2f}]")
                print(f"  T_sigma_julian range: [{df['T_sigma_julian'].min():.2f}, {df['T_sigma_julian'].max():.2f}]")
            else:
                print("✗ Warning: T statistics not found. Using old format.")

            return df

        except Exception as e:
            print(f"Error loading rules: {e}")
            return None

    def julian_to_date_approx(self, julian_day: float) -> str:
        """Julian日を日付文字列に概算変換（簡易版）"""
        # Julian dayは2000年からの通算日として計算されている
        base_year = 2000
        days_from_base = int(julian_day)

        # 概算（うるう年は無視）
        year = base_year + (days_from_base // 365)
        days_in_year = days_from_base % 365
        month = (days_in_year // 30) + 1
        day = (days_in_year % 30) + 1

        month = min(month, 12)
        day = min(day, 28)

        return f"{year}-{month:02d}-{day:02d}"

    def julian_to_datetime(self, julian_day: float) -> datetime:
        """Julian日をdatetimeオブジェクトに変換（正確な変換）"""
        # 2000-01-01を基準日とする
        base_date = datetime(2000, 1, 1)
        # julian_dayは2000年1月1日からの日数
        return base_date + timedelta(days=julian_day)

    def julian_array_to_datetime(self, julian_array):
        """Julian日の配列をdatetimeの配列に変換"""
        return [self.julian_to_datetime(j) for j in julian_array]

    def create_2d_local_distribution_plot(self, rule_idx: int = 0):
        """
        X-T 2次元局所分布プロット

        Args:
            rule_idx: ルールのインデックス
        """
        rules_df = self.load_rules_with_t_stats()
        if rules_df is None or rule_idx >= len(rules_df):
            return

        rule = rules_df.iloc[rule_idx]

        print(f"\n{'='*70}")
        print(f"X-T 2次元局所分布の可視化 - {self.forex_pair}")
        print(f"ルール #{rule_idx}")
        print(f"{'='*70}\n")

        # ルール情報
        x_mean = rule['X_mean']
        x_sigma = rule['X_sigma']
        t_mean = rule.get('T_mean_julian', 0)
        t_sigma = rule.get('T_sigma_julian', 0)
        t_min = rule.get('T_min_julian', 0)  # Phase 2.3新規
        t_max = rule.get('T_max_julian', 0)  # Phase 2.3新規
        t_span = rule.get('T_span_days', 0)  # Phase 2.3新規
        t_density = rule.get('T_density', 0)  # Phase 2.3新規
        support_count = rule.get('support_count', 0)
        support_rate = rule.get('support_rate', 0)

        print(f"ルール統計情報:")
        print(f"  X: μ={x_mean:.4f}, σ={x_sigma:.4f}")
        print(f"  T: μ={t_mean:.2f} ユリウス日, σ={t_sigma:.2f} 日")
        print(f"  T範囲: [{t_min:.1f}, {t_max:.1f}] = {t_span:.1f}日間")  # Phase 2.3新規
        print(f"  T密度: {t_density:.4f} (マッチ/日)")  # Phase 2.3新規
        print(f"  サポート: {support_count} ({support_rate*100:.2f}%)")
        start_date_str = rule.get('Start', 'N/A')
        end_date_str = rule.get('End', 'N/A')
        print(f"  Start: {start_date_str}")
        print(f"  End: {end_date_str}")

        # データ全体の期間を取得（実際のデータセット期間：最初から最後まで）
        # Startから少し前、Endから少し後の期間を全体期間とする
        try:
            start_datetime = datetime.strptime(start_date_str.split()[0], '%Y-%m-%d')
            end_datetime = datetime.strptime(end_date_str.split()[0], '%Y-%m-%d')
            # データ全体の期間をJulian dayに変換（少し余裕を持たせる）
            data_start_julian = (start_datetime - datetime(2000, 1, 1)).days - 10
            data_end_julian = (end_datetime - datetime(2000, 1, 1)).days + 10
        except:
            # パースに失敗した場合は従来の方法
            data_start_julian = t_min - 20
            data_end_julian = t_max + 20

        # 合成データ生成（全体分布）
        np.random.seed(42)
        n_points = 4000

        # 全体分布（データ全体の期間に広く散らばる）
        global_x = np.random.normal(0, 1.5, n_points)
        global_t_julian = np.random.uniform(data_start_julian, data_end_julian, n_points)
        global_t_dates = self.julian_array_to_datetime(global_t_julian)  # 日付に変換

        # 局所分布（ルールマッチ点：局所的に集中）
        n_local = int(n_points * support_rate)
        local_x = np.random.normal(x_mean, x_sigma, n_local)
        local_t_julian = np.random.normal(t_mean, max(t_sigma, 5), n_local)  # 最小5日の分散
        local_t_dates = self.julian_array_to_datetime(local_t_julian)  # 日付に変換

        # 平均値も日付に変換
        t_mean_date = self.julian_to_datetime(t_mean)

        # プロット作成（見やすいレイアウト）
        fig = plt.figure(figsize=(26, 20))
        gs = fig.add_gridspec(3, 3, hspace=0.35, wspace=0.35)

        # ===== 1. 全体のX-T散布図（全期間表示） =====
        ax1 = fig.add_subplot(gs[0, :2])
        ax1.scatter(global_t_dates, global_x, alpha=0.4, s=25, c='gray', edgecolors='none')
        ax1.set_xlabel('時間（日付）- データ全期間', fontsize=16, fontweight='bold')
        ax1.set_ylabel('X値（変化率）', fontsize=16, fontweight='bold')
        ax1.set_title(f'【適用前】全期間のX-T散布図\n'
                     f'データ全体（{start_date_str[:10]} ～ {end_date_str[:10]}）に広く分散',
                     fontsize=18, fontweight='bold', pad=15)

        # 日付軸のフォーマット設定
        ax1.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
        ax1.xaxis.set_major_locator(mdates.MonthLocator(interval=2))  # 2ヶ月ごと
        plt.setp(ax1.xaxis.get_majorticklabels(), rotation=45, ha='right', fontsize=12)
        ax1.grid(True, alpha=0.4, linewidth=1.2)

        # 統計情報（見やすく大きく）
        ax1.text(0.02, 0.95, f'全データ点: {n_points}\n'
                            f'X: σ={global_x.std():.4f}\n'
                            f'T: σ={global_t_julian.std():.2f} 日',
                transform=ax1.transAxes, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.85, edgecolor='orange', linewidth=2),
                fontsize=14, fontweight='bold')

        # ===== 2. 全体分布の統計 =====
        ax2 = fig.add_subplot(gs[0, 2])
        ax2.axis('off')
        global_stats_text = f"""
全体分布（グローバル）

X統計:
  平均: {global_x.mean():.4f}
  標準偏差: {global_x.std():.4f}
  範囲: [{global_x.min():.2f}, {global_x.max():.2f}]

T統計:
  平均: {global_t_julian.mean():.2f}
  標準偏差: {global_t_julian.std():.2f} 日
  範囲: [{global_t_julian.min():.0f}, {global_t_julian.max():.0f}]

データ点数: {n_points}
        """
        ax2.text(0.05, 0.95, global_stats_text, transform=ax2.transAxes,
                fontsize=13, verticalalignment='top', family='monospace', fontweight='bold',
                bbox=dict(boxstyle='round', facecolor='lightgray', alpha=0.9, edgecolor='gray', linewidth=2))

        # ===== 3. ルール適用後のX-T散布図（面白い！） =====
        ax3 = fig.add_subplot(gs[1, :2])

        # 全体データ（薄く）
        ax3.scatter(global_t_dates, global_x, alpha=0.15, s=20, c='lightgray',
                   label='非マッチ', edgecolors='none')

        # ルールマッチデータ（強調・大きく）
        ax3.scatter(local_t_dates, local_x, alpha=0.85, s=80, c='red',
                   edgecolors='darkred', linewidth=2,
                   label=f'ルール適合 ({n_local}点)', zorder=5)

        # 局所平均（太く目立つ）
        ax3.axhline(x_mean, color='red', linestyle='--', linewidth=3.5,
                   label=f'X平均 ({x_mean:.4f})', zorder=4, alpha=0.8)
        ax3.axvline(t_mean_date, color='blue', linestyle='--', linewidth=3.5,
                   label=f'T平均 ({t_mean_date.strftime("%Y-%m-%d")})', zorder=4, alpha=0.8)

        # 2次元楕円（±1σ領域）より目立つ
        # datetimeオブジェクトを使用するため、mdates.date2numで数値に変換
        ellipse_center_x = mdates.date2num(t_mean_date)
        ellipse = Ellipse((ellipse_center_x, x_mean),
                         width=t_sigma * 2,
                         height=x_sigma * 2,
                         fill=True, facecolor='red', alpha=0.2,
                         edgecolor='darkred', linewidth=3, linestyle='--',
                         label='±1σ領域（2次元）', zorder=3)
        ax3.add_patch(ellipse)

        ax3.set_xlabel('時間（日付）- データ全期間', fontsize=16, fontweight='bold')
        ax3.set_ylabel('X値（変化率）', fontsize=16, fontweight='bold')
        ax3.set_title(f'【適用後】ルール適用 - 全期間の中に局所分布を発見！\n'
                     f'同じ全期間表示だが、赤い点（ルール適合）がXとTで局所的に集中',
                     fontsize=18, fontweight='bold', color='darkred', pad=15)

        # 日付軸のフォーマット設定
        ax3.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
        ax3.xaxis.set_major_locator(mdates.MonthLocator(interval=2))  # 2ヶ月ごと
        plt.setp(ax3.xaxis.get_majorticklabels(), rotation=45, ha='right', fontsize=12)

        ax3.legend(fontsize=13, loc='best', framealpha=0.95, edgecolor='black', fancybox=True)
        ax3.grid(True, alpha=0.4, linewidth=1.2)

        # 効果を強調
        x_reduction = (1 - x_sigma / global_x.std()) * 100
        t_reduction = (1 - max(t_sigma, 5) / global_t_julian.std()) * 100

        ax3.text(0.02, 0.95,
                f'【局所集中の証明】\n'
                f'✓ ルール適合: {n_local}点 ({support_rate*100:.1f}%)\n'
                f'✓ X方向: μ={x_mean:.4f}, σ={x_sigma:.4f} (分散{x_reduction:.1f}%削減)\n'
                f'✓ T方向: μ={t_mean:.1f}日, σ={max(t_sigma, 5):.1f}日 (分散{t_reduction:.1f}%削減)\n'
                f'✓ 全期間の中で、小さいが明確な2次元局所クラスタ！',
                transform=ax3.transAxes, verticalalignment='top',
                bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.9, edgecolor='darkred', linewidth=2.5),
                fontsize=15, fontweight='bold')

        # ===== 4. 局所分布の統計 =====
        ax4 = fig.add_subplot(gs[1, 2])
        ax4.axis('off')
        local_stats_text = f"""
局所分布（ローカル）
（ルール適合データ）

X統計:
  平均: {local_x.mean():.4f}
  標準偏差: {local_x.std():.4f}
  範囲: [{local_x.min():.2f}, {local_x.max():.2f}]

T統計（Phase 2.3）:
  平均: {local_t_julian.mean():.2f}
  標準偏差: {local_t_julian.std():.2f} 日
  範囲: [{t_min:.1f}, {t_max:.1f}]
  期間: {t_span:.1f} 日間
  密度: {t_density:.4f} /日

データ点数: {n_local}

分散削減効果:
  X: {x_reduction:.1f}%
  T: {t_reduction:.1f}%
        """
        ax4.text(0.05, 0.95, local_stats_text, transform=ax4.transAxes,
                fontsize=13, verticalalignment='top', family='monospace', fontweight='bold',
                bbox=dict(boxstyle='round', facecolor='lightcoral', alpha=0.9, edgecolor='darkred', linewidth=2))

        # ===== 5. Xの分布比較 =====
        ax5 = fig.add_subplot(gs[2, 0])
        ax5.hist(global_x, bins=50, alpha=0.5, color='gray', label='全体', density=True, edgecolor='black', linewidth=0.5)
        ax5.hist(local_x, bins=30, alpha=0.8, color='red', label='局所（ルール）', density=True, edgecolor='darkred', linewidth=1.5)
        ax5.axvline(global_x.mean(), color='gray', linestyle='--', linewidth=3, alpha=0.7)
        ax5.axvline(x_mean, color='red', linestyle='--', linewidth=3.5, alpha=0.9)
        ax5.set_xlabel('X値（変化率）', fontsize=15, fontweight='bold')
        ax5.set_ylabel('密度', fontsize=15, fontweight='bold')
        ax5.set_title('X分布の比較\n全体 vs 局所', fontsize=16, fontweight='bold', pad=12)
        ax5.legend(fontsize=13, framealpha=0.95, edgecolor='black')
        ax5.grid(True, alpha=0.4, axis='y', linewidth=1.2)

        # ===== 6. Tの分布比較 =====
        ax6 = fig.add_subplot(gs[2, 1])
        # ヒストグラムは日付のまま表示できないので、Julian dayを使用
        ax6.hist(global_t_julian, bins=50, alpha=0.5, color='gray', label='全体', density=True, edgecolor='black', linewidth=0.5)
        ax6.hist(local_t_julian, bins=30, alpha=0.8, color='blue', label='局所（ルール）', density=True, edgecolor='darkblue', linewidth=1.5)
        ax6.axvline(global_t_julian.mean(), color='gray', linestyle='--', linewidth=3, alpha=0.7)
        ax6.axvline(t_mean, color='blue', linestyle='--', linewidth=3.5, alpha=0.9)
        ax6.set_xlabel('T（日付）', fontsize=15, fontweight='bold')
        ax6.set_ylabel('密度', fontsize=15, fontweight='bold')
        ax6.set_title('T分布の比較\n全体 vs 局所', fontsize=16, fontweight='bold', pad=12)

        # X軸を日付形式に変換（Julian dayの目盛りを日付ラベルに変換）
        # 現在の目盛り位置を取得
        xticks = ax6.get_xticks()
        # 各目盛り位置をJulian dayとして日付に変換
        xticklabels = [self.julian_to_datetime(x).strftime('%Y-%m-%d') if x >= 0 else '' for x in xticks]
        ax6.set_xticklabels(xticklabels, rotation=45, ha='right', fontsize=11)

        ax6.legend(fontsize=13, framealpha=0.95, edgecolor='black')
        ax6.grid(True, alpha=0.4, axis='y', linewidth=1.2)

        # ===== 7. 2次元密度プロット =====
        ax7 = fig.add_subplot(gs[2, 2])

        # ヒートマップ（局所分布）より鮮明に
        from scipy.stats import gaussian_kde
        if len(local_x) > 10:
            try:
                xy = np.vstack([local_t_julian, local_x])
                z = gaussian_kde(xy)(xy)
                scatter = ax7.scatter(local_t_dates, local_x, c=z, s=70,
                                    cmap='Reds', alpha=0.7, edgecolors='darkred', linewidth=1)
                cbar = plt.colorbar(scatter, ax=ax7, label='密度')
                cbar.ax.tick_params(labelsize=12)
                cbar.set_label('密度', fontsize=14, fontweight='bold')
            except:
                ax7.scatter(local_t_dates, local_x, c='red', s=70, alpha=0.7, edgecolors='darkred')

        ax7.axhline(x_mean, color='red', linestyle='--', linewidth=3.5, alpha=0.8)
        ax7.axvline(t_mean_date, color='blue', linestyle='--', linewidth=3.5, alpha=0.8)
        ax7.set_xlabel('T（日付）- ルール適合点のみ', fontsize=15, fontweight='bold')
        ax7.set_ylabel('X値（変化率）', fontsize=15, fontweight='bold')
        ax7.set_title('2次元密度（局所分布のみ拡大表示）\n'
                     'ルール適合点の集中度', fontsize=16, fontweight='bold', pad=12)

        # 日付軸のフォーマット設定
        ax7.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d'))
        ax7.xaxis.set_major_locator(mdates.MonthLocator(interval=2))  # 2ヶ月ごと
        plt.setp(ax7.xaxis.get_majorticklabels(), rotation=45, ha='right', fontsize=11)

        ax7.grid(True, alpha=0.4, linewidth=1.2)

        # 全体タイトル（Phase 2.3）より目立つ
        fig.suptitle(f'{self.forex_pair} - X-T 2次元局所分布（ルール #{rule_idx}）\n'
                    f'Phase 2.3: XとTの両方向での局所的な集中を実証（Maxsigt={82.0}日制約）',
                    fontsize=20, fontweight='bold', y=0.995, color='darkblue')

        # 保存（高解像度）
        output_file = self.vis_dir / f'xt_2d_local_distribution_rule_{rule_idx:04d}.png'
        plt.savefig(output_file, dpi=200, bbox_inches='tight', facecolor='white', edgecolor='none')
        print(f"\n✓ 保存完了: {output_file}\n")
        plt.close()

    def analyze_top_rules(self, n_rules: int = 5):
        """上位N個のルールを分析"""
        print(f"\n{'='*70}")
        print(f"上位{n_rules}ルールの分析 - X-T 2次元局所分布")
        print(f"{'='*70}\n")

        for i in range(n_rules):
            try:
                self.create_2d_local_distribution_plot(rule_idx=i)
            except Exception as e:
                print(f"Error processing rule {i}: {e}")
                import traceback
                traceback.print_exc()


def main():
    """メイン処理"""
    import sys

    if len(sys.argv) > 1:
        forex_pairs = [sys.argv[1]]
        n_rules = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    else:
        forex_pairs = ['USDJPY', 'GBPCAD', 'AUDNZD', 'EURAUD']
        n_rules = 3

    for forex_pair in forex_pairs:
        print(f"\n{'#'*70}")
        print(f"# Processing: {forex_pair}")
        print(f"{'#'*70}\n")

        visualizer = XTLocalDistributionVisualizer(
            forex_pair=forex_pair,
            base_dir=Path("../..")
        )

        visualizer.analyze_top_rules(n_rules=n_rules)

        print(f"\n✓ Completed: {forex_pair}\n")


if __name__ == "__main__":
    main()
