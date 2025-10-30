#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   時系列対応版GNMiner Phase 2 - 時間変数T統合版（actual_X追加版）

   【概要】
   このプログラムは、Genetic Network Programming (GNP)を使用して、
   時系列為替レートデータから予測ルールを自動発見するシステムです。

   【主要機能】
   1. 過去の複数時点の属性パターンから将来の為替レートを予測
   2. 時間遅延メカニズムによる時系列パターンの捕捉
   3. 適応的学習による効率的なルール発見
   4. 現在時点の実際値（actual_X）と予測値の両方を追跡
   5. 時間パターン（月次、四半期、曜日）の分析

   【Phase 2の特徴】
   - actual_X（現在時点の実際の値）を記録・分析
   - 予測値だけでなく、ルール適用時点の実際の値も把握可能
   - より詳細な時系列パターンの理解が可能
   ================================================================================
*/

/* ================================================================================
   定数定義セクション

   このセクションでは、プログラム全体で使用される定数を定義します。
   これらの値を変更することで、プログラムの動作を調整できます。
   ================================================================================
*/

/* 時系列パラメータ */
#define TIMESERIES_MODE 1       // 時系列モードの有効化（1:有効、0:無効）
#define MAX_TIME_DELAY 10       // Phase 1での最大時間遅延（未使用、互換性のため残存）
#define MAX_TIME_DELAY_PHASE2 4 // Phase 2での最大時間遅延（t-3まで参照可能）
#define MIN_TIME_DELAY 0        // 最小時間遅延（現在時点）
#define PREDICTION_SPAN 1       // 予測スパン（t+1を予測）
#define ADAPTIVE_DELAY 1        // 適応的遅延学習の有効化（1:有効、0:無効）

/* ディレクトリ構造
   出力ファイルを整理するためのディレクトリ構造を定義 */
#define OUTPUT_DIR "output"           // メイン出力ディレクトリ
#define OUTPUT_DIR_IL "output/IL"     // Individual Lists: ルールリスト
#define OUTPUT_DIR_IA "output/IA"     // Individual Analysis: 分析レポート
#define OUTPUT_DIR_IB "output/IB"     // Individual Backup: バックアップ（actual_X含む）
#define OUTPUT_DIR_POOL "output/pool" // グローバルルールプール
#define OUTPUT_DIR_DOC "output/doc"   // ドキュメント・統計
#define OUTPUT_DIR_VIS "output/vis"   // 可視化用データ

/* ファイル名
   入力データと出力ファイルのパスを定義 */
#define DATANAME "forex_data/gnminer_individual/USDJPY.txt" // 入力データファイル（デフォルト通貨ペアUSDJPY）
#define POOL_FILE_A "output/pool/zrp01a.txt"                // ルールプール出力A（詳細版）
#define POOL_FILE_B "output/pool/zrp01b.txt"                // ルールプール出力B（要約版）
#define CONT_FILE "output/doc/zrd01.txt"                    // 統計情報ファイル
#define RESULT_FILE "output/doc/zrmemo01.txt"               // メモファイル（未使用）

/* 動的ファイルパス（コマンドライン引数で変更可能） */
char forex_pair[20] = "USDJPY";              // 為替ペアコード
char data_file_path[512] = DATANAME;         // データファイルパス
char output_base_dir[256] = "output";        // 出力ベースディレクトリ

/* 極値方向フィルタリングモード */
int extreme_direction = 0;                   // 0: 両方, 1: 正のみ(X>=1.0), -1: 負のみ(X<=-1.0)
char pool_file_a[512] = POOL_FILE_A;         // 動的ルールプールA
char pool_file_b[512] = POOL_FILE_B;         // 動的ルールプールB
char cont_file[512] = CONT_FILE;             // 動的統計ファイル
char output_dir_il[512] = OUTPUT_DIR_IL;     // 動的IL出力
char output_dir_ia[512] = OUTPUT_DIR_IA;     // 動的IA出力
char output_dir_ib[512] = OUTPUT_DIR_IB;     // 動的IB出力
char output_dir_pool[512] = OUTPUT_DIR_POOL; // 動的pool出力
char output_dir_doc[512] = OUTPUT_DIR_DOC;   // 動的doc出力
char output_dir_vis[512] = OUTPUT_DIR_VIS;   // 動的vis出力

/* 主要為替ペアリスト（FX） */
const char *FOREX_PAIRS[] = {
    // 対円ペア (7ペア)
    "USDJPY", "EURJPY", "GBPJPY", "AUDJPY", "NZDJPY", "CADJPY", "CHFJPY",
    // 主要クロスペア (6ペア)
    "EURUSD", "GBPUSD", "AUDUSD", "NZDUSD", "USDCAD", "USDCHF",
    // その他の主要ペア (7ペア)
    "EURGBP", "EURAUD", "EURCHF", "GBPAUD", "GBPCAD", "AUDCAD", "AUDNZD",
    NULL // 終端マーカー
};

#define FOREX_PAIRS_COUNT 20 // 為替ペア数

/* データ構造パラメータ
   CSVファイルから読み込み時に動的に設定される */
int Nrd = 0; // レコード数（データの行数）
int Nzk = 0; // 属性数（カラム数 - X列 - T列）

/* ルールマイニング制約
   抽出するルールの品質を制御する閾値 */
#define Nrulemax 2002    // 最大ルール数（メモリ制限）
#define Minsup 0.1     // 最小サポート値（10%以上の頻度が必要)
#define Maxsigx 5.0     // 最大分散（分散が5.0以下のルールのみ採用）
#define MIN_ATTRIBUTES 2 // ルールの最小属性数（2個以上の属性が必要）

/* 実験パラメータ
   実験の規模と繰り返し回数を設定 */
#define Nstart 1000 // 試行開始番号（ファイル名に使用）
#define Ntry 10     // 試行回数（10回の独立した実験を実行）

/* GNPパラメータ
   Genetic Network Programmingの構造を定義 */
#define Generation 201 // 進化世代数（201世代まで進化）
#define Nkotai 120     // 個体数（120個体の集団）
#define Npn 10         // 処理ノード数（ルールの開始点）
#define Njg 100        // 判定ノード数（属性をチェックするノード）
#define Nmx 7          // 最大深さ（ルールの最大長）

/* 突然変異率
   進化操作での変異確率を制御（値が小さいほど変異頻度が高い） */
#define Muratep 1     // 処理ノードの接続変更確率（1/1 = 100%）
#define Muratej 6     // 判定ノードの接続変更確率（1/6 = 16.7%）
#define Muratea 6     // 属性変更確率（1/6 = 16.7%）
#define Muratedelay 6 // 時間遅延変更確率（1/6 = 16.7%）
#define Nkousa 20     // 交叉点の数（交叉操作で交換する遺伝子数）

/* ルール構造パラメータ
   ルールの内部表現に関する制限値 */
#define MAX_ATTRIBUTES 8       // ルール内の最大属性数
#define MAX_DEPTH 10           // 探索の最大深さ
#define HISTORY_GENERATIONS 5  // 履歴保持世代数（適応学習用）
#define MAX_RECORDS 10000      // 最大レコード数
#define MAX_ATTRS 1000         // 最大属性数
#define MAX_ATTR_NAME 50       // 属性名の最大長
#define MAX_LINE_LENGTH 100000 // CSVの1行の最大文字数

/* 時間分析パラメータ
   時系列パターン分析用の定数 */
#define DAYS_IN_WEEK 7     // 曜日数
#define MONTHS_IN_YEAR 12  // 月数
#define QUARTERS_IN_YEAR 4 // 四半期数
#define MAX_LAG 10         // 自己相関の最大ラグ

/* 進化計算パラメータ
   選択・交叉・突然変異の詳細設定 */
#define ELITE_SIZE (Nkotai / 3) // エリート個体数（上位1/3）
#define ELITE_COPIES 3          // エリート個体の複製数
#define CROSSOVER_PAIRS 20      // 交叉ペア数
#define MUTATION_START_40 40    // 突然変異開始位置1（個体40から）
#define MUTATION_START_80 80    // 突然変異開始位置2（個体80から）

/* 品質判定パラメータ
   ルールの品質評価に使用する閾値とボーナス */
#define HIGH_SUPPORT_BONUS 0.02    // 高サポートルールのボーナス閾値
#define LOW_VARIANCE_REDUCTION 1.0 // 低分散ルールの削減値
#define FITNESS_SUPPORT_WEIGHT 10  // 適応度計算：サポート値の重み
#define FITNESS_SIGMA_OFFSET 0.1   // 適応度計算：標準偏差のオフセット
#define FITNESS_NEW_RULE_BONUS 20  // 適応度計算：新規ルールボーナス
#define FITNESS_ATTRIBUTE_WEIGHT 1 // 適応度計算：属性数の重み
#define FITNESS_SIGMA_WEIGHT 4     // 適応度計算：標準偏差の重み

/* レポート間隔
   進捗報告とログ出力の頻度 */
#define REPORT_INTERVAL 5            // 進捗報告間隔（5世代ごと）
#define DELAY_DISPLAY_INTERVAL 20    // 遅延使用状況表示間隔
#define FILENAME_BUFFER_SIZE 256     // ファイル名バッファサイズ
#define FILENAME_DIGITS 5            // ファイル名の桁数
#define FITNESS_INIT_OFFSET -0.00001 // 適応度初期化オフセット
#define INITIAL_DELAY_HISTORY 1      // 遅延履歴の初期値
#define REFRESH_BONUS 2              // リフレッシュボーナス

/* ================================================================================
   構造体定義

   プログラムで使用する主要なデータ構造を定義します。
   これらの構造体は、時系列データ、ルール、統計情報などを管理します。
   ================================================================================
*/

/* 時間情報構造体
   各データポイントのタイムスタンプから抽出された時間情報を保持 */
struct time_info
{
    int year;         // 年
    int month;        // 月（1-12）
    int day;          // 日（1-31）
    int week_of_year; // 年初からの週番号
    int day_of_week;  // 曜日（1:月曜日～7:日曜日）
    int quarter;      // 四半期（1-4）
    int julian_day;   // ユリウス通算日（簡易計算）
};

/* 時間パターン統計構造体
   ルールがマッチした時点の時間的特徴を統計的に分析 */
struct temporal_statistics
{
    // 月別統計（12ヶ月分）
    double monthly_mean[MONTHS_IN_YEAR];     // 月別平均値
    double monthly_variance[MONTHS_IN_YEAR]; // 月別分散
    int monthly_count[MONTHS_IN_YEAR];       // 月別カウント

    // 四半期別統計（4四半期分）
    double quarterly_mean[QUARTERS_IN_YEAR];     // 四半期別平均値
    double quarterly_variance[QUARTERS_IN_YEAR]; // 四半期別分散
    int quarterly_count[QUARTERS_IN_YEAR];       // 四半期別カウント

    // 曜日別統計（7曜日分）
    double weekly_mean[DAYS_IN_WEEK];     // 曜日別平均値
    double weekly_variance[DAYS_IN_WEEK]; // 曜日別分散
    int weekly_count[DAYS_IN_WEEK];       // 曜日別カウント

    // 時系列特性
    double autocorrelation[MAX_LAG]; // 自己相関係数（未実装）
    double seasonality_strength;     // 季節性の強さ（未実装）
    double trend_coefficient;        // トレンド係数（未実装）
};

/* 時系列ルール構造体（Phase 2の中核）
   発見されたルールの完全な情報を保持 */
struct temporal_rule
{
    // ルールの前件部（IF部分）
    int antecedent_attrs[MAX_ATTRIBUTES]; // 属性ID配列（1-indexed、0は未使用）
    int time_delays[MAX_ATTRIBUTES];      // 各属性の時間遅延（0=t, 1=t-1, ...）

    // 予測値の統計（t+1の値）
    double x_mean;  // 予測値の平均
    double x_sigma; // 予測値の標準偏差

    // T（時間）の統計 - Phase 2.2新機能
    double t_mean_julian;  // Tの平均（Julian day）
    double t_sigma_julian; // Tの標準偏差（Julian day単位）

    // ルールの品質指標
    int support_count;     // サポートカウント（マッチした回数）
    int negative_count;    // ネガティブカウント（評価対象数）
    double support_rate;   // サポート率（support_count / negative_count）
    int high_support_flag; // 高サポートフラグ（1:高サポート）
    int low_variance_flag; // 低分散フラグ（1:低分散）
    int num_attributes;    // 属性数

    // 時間パターン情報
    struct temporal_statistics time_stats; // 時間統計
    int dominant_month;                    // 支配的な月
    int dominant_quarter;                  // 支配的な四半期
    int dominant_day_of_week;              // 支配的な曜日

    // 時間窓情報
    char start_date[20]; // ルールがマッチした最初の日付
    char end_date[20];   // ルールがマッチした最後の日付
    int time_span_days;  // 期間（日数）

    // マッチした時点のインデックス（可視化・分析用）
    int *matched_indices;  // マッチしたレコードのインデックス配列
    int matched_count_vis; // マッチ数（可視化用）

    // 極値シグナルスコア
    double signal_strength;      // シグナル強度 |X_mean|
    double SNR;                  // Signal-to-Noise Ratio
    double extremeness;          // 全体分布からの乖離度
    double t_statistic;          // t検定統計量
    double p_value;              // t検定p値
    double tail_event_rate;      // 極端イベント発生率
    double extreme_signal_score; // 総合極値シグナルスコア
};

/* 比較用ルール構造体
   ルールの重複チェック用の簡易版 */
struct cmrule
{
    int antecedent_attrs[MAX_ATTRIBUTES]; // 属性ID配列
    int num_attributes;                   // 属性数
};

/* 試行状態管理構造体
   各試行（トライアル）の実行状態を管理 */
struct trial_state
{
    int trial_id;                // 試行ID
    int rule_count;              // 発見したルール数
    int high_support_rule_count; // 高サポートルール数
    int low_variance_rule_count; // 低分散ルール数
    int generation;              // 現在の世代数
    double elapsed_time;         // 経過時間（秒）

    // 出力ファイル名
    char filename_rule[FILENAME_BUFFER_SIZE];   // ルールリストファイル
    char filename_report[FILENAME_BUFFER_SIZE]; // レポートファイル
    char filename_local[FILENAME_BUFFER_SIZE];  // ローカル詳細ファイル
};

/* ================================================================================
   グローバル変数

   プログラム全体で共有されるデータを管理します。
   動的メモリ割り当てを使用して、データサイズに応じて柔軟に対応します。
   ================================================================================
*/

/* データバッファ（動的割り当て）
   CSVから読み込んだデータを保持 */
int **data_buffer = NULL;                 // 属性データ [レコード数][属性数]
double *x_buffer = NULL;                  // X値（予測対象）[レコード数]
char **timestamp_buffer = NULL;           // タイムスタンプ [レコード数][20文字]
char **attribute_dictionary = NULL;       // 属性名辞書 [属性数+3][最大50文字]
struct time_info *time_info_array = NULL; // 時間情報配列 [レコード数]

/* ルールプール
   発見されたルールを格納（静的配列） */
struct temporal_rule rule_pool[Nrulemax]; // ルールプール（試行ごと）
struct cmrule compare_rules[Nrulemax];    // 比較用ルール

/* グローバルルールプール
   全試行を通じて発見された統合ルール */
struct temporal_rule *global_rule_pool = NULL; // グローバルルールプール（動的）
struct cmrule *global_compare_rules = NULL;    // グローバル比較用ルール
int global_rule_count = 0;                     // グローバルルール数
int global_high_support_count = 0;             // グローバル高サポート数
int global_low_variance_count = 0;             // グローバル低分散数

/* 時間遅延統計
   適応的学習のための遅延使用履歴 */
int delay_usage_history[HISTORY_GENERATIONS][MAX_TIME_DELAY_PHASE2 + 1]; // 遅延使用履歴
int delay_usage_count[MAX_TIME_DELAY_PHASE2 + 1];                        // 遅延使用カウント
int delay_tracking[MAX_TIME_DELAY_PHASE2 + 1];                           // 遅延追跡（累積）
double delay_success_rate[MAX_TIME_DELAY_PHASE2 + 1];                    // 成功率（未使用）

/* GNPノード構造（動的割り当て）
   各個体のネットワーク構造を表現 */
int ***node_structure = NULL; // [個体数][ノード数][3(属性,接続,遅延)]

/* 評価統計配列（動的割り当て）
   ルール評価時の各種統計を保持 */
int ***match_count = NULL;      // マッチカウント [個体][処理ノード][深さ]
int ***negative_count = NULL;   // ネガティブカウント
int ***evaluation_count = NULL; // 評価カウント
int ***attribute_chain = NULL;  // 属性チェーン（評価中の属性列）
int ***time_delay_chain = NULL; // 時間遅延チェーン
double ***x_sum = NULL;         // X値の合計（予測値）
double ***x_sigma_array = NULL; // X値の二乗和（予測値の分散計算用）

// T（時間）統計配列 - Phase 2.2新機能
double ***t_sum_julian = NULL;         // Julian dayの合計
double ***t_sigma_julian_array = NULL; // Julian dayの二乗和（分散計算用）

/* 時間パターン追跡配列
   マッチした時点の詳細を記録 */
int ****matched_time_indices = NULL; // マッチ時点のインデックス
int ***matched_time_count = NULL;    // マッチ時点のカウント

/* 属性使用統計（動的割り当て）
   適応的学習のための属性使用履歴 */
int **attribute_usage_history = NULL; // 属性使用履歴
int *attribute_usage_count = NULL;    // 属性使用カウント
int *attribute_tracking = NULL;       // 属性追跡（累積）

/* 適応度関連
   進化計算での個体評価 */
double fitness_value[Nkotai]; // 各個体の適応度値
int fitness_ranking[Nkotai];  // 適応度ランキング

/* 遺伝子プール（動的割り当て）
   次世代の遺伝情報を保持 */
int **gene_connection = NULL; // 接続遺伝子
int **gene_attribute = NULL;  // 属性遺伝子
int **gene_time_delay = NULL; // 時間遅延遺伝子

/* グローバル統計カウンタ
   全試行を通じた累積統計 */
int total_rule_count = 0;   // 総ルール数
int total_high_support = 0; // 総高サポートルール数
int total_low_variance = 0; // 総低分散ルール数

/* その他の作業用配列 */
int rules_per_trial[Ntry];               // 各試行のルール数
int new_rule_marker[Nrulemax];           // 新規ルールマーカー
int rules_by_attribute_count[MAX_DEPTH]; // 属性数別ルール数
int *attribute_set = NULL;               // 属性セット

/* データ列のインデックス
   CSVのどの列がX値とタイムスタンプかを記録 */
int x_column_index = -1; // X列のインデックス
int t_column_index = -1; // T列（タイムスタンプ）のインデックス

/* 統計情報 */
int rules_by_min_attributes = 0; // 最小属性数を満たすルール数

/* ================================================================================
   時間処理関数

   タイムスタンプの解析と時間関連の計算を行います。
   ================================================================================
*/

/**
 * タイムスタンプ文字列を解析して時間情報構造体に変換
 * @param timestamp YYYY-MM-DD形式の日付文字列
 * @param tinfo 解析結果を格納する時間情報構造体へのポインタ
 */
void parse_timestamp(const char *timestamp, struct time_info *tinfo)
{
    // YYYY-MM-DD形式の文字列から年月日を抽出
    sscanf(timestamp, "%d-%d-%d", &tinfo->year, &tinfo->month, &tinfo->day);

    // 四半期を計算（1-3月:Q1, 4-6月:Q2, 7-9月:Q3, 10-12月:Q4）
    if (tinfo->month <= 3)
        tinfo->quarter = 1;
    else if (tinfo->month <= 6)
        tinfo->quarter = 2;
    else if (tinfo->month <= 9)
        tinfo->quarter = 3;
    else
        tinfo->quarter = 4;

    // 曜日を計算（Zellerの公式の簡易版を使用）
    int y = tinfo->year;
    int m = tinfo->month;
    int d = tinfo->day;

    // Zellerの公式では1月と2月を前年の13月と14月として扱う
    if (m < 3)
    {
        m += 12;
        y -= 1;
    }

    // Zellerの公式による曜日計算
    int k = y % 100; // 年の下2桁
    int j = y / 100; // 年の上2桁（世紀）
    int h = (d + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)) % 7;
    tinfo->day_of_week = ((h + 5) % 7) + 1; // 1=月曜日, 7=日曜日に変換

    // 通算日数を計算（2000年を基準とした簡易版）
    // より正確な計算が必要な場合は、うるう年を考慮する必要がある
    tinfo->julian_day = (tinfo->year - 2000) * 365 + (tinfo->month - 1) * 30 + tinfo->day;

    // 週番号の計算（年初からの週数、簡易版）
    tinfo->week_of_year = ((tinfo->month - 1) * 4) + ((tinfo->day - 1) / 7) + 1;
}

/**
 * 2つの日付間の日数差を計算
 * @param t1 開始日の時間情報
 * @param t2 終了日の時間情報
 * @return 日数差の絶対値
 */
int calculate_days_between(struct time_info *t1, struct time_info *t2)
{
    return abs(t2->julian_day - t1->julian_day);
}

/**
 * 時間統計構造体を初期化（ゼロクリア）
 * @param stats 初期化する時間統計構造体へのポインタ
 */
void initialize_temporal_statistics(struct temporal_statistics *stats)
{
    int i;

    // 月別統計の初期化
    for (i = 0; i < MONTHS_IN_YEAR; i++)
    {
        stats->monthly_mean[i] = 0.0;
        stats->monthly_variance[i] = 0.0;
        stats->monthly_count[i] = 0;
    }

    // 四半期別統計の初期化
    for (i = 0; i < QUARTERS_IN_YEAR; i++)
    {
        stats->quarterly_mean[i] = 0.0;
        stats->quarterly_variance[i] = 0.0;
        stats->quarterly_count[i] = 0;
    }

    // 曜日別統計の初期化
    for (i = 0; i < DAYS_IN_WEEK; i++)
    {
        stats->weekly_mean[i] = 0.0;
        stats->weekly_variance[i] = 0.0;
        stats->weekly_count[i] = 0;
    }

    // 自己相関の初期化
    for (i = 0; i < MAX_LAG; i++)
    {
        stats->autocorrelation[i] = 0.0;
    }

    // その他の統計値の初期化
    stats->seasonality_strength = 0.0;
    stats->trend_coefficient = 0.0;
}

/* ================================================================================
   メモリ管理関数

   動的メモリの割り当てと解放を管理します。
   Phase 2ではactual_X用の配列も追加で割り当てます。
   ================================================================================
*/

/**
 * プログラムで使用する全ての動的メモリを割り当てる
 * CSVから読み込んだNrdとNzkの値に基づいてサイズを決定
 */
void allocate_dynamic_memory()
{
    int i, j, k;

    /* データバッファの割り当て */
    // 2次元配列：[レコード数][属性数]の属性データ
    data_buffer = (int **)malloc(Nrd * sizeof(int *));
    for (i = 0; i < Nrd; i++)
    {
        data_buffer[i] = (int *)malloc(Nzk * sizeof(int));
    }

    // 1次元配列：各レコードのX値（予測対象）
    x_buffer = (double *)malloc(Nrd * sizeof(double));

    // 2次元配列：各レコードのタイムスタンプ文字列（タイムゾーン付き対応）
    timestamp_buffer = (char **)malloc(Nrd * sizeof(char *));
    for (i = 0; i < Nrd; i++)
    {
        timestamp_buffer[i] = (char *)malloc(20 * sizeof(char)); // タイムゾーン付き文字列に対応
    }

    // 時間情報配列：各レコードの解析済み時間情報
    time_info_array = (struct time_info *)malloc(Nrd * sizeof(struct time_info));

    /* 属性辞書の割り当て */
    // CSVヘッダーから読み込んだ属性名を格納
    attribute_dictionary = (char **)malloc((Nzk + 3) * sizeof(char *));
    for (i = 0; i < Nzk + 3; i++)
    {
        attribute_dictionary[i] = (char *)malloc(MAX_ATTR_NAME * sizeof(char));
    }

    /* GNPノード構造の割り当て */
    // 3次元配列：[個体数][ノード数][3(属性ID,接続先,時間遅延)]
    node_structure = (int ***)malloc(Nkotai * sizeof(int **));
    for (i = 0; i < Nkotai; i++)
    {
        node_structure[i] = (int **)malloc((Npn + Njg) * sizeof(int *));
        for (j = 0; j < (Npn + Njg); j++)
        {
            node_structure[i][j] = (int *)malloc(3 * sizeof(int));
        }
    }

    /* 評価統計配列の割り当て */
    // 各種カウント配列：[個体][処理ノード][深さ]
    match_count = (int ***)malloc(Nkotai * sizeof(int **));
    negative_count = (int ***)malloc(Nkotai * sizeof(int **));
    evaluation_count = (int ***)malloc(Nkotai * sizeof(int **));
    attribute_chain = (int ***)malloc(Nkotai * sizeof(int **));
    time_delay_chain = (int ***)malloc(Nkotai * sizeof(int **));

    // 予測値の統計配列
    x_sum = (double ***)malloc(Nkotai * sizeof(double **));
    x_sigma_array = (double ***)malloc(Nkotai * sizeof(double **));

    // Phase 2.2で追加：T（時間）の統計配列
    t_sum_julian = (double ***)malloc(Nkotai * sizeof(double **));
    t_sigma_julian_array = (double ***)malloc(Nkotai * sizeof(double **));

    /* 時間パターン追跡配列 */
    matched_time_indices = (int ****)malloc(Nkotai * sizeof(int ***));
    matched_time_count = (int ***)malloc(Nkotai * sizeof(int **));

    // 各個体・処理ノード・深さごとに配列を割り当て
    for (i = 0; i < Nkotai; i++)
    {
        match_count[i] = (int **)malloc(Npn * sizeof(int *));
        negative_count[i] = (int **)malloc(Npn * sizeof(int *));
        evaluation_count[i] = (int **)malloc(Npn * sizeof(int *));
        attribute_chain[i] = (int **)malloc(Npn * sizeof(int *));
        time_delay_chain[i] = (int **)malloc(Npn * sizeof(int *));
        x_sum[i] = (double **)malloc(Npn * sizeof(double *));
        x_sigma_array[i] = (double **)malloc(Npn * sizeof(double *));
        t_sum_julian[i] = (double **)malloc(Npn * sizeof(double *));
        t_sigma_julian_array[i] = (double **)malloc(Npn * sizeof(double *));
        matched_time_indices[i] = (int ***)malloc(Npn * sizeof(int **));
        matched_time_count[i] = (int **)malloc(Npn * sizeof(int *));

        for (j = 0; j < Npn; j++)
        {
            match_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            negative_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            evaluation_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            attribute_chain[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            time_delay_chain[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            x_sum[i][j] = (double *)malloc(MAX_DEPTH * sizeof(double));
            x_sigma_array[i][j] = (double *)malloc(MAX_DEPTH * sizeof(double));
            t_sum_julian[i][j] = (double *)malloc(MAX_DEPTH * sizeof(double));
            t_sigma_julian_array[i][j] = (double *)malloc(MAX_DEPTH * sizeof(double));
            matched_time_count[i][j] = (int *)calloc(MAX_DEPTH, sizeof(int));
            matched_time_indices[i][j] = (int **)malloc(MAX_DEPTH * sizeof(int *));

            // マッチしたレコードのインデックスを記録する配列
            for (k = 0; k < MAX_DEPTH; k++)
            {
                matched_time_indices[i][j][k] = (int *)malloc(Nrd * sizeof(int));
            }
        }
    }

    /* 属性使用統計の割り当て */
    // 適応的学習のための履歴管理
    attribute_usage_history = (int **)malloc(HISTORY_GENERATIONS * sizeof(int *));
    for (i = 0; i < HISTORY_GENERATIONS; i++)
    {
        attribute_usage_history[i] = (int *)calloc(Nzk, sizeof(int));
    }
    attribute_usage_count = (int *)calloc(Nzk, sizeof(int));
    attribute_tracking = (int *)calloc(Nzk, sizeof(int));

    /* 遺伝子プールの割り当て */
    // 次世代の遺伝情報を保持
    gene_connection = (int **)malloc(Nkotai * sizeof(int *));
    gene_attribute = (int **)malloc(Nkotai * sizeof(int *));
    gene_time_delay = (int **)malloc(Nkotai * sizeof(int *));

    for (i = 0; i < Nkotai; i++)
    {
        gene_connection[i] = (int *)malloc((Npn + Njg) * sizeof(int));
        gene_attribute[i] = (int *)malloc((Npn + Njg) * sizeof(int));
        gene_time_delay[i] = (int *)malloc((Npn + Njg) * sizeof(int));
    }

    // 属性セット（作業用）
    attribute_set = (int *)malloc((Nzk + 1) * sizeof(int));

    /* ルールプールの初期化 */
    // 各ルールのマッチインデックス配列を事前に割り当て
    for (i = 0; i < Nrulemax; i++)
    {
        rule_pool[i].matched_indices = (int *)malloc(Nrd * sizeof(int));
    }

    /* グローバルルールプール（全試行統合用）の割り当て */
    // 最大で全試行のルール数を保持できるサイズ（重複除去で実際にはこれより少なくなる）
    global_rule_pool = (struct temporal_rule *)malloc(Nrulemax * Ntry * sizeof(struct temporal_rule));
    global_compare_rules = (struct cmrule *)malloc(Nrulemax * Ntry * sizeof(struct cmrule));

    // 各グローバルルールのマッチインデックス配列を事前に割り当て
    for (i = 0; i < Nrulemax * Ntry; i++)
    {
        global_rule_pool[i].matched_indices = (int *)malloc(Nrd * sizeof(int));
    }
}

/**
 * プログラムで使用した全ての動的メモリを解放
 * メモリリークを防ぐため、確実に全メモリを解放
 */
void free_dynamic_memory()
{
    int i, j, k;

    /* データバッファ解放 */
    if (data_buffer != NULL)
    {
        for (i = 0; i < Nrd; i++)
        {
            free(data_buffer[i]);
        }
        free(data_buffer);
    }

    free(x_buffer);

    if (timestamp_buffer != NULL)
    {
        for (i = 0; i < Nrd; i++)
        {
            free(timestamp_buffer[i]);
        }
        free(timestamp_buffer);
    }

    free(time_info_array);

    /* 属性辞書解放 */
    if (attribute_dictionary != NULL)
    {
        for (i = 0; i < Nzk + 3; i++)
        {
            free(attribute_dictionary[i]);
        }
        free(attribute_dictionary);
    }

    /* GNPノード構造解放 */
    if (node_structure != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < (Npn + Njg); j++)
            {
                free(node_structure[i][j]);
            }
            free(node_structure[i]);
        }
        free(node_structure);
    }

    /* 評価統計配列解放 */
    if (match_count != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                free(match_count[i][j]);
                free(negative_count[i][j]);
                free(evaluation_count[i][j]);
                free(attribute_chain[i][j]);
                free(time_delay_chain[i][j]);
                free(x_sum[i][j]);
                free(x_sigma_array[i][j]);
                free(t_sum_julian[i][j]);
                free(t_sigma_julian_array[i][j]);
                free(matched_time_count[i][j]);

                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(matched_time_indices[i][j][k]);
                }
                free(matched_time_indices[i][j]);
            }
            free(match_count[i]);
            free(negative_count[i]);
            free(evaluation_count[i]);
            free(attribute_chain[i]);
            free(time_delay_chain[i]);
            free(x_sum[i]);
            free(x_sigma_array[i]);
            free(t_sum_julian[i]);
            free(t_sigma_julian_array[i]);
            free(matched_time_indices[i]);
            free(matched_time_count[i]);
        }
        free(match_count);
        free(negative_count);
        free(evaluation_count);
        free(attribute_chain);
        free(time_delay_chain);
        free(x_sum);
        free(x_sigma_array);
        free(t_sum_julian);
        free(t_sigma_julian_array);
        free(matched_time_indices);
        free(matched_time_count);
    }

    /* 属性使用統計解放 */
    if (attribute_usage_history != NULL)
    {
        for (i = 0; i < HISTORY_GENERATIONS; i++)
        {
            free(attribute_usage_history[i]);
        }
        free(attribute_usage_history);
    }
    free(attribute_usage_count);
    free(attribute_tracking);

    /* 遺伝子プール解放 */
    if (gene_connection != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            free(gene_connection[i]);
            free(gene_attribute[i]);
            free(gene_time_delay[i]);
        }
        free(gene_connection);
        free(gene_attribute);
        free(gene_time_delay);
    }

    free(attribute_set);

    /* ルールプールのmatched_indices解放 */
    for (i = 0; i < Nrulemax; i++)
    {
        free(rule_pool[i].matched_indices);
    }

    /* グローバルルールプール解放 */
    if (global_rule_pool != NULL)
    {
        for (i = 0; i < Nrulemax * Ntry; i++)
        {
            free(global_rule_pool[i].matched_indices);
        }
        free(global_rule_pool);
    }
    if (global_compare_rules != NULL)
    {
        free(global_compare_rules);
    }
}

/* ================================================================================
   CSVヘッダー読み込み関数

   CSVファイルの構造を解析し、データを読み込みます。
   ヘッダー行から属性名を取得し、X列とT列を特定します。
   ================================================================================
*/

/**
 * CSVファイルをヘッダー付きで読み込む
 * 最初の行をヘッダーとして解析し、属性名を辞書に格納
 * X列（予測対象）とT列（タイムスタンプ）の位置を特定
 */
int load_csv_with_header()
{
    FILE *file;
    char line[MAX_LINE_LENGTH];
    char *token;
    int row = 0;
    int col = 0;
    int attr_count = 0;
    int i, j; /* ループ変数（C89スタイル） */

    printf("Loading CSV file with header: %s\n", data_file_path);

    // ファイルオープン
    file = fopen(data_file_path, "r");
    if (file == NULL)
    {
        printf("Warning: Cannot open data file: %s (skipping this forex pair)\n", data_file_path);
        return -1; // エラーを返す（exit しない）
    }

    /* ステップ1: 行数とカラム数を数える */
    int line_count = 0;
    int column_count = 0;

    while (fgets(line, MAX_LINE_LENGTH, file))
    {
        if (line_count == 0)
        {
            // 最初の行（ヘッダー）からカラム数を数える
            char temp_line[MAX_LINE_LENGTH];
            strcpy(temp_line, line);
            token = strtok(temp_line, ",\n");
            while (token != NULL)
            {
                column_count++;
                token = strtok(NULL, ",\n");
            }
        }
        line_count++;
    }

    // データ構造のサイズを設定
    Nrd = line_count - 1;   // ヘッダー行を除いたレコード数
    Nzk = column_count - 2; // X列とT列を除いた属性数

    printf("Detected %d records, %d attributes\n", Nrd, Nzk);

    // 動的メモリ割り当て
    allocate_dynamic_memory();

    /* ステップ2: ファイルを再読み込みしてヘッダーを解析 */
    rewind(file);

    // ヘッダー行を読み込み
    if (fgets(line, MAX_LINE_LENGTH, file))
    {
        col = 0;
        token = strtok(line, ",\n");

        while (token != NULL)
        {
            // トークンの前後の空白を除去
            while (*token == ' ')
                token++;
            char *end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\r' || *end == '\n'))
            {
                *end = '\0';
                end--;
            }

            // X列の検出（予測対象）
            if (strcmp(token, "X") == 0)
            {
                x_column_index = col;
                printf("Found X column at index %d\n", col);
            }
            // T列の検出（タイムスタンプ）
            else if (strcmp(token, "T") == 0 || strcmp(token, "timestamp") == 0)
            {
                t_column_index = col;
                printf("Found T column at index %d\n", col);
            }
            // その他の属性名を辞書に格納
            else
            {
                strcpy(attribute_dictionary[attr_count], token);
                attr_count++;
            }

            token = strtok(NULL, ",\n");
            col++;
        }
    }

    printf("Loaded %d attribute names\n", attr_count);

    /* ステップ3: データ行を読み込み */
    row = 0;
    while (fgets(line, MAX_LINE_LENGTH, file) && row < Nrd)
    {
        col = 0;
        int attr_idx = 0;
        token = strtok(line, ",\n");

        while (token != NULL)
        {
            // トークンの前後の空白を除去
            while (*token == ' ')
                token++;
            char *end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\r' || *end == '\n'))
            {
                *end = '\0';
                end--;
            }

            // カラムタイプに応じて適切なバッファに格納
            if (col == x_column_index)
            {
                // X値を読み込み
                x_buffer[row] = atof(token);
            }
            else if (col == t_column_index)
            {
                // タイムスタンプを読み込み
                strcpy(timestamp_buffer[row], token);
                // 時間情報を解析して構造体に格納
                parse_timestamp(token, &time_info_array[row]);
            }
            else
            {
                // 通常の属性データを読み込み（0/1の二値）
                data_buffer[row][attr_idx] = atoi(token);
                attr_idx++;
            }

            token = strtok(NULL, ",\n");
            col++;
        }
        row++;
    }

    fclose(file);

    // 【極値データフィルタリング】方向性を持つ極値データのみを残す
    int original_Nrd = Nrd;
    int filtered_count = 0;
    int positive_count = 0;  /* X >= 1.0 のカウント */
    int negative_count = 0;  /* X <= -1.0 のカウント */

    printf("\n========== Filtering Directional Extreme Data ==========\n");
    printf("Original records: %d\n", original_Nrd);

    if (extreme_direction == 1) {
        printf("Filter mode: POSITIVE ONLY (X >= 1.0)\n");
    } else if (extreme_direction == -1) {
        printf("Filter mode: NEGATIVE ONLY (X <= -1.0)\n");
    } else {
        printf("Filter mode: BOTH (will auto-select dominant direction)\n");
    }

    // フィルタリング後のデータを一時的に格納する配列
    double *filtered_x = (double *)malloc(Nrd * sizeof(double));
    int **filtered_data = (int **)malloc(Nrd * sizeof(int *));
    char **filtered_timestamps = (char **)malloc(Nrd * sizeof(char *));
    struct time_info *filtered_time_info = (struct time_info *)malloc(Nrd * sizeof(struct time_info));

    /* まず、正と負の極値をカウント */
    for (i = 0; i < Nrd; i++)
    {
        if (x_buffer[i] >= 1.0) positive_count++;
        if (x_buffer[i] <= -1.0) negative_count++;
    }

    /* extreme_direction == 0 の場合、多い方を自動選択 */
    if (extreme_direction == 0) {
        if (positive_count > negative_count) {
            extreme_direction = 1;
            printf("Auto-selected: POSITIVE (count=%d > %d)\n", positive_count, negative_count);
        } else {
            extreme_direction = -1;
            printf("Auto-selected: NEGATIVE (count=%d >= %d)\n", negative_count, positive_count);
        }
    }

    for (i = 0; i < Nrd; i++)
    {
        int keep_record = 0;

        /* 方向性に応じてフィルタリング */
        if (extreme_direction == 1 && x_buffer[i] >= 1.0) {
            keep_record = 1;  /* 正の極値のみ */
        } else if (extreme_direction == -1 && x_buffer[i] <= -1.0) {
            keep_record = 1;  /* 負の極値のみ */
        }

        if (keep_record)
        {
            filtered_x[filtered_count] = x_buffer[i];

            // 属性データをコピー
            filtered_data[filtered_count] = (int *)malloc(Nzk * sizeof(int));
            for (j = 0; j < Nzk; j++)
            {
                filtered_data[filtered_count][j] = data_buffer[i][j];
            }

            // タイムスタンプをコピー
            filtered_timestamps[filtered_count] = (char *)malloc(MAX_LINE_LENGTH * sizeof(char));
            strcpy(filtered_timestamps[filtered_count], timestamp_buffer[i]);

            // 時間情報をコピー
            filtered_time_info[filtered_count] = time_info_array[i];

            filtered_count++;
        }
    }

    printf("Positive extreme count (X >= 1.0): %d\n", positive_count);
    printf("Negative extreme count (X <= -1.0): %d\n", negative_count);
    printf("Filtered records (%s): %d (%.2f%% of original)\n",
           extreme_direction == 1 ? "POSITIVE" : "NEGATIVE",
           filtered_count, 100.0 * filtered_count / original_Nrd);
    printf("Removed records: %d\n", original_Nrd - filtered_count);

    // 元のデータを解放
    for (i = 0; i < Nrd; i++)
    {
        free(data_buffer[i]);
        free(timestamp_buffer[i]);
    }
    free(data_buffer);
    free(x_buffer);
    free(timestamp_buffer);
    free(time_info_array);

    // フィルタリング後のデータを元のバッファに再割り当て
    Nrd = filtered_count;
    x_buffer = filtered_x;
    data_buffer = filtered_data;
    timestamp_buffer = filtered_timestamps;
    time_info_array = filtered_time_info;

    printf("========================================================\n\n");

    // 読み込み結果のサマリを表示
    printf("Data loaded successfully (DIRECTIONAL EXTREME VALUES):\n");
    printf("  Records: %d (%s extreme)\n", Nrd,
           extreme_direction == 1 ? "POSITIVE" : "NEGATIVE");
    printf("  Direction: %s\n",
           extreme_direction == 1 ? "UP (X >= 1.0)" : "DOWN (X <= -1.0)");
    printf("  Attributes: %d\n", Nzk);
    printf("  X range: %.2f to %.2f\n",
           x_buffer[0], x_buffer[Nrd - 1]);
    printf("  Time range: %s to %s\n",
           timestamp_buffer[0], timestamp_buffer[Nrd - 1]);
    printf("  Minimum attributes per rule: %d\n", MIN_ATTRIBUTES);

    return 0; // 成功
}

/* ================================================================================
   初期化・設定系関数

   出力ディレクトリの作成、ルールプールの初期化、各種統計の初期化を行います。
   ================================================================================
*/

/**
 * 出力ディレクトリ構造を作成
 * プログラムの出力を整理するためのフォルダ階層を構築
 */
void create_output_directories()
{
    // ディレクトリを作成（Unixパーミッション755）
    // まず親ディレクトリ "output/" を作成
    mkdir("output", 0755);
    mkdir(output_base_dir, 0755); // メインディレクトリ
    mkdir(output_dir_ia, 0755);   // 分析レポート
    mkdir(output_dir_ib, 0755);   // バックアップ
    mkdir(output_dir_pool, 0755); // ルールプール
    mkdir(output_dir_doc, 0755);  // ドキュメント

    // ディレクトリ構造を表示
    printf("=== Directory Structure Created ===\n");
    printf("Forex Pair: %s\n", forex_pair);
    printf("%s/\n", output_base_dir);
    printf("├── IA/        (Analysis Reports)\n");
    printf("├── IB/        (Backup Files)\n");
    printf("├── pool/      (Global Rule Pool)\n");
    printf("└── doc/       (Documentation)\n");
    printf("===================================\n");

    // 時系列モードの設定を表示
    if (TIMESERIES_MODE)
    {
        printf("*** TIME-SERIES MODE ENABLED (Phase 2 - actual_X version) ***\n");
        printf("Single variable X with temporal analysis\n");
        printf("Adaptive delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE2);
        printf("Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
        printf("Prediction span: t+%d\n", PREDICTION_SPAN);
        printf("Minimum attributes: %d\n", MIN_ATTRIBUTES);
        printf("=========================================\n\n");
    }
}

/**
 * ルールプールを初期化
 * 全ルールの内容をゼロクリアし、初期状態にする
 */
void initialize_rule_pool()
{
    int i, j;

    for (i = 0; i < Nrulemax; i++)
    {
        // 属性配列の初期化
        for (j = 0; j < MAX_ATTRIBUTES; j++)
        {
            rule_pool[i].antecedent_attrs[j] = 0;
            rule_pool[i].time_delays[j] = 0;
            compare_rules[i].antecedent_attrs[j] = 0;
        }

        // 統計値の初期化
        rule_pool[i].x_mean = 0;
        rule_pool[i].x_sigma = 0;
        rule_pool[i].support_count = 0;
        rule_pool[i].negative_count = 0;
        rule_pool[i].high_support_flag = 0;
        rule_pool[i].low_variance_flag = 0;
        rule_pool[i].num_attributes = 0;

        // 時間統計の初期化
        initialize_temporal_statistics(&rule_pool[i].time_stats);
        rule_pool[i].dominant_month = 0;
        rule_pool[i].dominant_quarter = 0;
        rule_pool[i].dominant_day_of_week = 0;
        rule_pool[i].time_span_days = 0;
        strcpy(rule_pool[i].start_date, "");
        strcpy(rule_pool[i].end_date, "");
        rule_pool[i].matched_count_vis = 0;

        // 比較用ルールの初期化
        compare_rules[i].num_attributes = 0;
        new_rule_marker[i] = 0;
    }

    // 最小属性数を満たすルールのカウンタ初期化
    rules_by_min_attributes = 0;
}

/**
 * 時間遅延統計を初期化
 * 適応的学習のための遅延使用履歴を初期化
 */
void initialize_delay_statistics()
{
    int i, j;

    for (i = 0; i <= MAX_TIME_DELAY_PHASE2; i++)
    {
        delay_usage_count[i] = 0;
        delay_tracking[i] = INITIAL_DELAY_HISTORY; // 初期値1を設定（ゼロ除算回避）
        delay_success_rate[i] = 0.5;               // 初期成功率50%

        // 履歴を初期化
        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_usage_history[j][i] = INITIAL_DELAY_HISTORY;
        }
    }
}

/**
 * 属性使用統計を初期化
 * 適応的学習のための属性使用履歴を初期化
 */
void initialize_attribute_statistics()
{
    int i, j;

    // 属性使用カウントの初期化
    for (i = 0; i < Nzk; i++)
    {
        attribute_usage_count[i] = 0;
        attribute_tracking[i] = 0;

        // 最初の世代のみ初期値を設定
        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            attribute_usage_history[j][i] = (j == 0) ? INITIAL_DELAY_HISTORY : 0;
        }
    }

    // 属性数別ルール数の初期化
    for (i = 0; i < MAX_DEPTH; i++)
    {
        rules_by_attribute_count[i] = 0;
    }

    // 属性セットの初期化（0からNzkまでの連番）
    for (i = 0; i < Nzk + 1; i++)
    {
        attribute_set[i] = i;
    }
}

/**
 * グローバルカウンタを初期化
 * 全試行を通じた累積統計をリセット
 */
void initialize_global_counters()
{
    total_rule_count = 0;
    total_high_support = 0;
    total_low_variance = 0;
}

/* ================================================================================
   データアクセス関数

   時系列データへの安全なアクセスを提供します。
   時間遅延を考慮した範囲チェックを行います。
   ================================================================================
*/

/**
 * 時系列データの安全な範囲を取得
 * 時間遅延と予測スパンを考慮して、アクセス可能な範囲を計算
 * @param start_index 開始インデックスを格納する変数へのポインタ
 * @param end_index 終了インデックスを格納する変数へのポインタ
 */
void get_safe_data_range(int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        // 時系列モード：過去参照と未来予測の両方を考慮
        *start_index = MAX_TIME_DELAY_PHASE2; // t-3以降から開始
        *end_index = Nrd - PREDICTION_SPAN;   // t+1が存在する範囲まで
    }
    else
    {
        // 通常モード：全データを使用
        *start_index = 0;
        *end_index = Nrd;
    }
}

/**
 * 過去の属性値を取得
 * 指定した時点から指定した遅延分過去の属性値を返す
 * @param current_time 現在の時点インデックス
 * @param time_delay 時間遅延（0=現在, 1=t-1, 2=t-2, ...）
 * @param attribute_id 属性ID（0-indexed）
 * @return 属性値（0/1）、範囲外の場合は-1
 */
int get_past_attribute_value(int current_time, int time_delay, int attribute_id)
{
    // 過去のインデックスを計算
    int data_index = current_time - time_delay;

    // 範囲チェック
    if (data_index < 0 || data_index >= Nrd)
    {
        return -1; // 範囲外
    }

    return data_buffer[data_index][attribute_id];
}

/**
 * 将来の目標値を取得
 * 予測対象となる将来時点のX値を返す
 * @param current_time 現在の時点インデックス
 * @return t+PREDICTION_SPAN時点のX値
 */
double get_future_target_value(int current_time)
{
    // 将来のインデックスを計算
    int future_index = current_time + PREDICTION_SPAN;

    // 範囲チェック
    if (future_index >= Nrd)
    {
        return 0.0; // 範囲外
    }

    return x_buffer[future_index];
}

/**
 * 現在の実際の値を取得（Phase 2で追加）
 * ルール適用時点での実際のX値を返す
 * @param current_time 現在の時点インデックス
 * @return 現在時点のX値
 */
double get_current_actual_value(int current_time)
{
    // 範囲チェック
    if (current_time < 0 || current_time >= Nrd)
    {
        return 0.0; // 範囲外
    }

    return x_buffer[current_time];
}

/* ================================================================================
   GNP個体管理系関数

   遺伝的ネットワークプログラミングの個体を管理します。
   個体の初期化、遺伝子のコピー、統計の初期化を行います。
   ================================================================================
*/

/**
 * 初期個体群を生成
 * ランダムに遺伝子を生成して初期集団を作成
 */
void create_initial_population()
{
    int i, j;

    for (i = 0; i < Nkotai; i++)
    {
        for (j = 0; j < (Npn + Njg); j++)
        {
            // ランダムに接続先を設定（判定ノードのみ）
            gene_connection[i][j] = rand() % Njg + Npn;

            // ランダムに属性を設定
            gene_attribute[i][j] = rand() % Nzk;

            // ランダムに時間遅延を設定（0～MAX_TIME_DELAY_PHASE2）
            gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE2 + 1);
        }
    }
}

/**
 * 遺伝子情報をノード構造にコピー
 * 進化操作後の遺伝子をGNPの実行形式に変換
 */
void copy_genes_to_nodes()
{
    int individual, node;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (node = 0; node < (Npn + Njg); node++)
        {
            // [0]: 属性ID
            node_structure[individual][node][0] = gene_attribute[individual][node];
            // [1]: 接続先ノードID
            node_structure[individual][node][1] = gene_connection[individual][node];
            // [2]: 時間遅延
            node_structure[individual][node][2] = gene_time_delay[individual][node];
        }
    }
}

/**
 * 個体統計を初期化
 * 各個体の評価統計をゼロクリアし、適応度を初期化
 */
void initialize_individual_statistics()
{
    int individual, k, i, d;

    for (individual = 0; individual < Nkotai; individual++)
    {
        // 適応度を初期化（わずかに異なる値にして同順位を避ける）
        fitness_value[individual] = (double)individual * FITNESS_INIT_OFFSET;
        fitness_ranking[individual] = 0;

        // 各処理ノードと深さの統計を初期化
        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < MAX_DEPTH; i++)
            {
                // カウンタのクリア
                match_count[individual][k][i] = 0;
                attribute_chain[individual][k][i] = 0;
                time_delay_chain[individual][k][i] = 0;
                negative_count[individual][k][i] = 0;
                evaluation_count[individual][k][i] = 0;

                // 統計値のクリア
                x_sum[individual][k][i] = 0;
                x_sigma_array[individual][k][i] = 0;
                t_sum_julian[individual][k][i] = 0;
                t_sigma_julian_array[individual][k][i] = 0;
                matched_time_count[individual][k][i] = 0;

                // マッチした時点のインデックスをクリア
                for (d = 0; d < Nrd; d++)
                {
                    matched_time_indices[individual][k][i][d] = -1;
                }
            }
        }
    }
}

/* ================================================================================
   ルール評価系関数

   GNP個体がデータを走査し、ルール候補を評価します。
   Phase 2では実際のX値（actual_X）も同時に追跡します。
   ================================================================================
*/

/**
 * 単一のデータインスタンスを評価
 * 全個体が指定時点のデータに対してルールマッチングを実行
 * @param time_index 評価するデータの時点インデックス
 */
void evaluate_single_instance(int time_index)
{
    double future_x; // 予測対象（t+1の値）
    int current_node_id, depth, match_flag;
    int time_delay, data_index;
    int individual, k;

    // 予測対象を取得
    future_x = get_future_target_value(time_index);

    // 全個体に対して評価を実行
    for (individual = 0; individual < Nkotai; individual++)
    {
        // 各処理ノード（開始点）から探索
        for (k = 0; k < Npn; k++)
        {
            current_node_id = k; // 処理ノードから開始
            depth = 0;           // 深さ0から開始
            match_flag = 1;      // マッチフラグ（1=マッチ中）

            // 深さ0の統計を更新（無条件でカウント）
            match_count[individual][k][depth]++;
            evaluation_count[individual][k][depth]++;

            // 最初の判定ノードへ移動
            current_node_id = node_structure[individual][current_node_id][1];

            // 判定ノードを辿りながらルールを構築
            while (current_node_id > (Npn - 1) && depth < Nmx)
            {
                depth++; // 深さを増やす

                // 現在のノードの属性を記録（1-indexed）
                attribute_chain[individual][k][depth] =
                    node_structure[individual][current_node_id][0] + 1;

                // 現在のノードの時間遅延を取得
                time_delay = node_structure[individual][current_node_id][2];
                time_delay_chain[individual][k][depth] = time_delay;

                // 過去のデータインデックスを計算
                data_index = time_index - time_delay;

                // 範囲チェック
                if (data_index < 0)
                {
                    current_node_id = k; // 処理ノードに戻る
                    break;
                }

                // 属性値を取得
                int attr_value = data_buffer[data_index][node_structure[individual][current_node_id][0]];

                // 属性値に応じて分岐
                if (attr_value == 1)
                {
                    // 属性値が1：継続側へ進む
                    if (match_flag == 1)
                    {
                        // マッチ継続中の場合、統計を更新
                        match_count[individual][k][depth]++;

                        // 予測値の累積
                        x_sum[individual][k][depth] += future_x;
                        x_sigma_array[individual][k][depth] += future_x * future_x;

                        // T（時間）の累積 - Phase 2.2新機能
                        double current_julian = (double)time_info_array[time_index].julian_day;
                        t_sum_julian[individual][k][depth] += current_julian;
                        t_sigma_julian_array[individual][k][depth] += current_julian * current_julian;

                        // マッチした時点を記録
                        if (matched_time_count[individual][k][depth] < Nrd)
                        {
                            matched_time_indices[individual][k][depth][matched_time_count[individual][k][depth]] = time_index;
                            matched_time_count[individual][k][depth]++;
                        }
                    }
                    evaluation_count[individual][k][depth]++;
                    // 次の判定ノードへ
                    current_node_id = node_structure[individual][current_node_id][1];
                }
                else if (attr_value == 0)
                {
                    // 属性値が0：スキップ側へ進む（処理ノードに戻る）
                    current_node_id = k;
                }
                else
                {
                    // 属性値が不正（-1など）：マッチ失敗
                    evaluation_count[individual][k][depth]++;
                    match_flag = 0; // マッチフラグをオフ
                    // 次の判定ノードへ（統計は更新しない）
                    current_node_id = node_structure[individual][current_node_id][1];
                }
            }
        }
    }
}

/**
 * 全個体を全データに対して評価
 * 安全な範囲内の全時点でルール評価を実行
 */
void evaluate_all_individuals()
{
    int safe_start, safe_end;
    int i;

    // 時系列を考慮した安全な範囲を取得
    get_safe_data_range(&safe_start, &safe_end);

    // 全時点で評価を実行
    for (i = safe_start; i < safe_end; i++)
    {
        evaluate_single_instance(i);
    }
}

/**
 * ネガティブカウントを計算
 * サポート値計算用の分母を求める
 */
void calculate_negative_counts()
{
    int individual, k, i;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < Nmx; i++)
            {
                // ネガティブカウント = 全体 - 評価外 + マッチ
                // これはサポート計算の分母として使用される
                negative_count[individual][k][i] =
                    match_count[individual][k][0] - evaluation_count[individual][k][i] +
                    match_count[individual][k][i];
            }
        }
    }
}

/**
 * ルール統計を計算
 * 累積値から平均と標準偏差を計算
 */
void calculate_rule_statistics()
{
    int individual, k, j;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (j = 1; j < 9; j++)
            {
                if (match_count[individual][k][j] != 0)
                {
                    // 予測値の平均を計算
                    x_sum[individual][k][j] /= (double)match_count[individual][k][j];

                    // 予測値の標準偏差を計算（分散の平方根）
                    x_sigma_array[individual][k][j] =
                        x_sigma_array[individual][k][j] / (double)match_count[individual][k][j] -
                        x_sum[individual][k][j] * x_sum[individual][k][j];

                    // 負の分散を防ぐ（数値誤差対策）
                    if (x_sigma_array[individual][k][j] < 0)
                    {
                        x_sigma_array[individual][k][j] = 0;
                    }

                    x_sigma_array[individual][k][j] = sqrt(x_sigma_array[individual][k][j]);

                    // T（時間）の平均を計算 - Phase 2.2新機能
                    t_sum_julian[individual][k][j] /= (double)match_count[individual][k][j];

                    // T（時間）の標準偏差を計算
                    t_sigma_julian_array[individual][k][j] =
                        t_sigma_julian_array[individual][k][j] / (double)match_count[individual][k][j] -
                        t_sum_julian[individual][k][j] * t_sum_julian[individual][k][j];

                    // 負の分散を防ぐ
                    if (t_sigma_julian_array[individual][k][j] < 0)
                    {
                        t_sigma_julian_array[individual][k][j] = 0;
                    }

                    t_sigma_julian_array[individual][k][j] = sqrt(t_sigma_julian_array[individual][k][j]);
                }
            }
        }
    }
}

/**
 * サポート値を計算
 * @param matched_count マッチした回数
 * @param negative_count_val ネガティブカウント（分母）
 * @return サポート値（0.0～1.0）
 */
double calculate_support_value(int matched_count, int negative_count_val)
{
    if (negative_count_val == 0)
    {
        return 0.0; // ゼロ除算防止
    }
    return (double)matched_count / (double)negative_count_val;
}

/* ================================================================================
   時間パターン分析関数

   ルールがマッチした時点の時間的特徴を分析します。
   月別、四半期別、曜日別の統計を計算します。
   ================================================================================
*/

/**
 * ルールの時間パターンを分析
 * マッチした時点の時間的特徴を統計的に分析
 * @param rule 分析対象のルール
 * @param individual 個体ID
 * @param k 処理ノードID
 * @param depth 深さ
 */
void analyze_temporal_patterns(struct temporal_rule *rule, int individual, int k, int depth)
{
    int i, idx;
    int count = matched_time_count[individual][k][depth];

    // マッチがない場合は処理をスキップ
    if (count == 0)
        return;

    // 時間統計の初期化
    initialize_temporal_statistics(&rule->time_stats);

    /* 第1パス：各時点での値を累積 */
    for (i = 0; i < count; i++)
    {
        idx = matched_time_indices[individual][k][depth][i];
        if (idx < 0 || idx >= Nrd)
            continue;

        struct time_info *tinfo = &time_info_array[idx];
        double x_val = x_buffer[idx + PREDICTION_SPAN];

        // 月別統計の累積
        rule->time_stats.monthly_mean[tinfo->month - 1] += x_val;
        rule->time_stats.monthly_count[tinfo->month - 1]++;

        // 四半期別統計の累積
        rule->time_stats.quarterly_mean[tinfo->quarter - 1] += x_val;
        rule->time_stats.quarterly_count[tinfo->quarter - 1]++;

        // 曜日別統計の累積
        rule->time_stats.weekly_mean[tinfo->day_of_week - 1] += x_val;
        rule->time_stats.weekly_count[tinfo->day_of_week - 1]++;
    }

    /* 平均値の計算 */
    // 月別平均
    for (i = 0; i < MONTHS_IN_YEAR; i++)
    {
        if (rule->time_stats.monthly_count[i] > 0)
        {
            rule->time_stats.monthly_mean[i] /= rule->time_stats.monthly_count[i];
        }
    }

    // 四半期別平均
    for (i = 0; i < QUARTERS_IN_YEAR; i++)
    {
        if (rule->time_stats.quarterly_count[i] > 0)
        {
            rule->time_stats.quarterly_mean[i] /= rule->time_stats.quarterly_count[i];
        }
    }

    // 曜日別平均
    for (i = 0; i < DAYS_IN_WEEK; i++)
    {
        if (rule->time_stats.weekly_count[i] > 0)
        {
            rule->time_stats.weekly_mean[i] /= rule->time_stats.weekly_count[i];
        }
    }

    /* 第2パス：分散の計算 */
    for (i = 0; i < count; i++)
    {
        idx = matched_time_indices[individual][k][depth][i];
        if (idx < 0 || idx >= Nrd)
            continue;

        struct time_info *tinfo = &time_info_array[idx];
        double x_val = x_buffer[idx + PREDICTION_SPAN];

        // 月別分散
        double diff = x_val - rule->time_stats.monthly_mean[tinfo->month - 1];
        rule->time_stats.monthly_variance[tinfo->month - 1] += diff * diff;

        // 四半期別分散
        diff = x_val - rule->time_stats.quarterly_mean[tinfo->quarter - 1];
        rule->time_stats.quarterly_variance[tinfo->quarter - 1] += diff * diff;

        // 曜日別分散
        diff = x_val - rule->time_stats.weekly_mean[tinfo->day_of_week - 1];
        rule->time_stats.weekly_variance[tinfo->day_of_week - 1] += diff * diff;
    }

    /* 分散の正規化（不偏分散） */
    // 月別分散
    for (i = 0; i < MONTHS_IN_YEAR; i++)
    {
        if (rule->time_stats.monthly_count[i] > 1)
        {
            rule->time_stats.monthly_variance[i] /= (rule->time_stats.monthly_count[i] - 1);
        }
    }

    // 四半期別分散
    for (i = 0; i < QUARTERS_IN_YEAR; i++)
    {
        if (rule->time_stats.quarterly_count[i] > 1)
        {
            rule->time_stats.quarterly_variance[i] /= (rule->time_stats.quarterly_count[i] - 1);
        }
    }

    // 曜日別分散
    for (i = 0; i < DAYS_IN_WEEK; i++)
    {
        if (rule->time_stats.weekly_count[i] > 1)
        {
            rule->time_stats.weekly_variance[i] /= (rule->time_stats.weekly_count[i] - 1);
        }
    }

    /* 支配的な時間パターンの特定 */
    // 最も頻度の高い月を特定
    int max_month_count = 0;
    for (i = 0; i < MONTHS_IN_YEAR; i++)
    {
        if (rule->time_stats.monthly_count[i] > max_month_count)
        {
            max_month_count = rule->time_stats.monthly_count[i];
            rule->dominant_month = i + 1;
        }
    }

    // 最も頻度の高い四半期を特定
    int max_quarter_count = 0;
    for (i = 0; i < QUARTERS_IN_YEAR; i++)
    {
        if (rule->time_stats.quarterly_count[i] > max_quarter_count)
        {
            max_quarter_count = rule->time_stats.quarterly_count[i];
            rule->dominant_quarter = i + 1;
        }
    }

    // 最も頻度の高い曜日を特定
    int max_week_count = 0;
    for (i = 0; i < DAYS_IN_WEEK; i++)
    {
        if (rule->time_stats.weekly_count[i] > max_week_count)
        {
            max_week_count = rule->time_stats.weekly_count[i];
            rule->dominant_day_of_week = i + 1;
        }
    }

    /* 時間範囲の設定 */
    if (count > 0)
    {
        // 最初と最後のマッチ時点
        int first_idx = matched_time_indices[individual][k][depth][0];
        int last_idx = matched_time_indices[individual][k][depth][count - 1];

        // 開始日と終了日を記録
        strcpy(rule->start_date, timestamp_buffer[first_idx]);
        strcpy(rule->end_date, timestamp_buffer[last_idx]);

        // 期間（日数）を計算
        rule->time_span_days = calculate_days_between(&time_info_array[first_idx],
                                                      &time_info_array[last_idx]);
    }

    /* マッチしたインデックスを保存（可視化用） */
    rule->matched_count_vis = count;
    for (i = 0; i < count && i < Nrd; i++)
    {
        rule->matched_indices[i] = matched_time_indices[individual][k][depth][i];
    }

    /* T（時間）統計を記録 - Phase 2.2新機能 */
    rule->t_mean_julian = t_sum_julian[individual][k][depth];
    rule->t_sigma_julian = t_sigma_julian_array[individual][k][depth];
}

/* ================================================================================
   ルール抽出系関数

   評価結果からルールを抽出し、品質チェックを行います。
   Phase 2では実際のX値（actual_X）も含めてルールを登録します。
   ================================================================================
*/

/**
 * ルールの品質をチェック
 * @param sigma_x 標準偏差
 * @param support サポート値
 * @param num_attributes 属性数
 * @return 品質基準を満たす場合1、満たさない場合0
 */
int check_rule_quality(double sigma_x, double support, int num_attributes)
{
    return (sigma_x <= Maxsigx &&              // 分散が閾値以下
            support >= Minsup &&               // サポート値が閾値以上
            num_attributes >= MIN_ATTRIBUTES); // 最小属性数以上
}

/**
 * ルールの重複をチェック
 * @param rule_candidate チェック対象のルール（属性配列）
 * @param rule_count 現在のルール数
 * @return 重複している場合1、重複していない場合0
 */
int check_rule_duplication(int *rule_candidate, int rule_count)
{
    int i, j;

    for (i = 0; i < rule_count; i++)
    {
        int is_duplicate = 1;

        // 全属性が一致するかチェック
        for (j = 0; j < MAX_ATTRIBUTES; j++)
        {
            if (rule_pool[i].antecedent_attrs[j] != rule_candidate[j])
            {
                is_duplicate = 0;
                break;
            }
        }

        if (is_duplicate)
        {
            return 1; // 重複あり
        }
    }
    return 0; // 重複なし
}

/**
 * 新規ルールを登録
 * ルールプールに新しいルールを追加し、各種統計を更新
 * @param state 試行状態
 * @param rule_candidate ルールの属性配列
 * @param time_delays 時間遅延配列
 * @param x_mean 予測値の平均
 * @param x_sigma 予測値の標準偏差
 * @param support_count サポートカウント
 * @param negative_count_val ネガティブカウント
 * @param support_value サポート値
 * @param num_attributes 属性数
 * @param individual 個体ID
 * @param k 処理ノードID
 * @param depth 深さ
 */
void register_new_rule(struct trial_state *state, int *rule_candidate, int *time_delays,
                       double x_mean, double x_sigma,
                       int support_count, int negative_count_val, double support_value,
                       int num_attributes,
                       int individual, int k, int depth)
{
    int idx = state->rule_count;
    int i;

    // ルールの属性と時間遅延をコピー
    for (i = 0; i < MAX_ATTRIBUTES; i++)
    {
        rule_pool[idx].antecedent_attrs[i] = rule_candidate[i];
        rule_pool[idx].time_delays[i] = (rule_candidate[i] > 0) ? time_delays[i] : 0;
    }

    // ルールの統計値を設定
    rule_pool[idx].x_mean = x_mean;
    rule_pool[idx].x_sigma = x_sigma;
    rule_pool[idx].support_count = support_count;
    rule_pool[idx].negative_count = negative_count_val;
    rule_pool[idx].support_rate = support_value; // サポート率を保存
    rule_pool[idx].num_attributes = num_attributes;

    // 時間パターンを分析
    analyze_temporal_patterns(&rule_pool[idx], individual, k, depth);

    // 高サポートフラグの設定
    int high_support_marker = 0;
    if (support_value >= (Minsup + HIGH_SUPPORT_BONUS))
    {
        high_support_marker = 1;
    }
    rule_pool[idx].high_support_flag = high_support_marker;

    if (rule_pool[idx].high_support_flag == 1)
    {
        state->high_support_rule_count++;
    }

    // 低分散フラグの設定
    int low_variance_marker = 0;
    if (x_sigma <= (Maxsigx - LOW_VARIANCE_REDUCTION))
    {
        low_variance_marker = 1;
    }
    rule_pool[idx].low_variance_flag = low_variance_marker;

    if (rule_pool[idx].low_variance_flag == 1)
    {
        state->low_variance_rule_count++;
    }

    // ルールカウントを増やす
    state->rule_count++;

    // 最小属性数を満たすルールのカウント
    if (num_attributes >= MIN_ATTRIBUTES)
    {
        rules_by_min_attributes++;
    }
}

/**
 * ルールから遅延学習を更新
 * 良いルールで使われた遅延を強化学習
 * @param time_delays 時間遅延配列
 * @param num_attributes 属性数
 * @param high_support 高サポートフラグ
 * @param low_variance 低分散フラグ
 */
void update_delay_learning_from_rule(int *time_delays, int num_attributes,
                                     int high_support, int low_variance)
{
    int k;

    for (k = 0; k < num_attributes; k++)
    {
        if (time_delays[k] >= 0 && time_delays[k] <= MAX_TIME_DELAY_PHASE2)
        {
            // 基本的な使用回数を増やす
            delay_usage_history[0][time_delays[k]] += 1;

            // 良質なルールの場合はボーナスを追加
            if (high_support || low_variance)
            {
                delay_usage_history[0][time_delays[k]] += REFRESH_BONUS;
            }
        }
    }
}

/**
 * 個体からルールを抽出
 * 1個体の全処理ノードからルールを抽出
 * @param state 試行状態
 * @param individual 個体ID
 */
void extract_rules_from_individual(struct trial_state *state, int individual)
{
    int rule_candidate_pre[MAX_DEPTH];   // 評価中の属性列（順序付き）
    int rule_candidate[MAX_ATTRIBUTES];  // 最終的な属性配列（ソート済み）
    int rule_memo[MAX_ATTRIBUTES];       // 属性のメモ
    int time_delay_candidate[MAX_DEPTH]; // 時間遅延候補
    int time_delay_memo[MAX_ATTRIBUTES]; // 時間遅延のメモ
    int k, loop_j, i2, j2, k2, k3, i5;

    // 各処理ノードから探索
    for (k = 0; k < Npn; k++)
    {
        // ルール数上限チェック
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }

        // 配列の初期化
        memset(rule_candidate_pre, 0, sizeof(rule_candidate_pre));
        memset(rule_candidate, 0, sizeof(rule_candidate));
        memset(rule_memo, 0, sizeof(rule_memo));
        memset(time_delay_candidate, 0, sizeof(time_delay_candidate));
        memset(time_delay_memo, 0, sizeof(time_delay_memo));

        // 最小属性数から最大深さまでループ
        for (loop_j = MIN_ATTRIBUTES; loop_j < 9; loop_j++)
        {
            // 現在の深さの統計を取得
            double sigma_x = x_sigma_array[individual][k][loop_j];
            int matched_count = match_count[individual][k][loop_j];
            double support = calculate_support_value(matched_count,
                                                     negative_count[individual][k][loop_j]);

            // 属性チェーンをコピー
            for (i2 = 1; i2 < 9; i2++)
            {
                rule_candidate_pre[i2 - 1] = attribute_chain[individual][k][i2];
                time_delay_candidate[i2 - 1] = time_delay_chain[individual][k][i2];
            }

            // 属性を昇順にソート（正規形にする）
            j2 = 0;
            for (k2 = 1; k2 < (1 + Nzk); k2++)
            {
                int found = 0;
                for (i2 = 0; i2 < loop_j; i2++)
                {
                    if (rule_candidate_pre[i2] == k2)
                    {
                        found = 1;
                        time_delay_memo[j2] = time_delay_candidate[i2];
                    }
                }
                if (found == 1)
                {
                    rule_candidate[j2] = k2;
                    rule_memo[j2] = k2;
                    j2++;
                }
            }

            // ルールの品質チェック
            if (check_rule_quality(sigma_x, support, j2))
            {
                if (j2 < 9 && j2 >= MIN_ATTRIBUTES)
                {
                    // 重複チェック
                    if (!check_rule_duplication(rule_candidate, state->rule_count))
                    {
                        // 新規ルールとして登録
                        register_new_rule(state, rule_candidate, time_delay_memo,
                                          x_sum[individual][k][loop_j], sigma_x,
                                          matched_count, negative_count[individual][k][loop_j],
                                          support, j2,
                                          individual, k, loop_j);

                        // 属性数別カウントを更新
                        rules_by_attribute_count[j2]++;

                        // 適応度を更新（新規ルールボーナス付き）
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (sigma_x + FITNESS_SIGMA_OFFSET) +
                            FITNESS_NEW_RULE_BONUS;

                        // 属性使用履歴を更新
                        for (k3 = 0; k3 < j2; k3++)
                        {
                            i5 = rule_memo[k3] - 1;
                            attribute_usage_history[0][i5]++;
                        }

                        // 遅延学習を更新
                        update_delay_learning_from_rule(time_delay_memo, j2,
                                                        rule_pool[state->rule_count - 1].high_support_flag,
                                                        rule_pool[state->rule_count - 1].low_variance_flag);
                    }
                    else
                    {
                        // 重複ルールの場合（新規ボーナスなし）
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (sigma_x + FITNESS_SIGMA_OFFSET);
                    }

                    // ルール数上限チェック
                    if (state->rule_count > (Nrulemax - 2))
                    {
                        break;
                    }
                }
            }
        }
    }
}

/**
 * 全個体からルールを抽出
 * 集団全体を走査してルールを収集
 * @param state 試行状態
 */
void extract_rules_from_population(struct trial_state *state)
{
    int individual;

    for (individual = 0; individual < Nkotai; individual++)
    {
        // ルール数上限チェック
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }
        extract_rules_from_individual(state, individual);
    }
}

/* ================================================================================
   進化計算系関数

   選択、交叉、突然変異などの遺伝的操作を実行します。
   適応的な突然変異により、良い遅延や属性を優先的に選択します。
   ================================================================================
*/

/**
 * 適応度ランキングを計算
 * 各個体の順位を決定（0が最良）
 */
void calculate_fitness_rankings()
{
    int i, j;

    for (i = 0; i < Nkotai; i++)
    {
        fitness_ranking[i] = 0;

        // 自分より適応度が高い個体数をカウント
        for (j = 0; j < Nkotai; j++)
        {
            if (fitness_value[j] > fitness_value[i])
            {
                fitness_ranking[i]++;
            }
        }
    }
}

/**
 * エリート選択を実行
 * 上位1/3の個体を3つずつコピーして次世代を構成
 */
void perform_selection()
{
    int i, j, copy, target_idx;

    for (i = 0; i < Nkotai; i++)
    {
        // エリート個体（上位1/3）の場合
        if (fitness_ranking[i] < ELITE_SIZE)
        {
            // 全ノードの遺伝子をコピー
            for (j = 0; j < (Npn + Njg); j++)
            {
                // 3つのコピーを作成
                for (copy = 0; copy < ELITE_COPIES; copy++)
                {
                    target_idx = fitness_ranking[i] + copy * ELITE_SIZE;
                    gene_attribute[target_idx][j] = node_structure[i][j][0];
                    gene_connection[target_idx][j] = node_structure[i][j][1];
                    gene_time_delay[target_idx][j] = node_structure[i][j][2];
                }
            }
        }
    }
}

/**
 * 交叉操作を実行
 * ペア間で遺伝子を交換
 */
void perform_crossover()
{
    int i, j, crossover_point, temp;

    // 上位ペアで交叉
    for (i = 0; i < CROSSOVER_PAIRS; i++)
    {
        for (j = 0; j < Nkousa; j++)
        {
            // ランダムに交叉点を選択（判定ノードのみ）
            crossover_point = rand() % Njg + Npn;

            // 接続遺伝子を交換
            temp = gene_connection[i + CROSSOVER_PAIRS][crossover_point];
            gene_connection[i + CROSSOVER_PAIRS][crossover_point] = gene_connection[i][crossover_point];
            gene_connection[i][crossover_point] = temp;

            // 属性遺伝子を交換
            temp = gene_attribute[i + CROSSOVER_PAIRS][crossover_point];
            gene_attribute[i + CROSSOVER_PAIRS][crossover_point] = gene_attribute[i][crossover_point];
            gene_attribute[i][crossover_point] = temp;

            // 時間遅延遺伝子を交換
            temp = gene_time_delay[i + CROSSOVER_PAIRS][crossover_point];
            gene_time_delay[i + CROSSOVER_PAIRS][crossover_point] = gene_time_delay[i][crossover_point];
            gene_time_delay[i][crossover_point] = temp;
        }
    }
}

/**
 * 処理ノードの突然変異
 * 処理ノードの接続先をランダムに変更
 */
void perform_mutation_processing_nodes()
{
    int i, j;

    // 全個体の処理ノードに対して
    for (i = 0; i < Nkotai; i++)
    {
        for (j = 0; j < Npn; j++)
        {
            // 確率的に突然変異
            if (rand() % Muratep == 0)
            {
                // 新しい接続先をランダムに選択
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

/**
 * 判定ノードの突然変異
 * 判定ノードの接続先をランダムに変更
 */
void perform_mutation_judgment_nodes()
{
    int i, j;

    // 個体40-79に対して
    for (i = MUTATION_START_40; i < MUTATION_START_80; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            // 確率的に突然変異
            if (rand() % Muratej == 0)
            {
                // 新しい接続先をランダムに選択
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

/**
 * ルーレット選択による遅延選択
 * 使用頻度に比例した確率で遅延を選択
 * @param total_usage 総使用回数
 * @return 選択された遅延値
 */
int roulette_wheel_selection_delay(int total_usage)
{
    int random_value, accumulated, i;

    // 使用履歴がない場合はランダム
    if (total_usage <= 0)
    {
        return rand() % (MAX_TIME_DELAY_PHASE2 + 1);
    }

    // ルーレット選択
    random_value = rand() % total_usage;
    accumulated = 0;

    for (i = 0; i <= MAX_TIME_DELAY_PHASE2; i++)
    {
        accumulated += delay_tracking[i];
        if (random_value < accumulated)
        {
            return i;
        }
    }

    return MAX_TIME_DELAY_PHASE2;
}

/**
 * ルーレット選択による属性選択
 * 使用頻度に比例した確率で属性を選択
 * @param total_usage 総使用回数
 * @return 選択された属性ID
 */
int roulette_wheel_selection_attribute(int total_usage)
{
    int random_value, accumulated, i;

    // 使用履歴がない場合はランダム
    if (total_usage <= 0)
    {
        return rand() % Nzk;
    }

    // ルーレット選択
    random_value = rand() % total_usage;
    accumulated = 0;

    for (i = 0; i < Nzk; i++)
    {
        accumulated += attribute_tracking[i];
        if (random_value < accumulated)
        {
            return i;
        }
    }

    return Nzk - 1;
}

/**
 * 適応的遅延突然変異
 * 良い遅延を優先的に選択する突然変異
 */
void perform_adaptive_delay_mutation()
{
    int total_delay_usage = 0;
    int i, j;

    // 総使用回数を計算
    for (i = 0; i <= MAX_TIME_DELAY_PHASE2; i++)
    {
        total_delay_usage += delay_tracking[i];
    }

    // 個体40-79に対して
    for (i = MUTATION_START_40; i < MUTATION_START_80; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            {
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    // ルーレット選択で良い遅延を優先
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    // ランダム選択
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE2 + 1);
                }
            }
        }
    }

    // 個体80-119に対して（より積極的な突然変異）
    for (i = MUTATION_START_80; i < Nkotai; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            {
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    // ルーレット選択で良い遅延を優先
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    // ランダム選択
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE2 + 1);
                }
            }
        }
    }
}

/**
 * 適応的属性突然変異
 * 良い属性を優先的に選択する突然変異
 */
void perform_adaptive_attribute_mutation()
{
    int total_attribute_usage = 0;
    int i, j;

    // 総使用回数を計算
    for (i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    // 個体80-119に対して
    for (i = MUTATION_START_80; i < Nkotai; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratea == 0)
            {
                // ルーレット選択で良い属性を優先
                gene_attribute[i][j] = roulette_wheel_selection_attribute(total_attribute_usage);
            }
        }
    }
}

/* ================================================================================
   統計・履歴管理系関数

   適応的学習のための統計情報を管理します。
   過去の成功パターンを記憶し、次世代に活用します。
   ================================================================================
*/

/**
 * 遅延統計を更新
 * 世代間の遅延使用履歴を管理
 * @param generation 現在の世代番号
 */
void update_delay_statistics(int generation)
{
    int total_delay_usage = 0;
    int i, j;

    // 過去世代の累積を計算
    for (i = 0; i <= MAX_TIME_DELAY_PHASE2; i++)
    {
        delay_tracking[i] = 0;

        // 過去HISTORY_GENERATIONS世代分を累積
        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_tracking[i] += delay_usage_history[j][i];
        }

        total_delay_usage += delay_tracking[i];
    }

    // 履歴をシフト（古い世代を削除）
    for (i = 0; i <= MAX_TIME_DELAY_PHASE2; i++)
    {
        // 1世代分シフト
        for (j = HISTORY_GENERATIONS - 1; j > 0; j--)
        {
            delay_usage_history[j][i] = delay_usage_history[j - 1][i];
        }

        // 最新世代を初期化
        delay_usage_history[0][i] = INITIAL_DELAY_HISTORY;

        // 定期的にリフレッシュ
        if (generation % REPORT_INTERVAL == 0)
        {
            delay_usage_history[0][i] = REFRESH_BONUS;
        }
    }
}

/**
 * 属性統計を更新
 * 世代間の属性使用履歴を管理
 * @param generation 現在の世代番号
 */
void update_attribute_statistics(int generation)
{
    int total_attribute_usage = 0;
    int i, j;

    // カウンタをリセット
    memset(attribute_usage_count, 0, sizeof(int) * Nzk);
    memset(attribute_tracking, 0, sizeof(int) * Nzk);

    // 過去世代の累積を計算
    for (i = 0; i < Nzk; i++)
    {
        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            attribute_tracking[i] += attribute_usage_history[j][i];
        }
    }

    // 履歴をシフト（古い世代を削除）
    for (i = 0; i < Nzk; i++)
    {
        // 1世代分シフト
        for (j = HISTORY_GENERATIONS - 1; j > 0; j--)
        {
            attribute_usage_history[j][i] = attribute_usage_history[j - 1][i];
        }

        // 最新世代を初期化
        attribute_usage_history[0][i] = 0;

        // 定期的にリフレッシュ
        if (generation % REPORT_INTERVAL == 0)
        {
            attribute_usage_history[0][i] = INITIAL_DELAY_HISTORY;
        }
    }

    // 総使用回数を計算
    for (i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    // 現世代の属性使用をカウント
    for (i = 0; i < Nkotai; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            attribute_usage_count[gene_attribute[i][j]]++;
        }
    }
}

/* ================================================================================
   可視化用データ出力関数

   ルールの時系列データを可視化用に出力します。
   ================================================================================
*/

/**
 * ルールの時系列データをCSVファイルに出力
 * @param rule_idx ルールのインデックス
 * @param trial_id 試行ID
 */
void export_rule_timeseries(int rule_idx, int trial_id)
{
    FILE *file;
    char filename[FILENAME_BUFFER_SIZE];
    int i;

    // ファイル名を生成
    sprintf(filename, "%s/rule_%04d_%04d.csv", output_dir_vis, trial_id, rule_idx);

    file = fopen(filename, "w");
    if (file == NULL)
        return;

    // CSVヘッダーを出力
    fprintf(file, "Timestamp,X,X_mean,X_sigma,Matched,Month,Quarter,DayOfWeek\n");

    // 全データポイントを出力
    for (i = 0; i < Nrd; i++)
    {
        int matched = 0;
        int j;

        // このインデックスがルールにマッチしているかチェック
        for (j = 0; j < rule_pool[rule_idx].matched_count_vis; j++)
        {
            if (rule_pool[rule_idx].matched_indices[j] == i)
            {
                matched = 1;
                break;
            }
        }

        // データ行を出力
        fprintf(file, "%s,%.3f,%.3f,%.3f,%d,%d,%d,%d\n",
                timestamp_buffer[i],
                x_buffer[i],
                rule_pool[rule_idx].x_mean,
                rule_pool[rule_idx].x_sigma,
                matched,
                time_info_array[i].month,
                time_info_array[i].quarter,
                time_info_array[i].day_of_week);
    }

    fclose(file);
}

/* ================================================================================
   ファイル入出力系関数

   ルール、統計情報、レポートをファイルに出力します。
   Phase 2ではactual_X列も含めて出力します。
   ================================================================================
*/

/**
 * 試行用ファイルを作成
 * 各試行の出力ファイルを初期化
 * @param state 試行状態
 */
void create_trial_files(struct trial_state *state)
{
    FILE *file;

    // ルールリストファイルの作成
    file = fopen(state->filename_rule, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\n");
        fclose(file);
    }

    // レポートファイルの作成
    file = fopen(state->filename_report, "w");
    if (file != NULL)
    {
        fprintf(file, "Generation\tRules\tHighSup\tLowVar\tAvgFitness\tMin%dAttr\n", MIN_ATTRIBUTES);
        fclose(file);
    }

    // ローカル詳細ファイルの作成
    file = fopen(state->filename_local, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file, "X_mean\tX_sigma\tsupport_count\tsupport_rate\tHighSup\tLowVar\tNumAttr\t");
        fprintf(file, "DomMonth\tDomQuarter\tDomDay\tTimeSpan\n");
        fclose(file);
    }
}

/**
 * ルールをファイルに書き込み
 * @param state 試行状態
 * @param rule_index ルールのインデックス
 */
void write_rule_to_file(struct trial_state *state, int rule_index)
{
    FILE *file = fopen(state->filename_rule, "a");
    int i;

    if (file == NULL)
        return;

    // 各属性と時間遅延を出力
    for (i = 0; i < MAX_ATTRIBUTES; i++)
    {
        int attr = rule_pool[rule_index].antecedent_attrs[i];
        int delay = rule_pool[rule_index].time_delays[i];

        if (attr > 0)
        {
            // 属性名(t-遅延)の形式で出力
            fprintf(file, "%s(t-%d)", attribute_dictionary[attr - 1], delay);
        }
        else
        {
            fprintf(file, "0");
        }

        if (i < MAX_ATTRIBUTES - 1)
            fprintf(file, "\t");
    }

    fprintf(file, "\n");
    fclose(file);
}

/**
 * 進捗レポートを出力
 * @param state 試行状態
 * @param generation 現在の世代
 */
void write_progress_report(struct trial_state *state, int generation)
{
    FILE *file = fopen(state->filename_report, "a");
    double fitness_average = 0;
    int i;

    if (file == NULL)
        return;

    // 平均適応度を計算
    for (i = 0; i < Nkotai; i++)
    {
        fitness_average += fitness_value[i];
    }
    fitness_average /= Nkotai;

    // レポート行を出力
    fprintf(file, "%5d\t%5d\t%5d\t%5d\t%9.3f\t%5d\n",
            generation,
            state->rule_count - 1,
            state->high_support_rule_count - 1,
            state->low_variance_rule_count - 1,
            fitness_average,
            rules_by_min_attributes);

    fclose(file);

    // コンソールにも出力
    printf("  Gen.=%5d: %5d rules (%5d high-sup, %5d low-var, %5d min-%d-attr)\n",
           generation, state->rule_count - 1,
           state->high_support_rule_count - 1,
           state->low_variance_rule_count - 1,
           rules_by_min_attributes,
           MIN_ATTRIBUTES);

    // 定期的に遅延使用状況を表示
    if (generation % DELAY_DISPLAY_INTERVAL == 0 && ADAPTIVE_DELAY)
    {
        printf("    Delay usage: ");
        for (i = 0; i <= MAX_TIME_DELAY_PHASE2; i++)
        {
            printf("t-%d:%d ", i, delay_tracking[i]);
        }
        printf("\n");
    }
}

/**
 * ローカル詳細ファイルに出力
 * actual_X統計を含む詳細情報を出力
 * @param state 試行状態
 */
void write_local_output(struct trial_state *state)
{
    FILE *file = fopen(state->filename_local, "a");
    int i, j;

    if (file == NULL)
        return;

    // 全ルールを出力
    for (i = 1; i < state->rule_count; i++)
    {
        // 属性と遅延
        for (j = 0; j < MAX_ATTRIBUTES; j++)
        {
            int attr = rule_pool[i].antecedent_attrs[j];
            int delay = rule_pool[i].time_delays[j];

            if (attr > 0)
            {
                fprintf(file, "%s(t-%d)\t", attribute_dictionary[attr - 1], delay);
            }
            else
            {
                fprintf(file, "0\t");
            }
        }

        // 予測値の統計
        fprintf(file, "%8.3f\t%5.3f\t%d\t%6.4f\t%2d\t%2d\t",
                rule_pool[i].x_mean,
                rule_pool[i].x_sigma,
                rule_pool[i].support_count,
                rule_pool[i].support_rate,
                rule_pool[i].high_support_flag,
                rule_pool[i].low_variance_flag);

        // 属性数と時間パターン
        fprintf(file, "%d\t",
                rule_pool[i].num_attributes);

        // 時間パターン
        fprintf(file, "%2d\t%d\t%d\t%d\n",
                rule_pool[i].dominant_month,
                rule_pool[i].dominant_quarter,
                rule_pool[i].dominant_day_of_week,
                rule_pool[i].time_span_days);
    }

    fclose(file);
}

/**
 * 現在の試行のルールをグローバルプールに統合
 * 重複するルールは除外し、新しいルールのみを追加
 * @param state 試行状態
 */
void merge_trial_rules_to_global_pool(struct trial_state *state)
{
    int i, j, k;
    int is_duplicate;

    // 現在の試行の各ルールについて
    for (i = 1; i < state->rule_count; i++)
    {
        is_duplicate = 0;

        // 既存のグローバルプール内のルールと比較
        for (j = 0; j < global_rule_count; j++)
        {
            int attr_match = 1;

            // 属性リストが一致するか確認
            for (k = 0; k < MAX_ATTRIBUTES; k++)
            {
                if (rule_pool[i].antecedent_attrs[k] != global_rule_pool[j].antecedent_attrs[k] ||
                    rule_pool[i].time_delays[k] != global_rule_pool[j].time_delays[k])
                {
                    attr_match = 0;
                    break;
                }
            }

            // 完全一致する場合は重複とみなす
            if (attr_match)
            {
                is_duplicate = 1;
                break;
            }
        }

        // 重複していない場合のみグローバルプールに追加
        if (!is_duplicate)
        {
            // matched_indicesポインタを一時保存（構造体コピーで上書きされないように）
            int *temp_indices = global_rule_pool[global_rule_count].matched_indices;

            // ルール全体をコピー
            global_rule_pool[global_rule_count] = rule_pool[i];

            // matched_indicesポインタを元に戻す
            global_rule_pool[global_rule_count].matched_indices = temp_indices;

            // matched_indicesの内容を個別にコピー
            for (k = 0; k < rule_pool[i].support_count; k++)
            {
                global_rule_pool[global_rule_count].matched_indices[k] = rule_pool[i].matched_indices[k];
            }

            // 統計カウンタを更新
            if (global_rule_pool[global_rule_count].high_support_flag)
                global_high_support_count++;
            if (global_rule_pool[global_rule_count].low_variance_flag)
                global_low_variance_count++;

            global_rule_count++;

            // バッファオーバーフロー防止
            if (global_rule_count >= Nrulemax * Ntry)
            {
                printf("警告: グローバルルールプールが上限に達しました\n");
                return;
            }
        }
    }
}

/**
 * 極値シグナルスコアを計算してCSVに出力
 * 各ルールについて5つのシグナルメトリクスを計算
 */
void calculate_and_write_extreme_scores()
{
    FILE *file;
    char extreme_scores_path[512];
    int i, j;
    double global_X_sum = 0.0;
    double global_X_sum_sq = 0.0;
    double global_X_mean, global_X_std;
    int valid_data_count = 0;

    // グローバルX統計を計算（全データの平均と標準偏差）
    for (i = 0; i < Nrd; i++)
    {
        global_X_sum += x_buffer[i];
        global_X_sum_sq += x_buffer[i] * x_buffer[i];
        valid_data_count++;
    }
    global_X_mean = global_X_sum / valid_data_count;
    global_X_std = sqrt((global_X_sum_sq / valid_data_count) - (global_X_mean * global_X_mean));

    // 出力ファイルパスを構築
    sprintf(extreme_scores_path, "%s/pool/extreme_scores.csv", output_base_dir);
    file = fopen(extreme_scores_path, "w");
    if (file == NULL)
    {
        printf("エラー: extreme_scores.csvを開けません: %s\n", extreme_scores_path);
        return;
    }

    // CSVヘッダーを出力（方向性情報追加）
    fprintf(file, "rule_idx,extreme_direction,signal_strength,SNR,extremeness,t_statistic,p_value,extreme_rate,extreme_signal_score,matched_count,tail_event_rate,directional_bias,non_zero_rate,positive_rate,very_extreme_rate,mean_abs_ratio,spread_ratio,p75_abs,p90_abs,p95_abs\n");

    // 各ルールについてシグナルスコアを計算
    for (i = 0; i < global_rule_count; i++)
    {
        if (i % 1000 == 0)
        {
            printf("  Processing rule %d/%d...\n", i, global_rule_count);
        }

        struct temporal_rule *rule = &global_rule_pool[i];
        int matched_count = rule->support_count;

        // マッチしたX値を配列にコピー（ソート用）
        double *X_values = (double *)malloc(matched_count * sizeof(double));

        // マッチしたX値の統計を計算
        double X_sum = 0.0;
        double X_sum_sq = 0.0;
        int tail_count = 0;       // |X| > 2σの個数
        int extreme_count = 0;    // |X| > 1.0の個数（実際の極値変化率）
        int very_extreme_count = 0; // |X| > 2.0の個数（超極値）
        int positive_count = 0;   // X > 0.5の個数（プラス方向）
        int negative_count = 0;   // X < -0.5の個数（マイナス方向）
        int near_zero_count = 0;  // |X| < 0.3の個数（ゼロ付近）

        double abs_X_sum = 0.0;   // |X|の合計（平均絶対値用）

        for (j = 0; j < matched_count; j++)
        {
            int idx = rule->matched_indices[j];
            if (idx < 0 || idx >= Nrd)
                continue;

            double X_val = x_buffer[idx];
            X_values[j] = X_val;  // 配列にコピー

            X_sum += X_val;
            X_sum_sq += X_val * X_val;
            abs_X_sum += fabs(X_val);

            // Tail event: |X| > 2 * global_std
            if (fabs(X_val) > 2.0 * global_X_std)
            {
                tail_count++;
            }

            // Extreme event: |X| > 1.0 (実際の極値変化率1%以上)
            if (fabs(X_val) > 1.0)
            {
                extreme_count++;
            }

            // Very extreme event: |X| > 2.0
            if (fabs(X_val) > 2.0)
            {
                very_extreme_count++;
            }

            // 方向性のカウント
            if (X_val > 0.5)
            {
                positive_count++;
            }
            else if (X_val < -0.5)
            {
                negative_count++;
            }

            // ゼロ付近のカウント
            if (fabs(X_val) < 0.3)
            {
                near_zero_count++;
            }
        }

        double X_mean_actual = X_sum / matched_count;
        double X_variance = (X_sum_sq / matched_count) - (X_mean_actual * X_mean_actual);
        double X_std_actual = (X_variance > 0) ? sqrt(X_variance) : 0.0;

        // X値の絶対値配列を作成してソート（パーセンタイル計算用）
        double *abs_X_values = (double *)malloc(matched_count * sizeof(double));
        for (j = 0; j < matched_count; j++)
        {
            abs_X_values[j] = fabs(X_values[j]);
        }

        // バブルソート（簡易実装）
        for (j = 0; j < matched_count - 1; j++)
        {
            for (int k = 0; k < matched_count - j - 1; k++)
            {
                if (abs_X_values[k] > abs_X_values[k + 1])
                {
                    double temp = abs_X_values[k];
                    abs_X_values[k] = abs_X_values[k + 1];
                    abs_X_values[k + 1] = temp;
                }
            }
        }

        // パーセンタイル計算
        int p50_idx = matched_count / 2;
        int p75_idx = (int)(matched_count * 0.75);
        int p90_idx = (int)(matched_count * 0.90);
        int p95_idx = (int)(matched_count * 0.95);

        double median_abs = abs_X_values[p50_idx];
        double p75_abs = abs_X_values[p75_idx];
        double p90_abs = abs_X_values[p90_idx];
        double p95_abs = abs_X_values[p95_idx];

        // 平均絶対値
        double mean_abs_X = abs_X_sum / matched_count;

        // メトリクス1: Signal Strength = |X_mean| (従来)
        double signal_strength = fabs(X_mean_actual);

        // メトリクス2: SNR = |X_mean| / X_std (従来)
        double SNR = (X_std_actual > 1e-6) ? (signal_strength / X_std_actual) : 0.0;

        // メトリクス3: Extremeness = 新指標（端への集中度）
        // 75パーセンタイルの絶対値 / グローバル標準偏差
        double extremeness = p75_abs / global_X_std;

        // メトリクス3b: 分布の広がり = std / global_std
        double spread_ratio = X_std_actual / global_X_std;

        // メトリクス3c: 平均絶対値 / global_std
        double mean_abs_ratio = mean_abs_X / global_X_std;

        free(abs_X_values);
        free(X_values);

        // メトリクス4: t検定
        double t_statistic = 0.0;
        double p_value = 1.0;
        if (matched_count > 1 && X_std_actual > 1e-6)
        {
            t_statistic = (X_mean_actual - global_X_mean) / (X_std_actual / sqrt(matched_count));

            // 簡易p値計算（両側検定の近似）
            // |t| > 3.0 → p < 0.01, |t| > 2.0 → p < 0.05
            double t_abs = fabs(t_statistic);
            if (t_abs > 3.0)
                p_value = 0.001;
            else if (t_abs > 2.58)
                p_value = 0.01;
            else if (t_abs > 1.96)
                p_value = 0.05;
            else if (t_abs > 1.64)
                p_value = 0.10;
            else
                p_value = 0.20;
        }

        // メトリクス5: Tail Event Rate
        double tail_event_rate = (double)tail_count / matched_count;

        // メトリクス6: Extreme Rate (|X| > 1.0の発生率)
        double extreme_rate = (double)extreme_count / matched_count;

        // メトリクス6b: Very Extreme Rate (|X| > 2.0の発生率)
        double very_extreme_rate = (double)very_extreme_count / matched_count;

        // メトリクス7: Directional Bias（方向性の偏り）
        double positive_rate = (double)positive_count / matched_count;
        double negative_rate = (double)negative_count / matched_count;
        double directional_bias = (positive_rate > negative_rate) ? positive_rate : negative_rate;

        // メトリクス8: Non-zero Rate（ゼロ以外の割合）
        double non_zero_rate = 1.0 - ((double)near_zero_count / matched_count);

        // 新しい総合スコア（端の分布を重視）
        // 1. 75パーセンタイルの絶対値（端への集中）
        // 2. 極値率（|X| > 1.0）
        // 3. 超極値率（|X| > 2.0）
        // 4. 平均絶対値（0からの距離）
        double extreme_signal_score = 0.35 * extremeness +        // 75パーセンタイル（端への集中）
                                      0.30 * extreme_rate +        // 極値発生率（|X| > 1.0）
                                      0.20 * very_extreme_rate +   // 超極値発生率（|X| > 2.0）
                                      0.15 * mean_abs_ratio;       // 平均絶対値

        // 構造体に保存（新指標を反映）
        rule->signal_strength = signal_strength;
        rule->SNR = SNR;
        rule->extremeness = extremeness;  // 新extremeness（p75_abs / global_std）
        rule->t_statistic = t_statistic;
        rule->p_value = p_value;
        rule->tail_event_rate = extreme_rate; // extreme_rateを保存
        rule->extreme_signal_score = extreme_signal_score;

        // CSVに1行出力（方向性情報を含む）
        fprintf(file, "%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                i,
                extreme_direction,  // 方向性: 1=POSITIVE, -1=NEGATIVE
                signal_strength,
                SNR,
                extremeness,          // 新extremeness (p75_abs / global_std)
                t_statistic,
                p_value,
                extreme_rate,         // 極値率 (|X| > 1.0)
                extreme_signal_score, // 新スコア
                matched_count,
                tail_event_rate,      // 参考値
                directional_bias,     // 方向性の偏り
                non_zero_rate,        // ゼロ以外の割合
                positive_rate,        // プラス方向の割合
                very_extreme_rate,    // 超極値率 (|X| > 2.0)
                mean_abs_ratio,       // 平均絶対値 / global_std
                spread_ratio,         // std / global_std
                p75_abs,              // 75パーセンタイル絶対値
                p90_abs,              // 90パーセンタイル絶対値
                p95_abs);             // 95パーセンタイル絶対値
    }

    fclose(file);
    printf("  Extreme scores written to: %s\n", extreme_scores_path);
    printf("  Total rules processed: %d\n", global_rule_count);
}

/**
 * グローバルルールプールを出力
 * 全試行を通じたルールプールを2つの形式で出力
 * @param state 試行状態（引数としては使用しないが互換性のため保持）
 */
void write_global_pool(struct trial_state *state)
{
    FILE *file_a = fopen(pool_file_a, "w");
    FILE *file_b = fopen(pool_file_b, "w");
    int i, j;

    /* ファイルA：詳細版（TSV形式） */
    if (file_a != NULL)
    {
        // ヘッダー行
        fprintf(file_a, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file_a, "X_mean\tX_sigma\tT_mean_julian\tT_sigma_julian\tsupport_count\tsupport_rate\tNegative\tHighSup\tLowVar\tNumAttr\t");
        fprintf(file_a, "Month\tQuarter\tDay\tStart\tEnd\t");
        fprintf(file_a, "SignalStrength\tSNR\tExtremeness\tt_stat\tp_value\tTailEventRate\tExtremeScore\n");

        // グローバルプールの全ルールを出力
        for (i = 0; i < global_rule_count; i++)
        {
            // 属性と遅延
            for (j = 0; j < MAX_ATTRIBUTES; j++)
            {
                int attr = global_rule_pool[i].antecedent_attrs[j];
                int delay = global_rule_pool[i].time_delays[j];

                if (attr > 0)
                {
                    fprintf(file_a, "%s(t-%d)\t", attribute_dictionary[attr - 1], delay);
                }
                else
                {
                    fprintf(file_a, "0\t");
                }
            }

            // 予測値とT（時間）の統計
            fprintf(file_a, "%8.3f\t%5.3f\t%8.2f\t%6.2f\t%d\t%6.4f\t%d\t%d\t%d\t",
                    global_rule_pool[i].x_mean, global_rule_pool[i].x_sigma,
                    global_rule_pool[i].t_mean_julian, global_rule_pool[i].t_sigma_julian,
                    global_rule_pool[i].support_count, global_rule_pool[i].support_rate,
                    global_rule_pool[i].negative_count,
                    global_rule_pool[i].high_support_flag, global_rule_pool[i].low_variance_flag);

            // 属性数と時間パターン
            fprintf(file_a, "%d\t",
                    global_rule_pool[i].num_attributes);

            // 時間パターン
            fprintf(file_a, "%d\t%d\t%d\t%s\t%s\t",
                    global_rule_pool[i].dominant_month,
                    global_rule_pool[i].dominant_quarter,
                    global_rule_pool[i].dominant_day_of_week,
                    global_rule_pool[i].start_date,
                    global_rule_pool[i].end_date);

            // 極値シグナルスコア
            fprintf(file_a, "%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\t%.6f\n",
                    global_rule_pool[i].signal_strength,
                    global_rule_pool[i].SNR,
                    global_rule_pool[i].extremeness,
                    global_rule_pool[i].t_statistic,
                    global_rule_pool[i].p_value,
                    global_rule_pool[i].tail_event_rate,
                    global_rule_pool[i].extreme_signal_score);
        }

        fclose(file_a);
    }

    /* ファイルB：要約版（読みやすい形式） */
    if (file_b != NULL)
    {
        fprintf(file_b, "# Global Rule Pool Summary (Integrated from All Trials)\n");
        fprintf(file_b, "# Total Rules: %d\n", global_rule_count);
        fprintf(file_b, "# High Support: %d\n", global_high_support_count);
        fprintf(file_b, "# Low Variance: %d\n", global_low_variance_count);
        fprintf(file_b, "\n");

        // 最初の10ルールを例示
        int display_limit = (global_rule_count < 10) ? global_rule_count : 10;
        for (i = 0; i < display_limit; i++)
        {
            fprintf(file_b, "Rule %d (%d attrs): ", i + 1, global_rule_pool[i].num_attributes);

            // 属性と遅延を出力
            for (j = 0; j < MAX_ATTRIBUTES; j++)
            {
                int attr = global_rule_pool[i].antecedent_attrs[j];
                int delay = global_rule_pool[i].time_delays[j];

                if (attr > 0)
                {
                    fprintf(file_b, "%s(t-%d) ", attribute_dictionary[attr - 1], delay);
                }
            }

            fprintf(file_b, "\n");
            fprintf(file_b, "   => Predicted_X: %.3f±%.3f\n",
                    global_rule_pool[i].x_mean, global_rule_pool[i].x_sigma);
            fprintf(file_b, "   Temporal: Month=%d, Quarter=Q%d, Day=%d, Span=%d days\n",
                    global_rule_pool[i].dominant_month,
                    global_rule_pool[i].dominant_quarter,
                    global_rule_pool[i].dominant_day_of_week,
                    global_rule_pool[i].time_span_days);
            fprintf(file_b, "   Extreme Signal: Strength=%.3f, SNR=%.3f, Score=%.3f (p=%.4f)\n",
                    global_rule_pool[i].signal_strength,
                    global_rule_pool[i].SNR,
                    global_rule_pool[i].extreme_signal_score,
                    global_rule_pool[i].p_value);
        }

        fclose(file_b);
    }
}

/**
 * ドキュメント統計を出力
 * 試行ごとの統計情報を記録
 * @param state 試行状態
 */
void write_document_stats(struct trial_state *state)
{
    FILE *file = fopen(cont_file, "a");

    if (file != NULL)
    {
        // 試行の統計情報を出力
        fprintf(file, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.2f\n",
                state->trial_id - Nstart + 1,       // 試行番号（1から）
                state->rule_count - 1,              // ルール数
                state->high_support_rule_count - 1, // 高サポートルール数
                state->low_variance_rule_count - 1, // 低分散ルール数
                total_rule_count,                   // 累積ルール数
                total_high_support,                 // 累積高サポート数
                total_low_variance,                 // 累積低分散数
                rules_by_min_attributes,            // 最小属性数を満たすルール数
                state->elapsed_time);               // 経過時間（秒）
        fclose(file);
    }
}

/* ================================================================================
   ユーティリティ系関数

   ファイル名生成、初期化、最終統計出力などの補助機能を提供します。
   ================================================================================
*/

/**
 * ファイル名を生成
 * 試行IDから5桁のファイル名を生成
 * @param filename 生成したファイル名を格納するバッファ
 * @param prefix ファイル名のプレフィックス（IL/IA/IB）
 * @param trial_id 試行ID
 */
void generate_filename(char *filename, const char *prefix, int trial_id)
{
    int digits[FILENAME_DIGITS];
    int temp = trial_id;
    int i;

    // 試行IDを5桁の数字に分解
    for (i = FILENAME_DIGITS - 1; i >= 0; i--)
    {
        digits[i] = temp % 10;
        temp /= 10;
    }

    // ディレクトリとファイル名を結合
    sprintf(filename, "%s/%s%d%d%d%d%d.txt",
            // プレフィックスに応じてディレクトリを選択
            (strcmp(prefix, "IL") == 0) ? output_dir_il : (strcmp(prefix, "IA") == 0) ? output_dir_ia
                                                                                      : output_dir_ib,
            prefix, digits[0], digits[1], digits[2], digits[3], digits[4]);
}

/**
 * ドキュメントファイルを初期化
 * 統計情報ファイルのヘッダーを作成
 */
void initialize_document_file()
{
    FILE *file = fopen(cont_file, "w");
    if (file != NULL)
    {
        // TSV形式のヘッダーを出力
        fprintf(file, "Trial\tRules\tHighSup\tLowVar\tTotal\tTotalHigh\tTotalLow\tMin%dAttr\tTime(sec)\n",
                MIN_ATTRIBUTES);
        fclose(file);
    }
}

/**
 * 最終統計を表示
 * プログラム終了時に全体の統計情報をコンソールに出力
 */
void print_final_statistics()
{
    int i;

    printf("\n========================================\n");
    printf("GNMiner Time-Series Edition Phase 2 Complete!\n");
    printf("(with actual_X tracking and Chi-square test)\n");
    printf("========================================\n");

    /* 時系列設定のサマリ */
    printf("Time-Series Configuration:\n");
    printf("  Mode: Phase 2 (Temporal Pattern Analysis + Actual X + Chi-square)\n");
    printf("  Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
    printf("  Delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE2);
    printf("  Prediction span: t+%d\n", PREDICTION_SPAN);
    printf("  Minimum attributes: %d\n", MIN_ATTRIBUTES);
    printf("========================================\n");

    /* データ統計 */
    printf("Data Statistics:\n");
    printf("  Records: %d\n", Nrd);
    printf("  Attributes: %d\n", Nzk);
    printf("  Time range: %s to %s\n",
           timestamp_buffer[0], timestamp_buffer[Nrd - 1]);
    printf("========================================\n");

    /* 属性数別ルール統計 */
    printf("Rule Statistics by Attribute Count:\n");
    for (i = MIN_ATTRIBUTES; i <= MAX_ATTRIBUTES && i < MAX_DEPTH; i++)
    {
        if (rules_by_attribute_count[i] > 0)
        {
            printf("  %d attributes: %d rules\n", i, rules_by_attribute_count[i]);
        }
    }
    printf("========================================\n");

    /* ディレクトリ構造のサマリ */
    printf("Output Directory Structure:\n");
    printf("output/\n");
    printf("├── IL/       %d rule list files\n", Ntry);
    printf("├── IA/       %d analysis report files\n", Ntry);
    printf("├── IB/       %d backup files (with actual_X)\n", Ntry);
    printf("├── pool/     Global rule pools (with actual_X)\n");
    printf("├── doc/      Documentation\n");
    printf("└── vis/      Visualization data (with actual_X)\n");

    /* 総合統計 */
    printf("\nTotal Rules Discovered: %d\n", total_rule_count);
    printf("High-Support Rules: %d\n", total_high_support);
    printf("Low-Variance Rules: %d\n", total_low_variance);
    printf("Rules with %d+ attributes: %d\n", MIN_ATTRIBUTES, rules_by_min_attributes);
    printf("========================================\n");
}

/* ================================================================================
   パス設定関数

   通貨ペアコードに基づいて、入力ファイルと出力ディレクトリのパスを設定します。
   ================================================================================
*/

/**
 * 通貨ペアコードに基づいてファイルパスとディレクトリを設定
 * @param code 通貨ペアコード（例: "7203"）
 */
void setup_paths_for_forex(const char *code)
{
    // 通貨ペアコードをコピー
    strncpy(forex_pair, code, sizeof(forex_pair) - 1);
    forex_pair[sizeof(forex_pair) - 1] = '\0';

    // データファイルパスを設定
    snprintf(data_file_path, sizeof(data_file_path),
             "forex_data/gnminer_individual/%s.txt", forex_pair);

    // 出力ベースディレクトリを設定（output/配下に通貨ペアごとに分ける）
    snprintf(output_base_dir, sizeof(output_base_dir),
             "output/%s", forex_pair);

    // 各サブディレクトリのパスを設定
    snprintf(output_dir_il, sizeof(output_dir_il),
             "%s/IL", output_base_dir);
    snprintf(output_dir_ia, sizeof(output_dir_ia),
             "%s/IA", output_base_dir);
    snprintf(output_dir_ib, sizeof(output_dir_ib),
             "%s/IB", output_base_dir);
    snprintf(output_dir_pool, sizeof(output_dir_pool),
             "%s/pool", output_base_dir);
    snprintf(output_dir_doc, sizeof(output_dir_doc),
             "%s/doc", output_base_dir);
    snprintf(output_dir_vis, sizeof(output_dir_vis),
             "%s/vis", output_base_dir);

    // ファイルパスを設定
    snprintf(pool_file_a, sizeof(pool_file_a),
             "%s/pool/zrp01a.txt", output_base_dir);
    snprintf(pool_file_b, sizeof(pool_file_b),
             "%s/pool/zrp01b.txt", output_base_dir);
    snprintf(cont_file, sizeof(cont_file),
             "%s/doc/zrd01.txt", output_base_dir);

    printf("\n=== Path Configuration ===\n");
    printf("Forex Pair: %s\n", forex_pair);
    printf("Input File: %s\n", data_file_path);
    printf("Output Dir: %s\n", output_base_dir);
    printf("=========================\n\n");
}

/* ================================================================================
   単一通貨ペア処理関数

   指定された通貨ペアに対して全試行を実行します。
   ================================================================================
*/

/**
 * 単一通貨ペアの分析を実行
 * @param code 通貨ペアコード
 * @return 成功時0、失敗時1
 */
int process_single_forex(const char *code)
{
    int trial, gen, i, prev_count;
    struct trial_state state;
    clock_t forex_start_time, forex_end_time;

    // 通貨ペアの処理開始時刻を記録
    forex_start_time = clock();

    printf("\n");
    printf("##################################################\n");
    printf("##  Processing Forex Pair: %s\n", code);
    printf("##################################################\n");

    // パス設定
    setup_paths_for_forex(code);

    // 出力ディレクトリ構造を作成
    create_output_directories();

    // CSVファイルからデータとヘッダーを読み込み
    if (load_csv_with_header() != 0)
    {
        // ファイルが存在しない場合はスキップ
        printf("Skipping forex pair %s (data file not found)\n\n", code);
        return 1; // 失敗を返す（次の通貨ペアへ）
    }

    // グローバルカウンタを初期化
    initialize_global_counters();

    // グローバルルールプールを初期化（全試行統合用）
    global_rule_count = 0;
    global_high_support_count = 0;
    global_low_variance_count = 0;

    // ドキュメントファイルを初期化
    initialize_document_file();

    /* ========== メイン試行ループ ========== */
    // Ntry回の独立した試行を実行
    for (trial = Nstart; trial < (Nstart + Ntry); trial++)
    {
        /* ---------- 試行の初期化 ---------- */

        // 試行開始時刻を記録
        clock_t trial_start_time = clock();

        // 試行状態構造体を初期化
        state.trial_id = trial;
        state.rule_count = 1; // インデックス0は未使用
        state.high_support_rule_count = 1;
        state.low_variance_rule_count = 1;
        state.generation = 0;
        state.elapsed_time = 0.0;

        // 出力ファイル名を生成
        generate_filename(state.filename_rule, "IL", trial);   // ルールリスト
        generate_filename(state.filename_report, "IA", trial); // 分析レポート
        generate_filename(state.filename_local, "IB", trial);  // 詳細バックアップ

        // 試行開始メッセージ
        printf("\n========== Trial %d/%d Started ==========\n", trial - Nstart + 1, Ntry);
        if (TIMESERIES_MODE && ADAPTIVE_DELAY)
        {
            printf("  [Time-Series Mode: Temporal Pattern Analysis]\n");
            printf("  [Minimum attributes: %d]\n", MIN_ATTRIBUTES);
        }

        // 各種データ構造を初期化
        initialize_rule_pool();            // ルールプールをクリア
        initialize_delay_statistics();     // 遅延統計をリセット
        initialize_attribute_statistics(); // 属性統計をリセット
        create_initial_population();       // 初期個体群を生成
        create_trial_files(&state);        // 出力ファイルを作成

        /* ---------- 進化計算ループ ---------- */
        // Generation世代まで進化を実行
        for (gen = 0; gen < Generation; gen++)
        {
            // ルール数上限チェック
            if (state.rule_count > (Nrulemax - 2))
            {
                printf("Rule limit reached. Stopping evolution.\n");
                break;
            }

            /* === 個体評価フェーズ === */

            // 遺伝子情報をGNPノード構造にコピー
            copy_genes_to_nodes();

            // 個体統計を初期化
            initialize_individual_statistics();

            // 全個体を全データで評価
            evaluate_all_individuals();

            // ネガティブカウントを計算
            calculate_negative_counts();

            // ルール統計（平均、標準偏差）を計算
            calculate_rule_statistics();

            /* === ルール抽出フェーズ === */

            // 全個体からルールを抽出
            extract_rules_from_population(&state);

            /* === 新規ルール出力フェーズ === */

            // 前回までのルール数を取得
            prev_count = rules_per_trial[trial - Nstart];

            // 新規ルールをファイルに出力
            for (i = prev_count + 1; i < state.rule_count; i++)
            {
                // (IL/visディレクトリへの出力は削除されました)
            }

            // 現在のルール数を記録
            rules_per_trial[trial - Nstart] = state.rule_count - 1;

            /* === 統計更新フェーズ === */

            // 適応的学習のための統計を更新
            update_delay_statistics(gen);
            update_attribute_statistics(gen);

            /* === 進化操作フェーズ === */

            // 適応度ランキングを計算
            calculate_fitness_rankings();

            // エリート選択（上位1/3を3つずつコピー）
            perform_selection();

            // 交叉（上位個体間で遺伝子交換）
            perform_crossover();

            // 処理ノードの突然変異
            perform_mutation_processing_nodes();

            // 判定ノードの突然変異
            perform_mutation_judgment_nodes();

            // 適応的遅延突然変異（良い遅延を優先）
            perform_adaptive_delay_mutation();

            // 適応的属性突然変異（良い属性を優先）
            perform_adaptive_attribute_mutation();

            /* === 進捗報告フェーズ === */

            // 定期的に進捗を報告
            if (gen % REPORT_INTERVAL == 0)
            {
                write_progress_report(&state, gen);
            }
        }

        /* ---------- 試行の終了処理 ---------- */

        // 試行終了時刻を記録
        clock_t trial_end_time = clock();
        state.elapsed_time = (double)(trial_end_time - trial_start_time) / CLOCKS_PER_SEC;

        // 統計情報を記録
        write_document_stats(&state);

        // 現在の試行のルールをグローバルプールに統合
        merge_trial_rules_to_global_pool(&state);

        // ローカル詳細ファイルに全ルールを出力（IB）
        write_local_output(&state);

        // 試行終了メッセージ
        printf("========== Trial %d/%d Completed (%.2fs) ==========\n",
               trial - Nstart + 1, Ntry, state.elapsed_time);
        printf("  Trial rules: %d\n", state.rule_count - 1);
        printf("  Global pool rules: %d (accumulated)\n", global_rule_count);
        printf("  High-support rules: %d\n", state.high_support_rule_count - 1);
        printf("  Low-variance rules: %d\n", state.low_variance_rule_count - 1);
    }

    /* ========== 最終処理 ========== */

    // 極値シグナルスコアを計算（グローバルプール出力前に実行）
    printf("\n========== Calculating Extreme Signal Scores ==========\n");
    printf("  Total integrated rules: %d\n", global_rule_count);
    calculate_and_write_extreme_scores();

    // 全試行を統合したグローバルプールを出力（極値スコア付き）
    printf("\n========== Writing Global Rule Pool ==========\n");
    printf("  Total integrated rules: %d\n", global_rule_count);
    printf("  High-support rules: %d\n", global_high_support_count);
    printf("  Low-variance rules: %d\n", global_low_variance_count);
    write_global_pool(&state);

    // 全試行の統計を表示
    print_final_statistics();

    // メモリ解放
    free_dynamic_memory();

    // 通貨ペアの処理終了時刻を記録
    forex_end_time = clock();
    double forex_elapsed = (double)(forex_end_time - forex_start_time) / CLOCKS_PER_SEC;

    printf("\n");
    printf("##################################################\n");
    printf("##  Forex Pair %s Completed (%.2fs)\n", code, forex_elapsed);
    printf("##################################################\n");
    printf("\n");

    return 0;
}

/* ================================================================================
   メイン関数

   プログラムのエントリポイント。
   全体の処理フローを制御し、各試行での進化計算を実行します。
   ================================================================================
*/

/**
 * プログラムのメイン関数
 * 時系列為替レート予測のためのGNMinerを実行
 * @param argc コマンドライン引数の数
 * @param argv コマンドライン引数の配列
 * @return 終了ステータス（0:正常終了）
 */
int main(int argc, char *argv[])
{
    int forex_idx;
    int success_count = 0;
    int failed_count = 0;
    clock_t batch_start_time, batch_end_time;
    FILE *batch_log_file = NULL;
    char batch_log_filename[256];
    time_t now;
    struct tm *tm_info;

    /* ========== コマンドライン引数処理 ========== */

    // 使用方法を表示
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        printf("==============================================\n");
        printf("  GNMiner Phase 2 - Time-Series Forex Predictor\n");
        printf("==============================================\n\n");
        printf("Usage: %s [options]\n\n", argv[0]);
        printf("Options:\n");
        printf("  -h, --help    Show this help message\n");
        printf("  --all         Process all forex pairs (%d pairs)\n", FOREX_PAIRS_COUNT);
        printf("  <forex_pair>  Process single forex pair (e.g., USDJPY)\n");
        printf("                If omitted, processes all forex pairs\n\n");
        printf("Examples:\n");
        printf("  %s           # Process all %d forex pairs\n", argv[0], FOREX_PAIRS_COUNT);
        printf("  %s --all     # Process all %d forex pairs\n", argv[0], FOREX_PAIRS_COUNT);
        printf("  %s USDJPY    # Process only USD/JPY\n", argv[0]);
        printf("  %s EURUSD    # Process only EUR/USD\n\n", argv[0]);
        printf("==============================================\n");
        return 0;
    }

    // 乱数シードを固定（再現性確保）
    srand(1);

    // バッチログファイルを作成
    now = time(NULL);
    tm_info = localtime(&now);
    snprintf(batch_log_filename, sizeof(batch_log_filename),
             "batch_run_%04d%02d%02d_%02d%02d%02d.log",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    batch_log_file = fopen(batch_log_filename, "w");

    // バッチ処理開始
    batch_start_time = clock();

    // 単一通貨ペアモードか全通貨ペアモードかを判定
    if (argc > 1 && strcmp(argv[1], "--all") != 0)
    {
        // 単一通貨ペアモード
        const char *forex_pair_arg = argv[1];

        printf("\n");
        printf("==================================================\n");
        printf("  GNMiner Phase 2 - Single Forex Pair Mode\n");
        printf("==================================================\n");
        printf("Forex Pair: %s\n", forex_pair_arg);
        printf("Log File: %s\n", batch_log_filename);
        printf("==================================================\n");
        printf("\n");

        if (batch_log_file != NULL)
        {
            fprintf(batch_log_file, "Single Forex Pair Mode\n");
            fprintf(batch_log_file, "Forex Pair: %s\n", forex_pair_arg);
            fprintf(batch_log_file, "Start Time: %04d-%02d-%02d %02d:%02d:%02d\n\n",
                    tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        }

        // 単一通貨ペアを処理
        int result = process_single_forex(forex_pair_arg);

        if (result == 0)
        {
            success_count = 1;
            if (batch_log_file != NULL)
            {
                fprintf(batch_log_file, "SUCCESS: %s\n", forex_pair_arg);
            }
        }
        else
        {
            failed_count = 1;
            if (batch_log_file != NULL)
            {
                fprintf(batch_log_file, "FAILED: %s\n", forex_pair_arg);
            }
        }
    }
    else
    {
        // 全通貨ペアモード
        printf("\n");
        printf("==================================================\n");
        printf("  GNMiner Phase 2 - Batch Processing Mode\n");
        printf("==================================================\n");
        printf("Total Forex Pairs: %d\n", FOREX_PAIRS_COUNT);
        printf("Log File: %s\n", batch_log_filename);
        printf("==================================================\n");
        printf("\n");

        if (batch_log_file != NULL)
        {
            fprintf(batch_log_file, "Batch Processing Mode\n");
            fprintf(batch_log_file, "Total Forex Pairs: %d\n", FOREX_PAIRS_COUNT);
            fprintf(batch_log_file, "Start Time: %04d-%02d-%02d %02d:%02d:%02d\n\n",
                    tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        }

        // 全通貨ペアを順次処理
        for (forex_idx = 0; FOREX_PAIRS[forex_idx] != NULL; forex_idx++)
        {
            const char *current_pair = FOREX_PAIRS[forex_idx];

            printf("\n");
            printf("==================================================\n");
            printf("  [%d/%d] Processing: %s\n", forex_idx + 1, FOREX_PAIRS_COUNT, current_pair);
            printf("==================================================\n");

            if (batch_log_file != NULL)
            {
                fprintf(batch_log_file, "\n[%d/%d] Forex Pair: %s\n", forex_idx + 1, FOREX_PAIRS_COUNT, current_pair);
                fflush(batch_log_file);
            }

            // 通貨ペアを処理
            int result = process_single_forex(current_pair);

            if (result == 0)
            {
                success_count++;
                printf("✓ SUCCESS: %s\n", current_pair);
                if (batch_log_file != NULL)
                {
                    fprintf(batch_log_file, "Result: SUCCESS\n");
                }
            }
            else
            {
                failed_count++;
                printf("✗ FAILED: %s\n", current_pair);
                if (batch_log_file != NULL)
                {
                    fprintf(batch_log_file, "Result: FAILED\n");
                }
            }

            // 進捗状況を表示
            printf("\n");
            printf("--------------------------------------------------\n");
            printf("Progress: %d/%d (%.1f%%)\n",
                   forex_idx + 1, FOREX_PAIRS_COUNT,
                   ((double)(forex_idx + 1) / FOREX_PAIRS_COUNT) * 100.0);
            printf("Success: %d | Failed: %d\n", success_count, failed_count);
            printf("--------------------------------------------------\n");
            printf("\n");
        }
    }

    /* ========== バッチ処理終了 ========== */

    // バッチ処理終了時刻を記録
    batch_end_time = clock();
    double total_elapsed = (double)(batch_end_time - batch_start_time) / CLOCKS_PER_SEC;

    // 最終サマリーを表示
    printf("\n");
    printf("==================================================\n");
    printf("  Batch Processing Complete\n");
    printf("==================================================\n");
    printf("Total Time: %.2fs (%.1fm)\n", total_elapsed, total_elapsed / 60.0);
    printf("Success: %d forex pairs\n", success_count);
    printf("Failed: %d forex pairs\n", failed_count);
    printf("Success Rate: %.1f%%\n",
           (double)success_count / (success_count + failed_count) * 100.0);
    printf("Log File: %s\n", batch_log_filename);
    printf("==================================================\n");
    printf("\n");

    // ログファイルに最終サマリーを記録
    if (batch_log_file != NULL)
    {
        fprintf(batch_log_file, "\n");
        fprintf(batch_log_file, "===== Batch Processing Complete =====\n");
        fprintf(batch_log_file, "Total Time: %.2fs (%.1fm)\n", total_elapsed, total_elapsed / 60.0);
        fprintf(batch_log_file, "Success: %d forex pairs\n", success_count);
        fprintf(batch_log_file, "Failed: %d forex pairs\n", failed_count);
        fprintf(batch_log_file, "Success Rate: %.1f%%\n",
                (double)success_count / (success_count + failed_count) * 100.0);
        fclose(batch_log_file);
    }

    return 0; // 正常終了
}
