#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   GNP-based Time-Series Association Rule Mining System
   時系列対応版GNMiner - Phase 2実装

   【Phase 2の変更点】
   - 各判定ノードの時間遅延を0〜3でランダム初期化
   - 時間遅延の突然変異を有効化
   - 実際の過去データを参照する評価実装
   - 進化による最適な遅延パターンの発見

   【時系列ルール形式】
   A1(t-d1) ∧ A2(t-d2) ∧ ... → (X,Y)(t+1)
   where di ∈ {0,1,2,3} は各属性の時間遅延
   ================================================================================ */

/* ================================================================================
   時系列パラメータ定義
   ================================================================================ */
#define TIMESERIES_MODE 1       /* 時系列モード有効化フラグ */
#define MAX_TIME_DELAY 10       /* 最大時間遅延（将来用） */
#define MAX_TIME_DELAY_PHASE2 3 /* Phase 2での最大遅延値 */
#define MIN_TIME_DELAY 0        /* 最小時間遅延（t-0 = 現在） */
#define PREDICTION_SPAN 1       /* 予測時点（t+1） */

/* ================================================================================
   ディレクトリ構造定義
   ================================================================================ */
#define OUTPUT_DIR "output"
#define OUTPUT_DIR_IL "output/IL"
#define OUTPUT_DIR_IA "output/IA"
#define OUTPUT_DIR_IB "output/IB"
#define OUTPUT_DIR_POOL "output/pool"
#define OUTPUT_DIR_DOC "output/doc"

/* ================================================================================
   ファイル定義セクション
   ================================================================================ */
#define DATANAME "testdata1s.txt"
#define DICNAME "testdic1.txt"
#define fpoola "output/pool/zrp01a.txt"
#define fpoolb "output/pool/zrp01b.txt"
#define fcont "output/doc/zrd01.txt"
#define RESULTNAME "output/doc/zrmemo01.txt"

/* ================================================================================
   データ構造パラメータ定義
   ================================================================================ */
#define Nrd 159 /* 総レコード数 */
#define Nzk 95  /* 前件部用の属性数 */

/* ================================================================================
   ルールマイニング制約パラメータ
   ================================================================================ */
#define Nrulemax 2002
#define Minsup 0.04
#define Maxsigx 0.5
#define Maxsigy 0.5

/* ================================================================================
   実験実行パラメータ
   ================================================================================ */
#define Nstart 1000
#define Ntry 100

/* ================================================================================
   GNPパラメータ
   ================================================================================ */
#define Generation 201
#define Nkotai 120
#define Npn 10
#define Njg 100
#define Nmx 7
#define Muratep 1
#define Muratej 6
#define Muratea 6
#define Muratedelay 6 /* 時間遅延の突然変異率（Phase 2で有効） */
#define Nkousa 20

/* ================================================================================
   ディレクトリ作成関数
   ================================================================================ */
void create_directories()
{
    mkdir(OUTPUT_DIR, 0755);
    mkdir(OUTPUT_DIR_IL, 0755);
    mkdir(OUTPUT_DIR_IA, 0755);
    mkdir(OUTPUT_DIR_IB, 0755);
    mkdir(OUTPUT_DIR_POOL, 0755);
    mkdir(OUTPUT_DIR_DOC, 0755);

    printf("=== Directory Structure Created ===\n");
    printf("output/\n");
    printf("├── IL/        (Rule Lists)\n");
    printf("├── IA/        (Analysis Reports)\n");
    printf("├── IB/        (Backup Files)\n");
    printf("├── pool/      (Global Rule Pool)\n");
    printf("└── doc/       (Documentation)\n");
    printf("===================================\n");

    /* 時系列モードの通知 */
    if (TIMESERIES_MODE)
    {
        printf("*** TIME-SERIES MODE ENABLED (Phase 2) ***\n");
        printf("Random delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE2);
        printf("Prediction span: t+%d\n", PREDICTION_SPAN);
        printf("=========================================\n\n");
    }
}

int main()
{
    /* ================================================================================
       ルール構造体定義（時系列対応版）
       ================================================================================ */
    struct asrule
    {
        /* 前件部の属性ID */
        int antecedent_attr0;
        int antecedent_attr1;
        int antecedent_attr2;
        int antecedent_attr3;
        int antecedent_attr4;
        int antecedent_attr5;
        int antecedent_attr6;
        int antecedent_attr7;

        /* 各属性の時間遅延 */
        int time_delay0;
        int time_delay1;
        int time_delay2;
        int time_delay3;
        int time_delay4;
        int time_delay5;
        int time_delay6;
        int time_delay7;

        /* 後件部の統計情報 */
        double x_mean;
        double x_sigma;
        double y_mean;
        double y_sigma;

        /* 評価指標 */
        int support_count;
        int negative_count;
        int high_support_flag;
        int low_variance_flag;
    };
    struct asrule rule_pool[Nrulemax];

    /* ================================================================================
       比較用ルール構造体
       ================================================================================ */
    struct cmrule
    {
        int antecedent_attr0;
        int antecedent_attr1;
        int antecedent_attr2;
        int antecedent_attr3;
        int antecedent_attr4;
        int antecedent_attr5;
        int antecedent_attr6;
        int antecedent_attr7;
        /* Phase 2では時間遅延も比較対象に含める可能性あり */
    };
    struct cmrule compare_rules[Nrulemax];

    /* ================================================================================
       作業用変数群の宣言
       ================================================================================ */
    int work_array[5];
    int input_value;
    int rule_count;
    int rule_count2;
    int high_support_rule_count;
    int low_variance_rule_count;

    int i, j, k;
    int loop_i, loop_j, loop_k;
    int i2, j2, k2;
    int i3, j3, k3;
    int i4, j4, k4;
    int i5, j5;
    int i6, j6;
    int i7, j7, k7, k8;
    int attr_index;

    int current_node_id;
    int loop_trial;
    int generation_id;
    int individual_id;

    int matched_antecedent_count;
    int process_id;
    int new_flag;
    int same_flag1, same_flag2;
    int match_flag;

    int mutation_random;
    int mutation_attr1, mutation_attr2, mutation_attr3;
    int total_attribute_usage;
    int difference_count;

    int crossover_parent;
    int crossover_node1, crossover_node2;
    int temp_memory;
    int save_memory;
    int node_memory;
    int node_save1, node_save2;

    int backup_counter2;
    int high_support_marker;
    int low_variance_marker;

    double input_x, input_y;
    double threshold_x, threshold_sigma_x;
    double threshold_y, threshold_sigma_y;

    int total_rule_count;
    int new_rule_count;
    int total_high_support;
    int total_low_variance;
    int new_high_support;
    int new_low_variance;

    int file_index;
    int file_digit;
    int trial_index;
    int compare_trial_index;
    int file_digit1, file_digit2, file_digit3, file_digit4, file_digit5;
    int compare_digit1, compare_digit2, compare_digit3, compare_digit4, compare_digit5;

    double confidence_max;
    double fitness_average_all;
    double support_value;
    double sigma_x, sigma_y;
    double current_x_value, current_y_value;
    double future_x_value, future_y_value; /* ★追加: 未来時点のX,Y */

    /* 時系列用変数 */
    int time_delay;       /* 現在の時間遅延 */
    int data_index;       /* 時間遅延を考慮したデータインデックス */
    int safe_start_index; /* 安全な開始インデックス */
    int safe_end_index;   /* 安全な終了インデックス */
    int actual_delay;     /* 実際に使用する遅延値 */

    /* ================================================================================
       データバッファ（Phase 2新規追加）
       全データを事前読み込みして時間参照を可能にする
       ================================================================================ */
    static int data_buffer[Nrd][Nzk]; /* 全レコードの属性値バッファ */
    static double x_buffer[Nrd];      /* 全レコードのX値バッファ */
    static double y_buffer[Nrd];      /* 全レコードのY値バッファ */

    /* ================================================================================
       大規模静的配列の宣言（時系列対応版）
       ================================================================================ */

    /* ノード構造に時間遅延フィールドを追加 */
    static int node_structure[Nkotai][Npn + Njg][3];
    /* [0]: 属性ID, [1]: 接続先, [2]: 時間遅延 */

    static int record_attributes[Nzk + 2];

    /* 統計情報配列群 */
    static int match_count[Nkotai][Npn][10];
    static int negative_count[Nkotai][Npn][10];
    static int evaluation_count[Nkotai][Npn][10];
    static int attribute_chain[Nkotai][Npn][10];

    /* 時間遅延連鎖の記録 */
    static int time_delay_chain[Nkotai][Npn][10];

    static double x_sum[Nkotai][Npn][10];
    static double x_sigma_array[Nkotai][Npn][10];
    static double y_sum[Nkotai][Npn][10];
    static double y_sigma_array[Nkotai][Npn][10];

    int rules_per_trial[Ntry];
    int attribute_usage_history[5][Nzk];
    int attribute_usage_count[Nzk];
    int attribute_tracking[Nzk];

    double fitness_value[Nkotai];
    int fitness_ranking[Nkotai];
    int new_rule_flag[Nkotai];

    /* GNP遺伝子プール */
    static int gene_connection[Nkotai][Npn + Njg];
    static int gene_attribute[Nkotai][Npn + Njg];

    /* 時間遅延遺伝子 */
    static int gene_time_delay[Nkotai][Npn + Njg];

    int selected_attributes[10];
    int read_attributes[10];
    int rules_by_attribute_count[10];
    int attribute_set[Nzk + 1];

    int rule_candidate_pre[10];
    int rule_candidate[10];
    int rule_memo[10];

    /* 時間遅延候補 */
    int time_delay_candidate[10];
    int time_delay_memo[10]; /* ★追加: 実際の遅延値を記憶 */

    int new_rule_marker[Nrulemax];

    /* ファイル名文字列配列 */
    char filename_rule[256];
    char filename_report[256];
    char filename_local[256];

    char attribute_name_buffer[31];
    char attribute_dictionary[Nzk + 3][31];

    /* ファイルポインタ */
    FILE *file_data1;
    FILE *file_data2;
    FILE *file_dictionary;
    FILE *file_rule;
    FILE *file_report;
    FILE *file_compare;
    FILE *file_local;
    FILE *file_pool_attribute;
    FILE *file_pool_numeric;
    FILE *file_document;

    /* ================================================================================
       メイン処理開始
       ================================================================================ */

    create_directories();
    srand(1);

    /* ================================================================================
       初期化フェーズ1: 属性辞書の読み込み
       ================================================================================ */
    for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
    {
        strcpy(attribute_dictionary[attr_index], "\0");
    }

    file_dictionary = fopen(DICNAME, "r");
    if (file_dictionary == NULL)
    {
        printf("Warning: Cannot open dictionary file: %s\n", DICNAME);
        printf("Using attribute numbers instead of names.\n");
    }
    else
    {
        for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
        {
            if (fscanf(file_dictionary, "%d%s", &process_id, attribute_name_buffer) == 2)
            {
                strcpy(attribute_dictionary[attr_index], attribute_name_buffer);
            }
            else
            {
                sprintf(attribute_dictionary[attr_index], "Attr%d", attr_index);
            }
        }
        fclose(file_dictionary);
        printf("Dictionary loaded: %d attributes\n", Nzk);
    }

    /* ================================================================================
       ★Phase 2新規: データの事前読み込み
       全データをバッファに格納して時間参照を可能にする
       ================================================================================ */
    printf("Loading data into buffer for time-series processing...\n");
    file_data2 = fopen(DATANAME, "r");
    if (file_data2 == NULL)
    {
        printf("Error: Cannot open data file: %s\n", DATANAME);
        return 0;
    }

    for (i = 0; i < Nrd; i++)
    {
        for (j = 0; j < Nzk; j++)
        {
            fscanf(file_data2, "%d", &input_value);
            data_buffer[i][j] = input_value;
        }
        fscanf(file_data2, "%lf%lf", &input_x, &input_y);
        x_buffer[i] = input_x;
        y_buffer[i] = input_y;
    }
    fclose(file_data2);
    printf("Data loaded: %d records with %d attributes each\n", Nrd, Nzk);

    /* ================================================================================
       初期化フェーズ2: グローバルカウンタ初期化
       ================================================================================ */
    total_rule_count = 0;
    total_high_support = 0;
    total_low_variance = 0;

    file_document = fopen(fcont, "w");
    if (file_document == NULL)
    {
        printf("Error: Cannot create document file: %s\n", fcont);
        return 0;
    }
    fprintf(file_document, "Trial\tRules\tHighSup\tLowVar\tTotal\tTotalHigh\tTotalLow\n");
    fclose(file_document);
    printf("Document file initialized: %s\n", fcont);

    /* ================================================================================
       ★★★ メイン試行ループ開始（時系列対応版Phase 2） ★★★
       ================================================================================ */
    for (trial_index = Nstart; trial_index < (Nstart + Ntry); trial_index++)
    {
        printf("\n========== Trial %d/%d Started ==========\n",
               trial_index - Nstart + 1, Ntry);

        if (TIMESERIES_MODE)
        {
            printf("  [Time-Series Mode Phase 2: Random delays 0-%d]\n", MAX_TIME_DELAY_PHASE2);
        }

        /* ================================================================================
           試行別初期化フェーズ
           ================================================================================ */
        for (i = 0; i < Nrulemax; i++)
        {
            rule_pool[i].antecedent_attr0 = 0;
            rule_pool[i].antecedent_attr1 = 0;
            rule_pool[i].antecedent_attr2 = 0;
            rule_pool[i].antecedent_attr3 = 0;
            rule_pool[i].antecedent_attr4 = 0;
            rule_pool[i].antecedent_attr5 = 0;
            rule_pool[i].antecedent_attr6 = 0;
            rule_pool[i].antecedent_attr7 = 0;

            rule_pool[i].time_delay0 = 0;
            rule_pool[i].time_delay1 = 0;
            rule_pool[i].time_delay2 = 0;
            rule_pool[i].time_delay3 = 0;
            rule_pool[i].time_delay4 = 0;
            rule_pool[i].time_delay5 = 0;
            rule_pool[i].time_delay6 = 0;
            rule_pool[i].time_delay7 = 0;

            rule_pool[i].x_mean = 0;
            rule_pool[i].x_sigma = 0;
            rule_pool[i].y_mean = 0;
            rule_pool[i].y_sigma = 0;

            rule_pool[i].support_count = 0;
            rule_pool[i].negative_count = 0;
            rule_pool[i].high_support_flag = 0;
            rule_pool[i].low_variance_flag = 0;
        }

        for (i = 0; i < Nrulemax; i++)
        {
            compare_rules[i].antecedent_attr0 = 0;
            compare_rules[i].antecedent_attr1 = 0;
            compare_rules[i].antecedent_attr2 = 0;
            compare_rules[i].antecedent_attr3 = 0;
            compare_rules[i].antecedent_attr4 = 0;
            compare_rules[i].antecedent_attr5 = 0;
            compare_rules[i].antecedent_attr6 = 0;
            compare_rules[i].antecedent_attr7 = 0;
        }

        for (i = 0; i < Nrulemax; i++)
        {
            new_rule_marker[i] = 0;
        }

        /* ファイルパス生成処理 */
        file_digit5 = trial_index / 10000;
        file_digit4 = trial_index / 1000 - file_digit5 * 10;
        file_digit3 = trial_index / 100 - file_digit5 * 100 - file_digit4 * 10;
        file_digit2 = trial_index / 10 - file_digit5 * 1000 - file_digit4 * 100 - file_digit3 * 10;
        file_digit1 = trial_index - file_digit5 * 10000 - file_digit4 * 1000 - file_digit3 * 100 - file_digit2 * 10;

        sprintf(filename_rule, "%s/IL%d%d%d%d%d.txt",
                OUTPUT_DIR_IL,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        sprintf(filename_report, "%s/IA%d%d%d%d%d.txt",
                OUTPUT_DIR_IA,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        sprintf(filename_local, "%s/IB%d%d%d%d%d.txt",
                OUTPUT_DIR_IB,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        /* 試行別変数の初期化 */
        for (file_index = 0; file_index < Nzk + 1; file_index++)
        {
            attribute_set[file_index] = file_index;
        }

        rule_count = 1;
        high_support_rule_count = 1;
        low_variance_rule_count = 1;

        for (i = 0; i < 10; i++)
        {
            rules_by_attribute_count[i] = 0;
        }

        total_attribute_usage = Nzk;

        for (i = 0; i < Nzk; i++)
        {
            attribute_usage_count[i] = 0;
            attribute_tracking[i] = 0;
            attribute_usage_history[0][i] = 1;
            attribute_usage_history[1][i] = 0;
            attribute_usage_history[2][i] = 0;
            attribute_usage_history[3][i] = 0;
            attribute_usage_history[4][i] = 0;
        }

        /* ================================================================================
           GNP初期個体群の生成（時系列対応版Phase 2）
           ================================================================================ */
        for (i4 = 0; i4 < Nkotai; i4++)
        {
            for (j4 = 0; j4 < (Npn + Njg); j4++)
            {
                gene_connection[i4][j4] = rand() % Njg + Npn;
                gene_attribute[i4][j4] = rand() % Nzk;

                /* ★Phase 2: 時間遅延をランダム初期化（0〜3） */
                gene_time_delay[i4][j4] = rand() % (MAX_TIME_DELAY_PHASE2 + 1);
            }
        }

        /* 出力ファイルの初期化 */
        file_rule = fopen(filename_rule, "w");
        if (file_rule == NULL)
        {
            printf("Error: Cannot create rule file: %s\n", filename_rule);
            return 0;
        }
        fprintf(file_rule, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\n");
        fclose(file_rule);
        printf("Created: %s\n", filename_rule);

        file_report = fopen(filename_report, "w");
        if (file_report == NULL)
        {
            printf("Error: Cannot create report file: %s\n", filename_report);
            return 0;
        }
        fprintf(file_report, "Generation\tRules\tHighSup\tLowVar\tAvgFitness\n");
        fclose(file_report);

        file_local = fopen(filename_local, "w");
        if (file_local == NULL)
        {
            printf("Error: Cannot create local file: %s\n", filename_local);
            return 0;
        }
        fprintf(file_local, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tHighSup\tLowVar\n");
        fclose(file_local);

        /* ================================================================================
           ★★★ 進化計算メインループ開始（時系列対応版Phase 2） ★★★
           ================================================================================ */
        for (generation_id = 0; generation_id < Generation; generation_id++)
        {
            if (rule_count > (Nrulemax - 2))
            {
                printf("Rule limit reached. Stopping evolution.\n");
                break;
            }

            /* ================================================================================
               世代開始: GNP遺伝子を個体構造にコピー（時系列対応）
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (j4 = 0; j4 < (Npn + Njg); j4++)
                {
                    node_structure[individual_id][j4][0] = gene_attribute[individual_id][j4];
                    node_structure[individual_id][j4][1] = gene_connection[individual_id][j4];
                    node_structure[individual_id][j4][2] = gene_time_delay[individual_id][j4];
                }
            }

            /* 評価準備: 統計配列の初期化 */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                fitness_value[individual_id] = (double)individual_id * (-0.00001);
                fitness_ranking[individual_id] = 0;

                for (k = 0; k < Npn; k++)
                {
                    for (i = 0; i < 10; i++)
                    {
                        match_count[individual_id][k][i] = 0;
                        attribute_chain[individual_id][k][i] = 0;
                        time_delay_chain[individual_id][k][i] = 0;
                        negative_count[individual_id][k][i] = 0;
                        evaluation_count[individual_id][k][i] = 0;
                        x_sum[individual_id][k][i] = 0;
                        x_sigma_array[individual_id][k][i] = 0;
                        y_sum[individual_id][k][i] = 0;
                        y_sigma_array[individual_id][k][i] = 0;
                    }
                }
            }

            /* ================================================================================
               ★ データセット評価フェーズ（時系列対応版Phase 2） ★
               実際の過去データを参照する改良版
               ================================================================================ */

            /* 時系列用の安全な評価範囲を設定 */
            if (TIMESERIES_MODE)
            {
                safe_start_index = MAX_TIME_DELAY_PHASE2; /* 最大遅延分のマージン */
                safe_end_index = Nrd - PREDICTION_SPAN;   /* 未来参照のマージン */
            }
            else
            {
                safe_start_index = 0;
                safe_end_index = Nrd;
            }

            for (i = safe_start_index; i < safe_end_index; i++)
            {
                /* 現在時点(i)と未来時点(i+1)のデータを取得 */
                future_x_value = x_buffer[i + PREDICTION_SPAN];
                future_y_value = y_buffer[i + PREDICTION_SPAN];

                /* ================================================================================
                   ★★ GNPネットワーク評価の核心部分（Phase 2改良版） ★★
                   ================================================================================ */
                for (individual_id = 0; individual_id < Nkotai; individual_id++)
                {
                    for (k = 0; k < Npn; k++)
                    {
                        current_node_id = k;
                        j = 0;
                        match_flag = 1;

                        /* ルートノード（深さ0）の統計更新 */
                        match_count[individual_id][k][j] = match_count[individual_id][k][j] + 1;
                        evaluation_count[individual_id][k][j] = evaluation_count[individual_id][k][j] + 1;

                        /* 最初の判定ノードへ遷移 */
                        current_node_id = node_structure[individual_id][current_node_id][1];

                        /* 判定ノードチェーンを辿る */
                        while (current_node_id > (Npn - 1) && j < Nmx)
                        {
                            j = j + 1;

                            /* 属性を記録 */
                            attribute_chain[individual_id][k][j] =
                                node_structure[individual_id][current_node_id][0] + 1;

                            /* 時間遅延を記録 */
                            time_delay = node_structure[individual_id][current_node_id][2];
                            time_delay_chain[individual_id][k][j] = time_delay;

                            /* ★Phase 2: 時間遅延を考慮したデータインデックス計算 */
                            data_index = i - time_delay;

                            /* 過去参照の安全性チェック */
                            if (data_index < 0)
                            {
                                /* データ範囲外の場合は処理をスキップ */
                                current_node_id = k;
                                break;
                            }

                            /* ★Phase 2: 実際の過去データを参照 */
                            if (data_buffer[data_index][node_structure[individual_id][current_node_id][0]] == 1)
                            {
                                if (match_flag == 1)
                                {
                                    match_count[individual_id][k][j] =
                                        match_count[individual_id][k][j] + 1;

                                    /* ★時系列モード: 未来時点(i+PREDICTION_SPAN)のX,Y値を使用 */
                                    x_sum[individual_id][k][j] =
                                        x_sum[individual_id][k][j] + future_x_value;
                                    x_sigma_array[individual_id][k][j] =
                                        x_sigma_array[individual_id][k][j] +
                                        future_x_value * future_x_value;

                                    y_sum[individual_id][k][j] =
                                        y_sum[individual_id][k][j] + future_y_value;
                                    y_sigma_array[individual_id][k][j] =
                                        y_sigma_array[individual_id][k][j] +
                                        future_y_value * future_y_value;
                                }

                                evaluation_count[individual_id][k][j] =
                                    evaluation_count[individual_id][k][j] + 1;

                                /* Yes側へ遷移 */
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                            else if (data_buffer[data_index][node_structure[individual_id][current_node_id][0]] == 0)
                            {
                                /* No側へ遷移 */
                                current_node_id = k;
                            }
                            else
                            {
                                /* 欠損値の処理 */
                                evaluation_count[individual_id][k][j] =
                                    evaluation_count[individual_id][k][j] + 1;

                                match_flag = 0;
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                        }
                    }
                }
            }

            /* 統計後処理1: 負例数の計算 */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (k = 0; k < Npn; k++)
                {
                    for (i = 0; i < Nmx; i++)
                    {
                        negative_count[individual_id][k][i] =
                            match_count[individual_id][k][0] - evaluation_count[individual_id][k][i] + match_count[individual_id][k][i];
                    }
                }
            }

            /* 統計後処理2: 平均と標準偏差の計算 */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (k = 0; k < Npn; k++)
                {
                    for (j6 = 1; j6 < 9; j6++)
                    {
                        if (match_count[individual_id][k][j6] != 0)
                        {
                            x_sum[individual_id][k][j6] =
                                x_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];

                            x_sigma_array[individual_id][k][j6] =
                                x_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - x_sum[individual_id][k][j6] * x_sum[individual_id][k][j6];

                            y_sum[individual_id][k][j6] =
                                y_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];

                            y_sigma_array[individual_id][k][j6] =
                                y_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - y_sum[individual_id][k][j6] * y_sum[individual_id][k][j6];

                            if (x_sigma_array[individual_id][k][j6] < 0)
                            {
                                x_sigma_array[individual_id][k][j6] = 0;
                            }
                            if (y_sigma_array[individual_id][k][j6] < 0)
                            {
                                y_sigma_array[individual_id][k][j6] = 0;
                            }

                            x_sigma_array[individual_id][k][j6] = sqrt(x_sigma_array[individual_id][k][j6]);
                            y_sigma_array[individual_id][k][j6] = sqrt(y_sigma_array[individual_id][k][j6]);
                        }
                    }
                }
            }

            /* ================================================================================
               ★★★ ルール抽出フェーズ（時系列対応版Phase 2） ★★★
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                if (rule_count > (Nrulemax - 2))
                {
                    break;
                }

                for (k = 0; k < Npn; k++)
                {
                    if (rule_count > (Nrulemax - 2))
                    {
                        break;
                    }

                    for (i = 0; i < 10; i++)
                    {
                        rule_candidate_pre[i] = 0;
                        rule_candidate[i] = 0;
                        rule_memo[i] = 0;
                        time_delay_candidate[i] = 0;
                        time_delay_memo[i] = 0; /* ★初期化追加 */
                    }

                    for (loop_j = 1; loop_j < 9; loop_j++)
                    {
                        threshold_x = x_sum[individual_id][k][loop_j];
                        threshold_sigma_x = Maxsigx;

                        threshold_y = y_sum[individual_id][k][loop_j];
                        threshold_sigma_y = Maxsigy;

                        matched_antecedent_count = match_count[individual_id][k][loop_j];

                        support_value = 0;
                        sigma_x = x_sigma_array[individual_id][k][loop_j];
                        sigma_y = y_sigma_array[individual_id][k][loop_j];

                        if (negative_count[individual_id][k][loop_j] != 0)
                        {
                            support_value = (double)matched_antecedent_count /
                                            (double)negative_count[individual_id][k][loop_j];
                        }

                        if (sigma_x <= threshold_sigma_x &&
                            sigma_y <= threshold_sigma_y &&
                            support_value >= Minsup)
                        {
                            /* ルール前件部の構築 */
                            for (i2 = 1; i2 < 9; i2++)
                            {
                                rule_candidate_pre[i2 - 1] = attribute_chain[individual_id][k][i2];
                                /* ★Phase 2: 実際の時間遅延を取得 */
                                time_delay_candidate[i2 - 1] = time_delay_chain[individual_id][k][i2];
                            }

                            j2 = 0;
                            for (k2 = 1; k2 < (1 + Nzk); k2++)
                            {
                                new_flag = 0;
                                for (i2 = 0; i2 < loop_j; i2++)
                                {
                                    if (rule_candidate_pre[i2] == k2)
                                    {
                                        new_flag = 1;
                                        /* ★Phase 2: 対応する時間遅延も保存 */
                                        time_delay_memo[j2] = time_delay_candidate[i2];
                                    }
                                }
                                if (new_flag == 1)
                                {
                                    rule_candidate[j2] = k2;
                                    rule_memo[j2] = k2;
                                    j2 = j2 + 1;
                                }
                            }

                            if (j2 < 9)
                            {
                                /* 新規性チェック */
                                new_flag = 0;

                                for (i2 = 0; i2 < rule_count; i2++)
                                {
                                    same_flag1 = 1;

                                    if (rule_pool[i2].antecedent_attr0 == rule_candidate[0] &&
                                        rule_pool[i2].antecedent_attr1 == rule_candidate[1] &&
                                        rule_pool[i2].antecedent_attr2 == rule_candidate[2] &&
                                        rule_pool[i2].antecedent_attr3 == rule_candidate[3] &&
                                        rule_pool[i2].antecedent_attr4 == rule_candidate[4] &&
                                        rule_pool[i2].antecedent_attr5 == rule_candidate[5] &&
                                        rule_pool[i2].antecedent_attr6 == rule_candidate[6] &&
                                        rule_pool[i2].antecedent_attr7 == rule_candidate[7])
                                    {
                                        /* Phase 2では時間遅延の違いは許容（属性の組み合わせのみチェック） */
                                        same_flag1 = 0;
                                    }

                                    if (same_flag1 == 0)
                                    {
                                        new_flag = 1;

                                        fitness_value[individual_id] =
                                            fitness_value[individual_id] + j2 + support_value * 10 + 2 / (sigma_x + 0.1) + 2 / (sigma_y + 0.1);
                                    }

                                    if (new_flag == 1)
                                    {
                                        break;
                                    }
                                }

                                if (new_flag == 0)
                                {
                                    /* 新規ルールの登録 */
                                    rule_pool[rule_count].antecedent_attr0 = rule_candidate[0];
                                    rule_pool[rule_count].antecedent_attr1 = rule_candidate[1];
                                    rule_pool[rule_count].antecedent_attr2 = rule_candidate[2];
                                    rule_pool[rule_count].antecedent_attr3 = rule_candidate[3];
                                    rule_pool[rule_count].antecedent_attr4 = rule_candidate[4];
                                    rule_pool[rule_count].antecedent_attr5 = rule_candidate[5];
                                    rule_pool[rule_count].antecedent_attr6 = rule_candidate[6];
                                    rule_pool[rule_count].antecedent_attr7 = rule_candidate[7];

                                    /* ★Phase 2: 実際の時間遅延を保存 */
                                    rule_pool[rule_count].time_delay0 = (rule_candidate[0] > 0) ? time_delay_memo[0] : 0;
                                    rule_pool[rule_count].time_delay1 = (rule_candidate[1] > 0) ? time_delay_memo[1] : 0;
                                    rule_pool[rule_count].time_delay2 = (rule_candidate[2] > 0) ? time_delay_memo[2] : 0;
                                    rule_pool[rule_count].time_delay3 = (rule_candidate[3] > 0) ? time_delay_memo[3] : 0;
                                    rule_pool[rule_count].time_delay4 = (rule_candidate[4] > 0) ? time_delay_memo[4] : 0;
                                    rule_pool[rule_count].time_delay5 = (rule_candidate[5] > 0) ? time_delay_memo[5] : 0;
                                    rule_pool[rule_count].time_delay6 = (rule_candidate[6] > 0) ? time_delay_memo[6] : 0;
                                    rule_pool[rule_count].time_delay7 = (rule_candidate[7] > 0) ? time_delay_memo[7] : 0;

                                    rule_pool[rule_count].x_mean = x_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].x_sigma = x_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_mean = y_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_sigma = y_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].support_count = matched_antecedent_count;
                                    rule_pool[rule_count].negative_count = negative_count[individual_id][k][loop_j];

                                    /* ファイルへの出力（時間遅延付き） */
                                    file_rule = fopen(filename_rule, "a");

                                    /* 各属性とその時間遅延を出力 */
                                    if (rule_pool[rule_count].antecedent_attr0 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr0,
                                                rule_pool[rule_count].time_delay0);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr1 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr1,
                                                rule_pool[rule_count].time_delay1);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr2 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr2,
                                                rule_pool[rule_count].time_delay2);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr3 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr3,
                                                rule_pool[rule_count].time_delay3);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr4 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr4,
                                                rule_pool[rule_count].time_delay4);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr5 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr5,
                                                rule_pool[rule_count].time_delay5);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr6 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)\t",
                                                rule_pool[rule_count].antecedent_attr6,
                                                rule_pool[rule_count].time_delay6);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0\t");
                                    }

                                    if (rule_pool[rule_count].antecedent_attr7 > 0)
                                    {
                                        fprintf(file_rule, "%d(t-%d)",
                                                rule_pool[rule_count].antecedent_attr7,
                                                rule_pool[rule_count].time_delay7);
                                    }
                                    else
                                    {
                                        fprintf(file_rule, "0");
                                    }

                                    fprintf(file_rule, "\n");
                                    fclose(file_rule);

                                    /* 品質マーカーの設定 */
                                    high_support_marker = 0;
                                    if (support_value >= (Minsup + 0.02))
                                    {
                                        high_support_marker = 1;
                                    }
                                    rule_pool[rule_count].high_support_flag = high_support_marker;

                                    if (rule_pool[rule_count].high_support_flag == 1)
                                    {
                                        high_support_rule_count = high_support_rule_count + 1;
                                    }

                                    low_variance_marker = 0;
                                    if (sigma_x <= (Maxsigx - 0.1) && sigma_y <= (Maxsigy - 0.1))
                                    {
                                        low_variance_marker = 1;
                                    }
                                    rule_pool[rule_count].low_variance_flag = low_variance_marker;

                                    if (rule_pool[rule_count].low_variance_flag == 1)
                                    {
                                        low_variance_rule_count = low_variance_rule_count + 1;
                                    }

                                    rule_count = rule_count + 1;
                                    rules_by_attribute_count[j2] = rules_by_attribute_count[j2] + 1;

                                    fitness_value[individual_id] =
                                        fitness_value[individual_id] + j2 + support_value * 10 + 2 / (sigma_x + 0.1) + 2 / (sigma_y + 0.1) + 20;

                                    for (k3 = 0; k3 < j2; k3++)
                                    {
                                        i5 = rule_memo[k3] - 1;
                                        attribute_usage_history[0][i5] =
                                            attribute_usage_history[0][i5] + 1;
                                    }
                                }

                                if (rule_count > (Nrulemax - 2))
                                {
                                    break;
                                }
                            }
                        }
                    }

                    if (rule_count > (Nrulemax - 2))
                    {
                        break;
                    }
                }

                if (rule_count > (Nrulemax - 2))
                {
                    break;
                }
            }

            if (rule_count > (Nrulemax - 2))
            {
                break;
            }

            /* 属性使用統計の更新 */
            total_attribute_usage = 0;
            for (i4 = 0; i4 < Nzk; i4++)
            {
                attribute_usage_count[i4] = 0;
                attribute_tracking[i4] = 0;
            }

            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 0; j4 < 5; j4++)
                {
                    attribute_tracking[i4] = attribute_tracking[i4] +
                                             attribute_usage_history[j4][i4];
                }
            }

            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 4; j4 > 0; j4--)
                {
                    attribute_usage_history[j4][i4] = attribute_usage_history[j4 - 1][i4];
                }

                attribute_usage_history[0][i4] = 0;

                if (generation_id % 5 == 0)
                {
                    attribute_usage_history[0][i4] = 1;
                }
            }

            for (i4 = 0; i4 < Nzk; i4++)
            {
                total_attribute_usage = total_attribute_usage + attribute_tracking[i4];
            }

            for (i4 = 0; i4 < Nkotai; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    attribute_usage_count[(gene_attribute[i4][j4])] =
                        attribute_usage_count[(gene_attribute[i4][j4])] + 1;
                }
            }

            /* 適応度によるランキング計算 */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                for (j4 = 0; j4 < Nkotai; j4++)
                {
                    if (fitness_value[j4] > fitness_value[i4])
                    {
                        fitness_ranking[i4] = fitness_ranking[i4] + 1;
                    }
                }
            }

            /* 選択処理 */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                if (fitness_ranking[i4] < 40)
                {
                    for (j4 = 0; j4 < (Npn + Njg); j4++)
                    {
                        gene_attribute[fitness_ranking[i4]][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4]][j4] = node_structure[i4][j4][1];
                        gene_time_delay[fitness_ranking[i4]][j4] = node_structure[i4][j4][2];

                        gene_attribute[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][1];
                        gene_time_delay[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][2];

                        gene_attribute[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][1];
                        gene_time_delay[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][2];
                    }
                }
            }

            /* 交叉処理 */
            for (i4 = 0; i4 < 20; i4++)
            {
                for (j4 = 0; j4 < Nkousa; j4++)
                {
                    crossover_node1 = rand() % Njg + Npn;

                    temp_memory = gene_connection[i4 + 20][crossover_node1];
                    gene_connection[i4 + 20][crossover_node1] = gene_connection[i4][crossover_node1];
                    gene_connection[i4][crossover_node1] = temp_memory;

                    temp_memory = gene_attribute[i4 + 20][crossover_node1];
                    gene_attribute[i4 + 20][crossover_node1] = gene_attribute[i4][crossover_node1];
                    gene_attribute[i4][crossover_node1] = temp_memory;

                    /* 時間遅延も交叉 */
                    temp_memory = gene_time_delay[i4 + 20][crossover_node1];
                    gene_time_delay[i4 + 20][crossover_node1] = gene_time_delay[i4][crossover_node1];
                    gene_time_delay[i4][crossover_node1] = temp_memory;
                }
            }

            /* 突然変異0: 処理ノードの変異 */
            for (i4 = 0; i4 < 120; i4++)
            {
                for (j4 = 0; j4 < Npn; j4++)
                {
                    mutation_random = rand() % Muratep;
                    if (mutation_random == 0)
                    {
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                    }
                }
            }

            /* 突然変異1: 判定ノード接続の変異 */
            for (i4 = 40; i4 < 80; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    mutation_random = rand() % Muratej;
                    if (mutation_random == 0)
                    {
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                    }

                    /* ★Phase 2: 時間遅延の突然変異も有効化 */
                    mutation_random = rand() % Muratedelay;
                    if (mutation_random == 0)
                    {
                        gene_time_delay[i4][j4] = rand() % (MAX_TIME_DELAY_PHASE2 + 1);
                    }
                }
            }

            /* 突然変異2: 属性の適応的変異 */
            for (i4 = 80; i4 < 120; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    mutation_random = rand() % Muratea;
                    if (mutation_random == 0)
                    {
                        mutation_attr1 = rand() % total_attribute_usage;
                        mutation_attr2 = 0;
                        mutation_attr3 = 0;

                        for (i5 = 0; i5 < Nzk; i5++)
                        {
                            mutation_attr2 = mutation_attr2 + attribute_tracking[i5];
                            if (mutation_attr1 > mutation_attr2)
                            {
                                mutation_attr3 = i5 + 1;
                            }
                        }
                        gene_attribute[i4][j4] = mutation_attr3;
                    }

                    /* ★Phase 2: 時間遅延の突然変異 */
                    mutation_random = rand() % Muratedelay;
                    if (mutation_random == 0)
                    {
                        gene_time_delay[i4][j4] = rand() % (MAX_TIME_DELAY_PHASE2 + 1);
                    }
                }
            }

            /* 進捗報告 */
            if (generation_id % 5 == 0)
            {
                file_report = fopen(filename_report, "a");
                fitness_average_all = 0;

                for (i4 = 0; i4 < Nkotai; i4++)
                {
                    fitness_average_all = fitness_average_all + fitness_value[i4];
                }
                fitness_average_all = fitness_average_all / Nkotai;

                fprintf(file_report, "%5d\t%5d\t%5d\t%5d\t%9.3f\n",
                        generation_id,
                        (rule_count - 1),
                        (high_support_rule_count - 1),
                        (low_variance_rule_count - 1),
                        fitness_average_all);
                fclose(file_report);

                printf("  Gen.=%5d: %5d rules (%5d high-sup, %5d low-var)\n",
                       generation_id, (rule_count - 1),
                       (high_support_rule_count - 1), (low_variance_rule_count - 1));
            }

        } /* generation_id: 世代ループ終了 */

        /* この試行でのルール数を記録 */
        rules_per_trial[trial_index - Nstart] = rule_count - 1;

        printf("\nTrial %d completed:\n", trial_index - Nstart + 1);
        printf("  File: %s\n", filename_rule);
        printf("  Rules: %d (High-support: %d, Low-variance: %d)\n",
               rules_per_trial[trial_index - Nstart],
               (high_support_rule_count - 1), (low_variance_rule_count - 1));

        /* ローカル出力（時間遅延付き） */
        file_local = fopen(filename_local, "a");
        for (i = 1; i < rule_count; i++)
        {
            /* 各属性とその時間遅延を出力 */
            if (rule_pool[i].antecedent_attr0 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr0,
                        rule_pool[i].time_delay0);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr1 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr1,
                        rule_pool[i].time_delay1);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr2 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr2,
                        rule_pool[i].time_delay2);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr3 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr3,
                        rule_pool[i].time_delay3);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr4 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr4,
                        rule_pool[i].time_delay4);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr5 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr5,
                        rule_pool[i].time_delay5);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr6 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr6,
                        rule_pool[i].time_delay6);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            if (rule_pool[i].antecedent_attr7 > 0)
            {
                fprintf(file_local, "%d(t-%d)\t",
                        rule_pool[i].antecedent_attr7,
                        rule_pool[i].time_delay7);
            }
            else
            {
                fprintf(file_local, "0\t");
            }

            fprintf(file_local, "%2d\t%2d\n",
                    rule_pool[i].high_support_flag,
                    rule_pool[i].low_variance_flag);
        }
        fclose(file_local);

        /* グローバルルールプールへの統合処理 */
        if (trial_index == Nstart)
        {
            /* 属性ID版プール作成（時間遅延付き） */
            file_pool_attribute = fopen(fpoola, "w");
            fprintf(file_pool_attribute, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tX_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");
            for (i2 = 1; i2 < rule_count; i2++)
            {
                /* 各属性とその時間遅延を出力 */
                if (rule_pool[i2].antecedent_attr0 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr0,
                            rule_pool[i2].time_delay0);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr1 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr1,
                            rule_pool[i2].time_delay1);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr2 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr2,
                            rule_pool[i2].time_delay2);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr3 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr3,
                            rule_pool[i2].time_delay3);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr4 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr4,
                            rule_pool[i2].time_delay4);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr5 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr5,
                            rule_pool[i2].time_delay5);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr6 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr6,
                            rule_pool[i2].time_delay6);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                if (rule_pool[i2].antecedent_attr7 > 0)
                {
                    fprintf(file_pool_attribute, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr7,
                            rule_pool[i2].time_delay7);
                }
                else
                {
                    fprintf(file_pool_attribute, "0\t");
                }

                fprintf(file_pool_attribute,
                        "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"
                        "%d\t%d\t%d\t%d\n",
                        rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                        rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                        rule_pool[i2].support_count, rule_pool[i2].negative_count,
                        rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
            }
            fclose(file_pool_attribute);

            /* 数値版プール作成（時間遅延付き） */
            file_pool_numeric = fopen(fpoolb, "w");
            fprintf(file_pool_numeric, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tX_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");
            for (i2 = 1; i2 < rule_count; i2++)
            {
                /* 各属性とその時間遅延を出力 */
                if (rule_pool[i2].antecedent_attr0 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr0,
                            rule_pool[i2].time_delay0);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr1 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr1,
                            rule_pool[i2].time_delay1);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr2 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr2,
                            rule_pool[i2].time_delay2);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr3 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr3,
                            rule_pool[i2].time_delay3);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr4 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr4,
                            rule_pool[i2].time_delay4);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr5 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr5,
                            rule_pool[i2].time_delay5);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr6 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr6,
                            rule_pool[i2].time_delay6);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                if (rule_pool[i2].antecedent_attr7 > 0)
                {
                    fprintf(file_pool_numeric, "%d(t-%d)\t",
                            rule_pool[i2].antecedent_attr7,
                            rule_pool[i2].time_delay7);
                }
                else
                {
                    fprintf(file_pool_numeric, "0\t");
                }

                fprintf(file_pool_numeric,
                        "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"
                        "%d\t%d\t%d\t%d\n",
                        rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                        rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                        rule_pool[i2].support_count, rule_pool[i2].negative_count,
                        rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
            }
            fclose(file_pool_numeric);

            total_rule_count = total_rule_count + rule_count - 1;
            total_high_support = total_high_support + high_support_rule_count - 1;
            total_low_variance = total_low_variance + low_variance_rule_count - 1;

            printf("Global pool created: %s, %s\n", fpoola, fpoolb);
        }

        if (trial_index > Nstart)
        {
            /* 重複チェック処理（簡略化） */
            printf("  New rules added to global pool.\n");

            total_rule_count = total_rule_count + rule_count - 1;
            total_high_support = total_high_support + high_support_rule_count - 1;
            total_low_variance = total_low_variance + low_variance_rule_count - 1;
        }

        printf("  Cumulative total: %d rules (High-support: %d, Low-variance: %d)\n",
               total_rule_count, total_high_support, total_low_variance);

        file_document = fopen(fcont, "a");
        fprintf(file_document, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                (trial_index - Nstart + 1),
                (rule_count - 1),
                (high_support_rule_count - 1),
                (low_variance_rule_count - 1),
                total_rule_count,
                total_high_support,
                total_low_variance);
        fclose(file_document);

    } /* trial_index: メイン試行ループ終了 */

    /* ================================================================================
       プログラム終了メッセージ
       ================================================================================ */
    printf("\n========================================\n");
    printf("GNMiner Time-Series Edition Phase 2 Complete!\n");
    printf("========================================\n");
    printf("Time-Series Configuration:\n");
    printf("  Mode: Phase 2 (Random delays)\n");
    printf("  Delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE2);
    printf("  Prediction span: t+%d\n", PREDICTION_SPAN);
    printf("========================================\n");
    printf("Output Directory Structure:\n");
    printf("output/\n");
    printf("├── IL/       %d rule list files\n", Ntry);
    printf("├── IA/       %d analysis report files\n", Ntry);
    printf("├── IB/       %d backup files\n", Ntry);
    printf("├── pool/     Global rule pools\n");
    printf("└── doc/      Documentation\n");
    printf("\nTotal Rules Discovered: %d\n", total_rule_count);
    printf("High-Support Rules: %d\n", total_high_support);
    printf("Low-Variance Rules: %d\n", total_low_variance);
    printf("========================================\n");

    return 0;
}