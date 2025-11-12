#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* 時系列パラメータ */
#define TIMESERIES_MODE 1
#define MAX_TIME_DELAY 2 // 過去スパン： t-0(これはデフォルト), t-1, t-2
#define MIN_TIME_DELAY 0
#define FUTURE_SPAN 2 // 未来予測スパン: t+1, t+2 の2時点
#define ADAPTIVE_DELAY 1

/* ディレクトリ構造 */
#define OUTPUT_DIR "output"
#define OUTPUT_DIR_IL "output/IL"
#define OUTPUT_DIR_IA "output/IA"
#define OUTPUT_DIR_IB "output/IB"
#define OUTPUT_DIR_POOL "output/pool"
#define OUTPUT_DIR_DOC "output/doc"
#define OUTPUT_DIR_VIS "output/vis"
#define OUTPUT_DIR_VERIFICATION "output/verification"

/* ファイル名 */
#define DATANAME "1-deta-enginnering/forex_data_daily/USDJPY.txt"
#define POOL_FILE_A "output/pool/zrp01a.txt"
#define POOL_FILE_B "output/pool/zrp01b.txt"
#define CONT_FILE "output/doc/zrd01.txt"
#define RESULT_FILE "output/doc/zrmemo01.txt"

/* ルールマイニング制約 - 象限集中方式（v5.0 - シンプル化：0ベース象限判定） */
#define Minsup 0.003                 // 0.3% サポート率（全マッチベース）
#define MIN_SUPPORT_COUNT 20         // 統計的信頼性（最低20回マッチ）
#define QUADRANT_THRESHOLD_RATE 0.50 // 50% 象限集中率（全マッチポイントの50%以上が同じ象限）
#define DEVIATION_THRESHOLD 1.0      // 変化率1.0の逸脱許容（支配象限から反対方向への最大許容逸脱）
#define Maxsigx 999.0                // 最大標準偏差（無効化：事実上全て通過）

#define MIN_ATTRIBUTES 1 // 最小属性数 使わないから後で削除

/* ルール品質フラグ設定の係数 */
#define HIGH_SUPPORT_MULTIPLIER 2.0 // 高サポートフラグの判定係数（Minsup * 2.0）
#define LOW_VARIANCE_MULTIPLIER 0.5 // 低分散フラグの判定係数（Maxsigx * 0.5）

/* 実験パラメータ */
#define Nrulemax 2002
#define Nstart 1000
#define Ntry 1

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

/* 適応度計算パラメータ（original code準拠: シンプルな4成分） */
#define FITNESS_SUPPORT_WEIGHT 10
#define FITNESS_SIGMA_WEIGHT 2
#define FITNESS_SIGMA_OFFSET 0.001
#define FITNESS_NEW_RULE_BONUS 20

/* レポート間隔 */
#define REPORT_INTERVAL 5
#define DELAY_DISPLAY_INTERVAL 20
#define FILENAME_BUFFER_SIZE 256
#define FILENAME_DIGITS 5
#define FITNESS_INIT_OFFSET -0.00001
#define INITIAL_DELAY_HISTORY 1
#define REFRESH_BONUS 2

/* 構造体定義 */

/* 時系列ルール構造体 */
struct temporal_rule
{
    int antecedent_attrs[MAX_ATTRIBUTES];
    int time_delays[MAX_ATTRIBUTES];

    double future_mean[FUTURE_SPAN];
    double future_sigma[FUTURE_SPAN];
    double future_min[FUTURE_SPAN]; // MinX統計
    double future_max[FUTURE_SPAN]; // MaxX統計

    int support_count;
    int negative_count;
    double support_rate;
    int high_support_flag;
    int low_variance_flag;
    int num_attributes;

    int *matched_indices;
    int matched_count_vis;
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
    double elapsed_time;

    char filename_rule[FILENAME_BUFFER_SIZE];
    char filename_report[FILENAME_BUFFER_SIZE];
    char filename_local[FILENAME_BUFFER_SIZE];
};

/* グローバル変数 */

char stock_code[20] = "BTC";
char data_file_path[512] = DATANAME;
char output_base_dir[256] = "output";
char pool_file_a[512] = POOL_FILE_A;
char pool_file_b[512] = POOL_FILE_B;
char cont_file[512] = CONT_FILE;
char output_dir_il[512] = OUTPUT_DIR_IL;
char output_dir_ia[512] = OUTPUT_DIR_IA;
char output_dir_ib[512] = OUTPUT_DIR_IB;
char output_dir_pool[512] = OUTPUT_DIR_POOL;
char output_dir_doc[512] = OUTPUT_DIR_DOC;
char output_dir_vis[512] = OUTPUT_DIR_VIS;
char output_dir_verification[512] = OUTPUT_DIR_VERIFICATION;

int Nrd = 0;
int Nzk = 0;

int **data_buffer = NULL;
double *x_buffer = NULL;
char **timestamp_buffer = NULL;
char **attribute_dictionary = NULL;

struct temporal_rule rule_pool[Nrulemax];
struct cmrule compare_rules[Nrulemax];

struct temporal_rule *global_rule_pool = NULL;
struct cmrule *global_compare_rules = NULL;
int global_rule_count = 0;
int global_high_support_count = 0;
int global_low_variance_count = 0;

int delay_usage_history[HISTORY_GENERATIONS][MAX_TIME_DELAY + 1];
int delay_usage_count[MAX_TIME_DELAY + 1];
int delay_tracking[MAX_TIME_DELAY + 1];

int ***node_structure = NULL;

int ***match_count = NULL;
int ***negative_count = NULL;
int ***evaluation_count = NULL;
int ***attribute_chain = NULL;
int ***time_delay_chain = NULL;

double ****future_sum = NULL;
double ****future_sigma_array = NULL;
double ****future_min = NULL;
double ****future_max = NULL;

double ****future_positive_sum = NULL;
double ****future_negative_sum = NULL;
int ****future_positive_count = NULL;
int ****future_negative_count = NULL;

int ****quadrant_count = NULL;

int **attribute_usage_history = NULL;
int *attribute_usage_count = NULL;
int *attribute_tracking = NULL;

double fitness_value[Nkotai];
int fitness_ranking[Nkotai];

int **gene_connection = NULL;
int **gene_attribute = NULL;
int **gene_time_delay = NULL;

int total_rule_count = 0;
int total_high_support = 0;
int total_low_variance = 0;

/* フィルタ統計カウンタ */
int filter_rejected_by_minsup = 0;
int filter_rejected_by_min_attributes = 0;
int filter_rejected_by_min_support_count = 0;
int filter_rejected_by_quadrant = 0;
int filter_rejected_by_quadrant_rate = 0;      // QUADRANT_THRESHOLD_RATEで却下（集中率不足）
int filter_rejected_by_quadrant_deviation = 0; // 支配象限からの逸脱で却下
int filter_rejected_by_maxsigx = 0;
int filter_passed_total = 0;
int filter_passed_duplicates = 0; // フィルター通過したが重複していたルール数

int rules_per_trial[Ntry];
int new_rule_marker[Nrulemax];
int rules_by_attribute_count[MAX_DEPTH];
int *attribute_set = NULL;

int x_column_index = -1;
int t_column_index = -1;

int rules_by_min_attributes = 0;

/* 関数プロトタイプ宣言 */
double get_future_value(int row_idx, int offset);

static void clear_3d_array_int(int ***arr, int dim1, int dim2, int dim3);
static void clear_4d_array_int(int ****arr, int dim1, int dim2, int dim3, int dim4);
static void clear_4d_array_double(double ****arr, int dim1, int dim2, int dim3, int dim4);
static int is_valid_time_index(int current_time, int delay);
static int is_valid_future_index(int current_time, int offset);
static int determine_quadrant_simple(double value1, double value2);

static void accumulate_future_statistics(int individual, int k, int depth, int time_index);
static void update_quadrant_statistics(int individual, int k, int depth, int time_index);

double calculate_concentration_ratio(int *quadrant_counts);

static double calculate_standard_deviation(double sum, double sum_of_squares, int count);

/* MinMax象限判定関数（v2.0） */
static double calculate_minimum_return(int *matched_indices, int match_count, int span_offset);
static double calculate_maximum_return(int *matched_indices, int match_count, int span_offset);
/* 割合ベース象限判定関数（v5.0 - 0ベース） */
static int determine_quadrant_by_rate_with_concentration(int *matched_indices, int match_count, double *concentration_out, int *in_quadrant_count_out);
static int rematch_rule_pattern(int *rule_attributes, int *time_delays, int num_attributes, int *matched_indices_out);
void get_safe_data_range(int *safe_start, int *safe_end);

/* ヘルパー関数実装 */

static void clear_3d_array_int(int ***arr, int dim1, int dim2, int dim3)
{
    int i, j;
    for (i = 0; i < dim1; i++)
    {
        for (j = 0; j < dim2; j++)
        {
            memset(arr[i][j], 0, dim3 * sizeof(int));
        }
    }
}

static void clear_4d_array_int(int ****arr, int dim1, int dim2, int dim3, int dim4)
{
    int i, j, k;
    for (i = 0; i < dim1; i++)
    {
        for (j = 0; j < dim2; j++)
        {
            for (k = 0; k < dim3; k++)
            {
                memset(arr[i][j][k], 0, dim4 * sizeof(int));
            }
        }
    }
}

static void clear_4d_array_double(double ****arr, int dim1, int dim2, int dim3, int dim4)
{
    int i, j, k;
    for (i = 0; i < dim1; i++)
    {
        for (j = 0; j < dim2; j++)
        {
            for (k = 0; k < dim3; k++)
            {
                memset(arr[i][j][k], 0, dim4 * sizeof(double));
            }
        }
    }
}

__attribute__((unused)) static int is_valid_time_index(int current_time, int delay)
{
    int data_index = current_time - delay;
    return (data_index >= 0 && data_index < Nrd);
}

__attribute__((unused)) static int is_valid_future_index(int current_time, int offset)
{
    int future_index = current_time + offset;
    return (future_index >= 0 && future_index < Nrd);
}

static int determine_quadrant_simple(double value1, double value2)
{
    if (value1 > 0.0 && value2 > 0.0)
        return 0; // Q1: 両方正
    if (value1 < 0.0 && value2 > 0.0)
        return 1; // Q2: t+1負, t+2正
    if (value1 < 0.0 && value2 < 0.0)
        return 2; // Q3: 両方負
    if (value1 > 0.0 && value2 < 0.0)
        return 3; // Q4: t+1正, t+2負
    return -1;    // どちらかが0（無効）
}

static void accumulate_future_statistics(int individual, int k, int depth, int time_index)
{
    int offset;
    double future_val;

    for (offset = 0; offset < FUTURE_SPAN; offset++)
    {
        future_val = get_future_value(time_index, offset + 1);
        if (isnan(future_val))
            continue;

        /* 既存の統計（後方互換性のため維持） */
        future_sum[individual][k][depth][offset] += future_val;
        future_sigma_array[individual][k][depth][offset] += future_val * future_val;

        /* Min/Max の更新（象限判定用） */
        if (future_val < future_min[individual][k][depth][offset])
        {
            future_min[individual][k][depth][offset] = future_val;
        }
        if (future_val > future_max[individual][k][depth][offset])
        {
            future_max[individual][k][depth][offset] = future_val;
        }

        /* 方向性分離統計の累積 */
        if (future_val > 0.0)
        {
            future_positive_sum[individual][k][depth][offset] += future_val;
            future_positive_count[individual][k][depth][offset]++;
        }
        else if (future_val < 0.0)
        {
            future_negative_sum[individual][k][depth][offset] += future_val;
            future_negative_count[individual][k][depth][offset]++;
        }
        /* future_val == 0.0 の場合は何もしない */
    }
}

static void update_quadrant_statistics(int individual, int k, int depth, int time_index)
{
    double future_t1 = get_future_value(time_index, 1); // t+1
    double future_t2 = get_future_value(time_index, 2); // t+2
    int quadrant;

    if (isnan(future_t1) || isnan(future_t2))
        return;

    quadrant = determine_quadrant_simple(future_t1, future_t2);
    if (quadrant >= 0)
    {
        quadrant_count[individual][k][depth][quadrant]++;
    }
}

__attribute__((unused)) static double calculate_standard_deviation(double sum, double sum_of_squares, int count)
{
    double mean, variance;

    if (count < 2)
        return 0.0; // サンプル数不足

    mean = sum / count;
    variance = (sum_of_squares / count) - (mean * mean);

    if (variance < 0.0)
        variance = 0.0; // 数値誤差対策

    return sqrt(variance);
}

/* MinMax象限判定関数実装（v2.0） */

__attribute__((unused)) static double calculate_minimum_return(int *matched_indices, int match_count, int span_offset)
{
    double min_val = INFINITY;
    int i;
    double future_val;

    for (i = 0; i < match_count; i++)
    {
        future_val = get_future_value(matched_indices[i], span_offset);
        if (!isnan(future_val) && future_val < min_val)
        {
            min_val = future_val;
        }
    }

    return (min_val == INFINITY) ? 0.0 : min_val;
}

__attribute__((unused)) static double calculate_maximum_return(int *matched_indices, int match_count, int span_offset)
{
    double max_val = -INFINITY;
    int i;
    double future_val;

    for (i = 0; i < match_count; i++)
    {
        future_val = get_future_value(matched_indices[i], span_offset);
        if (!isnan(future_val) && future_val > max_val)
        {
            max_val = future_val;
        }
    }

    return (max_val == -INFINITY) ? 0.0 : max_val;
}

/* 割合ベース象限判定（集中率も返すバージョン） */
static int determine_quadrant_by_rate_with_concentration(int *matched_indices, int match_count, double *concentration_out, int *in_quadrant_count_out)
{
    int quadrant_counts[4] = {0, 0, 0, 0}; // Q1, Q2, Q3, Q4
    int i;
    double future_t1, future_t2;
    int total_valid_matches = 0;

    /* Step 1: 象限分布を集計（全マッチポイント - 閾値なし） */
    for (i = 0; i < match_count; i++)
    {
        future_t1 = get_future_value(matched_indices[i], 1);
        future_t2 = get_future_value(matched_indices[i], 2);

        if (isnan(future_t1) || isnan(future_t2))
            continue;

        total_valid_matches++;

        /* 象限判定（0ベース - QUADRANT_THRESHOLD削除） */
        if (future_t1 >= 0.0 && future_t2 >= 0.0)
        {
            quadrant_counts[0]++; // Q1 (++)
        }
        else if (future_t1 < 0.0 && future_t2 >= 0.0)
        {
            quadrant_counts[1]++; // Q2 (-+)
        }
        else if (future_t1 < 0.0 && future_t2 < 0.0)
        {
            quadrant_counts[2]++; // Q3 (--)
        }
        else // future_t1 >= 0.0 && future_t2 < 0.0
        {
            quadrant_counts[3]++; // Q4 (+-)
        }
    }

    /* 象限内マッチ数を返す（全有効マッチ = 象限内マッチ） */
    if (in_quadrant_count_out != NULL)
    {
        *in_quadrant_count_out = total_valid_matches;
    }

    if (total_valid_matches == 0)
    {
        *concentration_out = 0.0;
        return 0;
    }

    /* Step 2: 支配象限を確定 */
    int max_count = 0;
    int dominant_quadrant = 0;

    for (i = 0; i < 4; i++)
    {
        if (quadrant_counts[i] > max_count)
        {
            max_count = quadrant_counts[i];
            dominant_quadrant = i + 1;
        }
    }

    /* Step 3: 集中度を計算（NEW: 全マッチポイントに対する割合） */
    double concentration_rate = (double)max_count / (double)total_valid_matches;
    *concentration_out = concentration_rate;

    /* Step 4: 集中度チェック */
    if (concentration_rate < QUADRANT_THRESHOLD_RATE)
    {
        filter_rejected_by_quadrant_rate++;
        return 0;
    }

    /* Step 5: 支配象限からの逸脱チェック（1.0%許容） */
    for (i = 0; i < match_count; i++)
    {
        future_t1 = get_future_value(matched_indices[i], 1);
        future_t2 = get_future_value(matched_indices[i], 2);

        if (isnan(future_t1) || isnan(future_t2))
            continue;

        // 支配象限に応じた逸脱チェック（1.0%まで許容）
        if (dominant_quadrant == 1) // Q1(++): 両方が正の領域
        {
            if (future_t1 < -DEVIATION_THRESHOLD || future_t2 < -DEVIATION_THRESHOLD)
            {
                filter_rejected_by_quadrant_deviation++;
                return 0; // Q1支配なのに負の領域に1.0%以上逸脱
            }
        }
        else if (dominant_quadrant == 2) // Q2(-+): t1が負、t2が正
        {
            if (future_t1 > +DEVIATION_THRESHOLD || future_t2 < -DEVIATION_THRESHOLD)
            {
                filter_rejected_by_quadrant_deviation++;
                return 0; // Q2支配なのに1.0%以上逸脱
            }
        }
        else if (dominant_quadrant == 3) // Q3(--): 両方が負の領域
        {
            if (future_t1 > +DEVIATION_THRESHOLD || future_t2 > +DEVIATION_THRESHOLD)
            {
                filter_rejected_by_quadrant_deviation++;
                return 0; // Q3支配なのに正の領域に1.0%以上逸脱
            }
        }
        else if (dominant_quadrant == 4) // Q4(+-): t1が正、t2が負
        {
            if (future_t1 < -DEVIATION_THRESHOLD || future_t2 > +DEVIATION_THRESHOLD)
            {
                filter_rejected_by_quadrant_deviation++;
                return 0; // Q4支配なのに1.0%以上逸脱
            }
        }
    }

    /* 全チェック通過 */
    return dominant_quadrant;
}

/* ルールパターンを再マッチングして matched_indices を取得するヘルパー関数 */
static int rematch_rule_pattern(int *rule_attributes, int *time_delays, int num_attributes, int *matched_indices_out)
{
    int matched_count = 0;
    int time_index, attr_idx, is_match;
    int safe_start, safe_end;

    /* 安全な範囲を取得（未来予測値が取得できる範囲に限定） */
    get_safe_data_range(&safe_start, &safe_end);

    /* 安全範囲のデータポイントのみをスキャン */
    for (time_index = safe_start; time_index < safe_end; time_index++)
    {
        is_match = 1;

        /* 各属性をチェック */
        for (attr_idx = 0; attr_idx < num_attributes; attr_idx++)
        {
            if (rule_attributes[attr_idx] > 0)
            {
                /* 属性ID（1-indexed → 0-indexed に変換） */
                int attr_id = rule_attributes[attr_idx] - 1;
                int delay = time_delays[attr_idx];
                int data_index = time_index - delay;

                /* 範囲チェック */
                if (data_index < 0 || data_index >= Nrd)
                {
                    is_match = 0;
                    break;
                }

                /* 属性値チェック（1である必要がある） */
                if (data_buffer[data_index][attr_id] != 1)
                {
                    is_match = 0;
                    break;
                }
            }
        }

        /* マッチした場合、インデックスを保存 */
        if (is_match == 1)
        {
            matched_indices_out[matched_count] = time_index;
            matched_count++;
        }
    }

    return matched_count;
}

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

    /* future_min配列の割り当て（MinMax象限判定用） */
    future_min = (double ****)malloc(Nkotai * sizeof(double ***));
    if (future_min == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_min (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_min[i] = (double ***)malloc(Npn * sizeof(double **));
        if (future_min[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_min (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_min[i][j] = (double **)malloc(MAX_DEPTH * sizeof(double *));
            if (future_min[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_min (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_min[i][j][k] = (double *)malloc(FUTURE_SPAN * sizeof(double));
                if (future_min[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_min (dimension 4)\n");
                    exit(1);
                }
                // Initialize with INFINITY
                for (int s = 0; s < FUTURE_SPAN; s++)
                {
                    future_min[i][j][k][s] = INFINITY;
                }
            }
        }
    }

    /* future_max配列の割り当て（MinMax象限判定用） */
    future_max = (double ****)malloc(Nkotai * sizeof(double ***));
    if (future_max == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_max (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_max[i] = (double ***)malloc(Npn * sizeof(double **));
        if (future_max[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_max (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_max[i][j] = (double **)malloc(MAX_DEPTH * sizeof(double *));
            if (future_max[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_max (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_max[i][j][k] = (double *)malloc(FUTURE_SPAN * sizeof(double));
                if (future_max[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_max (dimension 4)\n");
                    exit(1);
                }
                // Initialize with -INFINITY
                for (int s = 0; s < FUTURE_SPAN; s++)
                {
                    future_max[i][j][k][s] = -INFINITY;
                }
            }
        }
    }

    /* future_positive_sum配列の割り当て（方向性分離統計） */
    future_positive_sum = (double ****)malloc(Nkotai * sizeof(double ***));
    if (future_positive_sum == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_positive_sum (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_positive_sum[i] = (double ***)malloc(Npn * sizeof(double **));
        if (future_positive_sum[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_positive_sum (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_positive_sum[i][j] = (double **)malloc(MAX_DEPTH * sizeof(double *));
            if (future_positive_sum[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_positive_sum (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_positive_sum[i][j][k] = (double *)calloc(FUTURE_SPAN, sizeof(double));
                if (future_positive_sum[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_positive_sum (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }

    /* future_negative_sum配列の割り当て（方向性分離統計） */
    future_negative_sum = (double ****)malloc(Nkotai * sizeof(double ***));
    if (future_negative_sum == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_negative_sum (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_negative_sum[i] = (double ***)malloc(Npn * sizeof(double **));
        if (future_negative_sum[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_negative_sum (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_negative_sum[i][j] = (double **)malloc(MAX_DEPTH * sizeof(double *));
            if (future_negative_sum[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_negative_sum (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_negative_sum[i][j][k] = (double *)calloc(FUTURE_SPAN, sizeof(double));
                if (future_negative_sum[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_negative_sum (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }

    /* future_positive_count配列の割り当て（方向性分離統計・int型） */
    future_positive_count = (int ****)malloc(Nkotai * sizeof(int ***));
    if (future_positive_count == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_positive_count (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_positive_count[i] = (int ***)malloc(Npn * sizeof(int **));
        if (future_positive_count[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_positive_count (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_positive_count[i][j] = (int **)malloc(MAX_DEPTH * sizeof(int *));
            if (future_positive_count[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_positive_count (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_positive_count[i][j][k] = (int *)calloc(FUTURE_SPAN, sizeof(int));
                if (future_positive_count[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_positive_count (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }

    /* future_negative_count配列の割り当て（方向性分離統計・int型） */
    future_negative_count = (int ****)malloc(Nkotai * sizeof(int ***));
    if (future_negative_count == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate future_negative_count (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        future_negative_count[i] = (int ***)malloc(Npn * sizeof(int **));
        if (future_negative_count[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate future_negative_count (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            future_negative_count[i][j] = (int **)malloc(MAX_DEPTH * sizeof(int *));
            if (future_negative_count[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate future_negative_count (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                future_negative_count[i][j][k] = (int *)calloc(FUTURE_SPAN, sizeof(int));
                if (future_negative_count[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate future_negative_count (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }

    /* quadrant_count配列の割り当て（象限集中度統計・int型） */
    quadrant_count = (int ****)malloc(Nkotai * sizeof(int ***));
    if (quadrant_count == NULL)
    {
        fprintf(stderr, "ERROR: Failed to allocate quadrant_count (dimension 1)\n");
        exit(1);
    }

    for (i = 0; i < Nkotai; i++)
    {
        quadrant_count[i] = (int ***)malloc(Npn * sizeof(int **));
        if (quadrant_count[i] == NULL)
        {
            fprintf(stderr, "ERROR: Failed to allocate quadrant_count (dimension 2)\n");
            exit(1);
        }

        for (j = 0; j < Npn; j++)
        {
            quadrant_count[i][j] = (int **)malloc(MAX_DEPTH * sizeof(int *));
            if (quadrant_count[i][j] == NULL)
            {
                fprintf(stderr, "ERROR: Failed to allocate quadrant_count (dimension 3)\n");
                exit(1);
            }

            for (k = 0; k < MAX_DEPTH; k++)
            {
                quadrant_count[i][j][k] = (int *)calloc(4, sizeof(int)); // 4象限
                if (quadrant_count[i][j][k] == NULL)
                {
                    fprintf(stderr, "ERROR: Failed to allocate quadrant_count (dimension 4)\n");
                    exit(1);
                }
            }
        }
    }
}

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

                    /* 方向性分離統計の初期化 */
                    future_positive_sum[i][j][k][offset] = 0.0;
                    future_negative_sum[i][j][k][offset] = 0.0;
                    future_positive_count[i][j][k][offset] = 0;
                    future_negative_count[i][j][k][offset] = 0;
                }

                /* 象限集中度統計の初期化 */
                for (offset = 0; offset < 4; offset++)
                {
                    quadrant_count[i][j][k][offset] = 0;
                }
            }
        }
    }
}

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

    /* future_positive_sum配列の解放（方向性分離統計） */
    if (future_positive_sum != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(future_positive_sum[i][j][k]);
                }
                free(future_positive_sum[i][j]);
            }
            free(future_positive_sum[i]);
        }
        free(future_positive_sum);
    }

    /* future_negative_sum配列の解放（方向性分離統計） */
    if (future_negative_sum != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(future_negative_sum[i][j][k]);
                }
                free(future_negative_sum[i][j]);
            }
            free(future_negative_sum[i]);
        }
        free(future_negative_sum);
    }

    /* future_positive_count配列の解放（方向性分離統計・int型） */
    if (future_positive_count != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(future_positive_count[i][j][k]);
                }
                free(future_positive_count[i][j]);
            }
            free(future_positive_count[i]);
        }
        free(future_positive_count);
    }

    /* future_negative_count配列の解放（方向性分離統計・int型） */
    if (future_negative_count != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(future_negative_count[i][j][k]);
                }
                free(future_negative_count[i][j]);
            }
            free(future_negative_count[i]);
        }
        free(future_negative_count);
    }

    /* quadrant_count配列の解放（象限集中度統計・int型） */
    if (quadrant_count != NULL)
    {
        for (i = 0; i < Nkotai; i++)
        {
            for (j = 0; j < Npn; j++)
            {
                for (k = 0; k < MAX_DEPTH; k++)
                {
                    free(quadrant_count[i][j][k]);
                }
                free(quadrant_count[i][j]);
            }
            free(quadrant_count[i]);
        }
        free(quadrant_count);
    }
}

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
            strncpy(temp_line, line, MAX_LINE_LENGTH - 1);
            temp_line[MAX_LINE_LENGTH - 1] = '\0'; // NULL終端を保証
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
                strncpy(attribute_dictionary[attr_count], token, MAX_ATTR_NAME - 1);
                attribute_dictionary[attr_count][MAX_ATTR_NAME - 1] = '\0'; // NULL終端を保証
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
        printf("Future span: t+1 to t+%d\n", FUTURE_SPAN);
        printf("Minimum attributes: %d\n", MIN_ATTRIBUTES);
        printf("=========================================\n\n");
    }
}

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
            rule_pool[i].future_min[k] = 0.0;
            rule_pool[i].future_max[k] = 0.0;
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

void initialize_global_counters()
{
    total_rule_count = 0;
    total_high_support = 0;
    total_low_variance = 0;
}

void get_safe_data_range(int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        // 時系列モード：過去参照と未来予測の両方を考慮
        // 開始: MAX_TIME_DELAY 以降（過去参照が可能な範囲）
        // 終了: Nrd - FUTURE_SPAN 未満（t+1, t+2 が両方存在する範囲）
        *start_index = MAX_TIME_DELAY;
        *end_index = Nrd - FUTURE_SPAN; // t+1 と t+2 が両方存在する範囲まで
    }
    else
    {
        // 通常モード：全データを使用
        *start_index = 0;
        *end_index = Nrd;
    }
}

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

double get_future_target_value(int current_time)
{
    // 将来のインデックスを計算（t+1）
    // 注: この関数は現在未使用。get_future_value() を使用すること。
    int future_index = current_time + 1;

    // 範囲チェック
    if (future_index >= Nrd)
    {
        return 0.0; // 範囲外
    }

    return x_buffer[future_index];
}

double get_current_actual_value(int current_time)
{
    // 範囲チェック
    if (current_time < 0 || current_time >= Nrd)
    {
        return 0.0; // 範囲外
    }

    return x_buffer[current_time];
}

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

void initialize_individual_statistics()
{
    int individual;

    // 適応度配列を初期化
    for (individual = 0; individual < Nkotai; individual++)
    {
        fitness_value[individual] = (double)individual * FITNESS_INIT_OFFSET;
        fitness_ranking[individual] = 0;
    }

    // 3次元配列の一括初期化（Phase 1: Refactoring）
    clear_3d_array_int(match_count, Nkotai, Npn, MAX_DEPTH);
    clear_3d_array_int(attribute_chain, Nkotai, Npn, MAX_DEPTH);
    clear_3d_array_int(time_delay_chain, Nkotai, Npn, MAX_DEPTH);
    clear_3d_array_int(negative_count, Nkotai, Npn, MAX_DEPTH);
    clear_3d_array_int(evaluation_count, Nkotai, Npn, MAX_DEPTH);

    // 4次元配列の一括初期化（Phase 1: Refactoring）
    clear_4d_array_double(future_sum, Nkotai, Npn, MAX_DEPTH, FUTURE_SPAN);
    clear_4d_array_double(future_sigma_array, Nkotai, Npn, MAX_DEPTH, FUTURE_SPAN);
    clear_4d_array_double(future_positive_sum, Nkotai, Npn, MAX_DEPTH, FUTURE_SPAN);
    clear_4d_array_double(future_negative_sum, Nkotai, Npn, MAX_DEPTH, FUTURE_SPAN);
    clear_4d_array_int(future_positive_count, Nkotai, Npn, MAX_DEPTH, FUTURE_SPAN);
    clear_4d_array_int(future_negative_count, Nkotai, Npn, MAX_DEPTH, FUTURE_SPAN);
    clear_4d_array_int(quadrant_count, Nkotai, Npn, MAX_DEPTH, 4);
}

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

                        /* 統計更新（Phase 2: Refactoring） */
                        update_quadrant_statistics(individual, k, depth, time_index);
                        accumulate_future_statistics(individual, k, depth, time_index);
                    }
                    evaluation_count[individual][k][depth]++;
                    // 次の判定ノードへ
                    current_node_id = node_structure[individual][current_node_id][1];
                }
                else if (attr_value == 0)
                {
                    // 属性値が0：スキップ側へ進む（処理ノードに戻る）
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

void calculate_negative_counts()
{
    int individual, k, i;

    for (individual = 0; individual < Nkotai; individual++)
    {
        for (k = 0; k < Npn; k++)
        {
            for (i = 0; i < Nmx; i++)
            {
                negative_count[individual][k][i] = match_count[individual][k][0];
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

                        /* 方向性分離統計の計算 */
                        int pos_count = future_positive_count[individual][k][j][offset];
                        int neg_count = future_negative_count[individual][k][j][offset];

                        // 正の平均を計算（上昇時の平均）
                        if (pos_count > 0)
                        {
                            future_positive_sum[individual][k][j][offset] /= (double)pos_count;
                            // future_positive_sum を平均値で上書き（既存のfuture_sumと同様）
                        }
                        else
                        {
                            future_positive_sum[individual][k][j][offset] = 0.0;
                        }

                        // 負の平均を計算（下落時の平均）
                        if (neg_count > 0)
                        {
                            future_negative_sum[individual][k][j][offset] /= (double)neg_count;
                            // future_negative_sum を平均値で上書き
                        }
                        else
                        {
                            future_negative_sum[individual][k][j][offset] = 0.0;
                        }

                        /* 注: 勝率と損益比はルール登録時に計算（register_new_rule関数内） */
                    }
                }
            }
        }
    }
}

double calculate_support_value(int matched_count, int negative_count_val)
{
    if (negative_count_val == 0)
    {
        return 0.0; // ゼロ除算防止
    }
    return (double)matched_count / (double)negative_count_val;
}

double calculate_concentration_ratio(int *quadrant_counts)
{
    int q1 = quadrant_counts[0];
    int q2 = quadrant_counts[1];
    int q3 = quadrant_counts[2];
    int q4 = quadrant_counts[3];
    int total = q1 + q2 + q3 + q4;

    if (total == 0)
        return 0.0;

    // 最大象限を見つける
    int max_count = q1;
    if (q2 > max_count)
        max_count = q2;
    if (q3 > max_count)
        max_count = q3;
    if (q4 > max_count)
        max_count = q4;

    return (double)max_count / total;
}

int check_rule_quality(double *future_sigma_array, double *future_mean_array,
                       double support, int num_attributes,
                       double *future_min_array, double *future_max_array, int matched_count,
                       int *rule_attributes, int *time_delays)
{
    int quadrant;
    int i;
    static int debug_sample_count = 0;
    int should_debug = (debug_sample_count < 50); // 最初の50件のみデバッグ出力

    /* 最小属性数チェック（事前チェック） */
    if (num_attributes < MIN_ATTRIBUTES)
    {
        filter_rejected_by_min_attributes++;
        if (should_debug)
        {
            fprintf(stderr, "[FILTER] Rejected by MIN_ATTRIBUTES: attrs=%d < MIN=%d\n",
                    num_attributes, MIN_ATTRIBUTES);
            debug_sample_count++;
        }
        return 0;
    }

    /* Stage 1: 象限判定（NEW: 集中度を先にチェック） */
    // マッチングを再実行して matched_indices を取得
    int *matched_indices = (int *)malloc(Nrd * sizeof(int));
    if (matched_indices == NULL)
    {
        fprintf(stderr, "[ERROR] Failed to allocate memory for matched_indices\n");
        return 0;
    }

    int actual_matched_count = rematch_rule_pattern(rule_attributes, time_delays, num_attributes, matched_indices);

    // マッチ数の整合性チェック（デバッグ用）
    if (actual_matched_count != matched_count && should_debug)
    {
        fprintf(stderr, "[WARNING] Match count mismatch: expected=%d, actual=%d\n",
                matched_count, actual_matched_count);
    }

    // 集中度と象限内マッチ数を計算
    double concentration_rate = 0.0;
    int in_quadrant_count = 0;

    quadrant = determine_quadrant_by_rate_with_concentration(matched_indices, actual_matched_count, &concentration_rate, &in_quadrant_count);

    if (quadrant == 0)
    {
        // カウントは determine_quadrant_by_rate_with_concentration() 内で実施済み
        if (should_debug)
        {
            fprintf(stderr, "[FILTER] Rejected by Quadrant: concentration rate=%.1f%% < threshold=%.1f%%\n",
                    concentration_rate * 100.0, QUADRANT_THRESHOLD_RATE * 100.0);
            fprintf(stderr, "  In-quadrant matches: %d / All matches: %d\n",
                    in_quadrant_count, actual_matched_count);
            debug_sample_count++;
        }
        free(matched_indices); // メモリ解放
        return 0;              // 集中率が閾値未満 → 除外
    }

    free(matched_indices); // メモリ解放

    /* Stage 2: サポート率チェック（全マッチベース） */
    double support_rate = (double)actual_matched_count / (double)Nrd;

    if (support_rate < Minsup)
    {
        filter_rejected_by_minsup++;
        if (should_debug)
        {
            fprintf(stderr, "[FILTER] Rejected by Support Rate: %.4f%% < Minsup=%.4f%%\n",
                    support_rate * 100.0, Minsup * 100.0);
            fprintf(stderr, "  All matches: %d, In-quadrant: %d (%.1f%%)\n",
                    actual_matched_count, in_quadrant_count, concentration_rate * 100.0);
            debug_sample_count++;
        }
        return 0;
    }

    if (actual_matched_count < MIN_SUPPORT_COUNT)
    {
        filter_rejected_by_min_support_count++;
        if (should_debug)
        {
            fprintf(stderr, "[FILTER] Rejected by MIN_SUPPORT_COUNT: matched count=%d < MIN=%d\n",
                    actual_matched_count, MIN_SUPPORT_COUNT);
            debug_sample_count++;
        }
        return 0;
    }

    /* Stage 3: 分散チェック（全未来スパン） */
    for (i = 0; i < FUTURE_SPAN; i++)
    {
        if (future_sigma_array[i] > Maxsigx)
        {
            filter_rejected_by_maxsigx++;
            if (should_debug)
            {
                fprintf(stderr, "[FILTER] Rejected by Maxsigx: sigma[t+%d]=%.4f%% > Maxsigx=%.4f%%\n",
                        i + 1, future_sigma_array[i] * 100.0, Maxsigx * 100.0);
                debug_sample_count++;
            }
            return 0; // 高分散 → 除外
        }
    }

    // すべてのフィルタをパス
    filter_passed_total++;
    if (should_debug)
    {
        fprintf(stderr, "[FILTER] ✓ PASSED: support=%.4f%%, concentration=%.1f%%, attrs=%d, quadrant=Q%d\n",
                support_rate * 100.0, concentration_rate * 100.0, num_attributes, quadrant);
        fprintf(stderr, "  All matches: %d (in-quadrant: %d), mean_t1=%.4f%%, sigma_t1=%.4f%%\n",
                actual_matched_count, in_quadrant_count,
                future_mean_array[0], future_sigma_array[0]);
        debug_sample_count++;
    }
    return 1;
}

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

void register_new_rule(struct trial_state *state, int *rule_candidate, int *time_delays,
                       double *future_mean, double *future_sigma,
                       double *future_min, double *future_max,
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
        rule_pool[idx].future_min[i] = future_min[i];
        rule_pool[idx].future_max[i] = future_max[i];
    }

    rule_pool[idx].support_count = support_count;
    rule_pool[idx].negative_count = negative_count_val;
    rule_pool[idx].support_rate = support_value; // サポート率を保存
    rule_pool[idx].num_attributes = num_attributes;

    // 高サポートフラグの設定
    int high_support_marker = 0;
    if (support_value >= (Minsup * HIGH_SUPPORT_MULTIPLIER))
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
    if (max_sigma <= (Maxsigx * LOW_VARIANCE_MULTIPLIER))
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
            double *future_min_ptr = future_min[individual][k][loop_j];
            double *future_max_ptr = future_max[individual][k][loop_j];

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

            // ルールの品質チェック（サポート率、標準偏差、属性数、Min/Max象限判定、最小マッチ数をチェック）
            if (check_rule_quality(future_sigma_ptr, future_mean_ptr, support, j2,
                                   future_min_ptr, future_max_ptr, matched_count,
                                   rule_candidate, time_delay_memo))
            {
                if (j2 < 9 && j2 >= MIN_ATTRIBUTES)
                {
                    // 重複チェック
                    int is_duplicate = check_rule_duplication(rule_candidate, state->rule_count);

                    if (!is_duplicate)
                    {
                        // サポート率を再計算（全マッチベース）
                        double correct_support_rate = (double)matched_count / (double)Nrd;

                        // 新規ルールとして登録
                        register_new_rule(state, rule_candidate, time_delay_memo,
                                          future_mean_ptr, future_sigma_ptr,
                                          future_min_ptr, future_max_ptr,
                                          matched_count, negative_count[individual][k][loop_j],
                                          correct_support_rate, j2,
                                          individual, k, loop_j);

                        // 属性数別カウントを更新
                        rules_by_attribute_count[j2]++;

                        // 集中率を計算（品質指標）
                        int *matched_indices_for_conc = (int *)malloc(Nrd * sizeof(int));
                        double concentration_rate = 0.0;
                        int in_quadrant_count = 0;

                        if (matched_indices_for_conc != NULL)
                        {
                            int actual_matched = rematch_rule_pattern(rule_candidate, time_delay_memo, j2, matched_indices_for_conc);
                            int quadrant_dummy = determine_quadrant_by_rate_with_concentration(matched_indices_for_conc, actual_matched, &concentration_rate, &in_quadrant_count);
                            (void)quadrant_dummy; // 未使用変数警告回避
                            free(matched_indices_for_conc);
                        }

                        // サポート率を計算（頻度指標：全マッチベース）
                        double support_rate = (double)matched_count / (double)Nrd;

                        // 適応度を更新（サポート率 + 集中率）
                        fitness_value[individual] +=
                            support_rate * 10.0 +        // サポート率（10倍重み）
                            concentration_rate * 100.0 + // 集中率（100倍重み）
                            20.0;                        // 新規ルールボーナス

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
                        filter_passed_duplicates++; // 重複カウント

                        // 集中率を計算
                        int *matched_indices_for_conc = (int *)malloc(Nrd * sizeof(int));
                        double concentration_rate = 0.0;
                        int in_quadrant_count = 0;

                        if (matched_indices_for_conc != NULL)
                        {
                            int actual_matched = rematch_rule_pattern(rule_candidate, time_delay_memo, j2, matched_indices_for_conc);
                            int quadrant_dummy = determine_quadrant_by_rate_with_concentration(matched_indices_for_conc, actual_matched, &concentration_rate, &in_quadrant_count);
                            (void)quadrant_dummy;
                            free(matched_indices_for_conc);
                        }

                        // サポート率を計算（全マッチベース）
                        double support_rate = (double)matched_count / (double)Nrd;

                        // 適応度を更新（新規ボーナスなし）
                        fitness_value[individual] +=
                            support_rate * 10.0 +       // サポート率
                            concentration_rate * 100.0; // 集中率
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
            fprintf(file, "%8.3f\t%5.3f\t%8.3f\t%8.3f\t",
                    rule_pool[i].future_mean[k],
                    rule_pool[i].future_sigma[k],
                    rule_pool[i].future_min[k],
                    rule_pool[i].future_max[k]);
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

            // matched_indicesの内容を個別にコピー（matched_count_visを使用）
            for (k = 0; k < rule_pool[i].matched_count_vis; k++)
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
            fprintf(file_a, "X(t+%d)_mean\tX(t+%d)_sigma\tX(t+%d)_min\tX(t+%d)_max\t", k, k, k, k);
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
                fprintf(file_a, "%8.3f\t%5.3f\t%8.3f\t%8.3f\t",
                        global_rule_pool[i].future_mean[k],
                        global_rule_pool[i].future_sigma[k],
                        global_rule_pool[i].future_min[k],
                        global_rule_pool[i].future_max[k]);
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

void write_verification_files()
{
    printf("\n========== Writing Rule Verification Files ==========\n");

    // 全てのルールの検証ファイルを出力（CSV形式）
    int output_count = global_rule_count;
    for (int i = 0; i < output_count; i++)
    {
        write_rule_verification_csv(i);
    }

    printf("  Generated %d rule verification CSV files in: %s/\n", output_count, output_dir_verification);
    printf("====================================================\n\n");
}

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

    // ディレクトリとファイル名を結合（安全なsnprintf使用）
    snprintf(filename, FILENAME_BUFFER_SIZE, "%s/%s%d%d%d%d%d.txt",
             // プレフィックスに応じてディレクトリを選択
             (strcmp(prefix, "IL") == 0) ? output_dir_il : (strcmp(prefix, "IA") == 0) ? output_dir_ia
                                                                                       : output_dir_ib,
             prefix, digits[0], digits[1], digits[2], digits[3], digits[4]);
}

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

void write_filter_statistics_to_file()
{
    char filter_stats_path[512];
    FILE *fp;
    time_t now;
    struct tm *tm_info;
    char time_str[64];

    // ファイルパスを生成
    snprintf(filter_stats_path, sizeof(filter_stats_path),
             "%s/filter_statistics.txt", output_dir_doc);

    // ファイルを開く
    fp = fopen(filter_stats_path, "w");
    if (fp == NULL)
    {
        fprintf(stderr, "Warning: Could not create filter statistics file: %s\n", filter_stats_path);
        return;
    }

    // タイムスタンプを取得
    time(&now);
    tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // ヘッダー
    fprintf(fp, "========================================\n");
    fprintf(fp, "  Filter Statistics Report\n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "Generated: %s\n", time_str);
    fprintf(fp, "Stock Code: %s\n", stock_code);
    fprintf(fp, "Data File: %s\n", data_file_path);
    fprintf(fp, "========================================\n\n");

    // 閾値設定
    fprintf(fp, "========== Threshold Settings ==========\n");
    fprintf(fp, "Minsup (Support Rate):         %.4f%%\n", Minsup * 100.0);
    fprintf(fp, "QUADRANT_THRESHOLD_RATE:       %.1f%%\n", QUADRANT_THRESHOLD_RATE * 100.0);
    fprintf(fp, "DEVIATION_THRESHOLD:           %.4f%%\n", DEVIATION_THRESHOLD * 100.0);
    fprintf(fp, "Maxsigx (Max Std Dev):         %.4f%%\n", Maxsigx * 100.0);
    fprintf(fp, "MIN_SUPPORT_COUNT:             %d\n", MIN_SUPPORT_COUNT);
    fprintf(fp, "========================================\n\n");

    // フィルタ統計
    int total_quadrant_rejected = filter_rejected_by_quadrant_rate + filter_rejected_by_quadrant_deviation;
    int total_evaluated = filter_rejected_by_minsup + filter_rejected_by_min_attributes +
                          filter_rejected_by_min_support_count + total_quadrant_rejected +
                          filter_rejected_by_maxsigx + filter_passed_total;

    fprintf(fp, "========== Filter Statistics ==========\n");
    fprintf(fp, "Total Evaluated: %d rules\n\n", total_evaluated);

    if (total_evaluated > 0)
    {
        fprintf(fp, "Stage 1: Support Rate Filter\n");
        fprintf(fp, "  Rejected by Minsup:            %10d (%6.2f%%)\n",
                filter_rejected_by_minsup,
                100.0 * filter_rejected_by_minsup / total_evaluated);
        fprintf(fp, "  Rejected by MIN_SUPPORT_COUNT: %10d (%6.2f%%)\n\n",
                filter_rejected_by_min_support_count,
                100.0 * filter_rejected_by_min_support_count / total_evaluated);

        fprintf(fp, "Stage 2: Quadrant Filter (Concentration & Deviation)\n");
        fprintf(fp, "  Rejected by Quadrant (total):  %10d (%6.2f%%)\n",
                total_quadrant_rejected,
                100.0 * total_quadrant_rejected / total_evaluated);
        fprintf(fp, "    - By QUADRANT_THRESHOLD_RATE:%10d (%6.2f%%)\n",
                filter_rejected_by_quadrant_rate,
                100.0 * filter_rejected_by_quadrant_rate / total_evaluated);
        fprintf(fp, "    - By Quadrant Deviation:     %10d (%6.2f%%)\n\n",
                filter_rejected_by_quadrant_deviation,
                100.0 * filter_rejected_by_quadrant_deviation / total_evaluated);

        fprintf(fp, "Stage 3: Variance Filter\n");
        fprintf(fp, "  Rejected by Maxsigx:           %10d (%6.2f%%)\n\n",
                filter_rejected_by_maxsigx,
                100.0 * filter_rejected_by_maxsigx / total_evaluated);

        fprintf(fp, "Final Result:\n");
        fprintf(fp, "  Passed All Filters (total):    %10d (%6.2f%%)\n",
                filter_passed_total,
                100.0 * filter_passed_total / total_evaluated);
        fprintf(fp, "    - Unique rules:              %10d\n", filter_passed_total - filter_passed_duplicates);
        fprintf(fp, "    - Duplicates:                %10d\n", filter_passed_duplicates);
        fprintf(fp, "========================================\n\n");

        // 統計分析
        fprintf(fp, "========== Analysis ==========\n");
        fprintf(fp, "Primary Bottleneck:\n");

        int max_rejected = filter_rejected_by_minsup;
        const char *bottleneck = "Minsup (Support Rate)";

        if (filter_rejected_by_quadrant > max_rejected)
        {
            max_rejected = filter_rejected_by_quadrant;
            bottleneck = "Quadrant (MinMax Range)";
        }
        if (filter_rejected_by_min_support_count > max_rejected)
        {
            max_rejected = filter_rejected_by_min_support_count;
            bottleneck = "MIN_SUPPORT_COUNT";
        }
        if (filter_rejected_by_maxsigx > max_rejected)
        {
            max_rejected = filter_rejected_by_maxsigx;
            bottleneck = "Maxsigx (Variance)";
        }

        fprintf(fp, "  %s\n", bottleneck);
        fprintf(fp, "  Rejected: %d rules (%.1f%%)\n",
                max_rejected, 100.0 * max_rejected / total_evaluated);
        fprintf(fp, "========================================\n");
    }

    fclose(fp);
    printf("Filter statistics written to: %s\n", filter_stats_path);
}

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
    printf("  Future span: t+1 to t+%d\n", FUTURE_SPAN);
    printf("  Minimum attributes: %d\n", MIN_ATTRIBUTES);
    printf("========================================\n");

    /* データ統計 */
    printf("Data Statistics:\n");
    printf("  Records: %d\n", Nrd);
    printf("  Attributes: %d\n", Nzk);
    printf("========================================\n");

    /* フィルタ統計 */
    int total_quadrant_rejected_term = filter_rejected_by_quadrant_rate + filter_rejected_by_quadrant_deviation;
    int total_evaluated = filter_rejected_by_minsup + filter_rejected_by_min_attributes +
                          filter_rejected_by_min_support_count + total_quadrant_rejected_term +
                          filter_rejected_by_maxsigx + filter_passed_total;
    if (total_evaluated > 0)
    {
        printf("\n========== Filter Statistics ==========\n");
        printf("Total Evaluated: %d rules\n", total_evaluated);
        printf("  Rejected by Minsup:            %6d (%.1f%%)\n",
               filter_rejected_by_minsup,
               100.0 * filter_rejected_by_minsup / total_evaluated);
        printf("  Rejected by MIN_ATTRIBUTES:    %6d (%.1f%%)\n",
               filter_rejected_by_min_attributes,
               100.0 * filter_rejected_by_min_attributes / total_evaluated);
        printf("  Rejected by MIN_SUPPORT_COUNT: %6d (%.1f%%)\n",
               filter_rejected_by_min_support_count,
               100.0 * filter_rejected_by_min_support_count / total_evaluated);
        printf("  Rejected by Quadrant (total):  %6d (%.1f%%)\n",
               total_quadrant_rejected_term,
               100.0 * total_quadrant_rejected_term / total_evaluated);
        printf("    - By QUADRANT_THRESHOLD_RATE:%6d (%.1f%%)\n",
               filter_rejected_by_quadrant_rate,
               100.0 * filter_rejected_by_quadrant_rate / total_evaluated);
        printf("    - By Quadrant Deviation:     %6d (%.1f%%)\n",
               filter_rejected_by_quadrant_deviation,
               100.0 * filter_rejected_by_quadrant_deviation / total_evaluated);
        printf("  Rejected by Maxsigx:           %6d (%.1f%%)\n",
               filter_rejected_by_maxsigx,
               100.0 * filter_rejected_by_maxsigx / total_evaluated);
        printf("  Passed All Filters (total):    %6d (%.1f%%)\n",
               filter_passed_total,
               100.0 * filter_passed_total / total_evaluated);
        printf("    - Unique rules:              %6d\n", filter_passed_total - filter_passed_duplicates);
        printf("    - Duplicates:                %6d\n", filter_passed_duplicates);
        printf("========================================\n");
    }

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

void setup_paths_for_stock(const char *code)
{
    // 銘柄コードをコピー
    strncpy(stock_code, code, sizeof(stock_code) - 1);
    stock_code[sizeof(stock_code) - 1] = '\0';

    // データファイルパスを設定（為替日足データ）
    snprintf(data_file_path, sizeof(data_file_path),
             "1-deta-enginnering/forex_data_daily/%s.txt", stock_code);

    // 出力ベースディレクトリを設定（為替日足データは 1-deta-enginnering/forex_data_daily/output/ 配下）
    snprintf(output_base_dir, sizeof(output_base_dir),
             "1-deta-enginnering/forex_data_daily/output/%s", stock_code);

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

        // フィルタ統計カウンタをリセット
        filter_rejected_by_minsup = 0;
        filter_rejected_by_min_attributes = 0;
        filter_rejected_by_min_support_count = 0;
        filter_rejected_by_quadrant = 0;
        filter_rejected_by_quadrant_rate = 0;
        filter_rejected_by_quadrant_deviation = 0;
        filter_rejected_by_maxsigx = 0;
        filter_passed_total = 0;
        filter_passed_duplicates = 0;

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

        // フィルタ統計をファイルに出力
        write_filter_statistics_to_file();
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

int main(int argc, char *argv[])
{
    clock_t batch_start_time, batch_end_time;

    /* ========== コマンドライン引数処理 ========== */

    // 使用方法を表示
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0))
    {
        printf("==============================================\n");
        printf("  GNMiner - FX Rule Discovery\n");
        printf("==============================================\n\n");
        printf("Usage: %s <currency_pair>\n\n", argv[0]);
        printf("Arguments:\n");
        printf("  <currency_pair>  FX currency pair (e.g., USDJPY, EURUSD)\n\n");
        printf("Examples:\n");
        printf("  %s USDJPY  # Discover rules for USD/JPY\n", argv[0]);
        printf("  %s EURUSD  # Discover rules for EUR/USD\n", argv[0]);
        printf("  %s GBPJPY  # Discover rules for GBP/JPY\n\n", argv[0]);
        printf("Note:\n");
        printf("  - Data files must exist: forex_data_daily/{PAIR}.txt\n");
        printf("  - Results will be saved to: forex_data_daily/output/{PAIR}/\n");
        printf("  - Available pairs: 20 major FX pairs (60 dimensions)\n\n");
        printf("==============================================\n");
        return 0;
    }

    // 引数チェック
    if (argc < 2)
    {
        printf("\nERROR: Insufficient arguments\n\n");
        printf("Usage: %s <currency_pair>\n\n", argv[0]);
        printf("  <currency_pair>: FX currency pair (e.g., USDJPY, EURUSD)\n\n");
        printf("Examples:\n");
        printf("  %s USDJPY\n", argv[0]);
        printf("  %s EURUSD\n\n", argv[0]);
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
    printf("  GNMiner - FX Rule Discovery\n");
    printf("==================================================\n");
    printf("Currency Pair: %s\n", stock_code_arg);
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
    printf("Currency Pair: %s\n", stock_code_arg);
    printf("Total Time: %.2fs (%.1fm)\n", total_elapsed, total_elapsed / 60.0);
    printf("Status:     %s\n", result == 0 ? "SUCCESS" : "FAILED");
    printf("==================================================\n");
    printf("\n");

    return result; // 成功時0、失敗時1
}
