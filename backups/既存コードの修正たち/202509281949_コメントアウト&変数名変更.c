#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ========================================
   GNP-based Association Rule Mining System
   2次元連続値出力に対する局所分布ルールマイニング
   ======================================== */

/* 2012-1-30 初版 */
/* N_(x)<=6 制約条件：前件部属性数は最大6個まで */
/* 2021-1-12 改良版 */
/* 2021-1-21 機能追加 */
/* 2021-1-25 変数X,Yの標準偏差対応 */
/* 2023-5-09 軽微な修正 */

/* ========================================
   ファイル定義セクション
   ======================================== */
#define DATANAME "testdata1s.txt" /* 入力: 学習データ (159レコード×97列) */
#define DICNAME "testdic1.txt"    /* 入力: 属性名辞書 */
/* データ形式: (95属性 + 2連続値) × (159レコード) */

#define fpoola "zrp01a.txt"       /* 出力: ルールプール（属性名表記） */
#define fpoolb "zrp01b.txt"       /* 出力: ルールプール（数値表記） */
#define fcont "zrd01.txt"         /* 出力: ドキュメントファイル */
#define RESULTNAME "zrmemo01.txt" /* 出力: メモログファイル */

/* ========================================
   データパラメータ定義
   ======================================== */
#define Nrd 159 /* 総レコード数 */
#define Nzk 95  /* 前件部用の属性数（条件部で使用） */

/* ========================================
   ルール設定パラメータ
   ======================================== */
#define Nrulemax 2002 /* ルールプールの最大サイズ */
#define Minsup 0.04   /* 最小サポート値（4%） */
#define Maxsigx 0.5   /* X変数の最大標準偏差（局所分布判定用） */
#define Maxsigy 0.5   /* Y変数の最大標準偏差（局所分布判定用） */

/* ========================================
   ルールマイニング実行設定
   ======================================== */
#define Nstart 1000 /* ファイルID開始番号 */
#define Ntry 100    /* 試行回数（独立実行回数） */

/* ========================================
   GNP（遺伝的ネットワークプログラミング）パラメータ
   ======================================== */
#define Generation 201 /* 最終世代数 */
#define Nkotai 120     /* 母集団サイズ（個体数） */
#define Npn 10         /* 処理ノード数/個体 */
#define Njg 100        /* 判定ノード数/個体 */
#define Nmx 7          /* Yes側遷移の最大数+1（最大9） */
#define Muratep 1      /* 処理ノードの突然変異率 */
#define Muratej 6      /* 突然変異1（接続変更）の率 */
#define Muratea 6      /* 突然変異2（属性変更）の率 */
#define Nkousa 20      /* 交叉頻度（各個体あたり） */

void main()
{
    /* ========================================
       構造体定義：抽出されたルールを格納
       ======================================== */
    struct asrule
    {
        int antecedent_attr0;  /* 前件部属性1（0は未使用を示す） */
        int antecedent_attr1;  /* 前件部属性2 */
        int antecedent_attr2;  /* 前件部属性3 */
        int antecedent_attr3;  /* 前件部属性4 */
        int antecedent_attr4;  /* 前件部属性5 */
        int antecedent_attr5;  /* 前件部属性6 */
        int antecedent_attr6;  /* 前件部属性7 */
        int antecedent_attr7;  /* 前件部属性8（最大8属性） */
        double x_mean;         /* X変数の平均値 */
        double x_sigma;        /* X変数の標準偏差 */
        double y_mean;         /* Y変数の平均値 */
        double y_sigma;        /* Y変数の標準偏差 */
        int support_count;     /* 前件部を満たすレコード数 */
        int negative_count;    /* 負例数（評価用） */
        int high_support_flag; /* 高サポートマーカー（1:高い） */
        int low_variance_flag; /* 低分散マーカー（1:局所的） */
    };
    struct asrule rule_pool[Nrulemax]; /* ルールプール本体 */

    /* ========================================
       比較用ルール構造体（重複チェック用）
       ======================================== */
    struct cmrule
    {
        int antecedent_attr0; /* 比較用の前件部属性（8個） */
        int antecedent_attr1;
        int antecedent_attr2;
        int antecedent_attr3;
        int antecedent_attr4;
        int antecedent_attr5;
        int antecedent_attr6;
        int antecedent_attr7;
    };
    struct cmrule compare_rules[Nrulemax];

    /* ========================================
       変数宣言セクション
       ======================================== */
    int work_array[5], input_value;
    int rule_count, rule_count2, high_support_rule_count, low_variance_rule_count;
    int i, j, k, loop_i, loop_j, loop_k;
    int i2, j2, k2, i3, j3, k3, i4, j4, k4, i5, j5, i6, j6, i7, j7, k7, k8;
    int attr_index;
    int current_node_id;
    int loop_trial, generation_id, individual_id;
    int matched_antecedent_count, process_id;
    int new_flag, same_flag1, same_flag2, match_flag;
    int mutation_random, mutation_attr1, mutation_attr2, mutation_attr3;
    int total_attribute_usage, difference_count;
    int crossover_parent, crossover_node1, crossover_node2;
    int temp_memory, save_memory, node_memory, node_save1, node_save2;
    int backup_counter2, high_support_marker, low_variance_marker;
    double input_x, input_y, threshold_x, threshold_sigma_x, threshold_y, threshold_sigma_y;

    int total_rule_count, new_rule_count;
    int total_high_support, total_low_variance;
    int new_high_support, new_low_variance;
    int file_index, file_digit, trial_index, compare_trial_index;
    int file_digit1, file_digit2, file_digit3, file_digit4, file_digit5;
    int compare_digit1, compare_digit2, compare_digit3, compare_digit4, compare_digit5;

    double confidence_max, fitness_average_all, support_value, sigma_x, sigma_y;
    double current_x_value, current_y_value;

    /* ========================================
       静的配列宣言（GNP個体群のメモリ確保）
       ======================================== */
    static int node_structure[Nkotai][Npn + Njg][2]; /* GNPノード構造：[個体][ノード][属性,接続先] */

    static int record_attributes[Nzk + 2]; /* データレコード読込用バッファ */

    /* 各個体・各処理ノードでの統計情報格納配列 */
    static int match_count[Nkotai][Npn][10];      /* 各ノード連鎖でのマッチ数 */
    static int negative_count[Nkotai][Npn][10];   /* 負例数 */
    static int evaluation_count[Nkotai][Npn][10]; /* 評価数 */
    static int attribute_chain[Nkotai][Npn][10];  /* 属性連鎖記録 */

    /* X,Y変数の統計量格納配列 */
    static double x_sum[Nkotai][Npn][10];         /* X変数の和 */
    static double x_sigma_array[Nkotai][Npn][10]; /* X変数の標準偏差 */
    static double y_sum[Nkotai][Npn][10];         /* Y変数の和 */
    static double y_sigma_array[Nkotai][Npn][10]; /* Y変数の標準偏差 */

    int rules_per_trial[Ntry]; /* 各試行でのルール数記録 */

    int attribute_usage_history[5][Nzk];                /* 属性使用頻度の履歴（5世代分） */
    int attribute_usage_count[Nzk];                     /* 属性使用カウント */
    int attribute_tracking[Nzk];                        /* 属性トラッキング */
    double fitness_value[Nkotai];                       /* 各個体の適応度 */
    int fitness_ranking[Nkotai], new_rule_flag[Nkotai]; /* 適応度ランクと新規フラグ */

    /* GNP遺伝子情報 */
    static int gene_connection[Nkotai][Npn + Njg]; /* ノード接続遺伝子 */
    static int gene_attribute[Nkotai][Npn + Njg];  /* 属性遺伝子 */

    int selected_attributes[10], read_attributes[10];
    int rules_by_attribute_count[10]; /* 属性数別ルールカウント */

    int attribute_set[Nzk + 1]; /* 属性セット */

    int rule_candidate_pre[10]; /* ルール候補（前処理） */
    int rule_candidate[10];     /* ルール候補 */
    int rule_memo[10];          /* ルールメモ */

    int new_rule_marker[Nrulemax]; /* 新規ルールフラグ */

    /* ファイル名生成用文字列 */
    char filename_rule[12];
    char filename_report[12];
    char filename_temp[12];
    char filename_local[12];

    char attribute_name_buffer[31];         /* 属性名読込用 */
    char attribute_dictionary[Nzk + 3][31]; /* 属性名辞書 */

    /* ========================================
       ファイルポインタ宣言
       ======================================== */
    FILE *file_data1;
    FILE *file_data2;
    FILE *file_dictionary; /* DICNAME */
    FILE *file_rule;
    FILE *file_report;
    FILE *file_compare;
    FILE *file_local;
    FILE *file_pool_attribute; /* fpool a */
    FILE *file_pool_numeric;   /* fpool b */
    FILE *file_document;       /* fcont */

    /* ========================================
       メイン処理開始
       ======================================== */

    srand(1); /* 乱数シード固定（再現性のため） */

    /* ========================================
       属性名辞書の読み込み
       ======================================== */
    for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
    {
        strcpy(attribute_dictionary[attr_index], "\0"); /* 属性名配列初期化 */
    }

    file_dictionary = fopen(DICNAME, "r");
    for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
    {
        fscanf(file_dictionary, "%d%s", &process_id, attribute_name_buffer);
        strcpy(attribute_dictionary[attr_index], attribute_name_buffer);
    }
    fclose(file_dictionary);

    /* ========================================
       グローバルカウンタ初期化
       ======================================== */
    total_rule_count = 0;   /* 総ルール数 */
    total_high_support = 0; /* 高サポートルール総数 */
    total_low_variance = 0; /* 低分散ルール総数 */

    /* ドキュメントファイル初期化 */
    file_document = fopen(fcont, "w");
    fprintf(file_document, "\n");
    fclose(file_document);

    /* ========================================
       メイン試行ループ（Ntry回独立実行）
       ======================================== */
    for (trial_index = Nstart; trial_index < (Nstart + Ntry); trial_index++)
    {

        /* ========================================
           各試行での初期化処理
           ======================================== */

        /* ルール構造体の初期化 */
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
            rule_pool[i].x_mean = 0;
            rule_pool[i].x_sigma = 0;
            rule_pool[i].y_mean = 0;
            rule_pool[i].y_sigma = 0;
            rule_pool[i].support_count = 0;
            rule_pool[i].negative_count = 0;
            rule_pool[i].high_support_flag = 0;
            rule_pool[i].low_variance_flag = 0;
        }

        /* 比較用ルールの初期化 */
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

        /* 新規ルールマーカー初期化 */
        for (i = 0; i < Nrulemax; i++)
        {
            new_rule_marker[i] = 0;
        }

        /* ========================================
           ファイル名生成（5桁の番号から）
           例: 01234 → IL01234.txt
           ======================================== */
        file_digit5 = trial_index / 10000;
        file_digit4 = trial_index / 1000 - file_digit5 * 10;
        file_digit3 = trial_index / 100 - file_digit5 * 100 - file_digit4 * 10;
        file_digit2 = trial_index / 10 - file_digit5 * 1000 - file_digit4 * 100 - file_digit3 * 10;
        file_digit1 = trial_index - file_digit5 * 10000 - file_digit4 * 1000 - file_digit3 * 100 - file_digit2 * 10;

        /* ルールファイル名生成 */
        filename_rule[0] = 73; /* 'I' */
        filename_rule[1] = 76; /* 'L' */
        filename_rule[2] = 48 + file_digit5;
        filename_rule[3] = 48 + file_digit4;
        filename_rule[4] = 48 + file_digit3;
        filename_rule[5] = 48 + file_digit2;
        filename_rule[6] = 48 + file_digit1;
        filename_rule[7] = 46;   /* '.' */
        filename_rule[8] = 116;  /* 't' */
        filename_rule[9] = 120;  /* 'x' */
        filename_rule[10] = 116; /* 't' */
        filename_rule[11] = 0;

        /* レポートファイル名生成 */
        filename_report[0] = 73; /* 'I' */
        filename_report[1] = 65; /* 'A' */
        filename_report[2] = 48 + file_digit5;
        filename_report[3] = 48 + file_digit4;
        filename_report[4] = 48 + file_digit3;
        filename_report[5] = 48 + file_digit2;
        filename_report[6] = 48 + file_digit1;
        filename_report[7] = 46;
        filename_report[8] = 116;
        filename_report[9] = 120;
        filename_report[10] = 116;
        filename_report[11] = 0;

        /* 一時ファイル名生成 */
        filename_temp[0] = 73; /* 'I' */
        filename_temp[1] = 84; /* 'T' */
        filename_temp[2] = 48 + file_digit5;
        filename_temp[3] = 48 + file_digit4;
        filename_temp[4] = 48 + file_digit3;
        filename_temp[5] = 48 + file_digit2;
        filename_temp[6] = 48 + file_digit1;
        filename_temp[7] = 46;
        filename_temp[8] = 116;
        filename_temp[9] = 120;
        filename_temp[10] = 116;
        filename_temp[11] = 0;

        /* ローカルファイル名生成 */
        filename_local[0] = 73; /* 'I' */
        filename_local[1] = 66; /* 'B' */
        filename_local[2] = 48 + file_digit5;
        filename_local[3] = 48 + file_digit4;
        filename_local[4] = 48 + file_digit3;
        filename_local[5] = 48 + file_digit2;
        filename_local[6] = 48 + file_digit1;
        filename_local[7] = 46;
        filename_local[8] = 116;
        filename_local[9] = 120;
        filename_local[10] = 116;
        filename_local[11] = 0;

        /* ========================================
           属性セット初期化
           ======================================== */
        for (file_index = 0; file_index < Nzk + 1; file_index++)
        {
            attribute_set[file_index] = file_index;
        }

        /* カウンタ初期化 */
        rule_count = 1;
        high_support_rule_count = 1;
        low_variance_rule_count = 1;

        /* ルール数カウント配列初期化 */
        for (i = 0; i < 10; i++)
        {
            rules_by_attribute_count[i] = 0;
        }

        total_attribute_usage = Nzk;

        /* 属性使用状況の初期化 */
        for (i = 0; i < Nzk; i++)
        {
            attribute_usage_count[i] = 0;
            attribute_tracking[i] = 0;
            attribute_usage_history[0][i] = 1; /* 初期世代は全属性使用可能 */
            attribute_usage_history[1][i] = 0;
            attribute_usage_history[2][i] = 0;
            attribute_usage_history[3][i] = 0;
            attribute_usage_history[4][i] = 0;
        }

        /* ========================================
           GNP初期個体群の生成（ランダム）
           ======================================== */
        for (i4 = 0; i4 < Nkotai; i4++)
        {
            for (j4 = 0; j4 < (Npn + Njg); j4++)
            {
                gene_connection[i4][j4] = rand() % Njg + Npn; /* 接続先をランダムに設定 */
                gene_attribute[i4][j4] = rand() % Nzk;        /* 属性をランダムに選択 */
            }
        }

        /* 出力ファイル初期化 */
        file_rule = fopen(filename_rule, "w");
        fprintf(file_rule, "");
        fclose(file_rule);

        file_report = fopen(filename_report, "w");
        fprintf(file_report, "\n");
        fclose(file_report);

        file_local = fopen(filename_local, "w");
        fprintf(file_local, "\n");
        fclose(file_local);

        /* ========================================
           進化計算メインループ
           ======================================== */
        for (generation_id = 0; generation_id < Generation; generation_id++)
        {
            /* ルール数上限チェック */
            if (rule_count > (Nrulemax - 2))
            {
                break;
            }

            /* ========================================
               GNP遺伝子を個体構造にコピー
               ======================================== */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (j4 = 0; j4 < (Npn + Njg); j4++)
                {
                    node_structure[individual_id][j4][0] = gene_attribute[individual_id][j4];
                    node_structure[individual_id][j4][1] = gene_connection[individual_id][j4];
                }
            }

            /* ========================================
               評価用配列の初期化
               ======================================== */
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
                        negative_count[individual_id][k][i] = 0;
                        evaluation_count[individual_id][k][i] = 0;
                        x_sum[individual_id][k][i] = 0;
                        x_sigma_array[individual_id][k][i] = 0;
                        y_sum[individual_id][k][i] = 0;
                        y_sigma_array[individual_id][k][i] = 0;
                    }
                }
            }

            /* ========================================
               データセットを使った個体評価
               ======================================== */
            file_data2 = fopen(DATANAME, "r");

            for (i = 0; i < Nrd; i++) /* 各レコードについて */
            {
                /* 95属性の値を読込 */
                for (j7 = 0; j7 < Nzk; j7++)
                {
                    fscanf(file_data2, "%d", &input_value);
                    record_attributes[j7] = input_value;
                }

                /* X,Y連続値を読込 */
                fscanf(file_data2, "%lf%lf", &input_x, &input_y);
                current_x_value = input_x;
                current_y_value = input_y;

                /* ========================================
                   各個体のGNPネットワークでレコードを評価
                   ======================================== */
                for (individual_id = 0; individual_id < Nkotai; individual_id++)
                {
                    for (k = 0; k < Npn; k++) /* 各処理ノードから開始 */
                    {
                        current_node_id = k;
                        j = 0;          /* 判定ノード連鎖の深さ */
                        match_flag = 1; /* 有効フラグ */
                        match_count[individual_id][k][j] = match_count[individual_id][k][j] + 1;
                        evaluation_count[individual_id][k][j] = evaluation_count[individual_id][k][j] + 1;
                        current_node_id = node_structure[individual_id][current_node_id][1];

                        /* 判定ノード連鎖を辿る */
                        while (current_node_id > (Npn - 1) && j < Nmx)
                        {
                            j = j + 1;
                            attribute_chain[individual_id][k][j] = node_structure[individual_id][current_node_id][0] + 1;

                            if (record_attributes[node_structure[individual_id][current_node_id][0]] == 1)
                            {
                                if (match_flag == 1)
                                {
                                    match_count[individual_id][k][j] = match_count[individual_id][k][j] + 1;
                                    x_sum[individual_id][k][j] = x_sum[individual_id][k][j] + current_x_value;
                                    x_sigma_array[individual_id][k][j] = x_sigma_array[individual_id][k][j] + current_x_value * current_x_value;
                                    y_sum[individual_id][k][j] = y_sum[individual_id][k][j] + current_y_value;
                                    y_sigma_array[individual_id][k][j] = y_sigma_array[individual_id][k][j] + current_y_value * current_y_value;
                                }
                                evaluation_count[individual_id][k][j] = evaluation_count[individual_id][k][j] + 1;
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                            else if (record_attributes[node_structure[individual_id][current_node_id][0]] == 0)
                            {
                                current_node_id = k; /* 処理ノードに戻る */
                            }
                            else /* 値が2の場合 */
                            {
                                evaluation_count[individual_id][k][j] = evaluation_count[individual_id][k][j] + 1;
                                match_flag = 0;
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                        }
                    } /* k: 処理ノード */
                } /* individual_id: 個体 */
            } /* i: レコード */
            fclose(file_data2);

            /* ========================================
               負例数の計算
               ======================================== */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (k = 0; k < Npn; k++)
                {
                    for (i = 0; i < Nmx; i++)
                    {
                        negative_count[individual_id][k][i] = match_count[individual_id][k][0] - evaluation_count[individual_id][k][i] + match_count[individual_id][k][i];
                    }
                }
            }

            /* ========================================
               統計量の計算（平均と標準偏差）
               ======================================== */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (k = 0; k < Npn; k++)
                {
                    for (j6 = 1; j6 < 9; j6++)
                    {
                        if (match_count[individual_id][k][j6] != 0)
                        {
                            /* X変数の平均と分散計算 */
                            x_sum[individual_id][k][j6] = x_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];
                            x_sigma_array[individual_id][k][j6] = x_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - x_sum[individual_id][k][j6] * x_sum[individual_id][k][j6];

                            /* Y変数の平均と分散計算 */
                            y_sum[individual_id][k][j6] = y_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];
                            y_sigma_array[individual_id][k][j6] = y_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - y_sum[individual_id][k][j6] * y_sum[individual_id][k][j6];

                            /* 数値誤差による負の分散を補正 */
                            if (x_sigma_array[individual_id][k][j6] < 0)
                            {
                                x_sigma_array[individual_id][k][j6] = 0;
                            }
                            if (y_sigma_array[individual_id][k][j6] < 0)
                            {
                                y_sigma_array[individual_id][k][j6] = 0;
                            }

                            /* 標準偏差に変換 */
                            x_sigma_array[individual_id][k][j6] = sqrt(x_sigma_array[individual_id][k][j6]);
                            y_sigma_array[individual_id][k][j6] = sqrt(y_sigma_array[individual_id][k][j6]);
                        }
                    }
                }
            }

            /* ========================================
               ルール抽出処理（各個体から）
               ======================================== */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                if (rule_count > (Nrulemax - 2))
                {
                    break;
                }

                for (k = 0; k < Npn; k++) /* 各処理ノードチェーンについて */
                {
                    if (rule_count > (Nrulemax - 2))
                    {
                        break;
                    }

                    /* ルール候補配列初期化 */
                    for (i = 0; i < 10; i++)
                    {
                        rule_candidate_pre[i] = 0;
                        rule_candidate[i] = 0;
                        rule_memo[i] = 0;
                    }

                    /* 各深さのノード連鎖をチェック */
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
                            support_value = (double)matched_antecedent_count / (double)negative_count[individual_id][k][loop_j];
                        }

                        /* ========================================
                           興味深いルールの条件判定
                           ======================================== */
                        if (sigma_x <= threshold_sigma_x && sigma_y <= threshold_sigma_y && support_value >= Minsup)
                        {
                            /* ルール前件部の属性を取得 */
                            for (i2 = 1; i2 < 9; i2++)
                            {
                                rule_candidate_pre[i2 - 1] = attribute_chain[individual_id][k][i2];
                            }

                            /* 有効な属性のみ抽出 */
                            j2 = 0;
                            for (k2 = 1; k2 < (1 + Nzk); k2++)
                            {
                                new_flag = 0;
                                for (i2 = 0; i2 < loop_j; i2++)
                                {
                                    if (rule_candidate_pre[i2] == k2)
                                    {
                                        new_flag = 1;
                                    }
                                }
                                if (new_flag == 1)
                                {
                                    rule_candidate[j2] = k2;
                                    rule_memo[j2] = k2;
                                    j2 = j2 + 1;
                                }
                            }

                            if (j2 < 9) /* 属性数が上限内 */
                            {
                                /* ========================================
                                   新規ルールかどうかの判定
                                   ======================================== */
                                new_flag = 0;
                                for (i2 = 0; i2 < rule_count; i2++)
                                {
                                    same_flag1 = 1;

                                    /* 既存ルールと完全一致するかチェック */
                                    if (rule_pool[i2].antecedent_attr0 == rule_candidate[0] &&
                                        rule_pool[i2].antecedent_attr1 == rule_candidate[1] &&
                                        rule_pool[i2].antecedent_attr2 == rule_candidate[2] &&
                                        rule_pool[i2].antecedent_attr3 == rule_candidate[3] &&
                                        rule_pool[i2].antecedent_attr4 == rule_candidate[4] &&
                                        rule_pool[i2].antecedent_attr5 == rule_candidate[5] &&
                                        rule_pool[i2].antecedent_attr6 == rule_candidate[6] &&
                                        rule_pool[i2].antecedent_attr7 == rule_candidate[7])
                                    {
                                        same_flag1 = 0;
                                    }

                                    if (same_flag1 == 0)
                                    {
                                        new_flag = 1;
                                        fitness_value[individual_id] = fitness_value[individual_id] + j2 + support_value * 10 + 2 / (sigma_x + 0.1) + 2 / (sigma_y + 0.1);
                                    }
                                    if (new_flag == 1)
                                    {
                                        break;
                                    }
                                }

                                if (new_flag == 0) /* 新規ルールの場合 */
                                {
                                    /* ルールプールに追加 */
                                    rule_pool[rule_count].antecedent_attr0 = rule_candidate[0];
                                    rule_pool[rule_count].antecedent_attr1 = rule_candidate[1];
                                    rule_pool[rule_count].antecedent_attr2 = rule_candidate[2];
                                    rule_pool[rule_count].antecedent_attr3 = rule_candidate[3];
                                    rule_pool[rule_count].antecedent_attr4 = rule_candidate[4];
                                    rule_pool[rule_count].antecedent_attr5 = rule_candidate[5];
                                    rule_pool[rule_count].antecedent_attr6 = rule_candidate[6];
                                    rule_pool[rule_count].antecedent_attr7 = rule_candidate[7];
                                    rule_pool[rule_count].x_mean = x_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].x_sigma = x_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_mean = y_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_sigma = y_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].support_count = matched_antecedent_count;
                                    rule_pool[rule_count].negative_count = negative_count[individual_id][k][loop_j];

                                    /* ファイルに出力 */
                                    file_rule = fopen(filename_rule, "a");
                                    fprintf(file_rule, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                                            rule_candidate[0], rule_candidate[1], rule_candidate[2], rule_candidate[3],
                                            rule_candidate[4], rule_candidate[5], rule_candidate[6], rule_candidate[7]);
                                    fclose(file_rule);

                                    /* 高サポートマーカー設定 */
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

                                    /* 低分散マーカー設定 */
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

                                    /* 新規ルール発見ボーナス */
                                    fitness_value[individual_id] = fitness_value[individual_id] + j2 + support_value * 10 + 2 / (sigma_x + 0.1) + 2 / (sigma_y + 0.1) + 20;

                                    /* 使用属性の頻度更新 */
                                    for (k3 = 0; k3 < j2; k3++)
                                    {
                                        i5 = rule_memo[k3] - 1;
                                        attribute_usage_history[0][i5] = attribute_usage_history[0][i5] + 1;
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

            /* ========================================
               属性使用統計の更新
               ======================================== */
            total_attribute_usage = 0;
            for (i4 = 0; i4 < Nzk; i4++)
            {
                attribute_usage_count[i4] = 0;
                attribute_tracking[i4] = 0;
            }

            /* 5世代分の履歴から合計 */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 0; j4 < 5; j4++)
                {
                    attribute_tracking[i4] = attribute_tracking[i4] + attribute_usage_history[j4][i4];
                }
            }

            /* 履歴をシフト */
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

            /* 属性使用総数計算 */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                total_attribute_usage = total_attribute_usage + attribute_tracking[i4];
            }

            /* 現在の個体群での属性使用頻度 */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    attribute_usage_count[(gene_attribute[i4][j4])] = attribute_usage_count[(gene_attribute[i4][j4])] + 1;
                }
            }

            /* ========================================
               適応度によるランキング
               ======================================== */
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

            /* ========================================
               選択（上位1/3を3倍に複製）
               ======================================== */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                if (fitness_ranking[i4] < 40)
                {
                    for (j4 = 0; j4 < (Npn + Njg); j4++)
                    {
                        gene_attribute[fitness_ranking[i4]][j4] = node_structure[i4][j4][0];
                        gene_attribute[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][0];
                        gene_attribute[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4]][j4] = node_structure[i4][j4][1];
                        gene_connection[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][1];
                        gene_connection[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][1];
                    }
                }
            }

            /* ========================================
               交叉（一様交叉）
               ======================================== */
            for (i4 = 0; i4 < 20; i4++)
            {
                for (j4 = 0; j4 < Nkousa; j4++)
                {
                    crossover_node1 = rand() % Njg + Npn;

                    /* 接続遺伝子の交換 */
                    temp_memory = gene_connection[i4 + 20][crossover_node1];
                    gene_connection[i4 + 20][crossover_node1] = gene_connection[i4][crossover_node1];
                    gene_connection[i4][crossover_node1] = temp_memory;

                    /* 属性遺伝子の交換 */
                    temp_memory = gene_attribute[i4 + 20][crossover_node1];
                    gene_attribute[i4 + 20][crossover_node1] = gene_attribute[i4][crossover_node1];
                    gene_attribute[i4][crossover_node1] = temp_memory;
                }
            }

            /* ========================================
               突然変異：処理ノード（全個体対象）
               ======================================== */
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

            /* ========================================
               突然変異1：判定ノード接続（40-79の個体）
               ======================================== */
            for (i4 = 40; i4 < 80; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    mutation_random = rand() % Muratej;
                    if (mutation_random == 0)
                    {
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                    }
                }
            }

            /* ========================================
               突然変異2：属性変更（80-119の個体）
               ======================================== */
            for (i4 = 80; i4 < 120; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    mutation_random = rand() % Muratea;
                    if (mutation_random == 0)
                    {
                        /* 使用頻度に基づく重み付きランダム選択 */
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
                }
            }

            /* ========================================
               5世代ごとの進捗報告
               ======================================== */
            if (generation_id % 5 == 0)
            {
                file_report = fopen(filename_report, "a");
                fitness_average_all = 0;

                for (i4 = 0; i4 < Nkotai; i4++)
                {
                    fitness_average_all = fitness_average_all + fitness_value[i4];
                }
                fitness_average_all = fitness_average_all / Nkotai;

                fprintf(file_report, "%5d\t%5d\t%5d\t%5d\t%9.3f \n",
                        generation_id, (rule_count - 1), (high_support_rule_count - 1), (low_variance_rule_count - 1), fitness_average_all);
                fclose(file_report);

                printf("Gen.=%5d \t %5d rules extracted (%5d %5d) \n",
                       generation_id, (rule_count - 1), (high_support_rule_count - 1), (low_variance_rule_count - 1));
            }
        }

        /* ========================================
           試行終了処理
           ======================================== */
        rules_per_trial[trial_index - Nstart] = rule_count - 1;

        printf("\n %s rule extraction finished. \n", filename_rule);
        printf("==> %5d rules extracted (%5d %5d) \n",
               rules_per_trial[trial_index - Nstart], (high_support_rule_count - 1), (low_variance_rule_count - 1));

        /* ========================================
           ローカルファイルへの出力
           ======================================== */
        file_local = fopen(filename_local, "w");
        for (i = 1; i < rule_count; i++)
        {
            fprintf(file_local, "%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t( %2d %2d )\n",
                    rule_pool[i].antecedent_attr0, rule_pool[i].antecedent_attr1,
                    rule_pool[i].antecedent_attr2, rule_pool[i].antecedent_attr3,
                    rule_pool[i].antecedent_attr4, rule_pool[i].antecedent_attr5,
                    rule_pool[i].antecedent_attr6, rule_pool[i].antecedent_attr7,
                    rule_pool[i].high_support_flag, rule_pool[i].low_variance_flag);
            fprintf(file_local, "\n");
        }
        fclose(file_local);

        /* ========================================
           グローバルルールプールへの統合
           ======================================== */
        if (trial_index == Nstart) /* 最初の試行の場合 */
        {
            file_pool_attribute = fopen(fpoola, "w");
            for (i2 = 1; i2 < rule_count; i2++)
            {
                fprintf(file_pool_attribute, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                        attribute_dictionary[rule_pool[i2].antecedent_attr0],
                        attribute_dictionary[rule_pool[i2].antecedent_attr1],
                        attribute_dictionary[rule_pool[i2].antecedent_attr2],
                        attribute_dictionary[rule_pool[i2].antecedent_attr3],
                        attribute_dictionary[rule_pool[i2].antecedent_attr4],
                        attribute_dictionary[rule_pool[i2].antecedent_attr5],
                        attribute_dictionary[rule_pool[i2].antecedent_attr6],
                        attribute_dictionary[rule_pool[i2].antecedent_attr7],
                        rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                        rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                        rule_pool[i2].support_count, rule_pool[i2].negative_count,
                        rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
            }
            fclose(file_pool_attribute);

            file_pool_numeric = fopen(fpoolb, "w");
            for (i2 = 1; i2 < rule_count; i2++)
            {
                fprintf(file_pool_numeric, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                        rule_pool[i2].antecedent_attr0, rule_pool[i2].antecedent_attr1,
                        rule_pool[i2].antecedent_attr2, rule_pool[i2].antecedent_attr3,
                        rule_pool[i2].antecedent_attr4, rule_pool[i2].antecedent_attr5,
                        rule_pool[i2].antecedent_attr6, rule_pool[i2].antecedent_attr7,
                        rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                        rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                        rule_pool[i2].support_count, rule_pool[i2].negative_count,
                        rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
            }
            fclose(file_pool_numeric);

            total_rule_count = total_rule_count + rule_count - 1;
            total_high_support = total_high_support + high_support_rule_count - 1;
            total_low_variance = total_low_variance + low_variance_rule_count - 1;
        }

        if (trial_index > Nstart) /* 2回目以降の試行 */
        {
            /* ========================================
               過去の試行のルールと重複チェック
               ======================================== */
            for (compare_trial_index = Nstart; compare_trial_index < trial_index; compare_trial_index++)
            {
                /* ファイル名生成 */
                compare_digit5 = compare_trial_index / 10000;
                compare_digit4 = compare_trial_index / 1000 - compare_digit5 * 10;
                compare_digit3 = compare_trial_index / 100 - compare_digit5 * 100 - compare_digit4 * 10;
                compare_digit2 = compare_trial_index / 10 - compare_digit5 * 1000 - compare_digit4 * 100 - compare_digit3 * 10;
                compare_digit1 = compare_trial_index - compare_digit5 * 10000 - compare_digit4 * 1000 - compare_digit3 * 100 - compare_digit2 * 10;

                filename_rule[0] = 73;
                filename_rule[1] = 76;
                filename_rule[2] = 48 + compare_digit5;
                filename_rule[3] = 48 + compare_digit4;
                filename_rule[4] = 48 + compare_digit3;
                filename_rule[5] = 48 + compare_digit2;
                filename_rule[6] = 48 + compare_digit1;
                filename_rule[7] = 46;
                filename_rule[8] = 116;
                filename_rule[9] = 120;
                filename_rule[10] = 116;
                filename_rule[11] = 0;

                /* 過去のルールファイルを読込 */
                file_compare = fopen(filename_rule, "r");
                for (i7 = 1; i7 < (rules_per_trial[compare_trial_index - Nstart] + 1); i7++)
                {
                    fscanf(file_compare, "%d%d%d%d%d%d%d%d",
                           &read_attributes[0], &read_attributes[1],
                           &read_attributes[2], &read_attributes[3],
                           &read_attributes[4], &read_attributes[5],
                           &read_attributes[6], &read_attributes[7]);

                    compare_rules[i7].antecedent_attr0 = read_attributes[0];
                    compare_rules[i7].antecedent_attr1 = read_attributes[1];
                    compare_rules[i7].antecedent_attr2 = read_attributes[2];
                    compare_rules[i7].antecedent_attr3 = read_attributes[3];
                    compare_rules[i7].antecedent_attr4 = read_attributes[4];
                    compare_rules[i7].antecedent_attr5 = read_attributes[5];
                    compare_rules[i7].antecedent_attr6 = read_attributes[6];
                    compare_rules[i7].antecedent_attr7 = read_attributes[7];
                }
                fclose(file_compare);

                /* 重複チェック */
                for (i2 = 1; i2 < rule_count; i2++)
                {
                    for (i7 = 1; i7 < (rules_per_trial[compare_trial_index - Nstart] + 1); i7++)
                    {
                        if (rule_pool[i2].antecedent_attr0 == compare_rules[i7].antecedent_attr0 &&
                            rule_pool[i2].antecedent_attr1 == compare_rules[i7].antecedent_attr1 &&
                            rule_pool[i2].antecedent_attr2 == compare_rules[i7].antecedent_attr2 &&
                            rule_pool[i2].antecedent_attr3 == compare_rules[i7].antecedent_attr3 &&
                            rule_pool[i2].antecedent_attr4 == compare_rules[i7].antecedent_attr4 &&
                            rule_pool[i2].antecedent_attr5 == compare_rules[i7].antecedent_attr5 &&
                            rule_pool[i2].antecedent_attr6 == compare_rules[i7].antecedent_attr6 &&
                            rule_pool[i2].antecedent_attr7 == compare_rules[i7].antecedent_attr7)
                        {
                            new_rule_marker[i2] = 1;
                        }
                    }
                }

                printf(" (%4d rules) checked. \n", rules_per_trial[compare_trial_index - Nstart]);
            }

            /* 新規ルール数カウント */
            new_rule_count = 0;
            new_high_support = 0;
            new_low_variance = 0;

            /* 新規ルールのみをグローバルプールに追加 */
            for (i2 = 1; i2 < rule_count; i2++)
            {
                if (new_rule_marker[i2] == 0)
                {
                    selected_attributes[0] = attribute_set[rule_pool[i2].antecedent_attr0];
                    selected_attributes[1] = attribute_set[rule_pool[i2].antecedent_attr1];
                    selected_attributes[2] = attribute_set[rule_pool[i2].antecedent_attr2];
                    selected_attributes[3] = attribute_set[rule_pool[i2].antecedent_attr3];
                    selected_attributes[4] = attribute_set[rule_pool[i2].antecedent_attr4];
                    selected_attributes[5] = attribute_set[rule_pool[i2].antecedent_attr5];
                    selected_attributes[6] = attribute_set[rule_pool[i2].antecedent_attr6];
                    selected_attributes[7] = attribute_set[rule_pool[i2].antecedent_attr7];

                    file_pool_attribute = fopen(fpoola, "a");
                    fprintf(file_pool_attribute, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                            attribute_dictionary[selected_attributes[0]],
                            attribute_dictionary[selected_attributes[1]],
                            attribute_dictionary[selected_attributes[2]],
                            attribute_dictionary[selected_attributes[3]],
                            attribute_dictionary[selected_attributes[4]],
                            attribute_dictionary[selected_attributes[5]],
                            attribute_dictionary[selected_attributes[6]],
                            attribute_dictionary[selected_attributes[7]],
                            rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                            rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                            rule_pool[i2].support_count, rule_pool[i2].negative_count,
                            rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
                    fclose(file_pool_attribute);

                    file_pool_numeric = fopen(fpoolb, "a");
                    fprintf(file_pool_numeric, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                            selected_attributes[0], selected_attributes[1],
                            selected_attributes[2], selected_attributes[3],
                            selected_attributes[4], selected_attributes[5],
                            selected_attributes[6], selected_attributes[7],
                            rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                            rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                            rule_pool[i2].support_count, rule_pool[i2].negative_count,
                            rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
                    fclose(file_pool_numeric);

                    new_rule_count = new_rule_count + 1;

                    if (rule_pool[i2].high_support_flag == 1)
                    {
                        new_high_support = new_high_support + 1;
                    }
                    if (rule_pool[i2].low_variance_flag == 1)
                    {
                        new_low_variance = new_low_variance + 1;
                    }
                }
            }

            printf("\t %4d rules are new. (%4d %4d) \n", new_rule_count, new_high_support, new_low_variance);
            total_rule_count = total_rule_count + new_rule_count;
            total_high_support = total_high_support + new_high_support;
            total_low_variance = total_low_variance + new_low_variance;
        }

        printf("\t total= %7d rules (%4d %4d)\n\n", total_rule_count, total_high_support, total_low_variance);

        /* ========================================
           統計情報をドキュメントファイルに記録
           ======================================== */
        file_document = fopen(fcont, "a");
        fprintf(file_document, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                (trial_index - Nstart + 1), (rule_count - 1),
                (high_support_rule_count - 1), (low_variance_rule_count - 1),
                total_rule_count, total_high_support, total_low_variance);
        fclose(file_document);
    }

    return;
}