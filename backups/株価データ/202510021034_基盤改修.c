#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   時系列対応版GNMiner Phase 1 - 単一連続変数X版

   主な変更点：
   1. CSVヘッダー読み込み対応（データと辞書の統合）
   2. Y変数の除去（X単独での1次元ルール発見）
   3. 時間変数Tは読み込むが現段階では未使用
   ================================================================================
*/

/* ================================================================================
   定数定義セクション
   ================================================================================
*/

/* 時系列パラメータ */
#define TIMESERIES_MODE 1
#define MAX_TIME_DELAY 10
#define MAX_TIME_DELAY_PHASE1 3
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
#define DATANAME "nikkei225_data/gnminer_individual/7203.txt"
#define POOL_FILE_A "output/pool/zrp01a.txt"
#define POOL_FILE_B "output/pool/zrp01b.txt"
#define CONT_FILE "output/doc/zrd01.txt"
#define RESULT_FILE "output/doc/zrmemo01.txt"

/* データ構造パラメータ（実行時に動的設定） */
int Nrd = 0; // レコード数（CSVから取得）
int Nzk = 0; // 属性数（CSVから取得）

/* ルールマイニング制約 */
#define Nrulemax 2002
#define Minsup 0.04
#define Maxsigx 5.0 // X標準偏差の最大値

/* 実験パラメータ */
#define Nstart 1000
#define Ntry 100

/* GNPパラメータ */
#define Generation 201
#define Nkotai 120
#define Npn 10
#define Njg 100
#define Nmx 7

/* 突然変異率 */
#define Muratep 1
#define Muratej 6
#define Muratea 6
#define Muratedelay 6
#define Nkousa 20

/* ルール構造パラメータ */
#define MAX_ATTRIBUTES 8
#define MAX_DEPTH 10
#define HISTORY_GENERATIONS 5
#define MAX_RECORDS 10000
#define MAX_ATTRS 1000
#define MAX_ATTR_NAME 50
#define MAX_LINE_LENGTH 100000

/* 進化計算パラメータ */
#define ELITE_SIZE (Nkotai / 3)
#define ELITE_COPIES 3
#define CROSSOVER_PAIRS 20
#define MUTATION_START_40 40
#define MUTATION_START_80 80

/* 品質判定パラメータ */
#define HIGH_SUPPORT_BONUS 0.02
#define LOW_VARIANCE_REDUCTION 1.0
#define FITNESS_SUPPORT_WEIGHT 10
#define FITNESS_SIGMA_OFFSET 0.1
#define FITNESS_NEW_RULE_BONUS 20
#define FITNESS_ATTRIBUTE_WEIGHT 1
#define FITNESS_SIGMA_WEIGHT 4 // 1次元なので2倍に増加

/* レポート間隔 */
#define REPORT_INTERVAL 5
#define DELAY_DISPLAY_INTERVAL 20
#define FILENAME_BUFFER_SIZE 256
#define FILENAME_DIGITS 5
#define FITNESS_INIT_OFFSET -0.00001
#define INITIAL_DELAY_HISTORY 1
#define REFRESH_BONUS 2

/* ================================================================================
   構造体定義 - 1次元版
   ================================================================================
*/

/* 1次元ルール構造体 */
struct asrule_1d
{
    int antecedent_attrs[MAX_ATTRIBUTES];
    int time_delays[MAX_ATTRIBUTES];
    double x_mean;
    double x_sigma;
    int support_count;
    int negative_count;
    int high_support_flag;
    int low_variance_flag;
    int num_attributes;
};

/* 比較用ルール構造体 */
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

/* 動的割り当て用ポインタ */
int **data_buffer = NULL;
double *x_buffer = NULL;
char **timestamp_buffer = NULL;
char **attribute_dictionary = NULL;

/* ルールプール */
struct asrule_1d rule_pool[Nrulemax];
struct cmrule compare_rules[Nrulemax];

/* 時間遅延統計 */
int delay_usage_history[HISTORY_GENERATIONS][MAX_TIME_DELAY_PHASE1 + 1];
int delay_usage_count[MAX_TIME_DELAY_PHASE1 + 1];
int delay_tracking[MAX_TIME_DELAY_PHASE1 + 1];
double delay_success_rate[MAX_TIME_DELAY_PHASE1 + 1];

/* GNPノード構造（動的割り当て） */
int ***node_structure = NULL;

/* 評価統計配列（動的割り当て） */
int ***match_count = NULL;
int ***negative_count = NULL;
int ***evaluation_count = NULL;
int ***attribute_chain = NULL;
int ***time_delay_chain = NULL;
double ***x_sum = NULL;
double ***x_sigma_array = NULL;

/* 属性使用統計（動的割り当て） */
int **attribute_usage_history = NULL;
int *attribute_usage_count = NULL;
int *attribute_tracking = NULL;

/* 適応度関連 */
double fitness_value[Nkotai];
int fitness_ranking[Nkotai];

/* 遺伝子プール（動的割り当て） */
int **gene_connection = NULL;
int **gene_attribute = NULL;
int **gene_time_delay = NULL;

/* グローバル統計カウンタ */
int total_rule_count = 0;
int total_high_support = 0;
int total_low_variance = 0;

/* その他の作業用配列 */
int rules_per_trial[Ntry];
int new_rule_marker[Nrulemax];
int rules_by_attribute_count[MAX_DEPTH];
int *attribute_set = NULL;

/* X列のインデックス */
int x_column_index = -1;
int t_column_index = -1;

/* ================================================================================
   メモリ管理関数
   ================================================================================
*/

void allocate_dynamic_memory()
{
    int i, j;

    /* データバッファ */
    data_buffer = (int **)malloc(Nrd * sizeof(int *));
    for (i = 0; i < Nrd; i++)
    {
        data_buffer[i] = (int *)malloc(Nzk * sizeof(int));
    }

    x_buffer = (double *)malloc(Nrd * sizeof(double));

    timestamp_buffer = (char **)malloc(Nrd * sizeof(char *));
    for (i = 0; i < Nrd; i++)
    {
        timestamp_buffer[i] = (char *)malloc(20 * sizeof(char));
    }

    /* 属性辞書 */
    attribute_dictionary = (char **)malloc((Nzk + 3) * sizeof(char *));
    for (i = 0; i < Nzk + 3; i++)
    {
        attribute_dictionary[i] = (char *)malloc(MAX_ATTR_NAME * sizeof(char));
    }

    /* GNPノード構造 */
    node_structure = (int ***)malloc(Nkotai * sizeof(int **));
    for (i = 0; i < Nkotai; i++)
    {
        node_structure[i] = (int **)malloc((Npn + Njg) * sizeof(int *));
        for (j = 0; j < (Npn + Njg); j++)
        {
            node_structure[i][j] = (int *)malloc(3 * sizeof(int));
        }
    }

    /* 評価統計配列 */
    match_count = (int ***)malloc(Nkotai * sizeof(int **));
    negative_count = (int ***)malloc(Nkotai * sizeof(int **));
    evaluation_count = (int ***)malloc(Nkotai * sizeof(int **));
    attribute_chain = (int ***)malloc(Nkotai * sizeof(int **));
    time_delay_chain = (int ***)malloc(Nkotai * sizeof(int **));
    x_sum = (double ***)malloc(Nkotai * sizeof(double **));
    x_sigma_array = (double ***)malloc(Nkotai * sizeof(double **));

    for (i = 0; i < Nkotai; i++)
    {
        match_count[i] = (int **)malloc(Npn * sizeof(int *));
        negative_count[i] = (int **)malloc(Npn * sizeof(int *));
        evaluation_count[i] = (int **)malloc(Npn * sizeof(int *));
        attribute_chain[i] = (int **)malloc(Npn * sizeof(int *));
        time_delay_chain[i] = (int **)malloc(Npn * sizeof(int *));
        x_sum[i] = (double **)malloc(Npn * sizeof(double *));
        x_sigma_array[i] = (double **)malloc(Npn * sizeof(double *));

        for (j = 0; j < Npn; j++)
        {
            match_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            negative_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            evaluation_count[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            attribute_chain[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            time_delay_chain[i][j] = (int *)malloc(MAX_DEPTH * sizeof(int));
            x_sum[i][j] = (double *)malloc(MAX_DEPTH * sizeof(double));
            x_sigma_array[i][j] = (double *)malloc(MAX_DEPTH * sizeof(double));
        }
    }

    /* 属性使用統計 */
    attribute_usage_history = (int **)malloc(HISTORY_GENERATIONS * sizeof(int *));
    for (i = 0; i < HISTORY_GENERATIONS; i++)
    {
        attribute_usage_history[i] = (int *)calloc(Nzk, sizeof(int));
    }
    attribute_usage_count = (int *)calloc(Nzk, sizeof(int));
    attribute_tracking = (int *)calloc(Nzk, sizeof(int));

    /* 遺伝子プール */
    gene_connection = (int **)malloc(Nkotai * sizeof(int *));
    gene_attribute = (int **)malloc(Nkotai * sizeof(int *));
    gene_time_delay = (int **)malloc(Nkotai * sizeof(int *));

    for (i = 0; i < Nkotai; i++)
    {
        gene_connection[i] = (int *)malloc((Npn + Njg) * sizeof(int));
        gene_attribute[i] = (int *)malloc((Npn + Njg) * sizeof(int));
        gene_time_delay[i] = (int *)malloc((Npn + Njg) * sizeof(int));
    }

    attribute_set = (int *)malloc((Nzk + 1) * sizeof(int));
}

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

    /* その他の動的メモリ解放（省略せず全て記述） */
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
            }
            free(match_count[i]);
            free(negative_count[i]);
            free(evaluation_count[i]);
            free(attribute_chain[i]);
            free(time_delay_chain[i]);
            free(x_sum[i]);
            free(x_sigma_array[i]);
        }
        free(match_count);
        free(negative_count);
        free(evaluation_count);
        free(attribute_chain);
        free(time_delay_chain);
        free(x_sum);
        free(x_sigma_array);
    }

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
}

/* ================================================================================
   CSVヘッダー読み込み関数（新規追加）
   ================================================================================
*/

void load_csv_with_header()
{
    FILE *file;
    char line[MAX_LINE_LENGTH];
    char *token;
    int row = 0;
    int col = 0;
    int attr_count = 0;

    printf("Loading CSV file with header: %s\n", DATANAME);

    file = fopen(DATANAME, "r");
    if (file == NULL)
    {
        printf("Error: Cannot open data file: %s\n", DATANAME);
        exit(1);
    }

    /* まず行数とカラム数を数える */
    int line_count = 0;
    int column_count = 0;

    while (fgets(line, MAX_LINE_LENGTH, file))
    {
        if (line_count == 0)
        {
            /* ヘッダー行でカラム数を数える */
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

    Nrd = line_count - 1;   /* ヘッダーを除く */
    Nzk = column_count - 2; /* X列とT列を除く */

    printf("Detected %d records, %d attributes\n", Nrd, Nzk);

    /* メモリ割り当て */
    allocate_dynamic_memory();

    /* ファイルを再読み込み */
    rewind(file);

    /* ヘッダー行を読み込み */
    if (fgets(line, MAX_LINE_LENGTH, file))
    {
        col = 0;
        token = strtok(line, ",\n");

        while (token != NULL)
        {
            /* 前後の空白を削除 */
            while (*token == ' ')
                token++;
            char *end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\r' || *end == '\n'))
            {
                *end = '\0';
                end--;
            }

            if (strcmp(token, "X") == 0)
            {
                x_column_index = col;
                printf("Found X column at index %d\n", col);
            }
            else if (strcmp(token, "T") == 0 || strcmp(token, "timestamp") == 0)
            {
                t_column_index = col;
                printf("Found T column at index %d\n", col);
            }
            else
            {
                /* 通常の属性 */
                strcpy(attribute_dictionary[attr_count], token);
                attr_count++;
            }

            token = strtok(NULL, ",\n");
            col++;
        }
    }

    printf("Loaded %d attribute names\n", attr_count);

    /* データ行を読み込み */
    row = 0;
    while (fgets(line, MAX_LINE_LENGTH, file) && row < Nrd)
    {
        col = 0;
        int attr_idx = 0;
        token = strtok(line, ",\n");

        while (token != NULL)
        {
            /* 前後の空白を削除 */
            while (*token == ' ')
                token++;
            char *end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\r' || *end == '\n'))
            {
                *end = '\0';
                end--;
            }

            if (col == x_column_index)
            {
                /* X列の値 */
                x_buffer[row] = atof(token);
            }
            else if (col == t_column_index)
            {
                /* T列の値（タイムスタンプ） */
                strcpy(timestamp_buffer[row], token);
            }
            else
            {
                /* 通常の属性値（0 or 1） */
                data_buffer[row][attr_idx] = atoi(token);
                attr_idx++;
            }

            token = strtok(NULL, ",\n");
            col++;
        }
        row++;
    }

    fclose(file);

    printf("Data loaded successfully:\n");
    printf("  Records: %d\n", Nrd);
    printf("  Attributes: %d\n", Nzk);
    printf("  X range: %.2f to %.2f\n",
           x_buffer[0], x_buffer[Nrd - 1]);
    printf("  Time range: %s to %s\n",
           timestamp_buffer[0], timestamp_buffer[Nrd - 1]);
}

/* ================================================================================
   初期化・設定系関数
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
        printf("*** TIME-SERIES MODE ENABLED (Phase 1) ***\n");
        printf("Single variable X analysis\n");
        printf("Adaptive delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE1);
        printf("Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
        printf("Prediction span: t+%d\n", PREDICTION_SPAN);
        printf("=========================================\n\n");
    }
}

void initialize_rule_pool()
{
    int i, j;

    for (i = 0; i < Nrulemax; i++)
    {
        for (j = 0; j < MAX_ATTRIBUTES; j++)
        {
            rule_pool[i].antecedent_attrs[j] = 0;
            rule_pool[i].time_delays[j] = 0;
            compare_rules[i].antecedent_attrs[j] = 0;
        }

        rule_pool[i].x_mean = 0;
        rule_pool[i].x_sigma = 0;
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
    int i, j;

    for (i = 0; i <= MAX_TIME_DELAY_PHASE1; i++)
    {
        delay_usage_count[i] = 0;
        delay_tracking[i] = INITIAL_DELAY_HISTORY;
        delay_success_rate[i] = 0.5;

        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_usage_history[j][i] = INITIAL_DELAY_HISTORY;
        }
    }
}

void initialize_attribute_statistics()
{
    int i, j;

    for (i = 0; i < Nzk; i++)
    {
        attribute_usage_count[i] = 0;
        attribute_tracking[i] = 0;

        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            attribute_usage_history[j][i] = (j == 0) ? INITIAL_DELAY_HISTORY : 0;
        }
    }

    for (i = 0; i < MAX_DEPTH; i++)
    {
        rules_by_attribute_count[i] = 0;
    }

    for (i = 0; i < Nzk + 1; i++)
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
   データアクセス関数
   ================================================================================
*/

void get_safe_data_range(int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        *start_index = MAX_TIME_DELAY_PHASE1;
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

double get_future_target_value(int current_time)
{
    int future_index = current_time + PREDICTION_SPAN;

    if (future_index >= Nrd)
    {
        return 0.0;
    }

    return x_buffer[future_index];
}

/* ================================================================================
   GNP個体管理系関数
   ================================================================================
*/

void create_initial_population()
{
    int i, j;

    for (i = 0; i < Nkotai; i++)
    {
        for (j = 0; j < (Npn + Njg); j++)
        {
            gene_connection[i][j] = rand() % Njg + Npn;
            gene_attribute[i][j] = rand() % Nzk;
            gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE1 + 1);
        }
    }
}

void copy_genes_to_nodes()
{
    int individual, node;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (node = 0; node < (Npn + Njg); node++)
        {
            node_structure[individual][node][0] = gene_attribute[individual][node];
            node_structure[individual][node][1] = gene_connection[individual][node];
            node_structure[individual][node][2] = gene_time_delay[individual][node];
        }
    }
}

void initialize_individual_statistics()
{
    int individual, k, i;

    for (individual = 0; individual < Nkotai; individual++)
    {
        fitness_value[individual] = (double)individual * FITNESS_INIT_OFFSET;
        fitness_ranking[individual] = 0;

        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < MAX_DEPTH; i++)
            {
                match_count[individual][k][i] = 0;
                attribute_chain[individual][k][i] = 0;
                time_delay_chain[individual][k][i] = 0;
                negative_count[individual][k][i] = 0;
                evaluation_count[individual][k][i] = 0;
                x_sum[individual][k][i] = 0;
                x_sigma_array[individual][k][i] = 0;
            }
        }
    }
}

/* ================================================================================
   ルール評価系関数（1次元版）
   ================================================================================
*/

void evaluate_single_instance(int time_index)
{
    double future_x;
    int current_node_id, depth, match_flag;
    int time_delay, data_index;
    int individual, k;

    future_x = get_future_target_value(time_index);

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
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
    int i;

    get_safe_data_range(&safe_start, &safe_end);

    for (i = safe_start; i < safe_end; i++)
    {
        evaluate_single_instance(i);
    }
}

void calculate_negative_counts()
{
    int individual, k, i;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < Nmx; i++)
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
    int individual, k, j;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (j = 1; j < 9; j++)
            {
                if (match_count[individual][k][j] != 0)
                {
                    x_sum[individual][k][j] /= (double)match_count[individual][k][j];
                    x_sigma_array[individual][k][j] =
                        x_sigma_array[individual][k][j] / (double)match_count[individual][k][j] -
                        x_sum[individual][k][j] * x_sum[individual][k][j];

                    if (x_sigma_array[individual][k][j] < 0)
                    {
                        x_sigma_array[individual][k][j] = 0;
                    }

                    x_sigma_array[individual][k][j] = sqrt(x_sigma_array[individual][k][j]);
                }
            }
        }
    }
}

double calculate_support_value(int matched_count, int negative_count_val)
{
    if (negative_count_val == 0)
    {
        return 0.0;
    }
    return (double)matched_count / (double)negative_count_val;
}

/* ================================================================================
   ルール抽出系関数（1次元版）
   ================================================================================
*/

int check_rule_quality_1d(double sigma_x, double support)
{
    return (sigma_x <= Maxsigx && support >= Minsup);
}

int check_rule_duplication(int *rule_candidate, int rule_count)
{
    int i, j;

    for (i = 0; i < rule_count; i++)
    {
        int is_duplicate = 1;
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
            return 1;
        }
    }
    return 0;
}

void register_new_rule_1d(struct trial_state *state, int *rule_candidate, int *time_delays,
                          double x_mean, double x_sigma,
                          int support_count, int negative_count_val, double support_value)
{
    int idx = state->rule_count;
    int i;

    for (i = 0; i < MAX_ATTRIBUTES; i++)
    {
        rule_pool[idx].antecedent_attrs[i] = rule_candidate[i];
        rule_pool[idx].time_delays[i] = (rule_candidate[i] > 0) ? time_delays[i] : 0;
    }

    rule_pool[idx].x_mean = x_mean;
    rule_pool[idx].x_sigma = x_sigma;
    rule_pool[idx].support_count = support_count;
    rule_pool[idx].negative_count = negative_count_val;

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
    if (x_sigma <= (Maxsigx - LOW_VARIANCE_REDUCTION))
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
    int k;

    for (k = 0; k < num_attributes; k++)
    {
        if (time_delays[k] >= 0 && time_delays[k] <= MAX_TIME_DELAY_PHASE1)
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
    int k, loop_j, i2, j2, k2, k3, i5;

    for (k = 0; k < Npn; k++)
    {
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }

        memset(rule_candidate_pre, 0, sizeof(rule_candidate_pre));
        memset(rule_candidate, 0, sizeof(rule_candidate));
        memset(rule_memo, 0, sizeof(rule_memo));
        memset(time_delay_candidate, 0, sizeof(time_delay_candidate));
        memset(time_delay_memo, 0, sizeof(time_delay_memo));

        for (loop_j = 1; loop_j < 9; loop_j++)
        {
            double sigma_x = x_sigma_array[individual][k][loop_j];
            int matched_count = match_count[individual][k][loop_j];
            double support = calculate_support_value(matched_count,
                                                     negative_count[individual][k][loop_j]);

            if (check_rule_quality_1d(sigma_x, support))
            {
                for (i2 = 1; i2 < 9; i2++)
                {
                    rule_candidate_pre[i2 - 1] = attribute_chain[individual][k][i2];
                    time_delay_candidate[i2 - 1] = time_delay_chain[individual][k][i2];
                }

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

                if (j2 < 9)
                {
                    if (!check_rule_duplication(rule_candidate, state->rule_count))
                    {
                        register_new_rule_1d(state, rule_candidate, time_delay_memo,
                                             x_sum[individual][k][loop_j], sigma_x,
                                             matched_count, negative_count[individual][k][loop_j],
                                             support);

                        rules_by_attribute_count[j2]++;

                        /* 1次元用適応度計算 */
                        fitness_value[individual] +=
                            j2 * FITNESS_ATTRIBUTE_WEIGHT +
                            support * FITNESS_SUPPORT_WEIGHT +
                            FITNESS_SIGMA_WEIGHT / (sigma_x + FITNESS_SIGMA_OFFSET) +
                            FITNESS_NEW_RULE_BONUS;

                        for (k3 = 0; k3 < j2; k3++)
                        {
                            i5 = rule_memo[k3] - 1;
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
                            FITNESS_SIGMA_WEIGHT / (sigma_x + FITNESS_SIGMA_OFFSET);
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
    int individual;

    for (individual = 0; individual < Nkotai; individual++)
    {
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }
        extract_rules_from_individual(state, individual);
    }
}

/* ================================================================================
   進化計算系関数
   ================================================================================
*/

void calculate_fitness_rankings()
{
    int i, j;

    for (i = 0; i < Nkotai; i++)
    {
        fitness_ranking[i] = 0;
        for (j = 0; j < Nkotai; j++)
        {
            if (fitness_value[j] > fitness_value[i])
            {
                fitness_ranking[i]++;
            }
        }
    }
}

void perform_selection()
{
    int i, j, copy, target_idx;

    for (i = 0; i < Nkotai; i++)
    {
        if (fitness_ranking[i] < ELITE_SIZE)
        {
            for (j = 0; j < (Npn + Njg); j++)
            {
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

void perform_crossover()
{
    int i, j, crossover_point, temp;

    for (i = 0; i < CROSSOVER_PAIRS; i++)
    {
        for (j = 0; j < Nkousa; j++)
        {
            crossover_point = rand() % Njg + Npn;

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
    int i, j;

    for (i = 0; i < Nkotai; i++)
    {
        for (j = 0; j < Npn; j++)
        {
            if (rand() % Muratep == 0)
            {
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

void perform_mutation_judgment_nodes()
{
    int i, j;

    for (i = MUTATION_START_40; i < MUTATION_START_80; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
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
    int random_value, accumulated, i;

    if (total_usage <= 0)
    {
        return rand() % (MAX_TIME_DELAY_PHASE1 + 1);
    }

    random_value = rand() % total_usage;
    accumulated = 0;

    for (i = 0; i <= MAX_TIME_DELAY_PHASE1; i++)
    {
        accumulated += delay_tracking[i];
        if (random_value < accumulated)
        {
            return i;
        }
    }

    return MAX_TIME_DELAY_PHASE1;
}

int roulette_wheel_selection_attribute(int total_usage)
{
    int random_value, accumulated, i;

    if (total_usage <= 0)
    {
        return rand() % Nzk;
    }

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

void perform_adaptive_delay_mutation()
{
    int total_delay_usage = 0;
    int i, j;

    for (i = 0; i <= MAX_TIME_DELAY_PHASE1; i++)
    {
        total_delay_usage += delay_tracking[i];
    }

    for (i = MUTATION_START_40; i < MUTATION_START_80; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            {
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE1 + 1);
                }
            }
        }
    }

    for (i = MUTATION_START_80; i < Nkotai; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            {
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE1 + 1);
                }
            }
        }
    }
}

void perform_adaptive_attribute_mutation()
{
    int total_attribute_usage = 0;
    int i, j;

    for (i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    for (i = MUTATION_START_80; i < Nkotai; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratea == 0)
            {
                gene_attribute[i][j] = roulette_wheel_selection_attribute(total_attribute_usage);
            }
        }
    }
}

/* ================================================================================
   統計・履歴管理系関数
   ================================================================================
*/

void update_delay_statistics(int generation)
{
    int total_delay_usage = 0;
    int i, j;

    for (i = 0; i <= MAX_TIME_DELAY_PHASE1; i++)
    {
        delay_tracking[i] = 0;

        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            delay_tracking[i] += delay_usage_history[j][i];
        }

        total_delay_usage += delay_tracking[i];
    }

    for (i = 0; i <= MAX_TIME_DELAY_PHASE1; i++)
    {
        for (j = HISTORY_GENERATIONS - 1; j > 0; j--)
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

void update_attribute_statistics(int generation)
{
    int total_attribute_usage = 0;
    int i, j;

    memset(attribute_usage_count, 0, sizeof(int) * Nzk);
    memset(attribute_tracking, 0, sizeof(int) * Nzk);

    for (i = 0; i < Nzk; i++)
    {
        for (j = 0; j < HISTORY_GENERATIONS; j++)
        {
            attribute_tracking[i] += attribute_usage_history[j][i];
        }
    }

    for (i = 0; i < Nzk; i++)
    {
        for (j = HISTORY_GENERATIONS - 1; j > 0; j--)
        {
            attribute_usage_history[j][i] = attribute_usage_history[j - 1][i];
        }

        attribute_usage_history[0][i] = 0;

        if (generation % REPORT_INTERVAL == 0)
        {
            attribute_usage_history[0][i] = INITIAL_DELAY_HISTORY;
        }
    }

    for (i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    for (i = 0; i < Nkotai; i++)
    {
        for (j = Npn; j < (Npn + Njg); j++)
        {
            attribute_usage_count[gene_attribute[i][j]]++;
        }
    }
}

/* ================================================================================
   ファイル入出力系関数（1次元版）
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
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file, "X_mean\tX_sigma\tSupport\tHighSup\tLowVar\n");
        fclose(file);
    }
}

void write_rule_to_file(struct trial_state *state, int rule_index)
{
    FILE *file = fopen(state->filename_rule, "a");
    int i;

    if (file == NULL)
        return;

    for (i = 0; i < MAX_ATTRIBUTES; i++)
    {
        int attr = rule_pool[rule_index].antecedent_attrs[i];
        int delay = rule_pool[rule_index].time_delays[i];

        if (attr > 0)
        {
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

void write_progress_report(struct trial_state *state, int generation)
{
    FILE *file = fopen(state->filename_report, "a");
    double fitness_average = 0;
    int i;

    if (file == NULL)
        return;

    for (i = 0; i < Nkotai; i++)
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
        for (i = 0; i <= MAX_TIME_DELAY_PHASE1; i++)
        {
            printf("t-%d:%d ", i, delay_tracking[i]);
        }
        printf("\n");
    }
}

void write_local_output(struct trial_state *state)
{
    FILE *file = fopen(state->filename_local, "a");
    int i, j;

    if (file == NULL)
        return;

    for (i = 1; i < state->rule_count; i++)
    {
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

        fprintf(file, "%8.3f\t%5.3f\t%d\t%2d\t%2d\n",
                rule_pool[i].x_mean,
                rule_pool[i].x_sigma,
                rule_pool[i].support_count,
                rule_pool[i].high_support_flag,
                rule_pool[i].low_variance_flag);
    }

    fclose(file);
}

void write_global_pool(struct trial_state *state)
{
    FILE *file_a = fopen(POOL_FILE_A, "w");
    FILE *file_b = fopen(POOL_FILE_B, "w");
    int i, j;

    if (file_a != NULL)
    {
        fprintf(file_a, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file_a, "X_mean\tX_sigma\tSupport\tNegative\tHighSup\tLowVar\n");

        for (i = 1; i < state->rule_count; i++)
        {
            for (j = 0; j < MAX_ATTRIBUTES; j++)
            {
                int attr = rule_pool[i].antecedent_attrs[j];
                int delay = rule_pool[i].time_delays[j];

                if (attr > 0)
                {
                    fprintf(file_a, "%s(t-%d)\t", attribute_dictionary[attr - 1], delay);
                }
                else
                {
                    fprintf(file_a, "0\t");
                }
            }

            fprintf(file_a, "%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                    rule_pool[i].x_mean, rule_pool[i].x_sigma,
                    rule_pool[i].support_count, rule_pool[i].negative_count,
                    rule_pool[i].high_support_flag, rule_pool[i].low_variance_flag);
        }

        fclose(file_a);
    }

    if (file_b != NULL)
    {
        fprintf(file_b, "# Rule Pool Summary\n");
        fprintf(file_b, "# Total Rules: %d\n", state->rule_count - 1);
        fprintf(file_b, "# High Support: %d\n", state->high_support_rule_count - 1);
        fprintf(file_b, "# Low Variance: %d\n", state->low_variance_rule_count - 1);
        fprintf(file_b, "\n");

        for (i = 1; i < state->rule_count && i < 10; i++)
        {
            fprintf(file_b, "Rule %d: ", i);

            for (j = 0; j < MAX_ATTRIBUTES; j++)
            {
                int attr = rule_pool[i].antecedent_attrs[j];
                int delay = rule_pool[i].time_delays[j];

                if (attr > 0)
                {
                    fprintf(file_b, "%s(t-%d) ", attribute_dictionary[attr - 1], delay);
                }
            }

            fprintf(file_b, "=> X_mean=%.3f, X_sigma=%.3f\n",
                    rule_pool[i].x_mean, rule_pool[i].x_sigma);
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
   ユーティリティ系関数
   ================================================================================
*/

void generate_filename(char *filename, const char *prefix, int trial_id)
{
    int digits[FILENAME_DIGITS];
    int temp = trial_id;
    int i;

    for (i = FILENAME_DIGITS - 1; i >= 0; i--)
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
    printf("GNMiner Time-Series Edition Phase 1 Complete!\n");
    printf("========================================\n");
    printf("Time-Series Configuration:\n");
    printf("  Mode: Phase 1 (Single variable X)\n");
    printf("  Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
    printf("  Delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE1);
    printf("  Prediction span: t+%d\n", PREDICTION_SPAN);
    printf("========================================\n");
    printf("Data Statistics:\n");
    printf("  Records: %d\n", Nrd);
    printf("  Attributes: %d\n", Nzk);
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
    int trial, gen, i, prev_count;
    struct trial_state state;

    /* システム初期化 */
    srand(1);
    create_output_directories();

    /* CSVファイルからデータとヘッダーを読み込み */
    load_csv_with_header();

    initialize_global_counters();
    initialize_document_file();

    /* メイン試行ループ */
    for (trial = Nstart; trial < (Nstart + Ntry); trial++)
    {
        /* 試行状態の初期化 */
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
            printf("  [Time-Series Mode Phase 1: Single variable X]\n");
        }

        /* 試行初期化 */
        initialize_rule_pool();
        initialize_delay_statistics();
        initialize_attribute_statistics();
        create_initial_population();
        create_trial_files(&state);

        /* 進化計算ループ */
        for (gen = 0; gen < Generation; gen++)
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
            prev_count = rules_per_trial[trial - Nstart];
            for (i = prev_count + 1; i < state.rule_count; i++)
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

    /* メモリ解放 */
    free_dynamic_memory();

    return 0;
}