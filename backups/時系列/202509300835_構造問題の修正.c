#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   時系列対応版GNMiner Phase 3 - 構造改善版

   改善内容：
   1. 重複コードの解消（配列化による統一処理）
   2. マジックナンバーの定数化
   3. アクセス関数による安全性向上
   4. 既存アルゴリズムの完全維持
   ================================================================================
*/

/* ================================================================================
   定数定義セクション - マジックナンバーを排除
   ================================================================================
*/

/* 時系列パラメータ */
#define TIMESERIES_MODE 1
#define MAX_TIME_DELAY 10
#define MAX_TIME_DELAY_PHASE3 3
#define MIN_TIME_DELAY 0
#define PREDICTION_SPAN 1
#define ADAPTIVE_DELAY 1

/* ディレクトリ構造 */
#define OUTPUT_DIR "output"
#define OUTPUT_DIR_IL "output/IL"
#define OUTPUT_DIR_IA "output/IA"
#define OUTPUT_DIR_IB "output/IB"
#define OUTPUT_DIR_POOL "output/pool"
#define OUTPUT_DIR_DOC "output/doc"

/* ファイル名 */
#define DATANAME "testdata1s.txt"
#define DICNAME "testdic1.txt"
#define POOL_FILE_A "output/pool/zrp01a.txt"
#define POOL_FILE_B "output/pool/zrp01b.txt"
#define CONT_FILE "output/doc/zrd01.txt"
#define RESULT_FILE "output/doc/zrmemo01.txt"

/* データ構造パラメータ */
#define Nrd 159
#define Nzk 95

/* ルールマイニング制約 */
#define Nrulemax 2002
#define Minsup 0.04
#define Maxsigx 0.5
#define Maxsigy 0.5

/* 実験パラメータ */
#define Nstart 1000
#define Ntry 100

/* GNPパラメータ */
#define Generation 201
#define Nkotai 120
#define Npn 10
#define Njg 100
#define Nmx 7

/* 突然変異率の定数化（マジックナンバー排除） */
#define Muratep 1
#define Muratej 6
#define Muratea 6
#define Muratedelay 6
#define Nkousa 20

/* ルール構造パラメータ（新規追加） */
#define MAX_ATTRIBUTES 8      /* ルールの最大属性数 */
#define MAX_DEPTH 10          /* 判定チェーンの最大深さ */
#define HISTORY_GENERATIONS 5 /* 履歴保持世代数 */

/* 進化計算パラメータ（マジックナンバー排除） */
#define ELITE_SIZE (Nkotai / 3) /* エリート個体数: 40 */
#define ELITE_COPIES 3          /* エリートの複製数 */
#define CROSSOVER_PAIRS 20      /* 交叉ペア数 */
#define MUTATION_START_40 40    /* 突然変異開始インデックス1 */
#define MUTATION_START_80 80    /* 突然変異開始インデックス2 */

/* 品質判定パラメータ（マジックナンバー排除） */
#define HIGH_SUPPORT_BONUS 0.02    /* 高サポートボーナス閾値 */
#define LOW_VARIANCE_REDUCTION 0.1 /* 低分散削減閾値 */
#define FITNESS_SUPPORT_WEIGHT 10  /* サポート重み */
#define FITNESS_SIGMA_OFFSET 0.1   /* シグマオフセット */
#define FITNESS_NEW_RULE_BONUS 20  /* 新規ルールボーナス */
#define FITNESS_ATTRIBUTE_WEIGHT 1 /* 属性数重み */
#define FITNESS_SIGMA_WEIGHT 2     /* シグマ重み */

/* レポート間隔（マジックナンバー排除） */
#define REPORT_INTERVAL 5         /* レポート出力間隔 */
#define DELAY_DISPLAY_INTERVAL 20 /* 遅延統計表示間隔 */

/* ファイル名関連（マジックナンバー排除） */
#define FILENAME_BUFFER_SIZE 256 /* ファイル名バッファサイズ */
#define ATTRIBUTE_NAME_SIZE 31   /* 属性名最大サイズ */
#define FILENAME_DIGITS 5        /* ファイル名の桁数 */

/* 微小値定数（マジックナンバー排除） */
#define FITNESS_INIT_OFFSET -0.00001 /* 適応度初期オフセット */
#define INITIAL_DELAY_HISTORY 1      /* 遅延履歴初期値 */
#define REFRESH_BONUS 2              /* リフレッシュ時ボーナス */

/* ================================================================================
   改善された構造体定義 - 配列化による重複解消
   ================================================================================
*/

/* 改善されたルール構造体 - 配列化により重複を解消 */
struct asrule
{
    int antecedent_attrs[MAX_ATTRIBUTES]; /* 前件部属性配列 */
    int time_delays[MAX_ATTRIBUTES];      /* 時間遅延配列 */
    double x_mean;
    double x_sigma;
    double y_mean;
    double y_sigma;
    int support_count;
    int negative_count;
    int high_support_flag;
    int low_variance_flag;
    int num_attributes; /* 実際の属性数 */
};

/* 比較用ルール構造体も改善 */
struct cmrule
{
    int antecedent_attrs[MAX_ATTRIBUTES];
    int num_attributes;
};

/* 試行状態管理構造体 */
struct trial_state
{
    int trial_id;
    int rule_count;
    int high_support_rule_count;
    int low_variance_rule_count;
    int generation;
    char filename_rule[FILENAME_BUFFER_SIZE];
    char filename_report[FILENAME_BUFFER_SIZE];
    char filename_local[FILENAME_BUFFER_SIZE];
};

/* ================================================================================
   グローバル変数
   ================================================================================
*/

/* ルールプール */
struct asrule rule_pool[Nrulemax];
struct cmrule compare_rules[Nrulemax];

/* データバッファ */
static int data_buffer[Nrd][Nzk];
static double x_buffer[Nrd];
static double y_buffer[Nrd];

/* 時間遅延統計 */
int delay_usage_history[HISTORY_GENERATIONS][MAX_TIME_DELAY_PHASE3 + 1];
int delay_usage_count[MAX_TIME_DELAY_PHASE3 + 1];
int delay_tracking[MAX_TIME_DELAY_PHASE3 + 1];
double delay_success_rate[MAX_TIME_DELAY_PHASE3 + 1];

/* GNPノード構造 */
static int node_structure[Nkotai][Npn + Njg][3];

/* 評価統計配列 */
static int match_count[Nkotai][Npn][MAX_DEPTH];
static int negative_count[Nkotai][Npn][MAX_DEPTH];
static int evaluation_count[Nkotai][Npn][MAX_DEPTH];
static int attribute_chain[Nkotai][Npn][MAX_DEPTH];
static int time_delay_chain[Nkotai][Npn][MAX_DEPTH];

static double x_sum[Nkotai][Npn][MAX_DEPTH];
static double x_sigma_array[Nkotai][Npn][MAX_DEPTH];
static double y_sum[Nkotai][Npn][MAX_DEPTH];
static double y_sigma_array[Nkotai][Npn][MAX_DEPTH];

/* 属性使用統計 */
int attribute_usage_history[HISTORY_GENERATIONS][Nzk];
int attribute_usage_count[Nzk];
int attribute_tracking[Nzk];

/* 適応度関連 */
double fitness_value[Nkotai];
int fitness_ranking[Nkotai];

/* 遺伝子プール */
static int gene_connection[Nkotai][Npn + Njg];
static int gene_attribute[Nkotai][Npn + Njg];
static int gene_time_delay[Nkotai][Npn + Njg];

/* 属性辞書 */
char attribute_dictionary[Nzk + 3][ATTRIBUTE_NAME_SIZE];

/* グローバル統計カウンタ */
int total_rule_count = 0;
int total_high_support = 0;
int total_low_variance = 0;

/* その他の作業用配列 */
int rules_per_trial[Ntry];
int new_rule_marker[Nrulemax];
int rules_by_attribute_count[MAX_DEPTH];
int attribute_set[Nzk + 1];

/* ================================================================================
   ユーティリティ関数 - 配列アクセスの統一化
   ================================================================================
*/

/**
 * get_rule_attribute()
 * ルールの属性を取得する統一関数
 * 配列アクセスを抽象化
 */
int get_rule_attribute(struct asrule *rule, int index)
{
    if (index < 0 || index >= MAX_ATTRIBUTES)
        return 0;
    return rule->antecedent_attrs[index];
}

/**
 * set_rule_attribute()
 * ルールの属性を設定する統一関数
 */
void set_rule_attribute(struct asrule *rule, int index, int value)
{
    if (index < 0 || index >= MAX_ATTRIBUTES)
        return;
    rule->antecedent_attrs[index] = value;
}

/**
 * get_rule_delay()
 * ルールの時間遅延を取得する統一関数
 */
int get_rule_delay(struct asrule *rule, int index)
{
    if (index < 0 || index >= MAX_ATTRIBUTES)
        return 0;
    return rule->time_delays[index];
}

/**
 * set_rule_delay()
 * ルールの時間遅延を設定する統一関数
 */
void set_rule_delay(struct asrule *rule, int index, int value)
{
    if (index < 0 || index >= MAX_ATTRIBUTES)
        return;
    rule->time_delays[index] = value;
}

/**
 * copy_rule_attributes()
 * ルール属性を配列としてコピーする関数
 * 重複コードを解消
 */
void copy_rule_attributes(struct asrule *dest, int *src_attrs, int *src_delays, int num_attrs)
{
    for (int i = 0; i < MAX_ATTRIBUTES; i++)
    {
        if (i < num_attrs && src_attrs[i] > 0)
        {
            dest->antecedent_attrs[i] = src_attrs[i];
            dest->time_delays[i] = src_delays[i];
        }
        else
        {
            dest->antecedent_attrs[i] = 0;
            dest->time_delays[i] = 0;
        }
    }
    dest->num_attributes = num_attrs;
}

/* ================================================================================
   1. 初期化・設定系関数
   ================================================================================
*/

void create_output_directories()
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

    if (TIMESERIES_MODE)
    {
        printf("*** TIME-SERIES MODE ENABLED (Phase 3) ***\n");
        printf("Adaptive delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE3);
        printf("Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
        printf("Prediction span: t+%d\n", PREDICTION_SPAN);
        printf("=========================================\n\n");
    }
}

void load_attribute_dictionary()
{
    FILE *file_dictionary;
    int process_id;
    char attribute_name_buffer[ATTRIBUTE_NAME_SIZE];

    /* 辞書配列を初期化 */
    for (int i = 0; i < Nzk + 3; i++)
    {
        strcpy(attribute_dictionary[i], "\0");
    }

    file_dictionary = fopen(DICNAME, "r");
    if (file_dictionary == NULL)
    {
        printf("Warning: Cannot open dictionary file: %s\n", DICNAME);
        printf("Using attribute numbers instead of names.\n");
        for (int i = 0; i < Nzk + 3; i++)
        {
            sprintf(attribute_dictionary[i], "Attr%d", i);
        }
    }
    else
    {
        for (int i = 0; i < Nzk + 3; i++)
        {
            if (fscanf(file_dictionary, "%d%s", &process_id, attribute_name_buffer) == 2)
            {
                strcpy(attribute_dictionary[i], attribute_name_buffer);
            }
            else
            {
                sprintf(attribute_dictionary[i], "Attr%d", i);
            }
        }
        fclose(file_dictionary);
        printf("Dictionary loaded: %d attributes\n", Nzk);
    }
}

/**
 * initialize_rule_pool()
 * 改善版：配列を使った初期化でコードの重複を解消
 */
void initialize_rule_pool()
{
    for (int i = 0; i < Nrulemax; i++)
    {
        /* 配列として一括初期化 */
        for (int j = 0; j < MAX_ATTRIBUTES; j++)
        {
            rule_pool[i].antecedent_attrs[j] = 0;
            rule_pool[i].time_delays[j] = 0;
            compare_rules[i].antecedent_attrs[j] = 0;
        }

        rule_pool[i].x_mean = 0;
        rule_pool[i].x_sigma = 0;
        rule_pool[i].y_mean = 0;
        rule_pool[i].y_sigma = 0;
        rule_pool[i].support_count = 0;
        rule_pool[i].negative_count = 0;
        rule_pool[i].high_support_flag = 0;
        rule_pool[i].low_variance_flag = 0;
        rule_pool[i].num_attributes = 0;

        compare_rules[i].num_attributes = 0;
        new_rule_marker[i] = 0;
    }
}

void initialize_delay_statistics()
{
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        delay_usage_count[i] = 0;
        delay_tracking[i] = INITIAL_DELAY_HISTORY; /* マジックナンバー排除 */
        delay_success_rate[i] = 0.5;

        for (int j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_usage_history[j][i] = INITIAL_DELAY_HISTORY;
        }
    }
}

void initialize_attribute_statistics()
{
    for (int i = 0; i < Nzk; i++)
    {
        attribute_usage_count[i] = 0;
        attribute_tracking[i] = 0;

        /* 履歴の初期化も配列として処理 */
        for (int j = 0; j < HISTORY_GENERATIONS; j++)
        {
            attribute_usage_history[j][i] = (j == 0) ? INITIAL_DELAY_HISTORY : 0;
        }
    }

    for (int i = 0; i < MAX_DEPTH; i++)
    {
        rules_by_attribute_count[i] = 0;
    }

    for (int i = 0; i < Nzk + 1; i++)
    {
        attribute_set[i] = i;
    }
}

void initialize_global_counters()
{
    total_rule_count = 0;
    total_high_support = 0;
    total_low_variance = 0;
}

/* ================================================================================
   2. データ管理系関数
   ================================================================================
*/

void load_timeseries_data()
{
    FILE *file_data;
    int input_value;
    double input_x, input_y;

    printf("Loading data into buffer for time-series processing...\n");

    file_data = fopen(DATANAME, "r");
    if (file_data == NULL)
    {
        printf("Error: Cannot open data file: %s\n", DATANAME);
        exit(1);
    }

    for (int i = 0; i < Nrd; i++)
    {
        for (int j = 0; j < Nzk; j++)
        {
            if (fscanf(file_data, "%d", &input_value) != 1)
            {
                printf("Error: Failed to read data at record %d, attribute %d\n", i, j);
                fclose(file_data);
                exit(1);
            }
            data_buffer[i][j] = input_value;
        }

        if (fscanf(file_data, "%lf%lf", &input_x, &input_y) != 2)
        {
            printf("Error: Failed to read X,Y values at record %d\n", i);
            fclose(file_data);
            exit(1);
        }
        x_buffer[i] = input_x;
        y_buffer[i] = input_y;
    }
    fclose(file_data);
    printf("Data loaded: %d records with %d attributes each\n", Nrd, Nzk);
}

void get_safe_data_range(int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        *start_index = MAX_TIME_DELAY_PHASE3;
        *end_index = Nrd - PREDICTION_SPAN;
    }
    else
    {
        *start_index = 0;
        *end_index = Nrd;
    }
}

int get_past_attribute_value(int current_time, int time_delay, int attribute_id)
{
    int data_index = current_time - time_delay;

    if (data_index < 0 || data_index >= Nrd)
    {
        return -1;
    }

    return data_buffer[data_index][attribute_id];
}

void get_future_target_values(int current_time, double *x_value, double *y_value)
{
    int future_index = current_time + PREDICTION_SPAN;

    if (future_index >= Nrd)
    {
        *x_value = 0.0;
        *y_value = 0.0;
        return;
    }

    *x_value = x_buffer[future_index];
    *y_value = y_buffer[future_index];
}

/* ================================================================================
   3. GNP個体管理系関数
   ================================================================================
*/

void create_initial_population()
{
    for (int i = 0; i < Nkotai; i++)
    {
        for (int j = 0; j < (Npn + Njg); j++)
        {
            gene_connection[i][j] = rand() % Njg + Npn;
            gene_attribute[i][j] = rand() % Nzk;
            gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
        }
    }
}

void copy_genes_to_nodes()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        for (int node = 0; node < (Npn + Njg); node++)
        {
            node_structure[individual][node][0] = gene_attribute[individual][node];
            node_structure[individual][node][1] = gene_connection[individual][node];
            node_structure[individual][node][2] = gene_time_delay[individual][node];
        }
    }
}

void initialize_individual_statistics()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        fitness_value[individual] = (double)individual * FITNESS_INIT_OFFSET;
        fitness_ranking[individual] = 0;

        for (int k = 0; k < Npn; k++)
        {
            for (int i = 0; i < MAX_DEPTH; i++)
            {
                match_count[individual][k][i] = 0;
                attribute_chain[individual][k][i] = 0;
                time_delay_chain[individual][k][i] = 0;
                negative_count[individual][k][i] = 0;
                evaluation_count[individual][k][i] = 0;
                x_sum[individual][k][i] = 0;
                x_sigma_array[individual][k][i] = 0;
                y_sum[individual][k][i] = 0;
                y_sigma_array[individual][k][i] = 0;
            }
        }
    }
}

/* ================================================================================
   4. ルール評価系関数
   ================================================================================
*/

void evaluate_single_instance(int time_index)
{
    double future_x, future_y;
    int current_node_id, depth, match_flag;
    int time_delay, data_index;

    get_future_target_values(time_index, &future_x, &future_y);

    for (int individual = 0; individual < Nkotai; individual++)
    {
        for (int k = 0; k < Npn; k++)
        {
            current_node_id = k;
            depth = 0;
            match_flag = 1;

            match_count[individual][k][depth]++;
            evaluation_count[individual][k][depth]++;

            current_node_id = node_structure[individual][current_node_id][1];

            while (current_node_id > (Npn - 1) && depth < Nmx)
            {
                depth++;

                attribute_chain[individual][k][depth] =
                    node_structure[individual][current_node_id][0] + 1;

                time_delay = node_structure[individual][current_node_id][2];
                time_delay_chain[individual][k][depth] = time_delay;

                data_index = time_index - time_delay;

                if (data_index < 0)
                {
                    current_node_id = k;
                    break;
                }

                int attr_value = data_buffer[data_index][node_structure[individual][current_node_id][0]];

                if (attr_value == 1)
                {
                    if (match_flag == 1)
                    {
                        match_count[individual][k][depth]++;
                        x_sum[individual][k][depth] += future_x;
                        x_sigma_array[individual][k][depth] += future_x * future_x;
                        y_sum[individual][k][depth] += future_y;
                        y_sigma_array[individual][k][depth] += future_y * future_y;
                    }
                    evaluation_count[individual][k][depth]++;
                    current_node_id = node_structure[individual][current_node_id][1];
                }
                else if (attr_value == 0)
                {
                    current_node_id = k;
                }
                else
                {
                    evaluation_count[individual][k][depth]++;
                    match_flag = 0;
                    current_node_id = node_structure[individual][current_node_id][1];
                }
            }
        }
    }
}

void evaluate_all_individuals()
{
    int safe_start, safe_end;

    get_safe_data_range(&safe_start, &safe_end);

    for (int i = safe_start; i < safe_end; i++)
    {
        evaluate_single_instance(i);
    }
}

void calculate_negative_counts()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        for (int k = 0; k < Npn; k++)
        {
            for (int i = 0; i < Nmx; i++)
            {
                negative_count[individual][k][i] =
                    match_count[individual][k][0] - evaluation_count[individual][k][i] +
                    match_count[individual][k][i];
            }
        }
    }
}

void calculate_rule_statistics()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        for (int k = 0; k < Npn; k++)
        {
            for (int j = 1; j < 9; j++)
            {
                if (match_count[individual][k][j] != 0)
                {
                    x_sum[individual][k][j] /= (double)match_count[individual][k][j];
                    x_sigma_array[individual][k][j] =
                        x_sigma_array[individual][k][j] / (double)match_count[individual][k][j] -
                        x_sum[individual][k][j] * x_sum[individual][k][j];

                    y_sum[individual][k][j] /= (double)match_count[individual][k][j];
                    y_sigma_array[individual][k][j] =
                        y_sigma_array[individual][k][j] / (double)match_count[individual][k][j] -
                        y_sum[individual][k][j] * y_sum[individual][k][j];

                    if (x_sigma_array[individual][k][j] < 0)
                    {
                        x_sigma_array[individual][k][j] = 0;
                    }
                    if (y_sigma_array[individual][k][j] < 0)
                    {
                        y_sigma_array[individual][k][j] = 0;
                    }

                    x_sigma_array[individual][k][j] = sqrt(x_sigma_array[individual][k][j]);
                    y_sigma_array[individual][k][j] = sqrt(y_sigma_array[individual][k][j]);
                }
            }
        }
    }
}

double calculate_support_value(int matched_count, int negative_count)
{
    if (negative_count == 0)
    {
        return 0.0;
    }
    return (double)matched_count / (double)negative_count;
}

/* ================================================================================
   5. ルール抽出系関数 - 改善版
   ================================================================================
*/

int check_rule_quality(double sigma_x, double sigma_y, double support)
{
    return (sigma_x <= Maxsigx && sigma_y <= Maxsigy && support >= Minsup);
}

/**
 * check_rule_duplication()
 * 改善版：配列比較により重複チェックを簡潔に
 */
int check_rule_duplication(int *rule_candidate, int rule_count)
{
    for (int i = 0; i < rule_count; i++)
    {
        int is_duplicate = 1;
        for (int j = 0; j < MAX_ATTRIBUTES; j++)
        {
            if (rule_pool[i].antecedent_attrs[j] != rule_candidate[j])
            {
                is_duplicate = 0;
                break;
            }
        }
        if (is_duplicate)
        {
            return 1;
        }
    }
    return 0;
}

/**
 * register_new_rule()
 * 改善版：配列操作により重複コードを解消
 */
void register_new_rule(struct trial_state *state, int *rule_candidate, int *time_delays,
                       double x_mean, double x_sigma, double y_mean, double y_sigma,
                       int support_count, int negative_count_val, double support_value)
{
    int idx = state->rule_count;

    /* 配列として一括コピー（重複コード解消） */
    for (int i = 0; i < MAX_ATTRIBUTES; i++)
    {
        rule_pool[idx].antecedent_attrs[i] = rule_candidate[i];
        rule_pool[idx].time_delays[i] = (rule_candidate[i] > 0) ? time_delays[i] : 0;
    }

    rule_pool[idx].x_mean = x_mean;
    rule_pool[idx].x_sigma = x_sigma;
    rule_pool[idx].y_mean = y_mean;
    rule_pool[idx].y_sigma = y_sigma;
    rule_pool[idx].support_count = support_count;
    rule_pool[idx].negative_count = negative_count_val;

    /* マジックナンバーを定数化 */
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

    int low_variance_marker = 0;
    if (x_sigma <= (Maxsigx - LOW_VARIANCE_REDUCTION) &&
        y_sigma <= (Maxsigy - LOW_VARIANCE_REDUCTION))
    {
        low_variance_marker = 1;
    }
    rule_pool[idx].low_variance_flag = low_variance_marker;

    if (rule_pool[idx].low_variance_flag == 1)
    {
        state->low_variance_rule_count++;
    }

    state->rule_count++;
}

void update_delay_learning_from_rule(int *time_delays, int num_attributes,
                                     int high_support, int low_variance)
{
    for (int k = 0; k < num_attributes; k++)
    {
        if (time_delays[k] >= 0 && time_delays[k] <= MAX_TIME_DELAY_PHASE3)
        {
            delay_usage_history[0][time_delays[k]] += 1;

            if (high_support || low_variance)
            {
                delay_usage_history[0][time_delays[k]] += REFRESH_BONUS;
            }
        }
    }
}

void extract_rules_from_individual(struct trial_state *state, int individual)
{
    int rule_candidate_pre[MAX_DEPTH];
    int rule_candidate[MAX_ATTRIBUTES];
    int rule_memo[MAX_ATTRIBUTES];
    int time_delay_candidate[MAX_DEPTH];
    int time_delay_memo[MAX_ATTRIBUTES];

    for (int k = 0; k < Npn; k++)
    {
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }

        /* 配列の初期化も簡潔に */
        memset(rule_candidate_pre, 0, sizeof(rule_candidate_pre));
        memset(rule_candidate, 0, sizeof(rule_candidate));
        memset(rule_memo, 0, sizeof(rule_memo));
        memset(time_delay_candidate, 0, sizeof(time_delay_candidate));
        memset(time_delay_memo, 0, sizeof(time_delay_memo));

        for (int loop_j = 1; loop_j < 9; loop_j++)
        {
            double sigma_x = x_sigma_array[individual][k][loop_j];
            double sigma_y = y_sigma_array[individual][k][loop_j];
            int matched_count = match_count[individual][k][loop_j];
            double support = calculate_support_value(matched_count,
                                                     negative_count[individual][k][loop_j]);

            if (check_rule_quality(sigma_x, sigma_y, support))
            {
                for (int i2 = 1; i2 < 9; i2++)
                {
                    rule_candidate_pre[i2 - 1] = attribute_chain[individual][k][i2];
                    time_delay_candidate[i2 - 1] = time_delay_chain[individual][k][i2];
                }

                int j2 = 0;
                for (int k2 = 1; k2 < (1 + Nzk); k2++)
                {
                    int found = 0;
                    for (int i2 = 0; i2 < loop_j; i2++)
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

                if (j2 < 9)
                {
                    if (!check_rule_duplication(rule_candidate, state->rule_count))
                    {
                        register_new_rule(state, rule_candidate, time_delay_memo,
                                          x_sum[individual][k][loop_j], sigma_x,
                                          y_sum[individual][k][loop_j], sigma_y,
                                          matched_count, negative_count[individual][k][loop_j],
                                          support);

                        rules_by_attribute_count[j2]++;

                        /* マジックナンバーを定数化した適応度計算 */
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (sigma_x + FITNESS_SIGMA_OFFSET) +
                            FITNESS_SIGMA_WEIGHT / (sigma_y + FITNESS_SIGMA_OFFSET) +
                            FITNESS_NEW_RULE_BONUS;

                        for (int k3 = 0; k3 < j2; k3++)
                        {
                            int i5 = rule_memo[k3] - 1;
                            attribute_usage_history[0][i5]++;
                        }

                        update_delay_learning_from_rule(time_delay_memo, j2,
                                                        rule_pool[state->rule_count - 1].high_support_flag,
                                                        rule_pool[state->rule_count - 1].low_variance_flag);
                    }
                    else
                    {
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (sigma_x + FITNESS_SIGMA_OFFSET) +
                            FITNESS_SIGMA_WEIGHT / (sigma_y + FITNESS_SIGMA_OFFSET);
                    }

                    if (state->rule_count > (Nrulemax - 2))
                    {
                        break;
                    }
                }
            }
        }
    }
}

void extract_rules_from_population(struct trial_state *state)
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }
        extract_rules_from_individual(state, individual);
    }
}

/* ================================================================================
   6. 進化計算系関数 - 改善版
   ================================================================================
*/

void calculate_fitness_rankings()
{
    for (int i = 0; i < Nkotai; i++)
    {
        fitness_ranking[i] = 0;
        for (int j = 0; j < Nkotai; j++)
        {
            if (fitness_value[j] > fitness_value[i])
            {
                fitness_ranking[i]++;
            }
        }
    }
}

/**
 * perform_selection()
 * 改善版：マジックナンバーを定数化
 */
void perform_selection()
{
    for (int i = 0; i < Nkotai; i++)
    {
        if (fitness_ranking[i] < ELITE_SIZE)
        {
            for (int j = 0; j < (Npn + Njg); j++)
            {
                /* エリート個体を3つの位置にコピー */
                for (int copy = 0; copy < ELITE_COPIES; copy++)
                {
                    int target_idx = fitness_ranking[i] + copy * ELITE_SIZE;
                    gene_attribute[target_idx][j] = node_structure[i][j][0];
                    gene_connection[target_idx][j] = node_structure[i][j][1];
                    gene_time_delay[target_idx][j] = node_structure[i][j][2];
                }
            }
        }
    }
}

/**
 * perform_crossover()
 * 改善版：マジックナンバーを定数化
 */
void perform_crossover()
{
    for (int i = 0; i < CROSSOVER_PAIRS; i++)
    {
        for (int j = 0; j < Nkousa; j++)
        {
            int crossover_point = rand() % Njg + Npn;
            int temp;

            temp = gene_connection[i + CROSSOVER_PAIRS][crossover_point];
            gene_connection[i + CROSSOVER_PAIRS][crossover_point] = gene_connection[i][crossover_point];
            gene_connection[i][crossover_point] = temp;

            temp = gene_attribute[i + CROSSOVER_PAIRS][crossover_point];
            gene_attribute[i + CROSSOVER_PAIRS][crossover_point] = gene_attribute[i][crossover_point];
            gene_attribute[i][crossover_point] = temp;

            temp = gene_time_delay[i + CROSSOVER_PAIRS][crossover_point];
            gene_time_delay[i + CROSSOVER_PAIRS][crossover_point] = gene_time_delay[i][crossover_point];
            gene_time_delay[i][crossover_point] = temp;
        }
    }
}

void perform_mutation_processing_nodes()
{
    for (int i = 0; i < Nkotai; i++)
    {
        for (int j = 0; j < Npn; j++)
        {
            if (rand() % Muratep == 0)
            {
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

/**
 * perform_mutation_judgment_nodes()
 * 改善版：マジックナンバーを定数化
 */
void perform_mutation_judgment_nodes()
{
    for (int i = MUTATION_START_40; i < MUTATION_START_80; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratej == 0)
            {
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

int roulette_wheel_selection_delay(int total_usage)
{
    if (total_usage <= 0)
    {
        return rand() % (MAX_TIME_DELAY_PHASE3 + 1);
    }

    int random_value = rand() % total_usage;
    int accumulated = 0;

    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        accumulated += delay_tracking[i];
        if (random_value < accumulated)
        {
            return i;
        }
    }

    return MAX_TIME_DELAY_PHASE3;
}

int roulette_wheel_selection_attribute(int total_usage)
{
    if (total_usage <= 0)
    {
        return rand() % Nzk;
    }

    int random_value = rand() % total_usage;
    int accumulated = 0;

    for (int i = 0; i < Nzk; i++)
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
 * perform_adaptive_delay_mutation()
 * 改善版：マジックナンバーを定数化
 */
void perform_adaptive_delay_mutation()
{
    int total_delay_usage = 0;
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        total_delay_usage += delay_tracking[i];
    }

    /* 第1グループ（40-79） */
    for (int i = MUTATION_START_40; i < MUTATION_START_80; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            {
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
                }
            }
        }
    }

    /* 第2グループ（80-119） */
    for (int i = MUTATION_START_80; i < Nkotai; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            {
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
                }
            }
        }
    }
}

/**
 * perform_adaptive_attribute_mutation()
 * 改善版：マジックナンバーを定数化
 */
void perform_adaptive_attribute_mutation()
{
    int total_attribute_usage = 0;
    for (int i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    for (int i = MUTATION_START_80; i < Nkotai; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratea == 0)
            {
                gene_attribute[i][j] = roulette_wheel_selection_attribute(total_attribute_usage);
            }
        }
    }
}

/* ================================================================================
   7. 統計・履歴管理系関数 - 改善版
   ================================================================================
*/

/**
 * update_delay_statistics()
 * 改善版：マジックナンバーを定数化、配列処理の統一
 */
void update_delay_statistics(int generation)
{
    int total_delay_usage = 0;

    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        delay_tracking[i] = 0;

        for (int j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_tracking[i] += delay_usage_history[j][i];
        }

        total_delay_usage += delay_tracking[i];
    }

    /* 履歴配列のシフト */
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        for (int j = HISTORY_GENERATIONS - 1; j > 0; j--)
        {
            delay_usage_history[j][i] = delay_usage_history[j - 1][i];
        }

        delay_usage_history[0][i] = INITIAL_DELAY_HISTORY;

        if (generation % REPORT_INTERVAL == 0)
        {
            delay_usage_history[0][i] = REFRESH_BONUS;
        }
    }
}

/**
 * update_attribute_statistics()
 * 改善版：マジックナンバーを定数化、配列処理の統一
 */
void update_attribute_statistics(int generation)
{
    int total_attribute_usage = 0;

    /* 配列として一括処理 */
    memset(attribute_usage_count, 0, sizeof(int) * Nzk);
    memset(attribute_tracking, 0, sizeof(int) * Nzk);

    for (int i = 0; i < Nzk; i++)
    {
        for (int j = 0; j < HISTORY_GENERATIONS; j++)
        {
            attribute_tracking[i] += attribute_usage_history[j][i];
        }
    }

    /* 履歴配列のシフト */
    for (int i = 0; i < Nzk; i++)
    {
        for (int j = HISTORY_GENERATIONS - 1; j > 0; j--)
        {
            attribute_usage_history[j][i] = attribute_usage_history[j - 1][i];
        }

        attribute_usage_history[0][i] = 0;

        if (generation % REPORT_INTERVAL == 0)
        {
            attribute_usage_history[0][i] = INITIAL_DELAY_HISTORY;
        }
    }

    for (int i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    for (int i = 0; i < Nkotai; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            attribute_usage_count[gene_attribute[i][j]]++;
        }
    }
}

/* ================================================================================
   8. ファイル入出力系関数 - 改善版
   ================================================================================
*/

void create_trial_files(struct trial_state *state)
{
    FILE *file;

    file = fopen(state->filename_rule, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\n");
        fclose(file);
    }

    file = fopen(state->filename_report, "w");
    if (file != NULL)
    {
        fprintf(file, "Generation\tRules\tHighSup\tLowVar\tAvgFitness\n");
        fclose(file);
    }

    file = fopen(state->filename_local, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tHighSup\tLowVar\n");
        fclose(file);
    }
}

/**
 * write_rule_to_file()
 * 改善版：配列処理により重複コードを解消
 */
void write_rule_to_file(struct trial_state *state, int rule_index)
{
    FILE *file = fopen(state->filename_rule, "a");
    if (file == NULL)
        return;

    /* 配列として処理（重複コード解消） */
    for (int i = 0; i < MAX_ATTRIBUTES; i++)
    {
        int attr = rule_pool[rule_index].antecedent_attrs[i];
        int delay = rule_pool[rule_index].time_delays[i];

        if (attr > 0)
        {
            fprintf(file, "%d(t-%d)", attr, delay);
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
 * write_progress_report()
 * 改善版：マジックナンバーを定数化
 */
void write_progress_report(struct trial_state *state, int generation)
{
    FILE *file = fopen(state->filename_report, "a");
    if (file == NULL)
        return;

    double fitness_average = 0;
    for (int i = 0; i < Nkotai; i++)
    {
        fitness_average += fitness_value[i];
    }
    fitness_average /= Nkotai;

    fprintf(file, "%5d\t%5d\t%5d\t%5d\t%9.3f\n",
            generation,
            state->rule_count - 1,
            state->high_support_rule_count - 1,
            state->low_variance_rule_count - 1,
            fitness_average);

    fclose(file);

    printf("  Gen.=%5d: %5d rules (%5d high-sup, %5d low-var)\n",
           generation, state->rule_count - 1,
           state->high_support_rule_count - 1,
           state->low_variance_rule_count - 1);

    if (generation % DELAY_DISPLAY_INTERVAL == 0 && ADAPTIVE_DELAY)
    {
        printf("    Delay usage: ");
        for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
        {
            printf("t-%d:%d ", i, delay_tracking[i]);
        }
        printf("\n");
    }
}

/**
 * write_local_output()
 * 改善版：配列処理により重複コードを解消
 */
void write_local_output(struct trial_state *state)
{
    FILE *file = fopen(state->filename_local, "a");
    if (file == NULL)
        return;

    for (int i = 1; i < state->rule_count; i++)
    {
        /* 配列として処理 */
        for (int j = 0; j < MAX_ATTRIBUTES; j++)
        {
            int attr = rule_pool[i].antecedent_attrs[j];
            int delay = rule_pool[i].time_delays[j];

            if (attr > 0)
            {
                fprintf(file, "%d(t-%d)\t", attr, delay);
            }
            else
            {
                fprintf(file, "0\t");
            }
        }

        fprintf(file, "%2d\t%2d\n",
                rule_pool[i].high_support_flag,
                rule_pool[i].low_variance_flag);
    }

    fclose(file);
}

/**
 * write_global_pool()
 * 改善版：配列処理により重複コードを解消
 */
void write_global_pool(struct trial_state *state)
{
    FILE *file_a = fopen(POOL_FILE_A, "w");
    FILE *file_b = fopen(POOL_FILE_B, "w");

    if (file_a != NULL)
    {
        fprintf(file_a, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file_a, "X_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");

        for (int i = 1; i < state->rule_count; i++)
        {
            /* 配列として処理 */
            for (int j = 0; j < MAX_ATTRIBUTES; j++)
            {
                int attr = rule_pool[i].antecedent_attrs[j];
                int delay = rule_pool[i].time_delays[j];

                if (attr > 0)
                {
                    fprintf(file_a, "%d(t-%d)\t", attr, delay);
                }
                else
                {
                    fprintf(file_a, "0\t");
                }
            }

            fprintf(file_a, "%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                    rule_pool[i].x_mean, rule_pool[i].x_sigma,
                    rule_pool[i].y_mean, rule_pool[i].y_sigma,
                    rule_pool[i].support_count, rule_pool[i].negative_count,
                    rule_pool[i].high_support_flag, rule_pool[i].low_variance_flag);
        }

        fclose(file_a);
    }

    if (file_b != NULL)
    {
        fprintf(file_b, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file_b, "X_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");

        for (int i = 1; i < state->rule_count; i++)
        {
            /* 配列として処理 */
            for (int j = 0; j < MAX_ATTRIBUTES; j++)
            {
                int attr = rule_pool[i].antecedent_attrs[j];
                int delay = rule_pool[i].time_delays[j];

                if (attr > 0)
                {
                    fprintf(file_b, "%d(t-%d)\t", attr, delay);
                }
                else
                {
                    fprintf(file_b, "0\t");
                }
            }

            fprintf(file_b, "%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                    rule_pool[i].x_mean, rule_pool[i].x_sigma,
                    rule_pool[i].y_mean, rule_pool[i].y_sigma,
                    rule_pool[i].support_count, rule_pool[i].negative_count,
                    rule_pool[i].high_support_flag, rule_pool[i].low_variance_flag);
        }

        fclose(file_b);
    }
}

void write_document_stats(struct trial_state *state)
{
    FILE *file = fopen(CONT_FILE, "a");
    if (file != NULL)
    {
        fprintf(file, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                state->trial_id - Nstart + 1,
                state->rule_count - 1,
                state->high_support_rule_count - 1,
                state->low_variance_rule_count - 1,
                total_rule_count,
                total_high_support,
                total_low_variance);
        fclose(file);
    }
}

/* ================================================================================
   9. ユーティリティ系関数 - 改善版
   ================================================================================
*/

/**
 * generate_filename()
 * 改善版：マジックナンバーを定数化
 */
void generate_filename(char *filename, const char *prefix, int trial_id)
{
    int digits[FILENAME_DIGITS];
    int temp = trial_id;

    /* 桁分解を配列処理で統一 */
    for (int i = FILENAME_DIGITS - 1; i >= 0; i--)
    {
        digits[i] = temp % 10;
        temp /= 10;
    }

    sprintf(filename, "%s/%s%d%d%d%d%d.txt",
            (strcmp(prefix, "IL") == 0) ? OUTPUT_DIR_IL : (strcmp(prefix, "IA") == 0) ? OUTPUT_DIR_IA
                                                                                      : OUTPUT_DIR_IB,
            prefix, digits[0], digits[1], digits[2], digits[3], digits[4]);
}

void initialize_document_file()
{
    FILE *file = fopen(CONT_FILE, "w");
    if (file != NULL)
    {
        fprintf(file, "Trial\tRules\tHighSup\tLowVar\tTotal\tTotalHigh\tTotalLow\n");
        fclose(file);
    }
}

void print_final_statistics()
{
    printf("\n========================================\n");
    printf("GNMiner Time-Series Edition Phase 3 Complete!\n");
    printf("========================================\n");
    printf("Time-Series Configuration:\n");
    printf("  Mode: Phase 3 (Adaptive delays)\n");
    printf("  Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
    printf("  Delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE3);
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
}

/* ================================================================================
   メイン関数
   ================================================================================
*/

int main()
{
    /* システム初期化 */
    srand(1);
    create_output_directories();
    load_attribute_dictionary();
    load_timeseries_data();
    initialize_global_counters();
    initialize_document_file();

    /* メイン試行ループ */
    for (int trial = Nstart; trial < (Nstart + Ntry); trial++)
    {
        /* 試行状態の初期化 */
        struct trial_state state;
        state.trial_id = trial;
        state.rule_count = 1;
        state.high_support_rule_count = 1;
        state.low_variance_rule_count = 1;
        state.generation = 0;

        /* ファイル名生成 */
        generate_filename(state.filename_rule, "IL", trial);
        generate_filename(state.filename_report, "IA", trial);
        generate_filename(state.filename_local, "IB", trial);

        printf("\n========== Trial %d/%d Started ==========\n", trial - Nstart + 1, Ntry);
        if (TIMESERIES_MODE && ADAPTIVE_DELAY)
        {
            printf("  [Time-Series Mode Phase 3: Adaptive delays]\n");
        }

        /* 試行初期化 */
        initialize_rule_pool();
        initialize_delay_statistics();
        initialize_attribute_statistics();
        create_initial_population();
        create_trial_files(&state);

        /* 進化計算ループ */
        for (int gen = 0; gen < Generation; gen++)
        {
            if (state.rule_count > (Nrulemax - 2))
            {
                printf("Rule limit reached. Stopping evolution.\n");
                break;
            }

            /* 個体評価フェーズ */
            copy_genes_to_nodes();
            initialize_individual_statistics();
            evaluate_all_individuals();
            calculate_negative_counts();
            calculate_rule_statistics();

            /* ルール抽出フェーズ */
            extract_rules_from_population(&state);

            /* 新規ルールをファイルに出力 */
            int prev_count = rules_per_trial[trial - Nstart];
            for (int i = prev_count + 1; i < state.rule_count; i++)
            {
                write_rule_to_file(&state, i);
            }
            rules_per_trial[trial - Nstart] = state.rule_count - 1;

            /* 統計更新フェーズ */
            update_delay_statistics(gen);
            update_attribute_statistics(gen);

            /* 進化操作フェーズ */
            calculate_fitness_rankings();
            perform_selection();
            perform_crossover();
            perform_mutation_processing_nodes();
            perform_mutation_judgment_nodes();
            perform_adaptive_delay_mutation();
            perform_adaptive_attribute_mutation();

            /* 進捗報告 */
            if (gen % REPORT_INTERVAL == 0)
            {
                write_progress_report(&state, gen);
            }
        }

        /* 試行終了処理 */
        printf("\nTrial %d completed:\n", trial - Nstart + 1);
        printf("  File: %s\n", state.filename_rule);
        printf("  Rules: %d (High-support: %d, Low-variance: %d)\n",
               state.rule_count - 1,
               state.high_support_rule_count - 1,
               state.low_variance_rule_count - 1);

        /* ローカル出力 */
        write_local_output(&state);

        /* グローバルプール更新 */
        if (trial == Nstart)
        {
            write_global_pool(&state);
        }

        total_rule_count += state.rule_count - 1;
        total_high_support += state.high_support_rule_count - 1;
        total_low_variance += state.low_variance_rule_count - 1;

        printf("  Cumulative total: %d rules (High-support: %d, Low-variance: %d)\n",
               total_rule_count, total_high_support, total_low_variance);

        /* ドキュメント更新 */
        write_document_stats(&state);
    }

    /* プログラム終了 */
    print_final_statistics();

    return 0;
}