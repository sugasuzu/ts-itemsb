#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   時系列対応版GNMiner - ルール発見システム

   【概要】
   このプログラムは、Genetic Network Programming (GNP)を使用して、
   時系列データから予測ルールを自動発見するシステムです。

   【主要機能】
   1. 過去の複数時点の属性パターンから将来の値を予測
   2. 時間遅延メカニズムによる時系列パターンの捕捉
   3. 適応的学習による効率的なルール発見
   4. 複数時点の未来予測（FUTURE_SPAN対応）
   5. 動的な4次元配列による拡張性の高い設計
   ================================================================================
*/

/* ================================================================================
   定数定義セクション

   このセクションでは、プログラム全体で使用される定数を定義します。
   これらの値を変更することで、プログラムの動作を調整できます。
   ================================================================================
*/

/* 時系列パラメータ */
#define TIMESERIES_MODE 1 // 時系列モードの有効化（1:有効、0:無効）
#define MAX_TIME_DELAY 4  // 最大時間遅延（t-4まで参照可能）
#define MIN_TIME_DELAY 0  // 最小時間遅延（現在時点 t-0）
#define PREDICTION_SPAN 1 // 予測スパン（未使用、互換性のため残存）
#define FUTURE_SPAN 3     // 未来予測範囲（検証CSVに出力: t+1, t+2, ..., t+FUTURE_SPAN）
#define ADAPTIVE_DELAY 1  // 適応的遅延学習の有効化（1:有効、0:無効）

/* ディレクトリ構造
   出力ファイルを整理するためのディレクトリ構造を定義 */
#define OUTPUT_DIR "output"                           // メイン出力ディレクトリ
#define OUTPUT_DIR_IL "output/IL"                     // Individual Lists: ルールリスト
#define OUTPUT_DIR_IA "output/IA"                     // Individual Analysis: 分析レポート
#define OUTPUT_DIR_IB "output/IB"                     // Individual Backup: バックアップ
#define OUTPUT_DIR_POOL "output/pool"                 // グローバルルールプール
#define OUTPUT_DIR_DOC "output/doc"                   // ドキュメント・統計
#define OUTPUT_DIR_VIS "output/vis"                   // 可視化用データ
#define OUTPUT_DIR_VERIFICATION "output/verification" // ルール検証データ

/* ファイル名
   入力データと出力ファイルのパスを定義 */
#define DATANAME "crypto_data/gnminer_individual/BTC.txt" // 入力データファイル（デフォルト:BTC）
#define POOL_FILE_A "output/pool/zrp01a.txt"              // ルールプール出力A（詳細版）
#define POOL_FILE_B "output/pool/zrp01b.txt"              // ルールプール出力B（要約版）
#define CONT_FILE "output/doc/zrd01.txt"                  // 統計情報ファイル
#define RESULT_FILE "output/doc/zrmemo01.txt"             // メモファイル（未使用）

/* 動的ファイルパス（コマンドライン引数で変更可能） */
char stock_code[20] = "BTC";                                 // 銘柄/ペアコード
char data_file_path[512] = DATANAME;                         // データファイルパス
char output_base_dir[256] = "output";                        // 出力ベースディレクトリ
char pool_file_a[512] = POOL_FILE_A;                         // 動的ルールプールA
char pool_file_b[512] = POOL_FILE_B;                         // 動的ルールプールB
char cont_file[512] = CONT_FILE;                             // 動的統計ファイル
char output_dir_il[512] = OUTPUT_DIR_IL;                     // 動的IL出力
char output_dir_ia[512] = OUTPUT_DIR_IA;                     // 動的IA出力
char output_dir_ib[512] = OUTPUT_DIR_IB;                     // 動的IB出力
char output_dir_pool[512] = OUTPUT_DIR_POOL;                 // 動的pool出力
char output_dir_doc[512] = OUTPUT_DIR_DOC;                   // 動的doc出力
char output_dir_vis[512] = OUTPUT_DIR_VIS;                   // 動的vis出力
char output_dir_verification[512] = OUTPUT_DIR_VERIFICATION; // 動的verification出力

/* データ構造パラメータ
   CSVファイルから読み込み時に動的に設定される */
int Nrd = 0; // レコード数（データの行数）
int Nzk = 0; // 属性数（カラム数 - X列 - T列）

/* ルールマイニング制約
   抽出するルールの品質を制御する閾値 */
#define Nrulemax 2002       // 最大ルール数（メモリ制限）
#define Minsup 0.001        // 最小サポート値（1%以上の頻度が必要)
#define Maxsigx 3.0         // 最大X標準偏差（2.0%以下のルールのみ採用）
#define MIN_ATTRIBUTES 2    // ルールの最小属性数（2個以上の属性が必要）
#define MIN_SUPPORT_COUNT 2 // 最小サポートカウント（2回以上のマッチが必要、統計的信頼性のため）

/* 実験パラメータ
   実験の規模と繰り返し回数を設定 */
#define Nstart 1000 // 試行開始番号（ファイル名に使用）
#define Ntry 1      // 試行回数（10回の独立した実験を実行）

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

/* 時系列ルール構造体
   発見されたルールの完全な情報を保持 */
struct temporal_rule
{
    // ルールの前件部（IF部分）
    int antecedent_attrs[MAX_ATTRIBUTES]; // 属性ID配列（1-indexed、0は未使用）
    int time_delays[MAX_ATTRIBUTES];      // 各属性の時間遅延（0=t, 1=t-1, ...）

    // 未来予測値の統計（t+1, t+2, ..., t+FUTURE_SPAN）
    double future_mean[FUTURE_SPAN];  // 各未来時点の予測値平均
    double future_sigma[FUTURE_SPAN]; // 各未来時点の予測値標準偏差

    // ルールの品質指標
    int support_count;     // サポートカウント（マッチした回数）
    int negative_count;    // ネガティブカウント（評価対象数）
    double support_rate;   // サポート率（support_count / negative_count）
    int high_support_flag; // 高サポートフラグ（1:高サポート）
    int low_variance_flag; // 低分散フラグ（1:低分散）
    int num_attributes;    // 属性数

    // マッチした時点のインデックス（可視化・分析用）
    int *matched_indices;  // マッチしたレコードのインデックス配列
    int matched_count_vis; // マッチ数（可視化用）
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
int **data_buffer = NULL;           // 属性データ [レコード数][属性数]
double *x_buffer = NULL;            // X値（CSVのX列：現在時点の変化率）[レコード数]
char **timestamp_buffer = NULL;     // タイムスタンプ [レコード数][最大50文字]
char **attribute_dictionary = NULL; // 属性名辞書 [属性数+3][最大50文字]

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
int delay_usage_history[HISTORY_GENERATIONS][MAX_TIME_DELAY + 1]; // 遅延使用履歴
int delay_usage_count[MAX_TIME_DELAY + 1];                        // 遅延使用カウント
int delay_tracking[MAX_TIME_DELAY + 1];                           // 遅延追跡（累積）

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

/* 未来予測統計配列（4次元・動的割り当て）
   [個体][処理ノード][深さ][FUTURE_SPAN] */
double ****future_sum = NULL;         // 未来値の合計（t+1, t+2, ..., t+FUTURE_SPAN）
double ****future_sigma_array = NULL; // 未来値の二乗和（分散計算用）

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
int x_column_index = -1; // X列のインデックス（現在時点の変化率）
int t_column_index = -1; // T列（タイムスタンプ）のインデックス

/* 統計情報 */
int rules_by_min_attributes = 0; // 最小属性数を満たすルール数

/* ================================================================================
   関数プロトタイプ宣言
   ================================================================================
*/
double get_future_value(int row_idx, int offset);

/* ================================================================================
   メモリ管理関数

   動的メモリの割り当てと解放を管理します。
   ================================================================================
*/

/**
 * 未来予測統計用4次元配列を動的割り当て
 *
 * 【配列構造】
 * future_sum[個体][処理ノード][深さ][FUTURE_SPAN]
 * future_sigma_array[個体][処理ノード][深さ][FUTURE_SPAN]
 *
 * 各次元の意味:
 * - [個体]: GNP個体ID (0 ~ Nkotai-1)
 * - [処理ノード]: ルールの開始点ID (0 ~ Npn-1)
 * - [深さ]: ルールの属性数（深さ） (0 ~ MAX_DEPTH-1)
 * - [FUTURE_SPAN]: 未来予測の時点 (0=t+1, 1=t+2, 2=t+3, ...)
 *
 * この4次元構造により、FUTURE_SPANを変更するだけで
 * 任意の未来予測範囲に対応できます。
 */
void allocate_future_arrays()
{
    int i, j, k;

    /* future_sum配列の割り当て */
    future_sum = (double ****)malloc(Nkotai * sizeof(double ***));
    if (future_sum == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_sum (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_sum[i] = (double ***)malloc(Npn * sizeof(double **));
        if (future_sum[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_sum (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_sum[i][j] = (double **)malloc(MAX_DEPTH * sizeof(double *));
            if (future_sum[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_sum (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_sum[i][j][k] = (double *)calloc(FUTURE_SPAN, sizeof(double));
                if (future_sum[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_sum (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }

    /* future_sigma_array配列の割り当て */
    future_sigma_array = (double ****)malloc(Nkotai * sizeof(double ***));
    if (future_sigma_array == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_sigma_array (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_sigma_array[i] = (double ***)malloc(Npn * sizeof(double **));
        if (future_sigma_array[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_sigma_array (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_sigma_array[i][j] = (double **)malloc(MAX_DEPTH * sizeof(double *));
            if (future_sigma_array[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_sigma_array (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_sigma_array[i][j][k] = (double *)calloc(FUTURE_SPAN, sizeof(double));
                if (future_sigma_array[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_sigma_array (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }
}

/**
 * 未来予測統計用配列を0で初期化
 */
void initialize_future_arrays()
{
    int i, j, k, offset;

    for (i = 0; i < Nkotai; i++)
    {
        for (j = 0; j < Npn; j++)
        {
            for (k = 0; k < MAX_DEPTH; k++)
            {
                for (offset = 0; offset < FUTURE_SPAN; offset++)
                {
                    future_sum[i][j][k][offset] = 0.0;
                    future_sigma_array[i][j][k][offset] = 0.0;
                }
            }
        }
    }
}

/**
 * 未来予測統計用配列のメモリを解放
 */
void free_future_arrays()
{
    int i, j, k;

    if (future_sum != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(future_sum[i][j][k]);
                }
                free(future_sum[i][j]);
            }
            free(future_sum[i]);
        }
        free(future_sum);
    }

    if (future_sigma_array != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(future_sigma_array[i][j][k]);
                }
                free(future_sigma_array[i][j]);
            }
            free(future_sigma_array[i]);
        }
        free(future_sigma_array);
    }
}

/**
 * プログラムで使用する全ての動的メモリを割り当てる
 * CSVから読み込んだNrdとNzkの値に基づいてサイズを決定
 */
void allocate_dynamic_memory()
{
    int i, j;

    /* データバッファの割り当て */
    // 2次元配列：[レコード数][属性数]の属性データ
    data_buffer = (int **)malloc(Nrd * sizeof(int *));
    for (i = 0; i < Nrd; i++)
    {
        data_buffer[i] = (int *)malloc(Nzk * sizeof(int));
    }

    // 1次元配列：各レコードのX値（CSVのX列：現在時点の変化率）
    x_buffer = (double *)malloc(Nrd * sizeof(double));

    // 2次元配列：各レコードのタイムスタンプ文字列
    timestamp_buffer = (char **)malloc(Nrd * sizeof(char *));
    for (i = 0; i < Nrd; i++)
    {
        timestamp_buffer[i] = (char *)malloc(MAX_ATTR_NAME * sizeof(char));
    }

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

    // 各個体・処理ノード・深さごとに配列を割り当て
    for (i = 0; i < Nkotai; i++)
    {
        match_count[i] = (int **)malloc(Npn * sizeof(int *));
        negative_count[i] = (int **)malloc(Npn * sizeof(int *));
        evaluation_count[i] = (int **)malloc(Npn * sizeof(int *));
        attribute_chain[i] = (int **)malloc(Npn * sizeof(int *));
        time_delay_chain[i] = (int **)malloc(Npn * sizeof(int *));

        for (j = 0; j < Npn; j++)
        {
            match_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            negative_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            evaluation_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            attribute_chain[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            time_delay_chain[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
        }
    }

    /* 未来予測統計配列の割り当て（4次元） */
    allocate_future_arrays();

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
    int i, j;

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

    /* タイムスタンプバッファ解放 */
    if (timestamp_buffer != NULL)
    {
        for (i = 0; i < Nrd; i++)
        {
            free(timestamp_buffer[i]);
        }
        free(timestamp_buffer);
    }

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
            }
            free(match_count[i]);
            free(negative_count[i]);
            free(evaluation_count[i]);
            free(attribute_chain[i]);
            free(time_delay_chain[i]);
        }
        free(match_count);
        free(negative_count);
        free(evaluation_count);
        free(attribute_chain);
        free(time_delay_chain);
    }

    /* 未来予測統計配列解放（4次元） */
    free_future_arrays();

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
    int i; /* ループ変数（C89スタイル） */

    printf("Loading CSV file with header: %s\n", data_file_path);

    // ファイルオープン
    file = fopen(data_file_path, "r");
    if (file == NULL)
    {
        printf("Warning: Cannot open data file: %s (skipping this stock)\n", data_file_path);
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

            // X列の検出（翌日の変化率）
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
                // X値を読み込み（現在時点の変化率）
                x_buffer[row] = atof(token);
            }
            else if (col == t_column_index)
            {
                // タイムスタンプを保存（検証用）
                strncpy(timestamp_buffer[row], token, MAX_ATTR_NAME - 1);
                timestamp_buffer[row][MAX_ATTR_NAME - 1] = '\0';
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

    // データ統計情報を表示
    int positive_count = 0; /* X > 0 のカウント */
    int negative_count = 0; /* X < 0 のカウント */

    for (i = 0; i < Nrd; i++)
    {
        if (x_buffer[i] > 0.0)
            positive_count++;
        else if (x_buffer[i] < 0.0)
            negative_count++;
    }

    printf("\n========== Dataset Statistics ==========\n");
    printf("Total records: %d\n", Nrd);
    printf("Positive X count (X > 0): %d (%.1f%%)\n", positive_count, 100.0 * positive_count / Nrd);
    printf("Negative X count (X < 0): %d (%.1f%%)\n", negative_count, 100.0 * negative_count / Nrd);
    printf("Zero X count (X = 0): %d\n", Nrd - positive_count - negative_count);
    printf("========================================\n\n");

    // 読み込み結果のサマリを表示
    printf("Data loaded successfully:\n");
    printf("  Records: %d\n", Nrd);
    printf("  Attributes: %d\n", Nzk);
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
    // ディレクトリを再帰的に作成（mkdir -p 相当）
    char cmd[1024];
    int ret;

    // 親ディレクトリを含めて再帰的に作成
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"",
             output_base_dir, output_dir_ia, output_dir_ib,
             output_dir_pool, output_dir_doc, output_dir_verification);
    ret = system(cmd);

    if (ret != 0)
    {
        printf("ERROR: Failed to create output directories\n");
        printf("Command: %s\n", cmd);
        exit(1);
    }

    // ディレクトリ構造を表示
    printf("=== Directory Structure Created ===\n");
    printf("Stock Code: %s\n", stock_code);
    printf("%s/\n", output_base_dir);
    printf("├── IA/            (Analysis Reports)\n");
    printf("├── IB/            (Backup Files)\n");
    printf("├── pool/          (Global Rule Pool)\n");
    printf("├── doc/           (Documentation)\n");
    printf("└── verification/  (Rule Verification Data)\n");
    printf("===================================\n");

    // 時系列モードの設定を表示
    if (TIMESERIES_MODE)
    {
        printf("*** TIME-SERIES MODE ENABLED ***\n");
        printf("Single variable X with temporal analysis\n");
        printf("Adaptive delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY);
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

        // 未来予測統計値の初期化（t+1, t+2, ..., t+FUTURE_SPAN）
        for (int k = 0; k < FUTURE_SPAN; k++)
        {
            rule_pool[i].future_mean[k] = 0.0;
            rule_pool[i].future_sigma[k] = 0.0;
        }
        rule_pool[i].support_count = 0;
        rule_pool[i].negative_count = 0;
        rule_pool[i].high_support_flag = 0;
        rule_pool[i].low_variance_flag = 0;
        rule_pool[i].num_attributes = 0;

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

    for (i = 0; i <= MAX_TIME_DELAY; i++)
    {
        delay_usage_count[i] = 0;
        delay_tracking[i] = INITIAL_DELAY_HISTORY; // 初期値1を設定（ゼロ除算回避）

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
        // データ期間を確認し、t-4以降から開始するインデックスを設定
        // (MAX_TIME_DELAY=4のため、インデックス4から有効)
        *start_index = MAX_TIME_DELAY;
        *end_index = Nrd - PREDICTION_SPAN; // t+1が存在する範囲まで
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
 * 現在の実際の値を取得
 * ルール適用時点でのX値を返す
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

            // ランダムに時間遅延を設定（0～MAX_TIME_DELAY）
            gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY + 1);
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
    int individual, k, i;

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

                // 未来予測統計値のクリア（FUTURE_SPAN個）
                for (int offset = 0; offset < FUTURE_SPAN; offset++)
                {
                    future_sum[individual][k][i][offset] = 0.0;
                    future_sigma_array[individual][k][i][offset] = 0.0;
                }
            }
        }
    }
}

/* ================================================================================
   ルール評価系関数

   GNP個体がデータを走査し、ルール候補を評価します。
   時系列データの各時点でルール評価を実行します。
   ================================================================================
*/

/**
 * 単一のデータインスタンスを評価
 *
 * 全個体が指定時点のデータに対してルールマッチングを実行します。
 *
 * 【アルゴリズム】
 * 1. 各個体の処理ノード（開始点）から探索を開始
 * 2. 判定ノードを辿り、属性値が1の場合のみマッチを継続
 * 3. マッチ中の場合、future_sum/future_sigma_arrayに未来値を累積
 * 4. evaluation_countは属性値を評価した回数（0/1/-1すべて）をカウント
 *
 * @param time_index 評価するデータの時点インデックス
 */
void evaluate_single_instance(int time_index)
{
    int current_node_id, depth, match_flag;
    int time_delay, data_index;
    int individual, k;

    // 未来値はget_future_value()で各offset(t+1, t+2, t+3, ...)ごとに取得

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

                        // 未来予測値の累積（t+1, t+2, ..., t+FUTURE_SPAN）
                        for (int offset = 0; offset < FUTURE_SPAN; offset++)
                        {
                            double future_val = get_future_value(time_index, offset + 1);
                            if (!isnan(future_val))
                            {
                                future_sum[individual][k][depth][offset] += future_val;
                                future_sigma_array[individual][k][depth][offset] += future_val * future_val;
                            }
                        }
                    }
                    evaluation_count[individual][k][depth]++;
                    // 次の判定ノードへ
                    current_node_id = node_structure[individual][current_node_id][1];
                }
                else if (attr_value == 0)
                {
                    // 属性値が0：スキップ側へ進む（処理ノードに戻る）
                    //
                    // 【重要】この evaluation_count++ が negative_count 計算の正確性に不可欠
                    //
                    // negative_count = match_count[0] - evaluation_count[i] + match_count[i]
                    //                = 全データ数 - 評価したが不一致だった数 + マッチした数
                    //                = 評価可能なデータ数
                    //
                    // もし evaluation_count++ がないと、evaluation_count ≈ match_count となり、
                    // すべてのルールで negative_count = match_count[0]（定数）になってしまう。
                    //
                    // 修正履歴: 2025-11-02 - negative_count計算の正確性のため追加
                    evaluation_count[individual][k][depth]++;
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
 *
 * サポート値計算用の分母（評価可能なデータ数）を求めます。
 *
 * 【計算式の意味】
 * negative_count[i] = match_count[0] - evaluation_count[i] + match_count[i]
 *
 * 変形すると:
 *   = match_count[0] - (evaluation_count[i] - match_count[i])
 *   = 全データ数 - (評価したが不一致だった数)
 *   = 評価しなかったデータ数 + マッチしたデータ数
 *   = 深さiで評価可能なデータ数（有効範囲内）
 *
 * この値は support_rate = support_count / negative_count の分母として使用されます。
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
                    int n = match_count[individual][k][j];

                    // 未来予測値の平均と標準偏差を計算（t+1, t+2, ..., t+FUTURE_SPAN）
                    for (int offset = 0; offset < FUTURE_SPAN; offset++)
                    {
                        // 平均を計算
                        double mean = future_sum[individual][k][j][offset] / (double)n;
                        future_sum[individual][k][j][offset] = mean; // 平均値で上書き

                        // 標準偏差を計算（不偏標準偏差）
                        if (n > 1)
                        {
                            // 標本分散を計算
                            double variance = future_sigma_array[individual][k][j][offset] / (double)n -
                                              mean * mean;

                            // 負の分散を防ぐ（数値誤差対策）
                            if (variance < 0)
                            {
                                variance = 0;
                            }

                            // 不偏分散に変換: s² = n/(n-1) * σ²
                            variance = variance * n / (n - 1);

                            future_sigma_array[individual][k][j][offset] = sqrt(variance);
                        }
                        else
                        {
                            // サンプル数が1の場合、標準偏差は計算不可
                            future_sigma_array[individual][k][j][offset] = 0;
                        }
                    }
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

/* ================================================================================
   ルール抽出系関数

   評価結果からルールを抽出し、品質チェックを行います。
   ================================================================================
*/

/**
 * ルールの品質をチェック
 * @param future_sigma_array 未来予測のσ配列 [FUTURE_SPAN]（t+1, t+2, t+3, ...）
 * @param support サポート値
 * @param num_attributes 属性数
 * @param support_count サポートカウント（マッチした回数）
 * @return 品質基準を満たす場合1、満たさない場合0
 */
int check_rule_quality(double *future_sigma_array, double support, int num_attributes, int support_count)
{
    int i;

    // 最小サンプル数チェック（統計的信頼性を確保）
    if (support_count < MIN_SUPPORT_COUNT)
    {
        return 0; // サンプル数不足（sigma=0の根本原因）
    }

    // 基本的な品質チェック
    int basic_check = (support >= Minsup &&               // サポート値が閾値以上
                       num_attributes >= MIN_ATTRIBUTES); // 最小属性数以上

    if (!basic_check)
    {
        return 0; // 基本チェック失敗
    }

    // t+1, t+2, ..., t+FUTURE_SPAN の全時点で標準偏差をチェック
    for (i = 0; i < FUTURE_SPAN; i++)
    {
        if (future_sigma_array[i] > Maxsigx)
        {
            return 0; // いずれかの時点でσが閾値を超えたら失格
        }
    }

    return 1; // 全ての品質基準を満たす
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

/* 前方宣言 */
void collect_matched_indices(int rule_idx, int *rule_attrs, int *time_delays, int num_attrs);

/**
 * 新規ルールを登録
 * ルールプールに新しいルールを追加し、各種統計を更新
 * @param state 試行状態
 * @param rule_candidate ルールの属性配列
 * @param time_delays 時間遅延配列
 * @param x_mean X予測値の平均（CSVのX列：翌日の変化率）
 * @param x_sigma X予測値の標準偏差
 * @param x_plus1_mean X+1予測値の平均（CSVのX+1列：翌々日の変化率）
 * @param x_plus1_sigma X+1予測値の標準偏差
 * @param support_count サポートカウント
 * @param negative_count_val ネガティブカウント
 * @param support_value サポート値
 * @param num_attributes 属性数
 * @param individual 個体ID
 * @param k 処理ノードID
 * @param depth 深さ
 */
void register_new_rule(struct trial_state *state, int *rule_candidate, int *time_delays,
                       double *future_mean, double *future_sigma,
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

    // 未来予測統計値を設定（t+1, t+2, ..., t+FUTURE_SPAN）
    for (i = 0; i < FUTURE_SPAN; i++)
    {
        rule_pool[idx].future_mean[i] = future_mean[i];
        rule_pool[idx].future_sigma[i] = future_sigma[i];
    }
    rule_pool[idx].support_count = support_count;
    rule_pool[idx].negative_count = negative_count_val;
    rule_pool[idx].support_rate = support_value; // サポート率を保存
    rule_pool[idx].num_attributes = num_attributes;

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

    // 低分散フラグの設定（全時点のσの最大値を使用、FUTURE_SPANに動的対応）
    int low_variance_marker = 0;
    double max_sigma = 0.0;
    for (int offset = 0; offset < FUTURE_SPAN; offset++)
    {
        if (future_sigma[offset] > max_sigma)
        {
            max_sigma = future_sigma[offset];
        }
    }
    if (max_sigma <= (Maxsigx - LOW_VARIANCE_REDUCTION))
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

    // マッチインデックスを収集（検証用）
    collect_matched_indices(idx, rule_candidate, time_delays, num_attributes);
}

/**
 * ルールにマッチした時点のインデックスを収集
 * 検証用にルールがマッチした全データ行のインデックスを保存
 * @param rule_idx ルールプール内のインデックス
 * @param rule_attrs ルールの属性配列
 * @param time_delays 時間遅延配列
 * @param num_attrs 属性数
 */
void collect_matched_indices(int rule_idx, int *rule_attrs, int *time_delays, int num_attrs)
{
    int safe_start, safe_end;
    int time_index, attr_idx, is_match;
    int matched_count = 0;

    // 安全な範囲を取得
    get_safe_data_range(&safe_start, &safe_end);

    // 全時点を走査
    for (time_index = safe_start; time_index < safe_end; time_index++)
    {
        is_match = 1; // マッチフラグ

        // 全属性をチェック
        for (attr_idx = 0; attr_idx < num_attrs; attr_idx++)
        {
            if (rule_attrs[attr_idx] > 0)
            {
                // 属性ID（1-indexed → 0-indexed）
                int attr_id = rule_attrs[attr_idx] - 1;
                int delay = time_delays[attr_idx];
                int data_index = time_index - delay;

                // 範囲チェック
                if (data_index < 0 || data_index >= Nrd)
                {
                    is_match = 0;
                    break;
                }

                // 属性値チェック（1である必要がある）
                if (data_buffer[data_index][attr_id] != 1)
                {
                    is_match = 0;
                    break;
                }
            }
        }

        // マッチした場合、インデックスを保存
        if (is_match == 1)
        {
            rule_pool[rule_idx].matched_indices[matched_count] = time_index;
            matched_count++;
        }
    }

    // マッチ数を保存
    rule_pool[rule_idx].matched_count_vis = matched_count;
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
        if (time_delays[k] >= 0 && time_delays[k] <= MAX_TIME_DELAY)
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
            int matched_count = match_count[individual][k][loop_j];
            double support = calculate_support_value(matched_count,
                                                     negative_count[individual][k][loop_j]);

            // 未来予測統計へのポインタを取得
            double *future_mean_ptr = future_sum[individual][k][loop_j];
            double *future_sigma_ptr = future_sigma_array[individual][k][loop_j];

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

            // ルールの品質チェック（サポートカウント、標準偏差、属性数をチェック）
            if (check_rule_quality(future_sigma_ptr, support, j2, matched_count))
            {
                if (j2 < 9 && j2 >= MIN_ATTRIBUTES)
                {
                    // 重複チェック
                    if (!check_rule_duplication(rule_candidate, state->rule_count))
                    {
                        // 新規ルールとして登録
                        register_new_rule(state, rule_candidate, time_delay_memo,
                                          future_mean_ptr, future_sigma_ptr,
                                          matched_count, negative_count[individual][k][loop_j],
                                          support, j2,
                                          individual, k, loop_j);

                        // 属性数別カウントを更新
                        rules_by_attribute_count[j2]++;

                        // 適応度を更新（新規ルールボーナス付き、t+1のσを使用）
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (future_sigma_ptr[0] + FITNESS_SIGMA_OFFSET) +
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
                        // 重複ルールの場合（新規ボーナスなし、t+1のσを使用）
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (future_sigma_ptr[0] + FITNESS_SIGMA_OFFSET);
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
 * 汎用ルーレット選択
 * 使用頻度配列に基づいてルーレット選択を行う
 * @param usage_array 使用頻度配列
 * @param array_size 配列サイズ
 * @param total_usage 総使用回数
 * @param default_value 履歴がない場合のデフォルト値（フォールバックにも使用）
 * @return 選択されたインデックス
 */
static int roulette_wheel_selection(int *usage_array, int array_size, int total_usage, int default_value)
{
    int random_value, accumulated, i;

    // 使用履歴がない場合はデフォルト値を返す
    if (total_usage <= 0)
    {
        return default_value;
    }

    // ルーレット選択
    random_value = rand() % total_usage;
    accumulated = 0;

    for (i = 0; i < array_size; i++)
    {
        accumulated += usage_array[i];
        if (random_value < accumulated)
        {
            return i;
        }
    }

    // フォールバック（念のため）
    return default_value;
}

/**
 * ルーレット選択による遅延選択
 * 使用頻度に比例した確率で遅延を選択
 * @param total_usage 総使用回数
 * @return 選択された遅延値
 */
int roulette_wheel_selection_delay(int total_usage)
{
    // 使用履歴がない場合はランダム
    if (total_usage <= 0)
    {
        return rand() % (MAX_TIME_DELAY + 1);
    }

    // 汎用ルーレット選択を使用
    return roulette_wheel_selection(delay_tracking, MAX_TIME_DELAY + 1, total_usage, MAX_TIME_DELAY);
}

/**
 * ルーレット選択による属性選択
 * 使用頻度に比例した確率で属性を選択
 * @param total_usage 総使用回数
 * @return 選択された属性ID
 */
int roulette_wheel_selection_attribute(int total_usage)
{
    // 使用履歴がない場合はランダム
    if (total_usage <= 0)
    {
        return rand() % Nzk;
    }

    // 汎用ルーレット選択を使用
    return roulette_wheel_selection(attribute_tracking, Nzk, total_usage, Nzk - 1);
}

/**
 * 指定範囲の個体に対して遅延突然変異を適用
 * @param start_idx 開始個体インデックス
 * @param end_idx 終了個体インデックス
 * @param total_delay_usage 総遅延使用回数
 */
static void apply_delay_mutation_to_range(int start_idx, int end_idx, int total_delay_usage)
{
    int i, j;
    for (i = start_idx; i < end_idx; i++)
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
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY + 1);
                }
            }
        }
    }
}

/**
 * 適応的遅延突然変異
 * 良い遅延を優先的に選択する突然変異
 */
void perform_adaptive_delay_mutation()
{
    int total_delay_usage = 0;
    int i;

    // 総使用回数を計算
    for (i = 0; i <= MAX_TIME_DELAY; i++)
    {
        total_delay_usage += delay_tracking[i];
    }

    // 個体40-79に対して
    apply_delay_mutation_to_range(MUTATION_START_40, MUTATION_START_80, total_delay_usage);

    // 個体80-119に対して（より積極的な突然変異）
    apply_delay_mutation_to_range(MUTATION_START_80, Nkotai, total_delay_usage);
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
    int i, j;

    // 過去世代の累積を計算
    for (i = 0; i <= MAX_TIME_DELAY; i++)
    {
        delay_tracking[i] = 0;

        // 過去HISTORY_GENERATIONS世代分を累積
        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_tracking[i] += delay_usage_history[j][i];
        }
    }

    // 履歴をシフト（古い世代を削除）
    for (i = 0; i <= MAX_TIME_DELAY; i++)
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
   ファイル入出力系関数

   ルール、統計情報、レポートをファイルに出力します。
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
    else
    {
        fprintf(stderr, "Warning: Could not create file: %s\n", state->filename_rule);
    }

    // レポートファイルの作成
    file = fopen(state->filename_report, "w");
    if (file != NULL)
    {
        fprintf(file, "Generation\tRules\tHighSup\tLowVar\tAvgFitness\tMin%dAttr\n", MIN_ATTRIBUTES);
        fclose(file);
    }
    else
    {
        fprintf(stderr, "Warning: Could not create file: %s\n", state->filename_report);
    }

    // ローカル詳細ファイルの作成
    file = fopen(state->filename_local, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file, "X_mean\tX_sigma\tsupport_count\tsupport_rate\tHighSup\tLowVar\tNumAttr\n");
        fclose(file);
    }
    else
    {
        fprintf(stderr, "Warning: Could not create file: %s\n", state->filename_local);
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
    {
        fprintf(stderr, "Warning: Could not open file for writing: %s\n", state->filename_rule);
        return;
    }

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
    {
        fprintf(stderr, "Warning: Could not open file for writing: %s\n", state->filename_report);
        return;
    }

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
        for (i = 0; i <= MAX_TIME_DELAY; i++)
        {
            printf("t-%d:%d ", i, delay_tracking[i]);
        }
        printf("\n");
    }
}

/**
 * ローカル詳細ファイルに出力
 * 詳細情報を出力
 * @param state 試行状態
 */
void write_local_output(struct trial_state *state)
{
    FILE *file = fopen(state->filename_local, "a");
    int i, j;

    if (file == NULL)
    {
        fprintf(stderr, "Warning: Could not open file for writing: %s\n", state->filename_local);
        return;
    }

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

        // 未来予測統計（t+1, t+2, ..., t+FUTURE_SPAN）
        for (int k = 0; k < FUTURE_SPAN; k++)
        {
            fprintf(file, "%8.3f\t%5.3f\t",
                    rule_pool[i].future_mean[k],
                    rule_pool[i].future_sigma[k]);
        }

        // その他の統計
        fprintf(file, "%d\t%6.4f\t%2d\t%2d\t",
                rule_pool[i].support_count,
                rule_pool[i].support_rate,
                rule_pool[i].high_support_flag,
                rule_pool[i].low_variance_flag);

        // 属性数
        fprintf(file, "%d\n",
                rule_pool[i].num_attributes);
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

// calculate_and_write_extreme_scores() 関数は削除されました（不要な指標のため）

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
        // ヘッダー行（属性列）
        fprintf(file_a, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");

        // 未来予測統計列（動的生成）
        for (int k = 1; k <= FUTURE_SPAN; k++)
        {
            fprintf(file_a, "X(t+%d)_mean\tX(t+%d)_sigma\t", k, k);
        }

        // その他の統計列
        fprintf(file_a, "support_count\tsupport_rate\tNegative\tHighSup\tLowVar\tNumAttr\n");

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

            // 未来予測統計（t+1, t+2, ..., t+FUTURE_SPAN）
            for (int k = 0; k < FUTURE_SPAN; k++)
            {
                fprintf(file_a, "%8.3f\t%5.3f\t",
                        global_rule_pool[i].future_mean[k],
                        global_rule_pool[i].future_sigma[k]);
            }

            // その他の統計
            fprintf(file_a, "%d\t%6.4f\t%d\t%d\t%d\t",
                    global_rule_pool[i].support_count, global_rule_pool[i].support_rate,
                    global_rule_pool[i].negative_count,
                    global_rule_pool[i].high_support_flag, global_rule_pool[i].low_variance_flag);

            // 属性数
            fprintf(file_a, "%d\n",
                    global_rule_pool[i].num_attributes);
        }

        fclose(file_a);
    }
    else
    {
        fprintf(stderr, "Warning: Could not create file: %s\n", pool_file_a);
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

            // 未来予測統計を出力（t+1, t+2, ..., t+FUTURE_SPAN）
            for (int k = 0; k < FUTURE_SPAN; k++)
            {
                fprintf(file_b, "   => X(t+%d): %.3f±%.3f\n",
                        k + 1,
                        global_rule_pool[i].future_mean[k],
                        global_rule_pool[i].future_sigma[k]);
            }
            fprintf(file_b, "      Support: n=%d\n", global_rule_pool[i].support_count);
            fprintf(file_b, "      Note: For detailed verification data, see verification CSV\n");
        }

        fclose(file_b);
    }
    else
    {
        fprintf(stderr, "Warning: Could not create file: %s\n", pool_file_b);
    }
}

/**
 * 未来の特定時点の変化率を取得
 * @param row_idx マッチした行のインデックス
 * @param offset 未来のオフセット（1=t+1, 2=t+2, 3=t+3, ...）
 * @return 未来時点のX値（範囲外の場合はNAN）
 */
double get_future_value(int row_idx, int offset)
{
    int future_idx = row_idx + offset;

    // データ範囲チェック
    if (future_idx >= Nrd)
    {
        return NAN; // データ終端を超えた
    }

    // 未来時点のX値を返す
    return x_buffer[future_idx];
}

/**
 * 個別ルールの検証ファイルを出力（CSV形式、全データ、タイムスタンプ付き）
 * @param rule_idx ルールのインデックス
 */
void write_rule_verification_csv(int rule_idx)
{
    char csv_file[512];
    snprintf(csv_file, sizeof(csv_file), "%s/rule_%03d.csv", output_dir_verification, rule_idx + 1);

    FILE *file = fopen(csv_file, "w");
    if (file == NULL)
    {
        fprintf(stderr, "Warning: Could not create CSV file: %s\n", csv_file);
        return;
    }

    struct temporal_rule *rule = &global_rule_pool[rule_idx];

    // CSVヘッダー（列順: RowIndex,Timestamp,Attr1,...,X(t+1),X(t+2),...）
    fprintf(file, "RowIndex,Timestamp");
    for (int j = 0; j < MAX_ATTRIBUTES; j++)
    {
        int attr = rule->antecedent_attrs[j];
        if (attr > 0)
        {
            fprintf(file, ",Attr%d_Col%d_t-%d", j + 1, attr - 1, rule->time_delays[j]);
        }
    }
    // 未来予測列のヘッダー（FUTURE_SPAN個）
    for (int k = 1; k <= FUTURE_SPAN; k++)
    {
        fprintf(file, ",X(t+%d)", k);
    }
    fprintf(file, "\n");

    // 全マッチインスタンスを出力
    for (int i = 0; i < rule->matched_count_vis; i++)
    {
        int row_idx = rule->matched_indices[i];
        fprintf(file, "%d,%s",
                row_idx,
                timestamp_buffer[row_idx]);

        // 各属性の値を出力（属性名(日付) または 0）
        for (int j = 0; j < MAX_ATTRIBUTES; j++)
        {
            int attr = rule->antecedent_attrs[j];
            if (attr > 0)
            {
                int delay = rule->time_delays[j];
                int data_idx = row_idx - delay;
                if (data_idx >= 0 && data_idx < Nrd)
                {
                    if (data_buffer[data_idx][attr - 1] == 1)
                    {
                        // マッチしている場合: 属性名(日付)
                        fprintf(file, ",%s(%s)", attribute_dictionary[attr - 1], timestamp_buffer[data_idx]);
                    }
                    else
                    {
                        // マッチしていない場合: 0
                        fprintf(file, ",0");
                    }
                }
                else
                {
                    fprintf(file, ",-");
                }
            }
        }

        // 未来予測値を個別に出力（t+1, t+2, ..., t+FUTURE_SPAN）
        for (int k = 1; k <= FUTURE_SPAN; k++)
        {
            double future_val = get_future_value(row_idx, k);
            if (isnan(future_val))
            {
                fprintf(file, ",-"); // データ範囲外
            }
            else
            {
                fprintf(file, ",%.2f", future_val);
            }
        }
        fprintf(file, "\n");
    }

    fclose(file);
}

/**
 * 全ルールの検証ファイルを出力（最初の10ルールのみ、CSV形式）
 */
void write_verification_files()
{
    printf("\n========== Writing Rule Verification Files ==========\n");

    // 最初の10ルールのみ検証ファイルを出力（CSV形式）
    int output_count = (global_rule_count < 10) ? global_rule_count : 10;
    for (int i = 0; i < output_count; i++)
    {
        write_rule_verification_csv(i);
    }

    printf("  Generated %d rule verification CSV files in: %s/\n", output_count, output_dir_verification);
    printf("====================================================\n\n");
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
                global_rule_count,                  // 累積ルール数（グローバルプール）
                global_high_support_count,          // 累積高サポート数（グローバルプール）
                global_low_variance_count,          // 累積低分散数（グローバルプール）
                rules_by_min_attributes,            // 最小属性数を満たすルール数
                state->elapsed_time);               // 経過時間（秒）
        fclose(file);
    }
    else
    {
        fprintf(stderr, "Warning: Could not open file for writing: %s\n", cont_file);
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
    else
    {
        fprintf(stderr, "Warning: Could not create file: %s\n", cont_file);
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
    printf("GNMiner Time-Series Edition Complete!\n");
    printf("\n");
    printf("========================================\n");

    /* 時系列設定のサマリ */
    printf("Time-Series Configuration:\n");
    printf("  Mode: Temporal Pattern Analysis\n");
    printf("  Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
    printf("  Delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY);
    printf("  Prediction span: t+%d\n", PREDICTION_SPAN);
    printf("  Minimum attributes: %d\n", MIN_ATTRIBUTES);
    printf("========================================\n");

    /* データ統計 */
    printf("Data Statistics:\n");
    printf("  Records: %d\n", Nrd);
    printf("  Attributes: %d\n", Nzk);
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
    printf("├── IB/       %d backup files\n", Ntry);
    printf("├── pool/     Global rule pools\n");
    printf("├── doc/      Documentation\n");
    printf("└── vis/      Visualization data\n");

    /* 総合統計 */
    printf("\nTotal Rules Discovered: %d\n", total_rule_count);
    printf("High-Support Rules: %d\n", total_high_support);
    printf("Low-Variance Rules: %d\n", total_low_variance);
    printf("Rules with %d+ attributes: %d\n", MIN_ATTRIBUTES, rules_by_min_attributes);
    printf("========================================\n");
}

/* ================================================================================
   パス設定関数

   銘柄コードに基づいて、入力ファイルと出力ディレクトリのパスを設定します。
   ================================================================================
*/

/**
 * 銘柄コードに基づいてファイルパスとディレクトリを設定
 * @param code 銘柄コード（例: "7203"）
 */
void setup_paths_for_stock(const char *code)
{
    // 銘柄コードをコピー
    strncpy(stock_code, code, sizeof(stock_code) - 1);
    stock_code[sizeof(stock_code) - 1] = '\0';

    // データファイルパスを設定
    snprintf(data_file_path, sizeof(data_file_path),
             "crypto_data/gnminer_individual/%s.txt", stock_code);

    // 出力ベースディレクトリを設定（暗号通貨は crypto_data/output/ 配下）
    snprintf(output_base_dir, sizeof(output_base_dir),
             "crypto_data/output/%s", stock_code);

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
    snprintf(output_dir_verification, sizeof(output_dir_verification),
             "%s/verification", output_base_dir);

    // ファイルパスを設定
    snprintf(pool_file_a, sizeof(pool_file_a),
             "%s/pool/zrp01a.txt", output_base_dir);
    snprintf(pool_file_b, sizeof(pool_file_b),
             "%s/pool/zrp01b.txt", output_base_dir);
    snprintf(cont_file, sizeof(cont_file),
             "%s/doc/zrd01.txt", output_base_dir);

    printf("\n=== Path Configuration ===\n");
    printf("Stock Code: %s\n", stock_code);
    printf("Input File: %s\n", data_file_path);
    printf("Output Dir: %s\n", output_base_dir);
    printf("=========================\n\n");
}

/* ================================================================================
   単一銘柄処理関数

   指定された銘柄に対して全試行を実行します。
   ================================================================================
*/

/**
 * 単一銘柄の分析を実行
 * @param code 銘柄コード
 * @return 成功時0、失敗時1
 */
int process_single_stock(const char *code)
{
    int trial, gen;
    struct trial_state state;
    clock_t stock_start_time, stock_end_time;

    // 銘柄の処理開始時刻を記録
    stock_start_time = clock();

    printf("\n");
    printf("##################################################\n");
    printf("##  Processing: %s\n", code);
    printf("##################################################\n");

    // パス設定
    setup_paths_for_stock(code);

    // 出力ディレクトリ構造を作成
    create_output_directories();

    // CSVファイルからデータとヘッダーを読み込み
    if (load_csv_with_header() != 0)
    {
        // ファイルが存在しない場合はスキップ
        printf("Skipping stock code %s (data file not found)\n\n", code);
        return 1; // 失敗を返す（次の銘柄へ）
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

    // 全試行を統合したグローバルプールを出力（極値スコア付き）
    printf("\n========== Writing Global Rule Pool ==========\n");
    printf("  Total integrated rules: %d\n", global_rule_count);
    printf("  High-support rules: %d\n", global_high_support_count);
    printf("  Low-variance rules: %d\n", global_low_variance_count);
    write_global_pool(&state);

    // ルール検証ファイルを出力
    write_verification_files();

    // 全試行の統計を表示
    print_final_statistics();

    // メモリ解放
    free_dynamic_memory();

    // 銘柄の処理終了時刻を記録
    stock_end_time = clock();
    double stock_elapsed = (double)(stock_end_time - stock_start_time) / CLOCKS_PER_SEC;

    printf("\n");
    printf("##################################################\n");
    printf("##  Stock Code %s Completed (%.2fs)\n", code, stock_elapsed);
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
    clock_t batch_start_time, batch_end_time;

    /* ========== コマンドライン引数処理 ========== */

    // 使用方法を表示
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        printf("==============================================\n");
        printf("  GNMiner - Rule Discovery\n");
        printf("==============================================\n\n");
        printf("Usage: %s <stock_code>\n\n", argv[0]);
        printf("Arguments:\n");
        printf("  <stock_code>  Nikkei 225 stock code (e.g., 7203, 9984)\n\n");
        printf("Examples:\n");
        printf("  %s 7203  # Discover rules for Toyota (7203)\n", argv[0]);
        printf("  %s 9984  # Discover rules for SoftBank (9984)\n", argv[0]);
        printf("  %s BTC-USD  # Discover rules for Bitcoin\n\n", argv[0]);
        printf("Note:\n");
        printf("  - Data files must exist: crypto_data/gnminer_individual/{CODE}.txt\n");
        printf("  - Results will be saved to: output/{CODE}/\n");
        printf("  - Use Makefile: make test, make run-crypto, make run\n\n");
        printf("==============================================\n");
        return 0;
    }

    // 引数チェック
    if (argc < 2)
    {
        printf("\nERROR: Insufficient arguments\n\n");
        printf("Usage: %s <stock_code>\n\n", argv[0]);
        printf("  <stock_code>: Nikkei 225 stock code (e.g., 7203, 9984)\n\n");
        printf("Examples:\n");
        printf("  %s 7203\n", argv[0]);
        printf("  %s 9984\n\n", argv[0]);
        printf("For help: %s --help\n\n", argv[0]);
        return 1;
    }

    // 乱数シードを固定（再現性確保）
    srand(1);

    // コマンドライン引数から銘柄コードを取得
    const char *stock_code_arg = argv[1];

    // バッチ処理開始
    batch_start_time = clock();

    // 処理開始
    printf("\n");
    printf("==================================================\n");
    printf("  GNMiner - Rule Discovery\n");
    printf("==================================================\n");
    printf("Stock Code: %s\n", stock_code_arg);
    printf("==================================================\n");
    printf("\n");

    // 処理実行
    int result = process_single_stock(stock_code_arg);

    /* ========== 処理終了 ========== */

    // 処理終了時刻を記録
    batch_end_time = clock();
    double total_elapsed = (double)(batch_end_time - batch_start_time) / CLOCKS_PER_SEC;

    // 最終サマリーを表示
    printf("\n");
    printf("==================================================\n");
    printf("  Processing Complete\n");
    printf("==================================================\n");
    printf("Stock Code: %s\n", stock_code_arg);
    printf("Total Time: %.2fs (%.1fm)\n", total_elapsed, total_elapsed / 60.0);
    printf("Status:     %s\n", result == 0 ? "SUCCESS" : "FAILED");
    printf("==================================================\n");
    printf("\n");

    return result; // 成功時0、失敗時1
}
