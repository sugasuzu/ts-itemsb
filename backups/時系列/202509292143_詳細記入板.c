#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   GNP-based Time-Series Association Rule Mining System
   時系列対応版GNMiner - Phase 3実装

   【システム概要】
   このプログラムは、Genetic Network Programming (GNP)を用いて
   時系列関連ルールを自動的に発見するデータマイニングシステムです。

   【Phase 3の特徴】
   - 有効なルールの遅延パターンを学習し、進化過程で再利用
   - 遅延値の使用頻度統計を5世代分保持
   - ルーレット選択による適応的な遅延値選択
   - 高品質ルール（高サポート・低分散）のパターンを優先的に学習

   【時系列ルール形式】
   A1(t-d1) ∧ A2(t-d2) ∧ ... → (X,Y)(t+1)
   where:
     - Ai: 属性（条件）
     - di ∈ {0,1,2,3}: 各属性の時間遅延（進化的に最適化）
     - (X,Y): 予測対象の2次元連続値
     - t+1: 1時刻先の予測
   ================================================================================ */

/* ================================================================================
   時系列パラメータ定義
   これらのパラメータで時系列処理の挙動を制御
   ================================================================================ */
#define TIMESERIES_MODE 1       /* 時系列モード有効化フラグ（1:有効, 0:無効） */
#define MAX_TIME_DELAY 10       /* 最大時間遅延（将来の拡張用、現在未使用） */
#define MAX_TIME_DELAY_PHASE3 3 /* Phase 3での実際の最大遅延値（t-0〜t-3） */
#define MIN_TIME_DELAY 0        /* 最小時間遅延（t-0 = 現在時刻） */
#define PREDICTION_SPAN 1       /* 予測時点（t+1 = 1時刻先を予測） */
#define ADAPTIVE_DELAY 1        /* 適応的遅延選択の有効化（1:有効, 0:無効） */

/* ================================================================================
   ディレクトリ構造定義
   出力ファイルを整理するためのディレクトリ構成
   ================================================================================= */
#define OUTPUT_DIR "output"           /* メイン出力ディレクトリ */
#define OUTPUT_DIR_IL "output/IL"     /* Individual List: 個体別ルールリスト */
#define OUTPUT_DIR_IA "output/IA"     /* Individual Analysis: 個体別分析レポート */
#define OUTPUT_DIR_IB "output/IB"     /* Individual Backup: 個体別バックアップ */
#define OUTPUT_DIR_POOL "output/pool" /* グローバルルールプール */
#define OUTPUT_DIR_DOC "output/doc"   /* ドキュメント・統計情報 */

/* ================================================================================
   ファイル定義セクション
   入出力ファイルのパス定義
   ================================================================================= */
#define DATANAME "testdata1s.txt"            /* 入力データファイル（時系列データ） */
#define DICNAME "testdic1.txt"               /* 属性辞書ファイル（属性名定義） */
#define fpoola "output/pool/zrp01a.txt"      /* グローバルルールプール（属性名版） */
#define fpoolb "output/pool/zrp01b.txt"      /* グローバルルールプール（数値版） */
#define fcont "output/doc/zrd01.txt"         /* 実行統計ドキュメント */
#define RESULTNAME "output/doc/zrmemo01.txt" /* 結果メモファイル */

/* ================================================================================
   データ構造パラメータ定義
   データセットのサイズと構造を定義
   ================================================================================= */
#define Nrd 159 /* 総レコード数（時系列データの長さ） */
#define Nzk 95  /* 前件部用の属性数（条件として使える属性の数） */

/* ================================================================================
   ルールマイニング制約パラメータ
   発見するルールの品質基準
   ================================================================================= */
#define Nrulemax 2002 /* 最大ルール数（メモリ制約） */
#define Minsup 0.04   /* 最小サポート値（ルールの出現頻度閾値） */
#define Maxsigx 0.5   /* X方向の最大標準偏差（分散の制約） */
#define Maxsigy 0.5   /* Y方向の最大標準偏差（分散の制約） */

/* ================================================================================
   実験実行パラメータ
   実験の規模と繰り返し回数
   ================================================================================= */
#define Nstart 1000 /* 開始試行番号（ファイル名に使用） */
#define Ntry 100    /* 試行回数（独立した実験の回数） */

/* ================================================================================
   GNPパラメータ
   進化計算のパラメータ設定
   ================================================================================= */
#define Generation 201 /* 世代数（進化の繰り返し回数） */
#define Nkotai 120     /* 個体数（個体群のサイズ） */
#define Npn 10         /* 処理ノード数（GNPの開始点数） */
#define Njg 100        /* 判定ノード数（条件判定を行うノード数） */
#define Nmx 7          /* 最大ルール長（前件部の最大属性数） */
#define Muratep 1      /* 処理ノード突然変異率の逆数 */
#define Muratej 6      /* 判定ノード接続突然変異率の逆数 */
#define Muratea 6      /* 属性突然変異率の逆数 */
#define Muratedelay 6  /* 時間遅延突然変異率の逆数 */
#define Nkousa 20      /* 交叉点数（交叉操作で交換するノード数） */

/* ================================================================================
   ディレクトリ作成関数
   プログラム開始時に必要なディレクトリ構造を作成
   ================================================================================= */
void create_directories()
{
    /* 各ディレクトリを作成（既に存在する場合は何もしない） */
    mkdir(OUTPUT_DIR, 0755);      /* メインディレクトリ */
    mkdir(OUTPUT_DIR_IL, 0755);   /* ルールリスト用 */
    mkdir(OUTPUT_DIR_IA, 0755);   /* 分析レポート用 */
    mkdir(OUTPUT_DIR_IB, 0755);   /* バックアップ用 */
    mkdir(OUTPUT_DIR_POOL, 0755); /* ルールプール用 */
    mkdir(OUTPUT_DIR_DOC, 0755);  /* ドキュメント用 */

    /* ディレクトリ構造の表示 */
    printf("=== Directory Structure Created ===\n");
    printf("output/\n");
    printf("├── IL/        (Rule Lists)\n");
    printf("├── IA/        (Analysis Reports)\n");
    printf("├── IB/        (Backup Files)\n");
    printf("├── pool/      (Global Rule Pool)\n");
    printf("└── doc/       (Documentation)\n");
    printf("===================================\n");

    /* 時系列モードの設定を表示 */
    if (TIMESERIES_MODE)
    {
        printf("*** TIME-SERIES MODE ENABLED (Phase 3) ***\n");
        printf("Adaptive delay range: t-%d to t-%d\n", MIN_TIME_DELAY, MAX_TIME_DELAY_PHASE3);
        printf("Adaptive learning: %s\n", ADAPTIVE_DELAY ? "Enabled" : "Disabled");
        printf("Prediction span: t+%d\n", PREDICTION_SPAN);
        printf("=========================================\n\n");
    }
}

int main()
{
    /* ================================================================================
       ルール構造体定義（時系列対応版）
       発見されたルールを格納するデータ構造
       ================================================================================ */
    struct asrule
    {
        /* 前件部の属性ID（最大8個まで）
           値が0の場合はその位置に属性がないことを示す */
        int antecedent_attr0;
        int antecedent_attr1;
        int antecedent_attr2;
        int antecedent_attr3;
        int antecedent_attr4;
        int antecedent_attr5;
        int antecedent_attr6;
        int antecedent_attr7;

        /* 各属性の時間遅延（0〜3の値を取る）
           例: time_delay0=2 なら attr0 は t-2 の時点を参照 */
        int time_delay0;
        int time_delay1;
        int time_delay2;
        int time_delay3;
        int time_delay4;
        int time_delay5;
        int time_delay6;
        int time_delay7;

        /* 後件部の統計情報（予測対象の分布） */
        double x_mean;  /* X値の平均 */
        double x_sigma; /* X値の標準偏差 */
        double y_mean;  /* Y値の平均 */
        double y_sigma; /* Y値の標準偏差 */

        /* 評価指標 */
        int support_count;     /* サポート数（ルールが成立した回数） */
        int negative_count;    /* 負例数（全体数 - 評価数 + サポート数） */
        int high_support_flag; /* 高サポートフラグ（1:高サポート） */
        int low_variance_flag; /* 低分散フラグ（1:低分散） */
    };
    struct asrule rule_pool[Nrulemax]; /* ルールプール配列 */

    /* ================================================================================
       比較用ルール構造体
       ルールの重複チェックに使用（前件部のみ）
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
        /* 注: Phase 3では時間遅延の違いは許容するため比較対象外 */
    };
    struct cmrule compare_rules[Nrulemax];

    /* ================================================================================
       作業用変数群の宣言
       プログラム全体で使用する各種変数
       ================================================================================ */

    /* 汎用作業変数 */
    int work_array[5];
    int input_value;

    /* ルールカウンタ */
    int rule_count;              /* 現在のルール数 */
    int rule_count2;             /* 作業用ルールカウンタ */
    int high_support_rule_count; /* 高サポートルール数 */
    int low_variance_rule_count; /* 低分散ルール数 */

    /* ループカウンタ（多重ループで使用） */
    int i, j, k;
    int loop_i, loop_j, loop_k;
    int i2, j2, k2;
    int i3, j3, k3;
    int i4, j4, k4;
    int i5, j5;
    int i6, j6;
    int i7, j7, k7, k8;
    int attr_index;

    /* GNP関連変数 */
    int current_node_id; /* 現在処理中のノードID */
    int loop_trial;      /* 試行ループカウンタ */
    int generation_id;   /* 世代番号 */
    int individual_id;   /* 個体番号 */

    /* ルール評価変数 */
    int matched_antecedent_count; /* 前件部がマッチした回数 */
    int process_id;               /* 処理ID */
    int new_flag;                 /* 新規ルールフラグ */
    int same_flag1, same_flag2;   /* 同一性チェックフラグ */
    int match_flag;               /* マッチングフラグ */

    /* 突然変異関連変数 */
    int mutation_random;                                /* 突然変異判定用乱数 */
    int mutation_attr1, mutation_attr2, mutation_attr3; /* 属性突然変異用 */
    int total_attribute_usage;                          /* 属性使用頻度の合計 */
    int difference_count;                               /* 差分カウント */

    /* ★Phase 3追加: 遅延値選択用変数 */
    int mutation_delay1, mutation_delay2, mutation_delay3; /* 遅延突然変異用 */
    int total_delay_usage;                                 /* 遅延使用頻度の合計 */
    int selected_delay_value;                              /* 選択された遅延値 */

    /* 交叉関連変数 */
    int crossover_parent;                 /* 交叉親 */
    int crossover_node1, crossover_node2; /* 交叉ノード */
    int temp_memory;                      /* 一時記憶用 */
    int save_memory;
    int node_memory;
    int node_save1, node_save2;

    /* バックアップ・マーカー変数 */
    int backup_counter2;
    int high_support_marker; /* 高サポートマーカー */
    int low_variance_marker; /* 低分散マーカー */

    /* 数値データ関連変数 */
    double input_x, input_y;               /* 入力X,Y値 */
    double threshold_x, threshold_sigma_x; /* X閾値 */
    double threshold_y, threshold_sigma_y; /* Y閾値 */

    /* 累積統計変数 */
    int total_rule_count;   /* 総ルール数 */
    int new_rule_count;     /* 新規ルール数 */
    int total_high_support; /* 総高サポート数 */
    int total_low_variance; /* 総低分散数 */
    int new_high_support;   /* 新規高サポート数 */
    int new_low_variance;   /* 新規低分散数 */

    /* ファイル名生成用変数 */
    int file_index;
    int file_digit;
    int trial_index; /* 現在の試行番号 */
    int compare_trial_index;
    int file_digit1, file_digit2, file_digit3, file_digit4, file_digit5;
    int compare_digit1, compare_digit2, compare_digit3, compare_digit4, compare_digit5;

    /* 評価値関連変数 */
    double confidence_max;                   /* 最大信頼度 */
    double fitness_average_all;              /* 全体平均適応度 */
    double support_value;                    /* サポート値 */
    double sigma_x, sigma_y;                 /* 標準偏差 */
    double current_x_value, current_y_value; /* 現在のX,Y値 */
    double future_x_value, future_y_value;   /* 未来のX,Y値 */

    /* 時系列用変数 */
    int time_delay;       /* 現在の時間遅延 */
    int data_index;       /* 時間遅延を考慮したデータインデックス */
    int safe_start_index; /* 安全な開始インデックス（過去参照用マージン） */
    int safe_end_index;   /* 安全な終了インデックス（未来参照用マージン） */
    int actual_delay;     /* 実際に使用する遅延値 */

    /* ================================================================================
       データバッファ（Phase 2から継続）
       全データを事前読み込みして高速アクセスを可能にする
       ================================================================================ */
    static int data_buffer[Nrd][Nzk]; /* 全レコードの属性値バッファ */
    static double x_buffer[Nrd];      /* 全レコードのX値バッファ */
    static double y_buffer[Nrd];      /* 全レコードのY値バッファ */

    /* ================================================================================
       ★Phase 3新規: 時間遅延統計配列
       適応的遅延学習のためのデータ構造
       ================================================================================ */

    /* 遅延値使用履歴（5世代分×4種類の遅延値）
       過去の成功パターンを記憶して再利用 */
    int delay_usage_history[5][MAX_TIME_DELAY_PHASE3 + 1];

    /* 現世代での遅延値使用回数
       各遅延値がどれだけ使われたかをカウント */
    int delay_usage_count[MAX_TIME_DELAY_PHASE3 + 1];

    /* 累積遅延値使用統計
       過去5世代分の使用頻度を合計 */
    int delay_tracking[MAX_TIME_DELAY_PHASE3 + 1];

    /* 遅延パターン成功率（将来の拡張用）
       各遅延値の成功率を記録 */
    double delay_success_rate[MAX_TIME_DELAY_PHASE3 + 1];

    /* ================================================================================
       大規模静的配列の宣言（時系列対応版）
       メモリ効率のため静的配列として宣言
       ================================================================================ */

    /* ノード構造配列
       [個体番号][ノード番号][0:属性ID, 1:接続先, 2:時間遅延] */
    static int node_structure[Nkotai][Npn + Njg][3];

    /* レコード属性配列（作業用） */
    static int record_attributes[Nzk + 2];

    /* 統計情報配列群
       [個体番号][処理ノード番号][深さ] */
    static int match_count[Nkotai][Npn][10];      /* マッチ回数 */
    static int negative_count[Nkotai][Npn][10];   /* 負例数 */
    static int evaluation_count[Nkotai][Npn][10]; /* 評価回数 */
    static int attribute_chain[Nkotai][Npn][10];  /* 属性チェーン */

    /* 時間遅延連鎖の記録
       各深さでの時間遅延値を記録 */
    static int time_delay_chain[Nkotai][Npn][10];

    /* 統計値配列（X,Y方向） */
    static double x_sum[Nkotai][Npn][10];         /* X値の合計 */
    static double x_sigma_array[Nkotai][Npn][10]; /* X値の二乗和 */
    static double y_sum[Nkotai][Npn][10];         /* Y値の合計 */
    static double y_sigma_array[Nkotai][Npn][10]; /* Y値の二乗和 */

    /* 試行別統計配列 */
    int rules_per_trial[Ntry];           /* 各試行でのルール数 */
    int attribute_usage_history[5][Nzk]; /* 属性使用履歴（5世代分） */
    int attribute_usage_count[Nzk];      /* 属性使用カウント */
    int attribute_tracking[Nzk];         /* 属性追跡 */

    /* 適応度と順位配列 */
    double fitness_value[Nkotai]; /* 各個体の適応度 */
    int fitness_ranking[Nkotai];  /* 適応度順位 */
    int new_rule_flag[Nkotai];    /* 新規ルール発見フラグ */

    /* GNP遺伝子プール
       進化操作で変更される遺伝子情報 */
    static int gene_connection[Nkotai][Npn + Njg]; /* 接続遺伝子 */
    static int gene_attribute[Nkotai][Npn + Njg];  /* 属性遺伝子 */

    /* 時間遅延遺伝子（Phase 3で追加） */
    static int gene_time_delay[Nkotai][Npn + Njg];

    /* 作業用配列 */
    int selected_attributes[10];
    int read_attributes[10];
    int rules_by_attribute_count[10]; /* 属性数別ルールカウント */
    int attribute_set[Nzk + 1];       /* 属性セット */

    /* ルール候補配列 */
    int rule_candidate_pre[10]; /* ルール候補（前処理） */
    int rule_candidate[10];     /* ルール候補 */
    int rule_memo[10];          /* ルールメモ */

    /* 時間遅延候補配列 */
    int time_delay_candidate[10]; /* 時間遅延候補 */
    int time_delay_memo[10];      /* 時間遅延メモ */

    /* 新規ルールマーカー配列 */
    int new_rule_marker[Nrulemax];

    /* ファイル名文字列配列 */
    char filename_rule[256];   /* ルールファイル名 */
    char filename_report[256]; /* レポートファイル名 */
    char filename_local[256];  /* ローカルファイル名 */

    /* 属性辞書配列 */
    char attribute_name_buffer[31];         /* 属性名バッファ */
    char attribute_dictionary[Nzk + 3][31]; /* 属性辞書 */

    /* ファイルポインタ群 */
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

    /* ディレクトリ構造を作成 */
    create_directories();

    /* 乱数シード設定（再現性のため固定値） */
    srand(1);

    /* ================================================================================
       初期化フェーズ1: 属性辞書の読み込み
       属性IDと属性名の対応を読み込む
       ================================================================================ */

    /* 属性辞書配列を初期化 */
    for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
    {
        strcpy(attribute_dictionary[attr_index], "\0");
    }

    /* 辞書ファイルを開く */
    file_dictionary = fopen(DICNAME, "r");
    if (file_dictionary == NULL)
    {
        /* 辞書ファイルがない場合は属性番号を使用 */
        printf("Warning: Cannot open dictionary file: %s\n", DICNAME);
        printf("Using attribute numbers instead of names.\n");
    }
    else
    {
        /* 辞書ファイルから属性名を読み込み */
        for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
        {
            if (fscanf(file_dictionary, "%d%s", &process_id, attribute_name_buffer) == 2)
            {
                strcpy(attribute_dictionary[attr_index], attribute_name_buffer);
            }
            else
            {
                /* 読み込めない場合はデフォルト名を生成 */
                sprintf(attribute_dictionary[attr_index], "Attr%d", attr_index);
            }
        }
        fclose(file_dictionary);
        printf("Dictionary loaded: %d attributes\n", Nzk);
    }

    /* ================================================================================
       データの事前読み込み
       全データをメモリに読み込んで高速アクセスを可能にする
       ================================================================================ */
    printf("Loading data into buffer for time-series processing...\n");

    /* データファイルを開く */
    file_data2 = fopen(DATANAME, "r");
    if (file_data2 == NULL)
    {
        printf("Error: Cannot open data file: %s\n", DATANAME);
        return 0;
    }

    /* 全レコードを読み込んでバッファに格納 */
    for (i = 0; i < Nrd; i++)
    {
        /* 各属性値を読み込み */
        for (j = 0; j < Nzk; j++)
        {
            fscanf(file_data2, "%d", &input_value);
            data_buffer[i][j] = input_value;
        }
        /* X,Y値を読み込み */
        fscanf(file_data2, "%lf%lf", &input_x, &input_y);
        x_buffer[i] = input_x;
        y_buffer[i] = input_y;
    }
    fclose(file_data2);
    printf("Data loaded: %d records with %d attributes each\n", Nrd, Nzk);

    /* ================================================================================
       初期化フェーズ2: グローバルカウンタ初期化
       全試行を通しての累積統計を管理
       ================================================================================ */
    total_rule_count = 0;
    total_high_support = 0;
    total_low_variance = 0;

    /* ドキュメントファイルを作成（統計情報記録用） */
    file_document = fopen(fcont, "w");
    if (file_document == NULL)
    {
        printf("Error: Cannot create document file: %s\n", fcont);
        return 0;
    }
    /* ヘッダー行を書き込み */
    fprintf(file_document, "Trial\tRules\tHighSup\tLowVar\tTotal\tTotalHigh\tTotalLow\n");
    fclose(file_document);
    printf("Document file initialized: %s\n", fcont);

    /* ================================================================================
       ★★★ メイン試行ループ開始（時系列対応版Phase 3） ★★★
       Ntry回の独立した試行を実行
       ================================================================================ */
    for (trial_index = Nstart; trial_index < (Nstart + Ntry); trial_index++)
    {
        /* 試行開始メッセージ */
        printf("\n========== Trial %d/%d Started ==========\n",
               trial_index - Nstart + 1, Ntry);

        /* Phase 3モードの表示 */
        if (TIMESERIES_MODE && ADAPTIVE_DELAY)
        {
            printf("  [Time-Series Mode Phase 3: Adaptive delays]\n");
        }

        /* ================================================================================
           試行別初期化フェーズ
           各試行の開始時に全データ構造を初期化
           ================================================================================ */

        /* ルールプールを初期化 */
        for (i = 0; i < Nrulemax; i++)
        {
            /* 前件部属性を初期化 */
            rule_pool[i].antecedent_attr0 = 0;
            rule_pool[i].antecedent_attr1 = 0;
            rule_pool[i].antecedent_attr2 = 0;
            rule_pool[i].antecedent_attr3 = 0;
            rule_pool[i].antecedent_attr4 = 0;
            rule_pool[i].antecedent_attr5 = 0;
            rule_pool[i].antecedent_attr6 = 0;
            rule_pool[i].antecedent_attr7 = 0;

            /* 時間遅延を初期化 */
            rule_pool[i].time_delay0 = 0;
            rule_pool[i].time_delay1 = 0;
            rule_pool[i].time_delay2 = 0;
            rule_pool[i].time_delay3 = 0;
            rule_pool[i].time_delay4 = 0;
            rule_pool[i].time_delay5 = 0;
            rule_pool[i].time_delay6 = 0;
            rule_pool[i].time_delay7 = 0;

            /* 統計情報を初期化 */
            rule_pool[i].x_mean = 0;
            rule_pool[i].x_sigma = 0;
            rule_pool[i].y_mean = 0;
            rule_pool[i].y_sigma = 0;

            /* 評価指標を初期化 */
            rule_pool[i].support_count = 0;
            rule_pool[i].negative_count = 0;
            rule_pool[i].high_support_flag = 0;
            rule_pool[i].low_variance_flag = 0;
        }

        /* 比較用ルール配列を初期化 */
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

        /* 新規ルールマーカーを初期化 */
        for (i = 0; i < Nrulemax; i++)
        {
            new_rule_marker[i] = 0;
        }

        /* ★Phase 3: 遅延統計の初期化
           適応的遅延学習のための統計情報を初期化 */
        for (i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
        {
            delay_usage_count[i] = 0;
            delay_tracking[i] = 1;       /* 最小値1で初期化（ゼロ除算回避） */
            delay_success_rate[i] = 0.5; /* 中立的な初期値 */

            /* 履歴の初期化：全遅延値に等しいチャンスを与える */
            for (j = 0; j < 5; j++)
            {
                delay_usage_history[j][i] = 1;
            }
        }
        total_delay_usage = MAX_TIME_DELAY_PHASE3 + 1; /* 初期値は4（0〜3） */

        /* ファイルパス生成処理
           試行番号から5桁のファイル名を生成 */
        file_digit5 = trial_index / 10000;
        file_digit4 = trial_index / 1000 - file_digit5 * 10;
        file_digit3 = trial_index / 100 - file_digit5 * 100 - file_digit4 * 10;
        file_digit2 = trial_index / 10 - file_digit5 * 1000 - file_digit4 * 100 - file_digit3 * 10;
        file_digit1 = trial_index - file_digit5 * 10000 - file_digit4 * 1000 - file_digit3 * 100 - file_digit2 * 10;

        /* 各種出力ファイルのパスを生成 */
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

        /* カウンタの初期化（1から開始） */
        rule_count = 1;
        high_support_rule_count = 1;
        low_variance_rule_count = 1;

        /* 属性数別ルールカウントを初期化 */
        for (i = 0; i < 10; i++)
        {
            rules_by_attribute_count[i] = 0;
        }

        /* 属性使用統計の初期化 */
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
           GNP初期個体群の生成（時系列対応版Phase 3）
           ランダムな初期個体群を生成
           ================================================================================ */
        for (i4 = 0; i4 < Nkotai; i4++)
        {
            for (j4 = 0; j4 < (Npn + Njg); j4++)
            {
                /* ランダムな接続先を設定（判定ノードの範囲内） */
                gene_connection[i4][j4] = rand() % Njg + Npn;

                /* ランダムな属性を設定 */
                gene_attribute[i4][j4] = rand() % Nzk;

                /* Phase 3: 時間遅延をランダム初期化（0〜3） */
                gene_time_delay[i4][j4] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
            }
        }

        /* 出力ファイルの初期化 */

        /* ルールファイル作成 */
        file_rule = fopen(filename_rule, "w");
        if (file_rule == NULL)
        {
            printf("Error: Cannot create rule file: %s\n", filename_rule);
            return 0;
        }
        fprintf(file_rule, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\n");
        fclose(file_rule);
        printf("Created: %s\n", filename_rule);

        /* レポートファイル作成 */
        file_report = fopen(filename_report, "w");
        if (file_report == NULL)
        {
            printf("Error: Cannot create report file: %s\n", filename_report);
            return 0;
        }
        fprintf(file_report, "Generation\tRules\tHighSup\tLowVar\tAvgFitness\n");
        fclose(file_report);

        /* ローカルファイル作成 */
        file_local = fopen(filename_local, "w");
        if (file_local == NULL)
        {
            printf("Error: Cannot create local file: %s\n", filename_local);
            return 0;
        }
        fprintf(file_local, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tHighSup\tLowVar\n");
        fclose(file_local);

        /* ================================================================================
           ★★★ 進化計算メインループ開始（時系列対応版Phase 3） ★★★
           指定世代数まで進化を繰り返す
           ================================================================================ */
        for (generation_id = 0; generation_id < Generation; generation_id++)
        {
            /* ルール数上限チェック */
            if (rule_count > (Nrulemax - 2))
            {
                printf("Rule limit reached. Stopping evolution.\n");
                break;
            }

            /* ================================================================================
               世代開始: GNP遺伝子を個体構造にコピー（時系列対応）
               遺伝子情報をノード構造に変換
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (j4 = 0; j4 < (Npn + Njg); j4++)
                {
                    /* 属性、接続、時間遅延をコピー */
                    node_structure[individual_id][j4][0] = gene_attribute[individual_id][j4];
                    node_structure[individual_id][j4][1] = gene_connection[individual_id][j4];
                    node_structure[individual_id][j4][2] = gene_time_delay[individual_id][j4];
                }
            }

            /* 評価準備: 統計配列の初期化
               各個体の評価前に統計情報をクリア */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                /* 適応度を初期化（個体番号による微小な差をつける） */
                fitness_value[individual_id] = (double)individual_id * (-0.00001);
                fitness_ranking[individual_id] = 0;

                /* 各処理ノード・各深さの統計をクリア */
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
               ★ データセット評価フェーズ（時系列対応版Phase 3） ★
               全データに対して各個体のGNPネットワークを評価
               ================================================================================ */

            /* 時系列用の安全な評価範囲を設定
               過去参照と未来参照のマージンを確保 */
            if (TIMESERIES_MODE)
            {
                safe_start_index = MAX_TIME_DELAY_PHASE3; /* 最大遅延分のマージン */
                safe_end_index = Nrd - PREDICTION_SPAN;   /* 予測時点分のマージン */
            }
            else
            {
                safe_start_index = 0;
                safe_end_index = Nrd;
            }

            /* 各時刻のデータを評価 */
            for (i = safe_start_index; i < safe_end_index; i++)
            {
                /* 現在時点(i)と未来時点(i+PREDICTION_SPAN)のデータを取得 */
                future_x_value = x_buffer[i + PREDICTION_SPAN];
                future_y_value = y_buffer[i + PREDICTION_SPAN];

                /* ================================================================================
                   ★★ GNPネットワーク評価の核心部分 ★★
                   各個体のネットワーク構造に従ってルール候補を評価
                   ================================================================================ */
                for (individual_id = 0; individual_id < Nkotai; individual_id++)
                {
                    /* 各処理ノードから開始 */
                    for (k = 0; k < Npn; k++)
                    {
                        current_node_id = k;
                        j = 0;          /* 深さカウンタ */
                        match_flag = 1; /* マッチフラグ（1:マッチ継続中） */

                        /* ルートノード（深さ0）の統計更新 */
                        match_count[individual_id][k][j] = match_count[individual_id][k][j] + 1;
                        evaluation_count[individual_id][k][j] = evaluation_count[individual_id][k][j] + 1;

                        /* 最初の判定ノードへ遷移 */
                        current_node_id = node_structure[individual_id][current_node_id][1];

                        /* 判定ノードチェーンを辿る
                           条件が満たされる限り深く進む */
                        while (current_node_id > (Npn - 1) && j < Nmx)
                        {
                            j = j + 1; /* 深さを増加 */

                            /* 現在のノードの属性を記録 */
                            attribute_chain[individual_id][k][j] =
                                node_structure[individual_id][current_node_id][0] + 1;

                            /* 時間遅延を記録 */
                            time_delay = node_structure[individual_id][current_node_id][2];
                            time_delay_chain[individual_id][k][j] = time_delay;

                            /* ★時間遅延を考慮したデータインデックス計算
                               現在時刻から遅延分だけ過去を参照 */
                            data_index = i - time_delay;

                            /* 過去参照の安全性チェック */
                            if (data_index < 0)
                            {
                                /* データ範囲外の場合は処理を中断 */
                                current_node_id = k;
                                break;
                            }

                            /* ★実際の過去データを参照して条件判定 */
                            if (data_buffer[data_index][node_structure[individual_id][current_node_id][0]] == 1)
                            {
                                /* 条件が満たされた場合 */
                                if (match_flag == 1)
                                {
                                    /* マッチ回数を増加 */
                                    match_count[individual_id][k][j] =
                                        match_count[individual_id][k][j] + 1;

                                    /* ★未来時点(i+PREDICTION_SPAN)のX,Y値を累積
                                       これが予測対象となる */
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

                                /* 評価回数を増加 */
                                evaluation_count[individual_id][k][j] =
                                    evaluation_count[individual_id][k][j] + 1;

                                /* Yes側（条件成立側）へ遷移 */
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                            else if (data_buffer[data_index][node_structure[individual_id][current_node_id][0]] == 0)
                            {
                                /* 条件が満たされなかった場合：No側へ遷移 */
                                current_node_id = k; /* 処理ノードに戻る */
                            }
                            else
                            {
                                /* 欠損値の処理（値が0,1以外） */
                                evaluation_count[individual_id][k][j] =
                                    evaluation_count[individual_id][k][j] + 1;

                                match_flag = 0; /* マッチフラグをオフ */
                                /* とりあえず次のノードへ進む */
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                        }
                    }
                }
            }

            /* 統計後処理1: 負例数の計算
               負例数 = 全体数 - 評価数 + マッチ数 */
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
                            /* X方向の平均を計算 */
                            x_sum[individual_id][k][j6] =
                                x_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];

                            /* X方向の分散を計算（E[X^2] - E[X]^2） */
                            x_sigma_array[individual_id][k][j6] =
                                x_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - x_sum[individual_id][k][j6] * x_sum[individual_id][k][j6];

                            /* Y方向の平均を計算 */
                            y_sum[individual_id][k][j6] =
                                y_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];

                            /* Y方向の分散を計算（E[Y^2] - E[Y]^2） */
                            y_sigma_array[individual_id][k][j6] =
                                y_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - y_sum[individual_id][k][j6] * y_sum[individual_id][k][j6];

                            /* 分散が負になった場合の補正 */
                            if (x_sigma_array[individual_id][k][j6] < 0)
                            {
                                x_sigma_array[individual_id][k][j6] = 0;
                            }
                            if (y_sigma_array[individual_id][k][j6] < 0)
                            {
                                y_sigma_array[individual_id][k][j6] = 0;
                            }

                            /* 標準偏差を計算（分散の平方根） */
                            x_sigma_array[individual_id][k][j6] = sqrt(x_sigma_array[individual_id][k][j6]);
                            y_sigma_array[individual_id][k][j6] = sqrt(y_sigma_array[individual_id][k][j6]);
                        }
                    }
                }
            }

            /* ================================================================================
               ★★★ ルール抽出フェーズ（時系列対応版Phase 3） ★★★
               評価結果から興味深いルールを抽出
               ================================================================================= */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                /* ルール数上限チェック */
                if (rule_count > (Nrulemax - 2))
                {
                    break;
                }

                /* 各処理ノードのチェーンを調べる */
                for (k = 0; k < Npn; k++)
                {
                    if (rule_count > (Nrulemax - 2))
                    {
                        break;
                    }

                    /* 候補配列を初期化 */
                    for (i = 0; i < 10; i++)
                    {
                        rule_candidate_pre[i] = 0;
                        rule_candidate[i] = 0;
                        rule_memo[i] = 0;
                        time_delay_candidate[i] = 0;
                        time_delay_memo[i] = 0;
                    }

                    /* 各深さでルール候補をチェック */
                    for (loop_j = 1; loop_j < 9; loop_j++)
                    {
                        /* 統計値を取得 */
                        threshold_x = x_sum[individual_id][k][loop_j];
                        threshold_sigma_x = Maxsigx;

                        threshold_y = y_sum[individual_id][k][loop_j];
                        threshold_sigma_y = Maxsigy;

                        matched_antecedent_count = match_count[individual_id][k][loop_j];

                        /* サポート値と標準偏差を計算 */
                        support_value = 0;
                        sigma_x = x_sigma_array[individual_id][k][loop_j];
                        sigma_y = y_sigma_array[individual_id][k][loop_j];

                        if (negative_count[individual_id][k][loop_j] != 0)
                        {
                            support_value = (double)matched_antecedent_count /
                                            (double)negative_count[individual_id][k][loop_j];
                        }

                        /* ★興味深いルールの条件判定
                           - 標準偏差が閾値以下（分布が狭い）
                           - サポート値が閾値以上（頻度が高い） */
                        if (sigma_x <= threshold_sigma_x &&
                            sigma_y <= threshold_sigma_y &&
                            support_value >= Minsup)
                        {
                            /* ルール前件部の構築 */
                            for (i2 = 1; i2 < 9; i2++)
                            {
                                rule_candidate_pre[i2 - 1] = attribute_chain[individual_id][k][i2];
                                time_delay_candidate[i2 - 1] = time_delay_chain[individual_id][k][i2];
                            }

                            /* 重複する属性を除去して整理 */
                            j2 = 0;
                            for (k2 = 1; k2 < (1 + Nzk); k2++)
                            {
                                new_flag = 0;
                                for (i2 = 0; i2 < loop_j; i2++)
                                {
                                    if (rule_candidate_pre[i2] == k2)
                                    {
                                        new_flag = 1;
                                        /* 対応する時間遅延も保存 */
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

                            /* 属性数が上限未満の場合のみ処理 */
                            if (j2 < 9)
                            {
                                /* 既存ルールとの重複チェック */
                                new_flag = 0;

                                for (i2 = 0; i2 < rule_count; i2++)
                                {
                                    same_flag1 = 1;

                                    /* 前件部の属性が完全一致するかチェック */
                                    if (rule_pool[i2].antecedent_attr0 == rule_candidate[0] &&
                                        rule_pool[i2].antecedent_attr1 == rule_candidate[1] &&
                                        rule_pool[i2].antecedent_attr2 == rule_candidate[2] &&
                                        rule_pool[i2].antecedent_attr3 == rule_candidate[3] &&
                                        rule_pool[i2].antecedent_attr4 == rule_candidate[4] &&
                                        rule_pool[i2].antecedent_attr5 == rule_candidate[5] &&
                                        rule_pool[i2].antecedent_attr6 == rule_candidate[6] &&
                                        rule_pool[i2].antecedent_attr7 == rule_candidate[7])
                                    {
                                        /* Phase 3では時間遅延の違いは許容（属性の組み合わせのみチェック） */
                                        same_flag1 = 0;
                                    }

                                    if (same_flag1 == 0)
                                    {
                                        /* 既存ルールと重複 */
                                        new_flag = 1;

                                        /* 適応度を更新（重複ルールでも価値がある） */
                                        fitness_value[individual_id] =
                                            fitness_value[individual_id] + j2 + support_value * 10 + 2 / (sigma_x + 0.1) + 2 / (sigma_y + 0.1);
                                    }

                                    if (new_flag == 1)
                                    {
                                        break;
                                    }
                                }

                                /* 新規ルールの場合 */
                                if (new_flag == 0)
                                {
                                    /* ★新規ルールをプールに登録 */

                                    /* 前件部属性を保存 */
                                    rule_pool[rule_count].antecedent_attr0 = rule_candidate[0];
                                    rule_pool[rule_count].antecedent_attr1 = rule_candidate[1];
                                    rule_pool[rule_count].antecedent_attr2 = rule_candidate[2];
                                    rule_pool[rule_count].antecedent_attr3 = rule_candidate[3];
                                    rule_pool[rule_count].antecedent_attr4 = rule_candidate[4];
                                    rule_pool[rule_count].antecedent_attr5 = rule_candidate[5];
                                    rule_pool[rule_count].antecedent_attr6 = rule_candidate[6];
                                    rule_pool[rule_count].antecedent_attr7 = rule_candidate[7];

                                    /* ★実際の時間遅延を保存（Phase 3の重要な部分） */
                                    rule_pool[rule_count].time_delay0 = (rule_candidate[0] > 0) ? time_delay_memo[0] : 0;
                                    rule_pool[rule_count].time_delay1 = (rule_candidate[1] > 0) ? time_delay_memo[1] : 0;
                                    rule_pool[rule_count].time_delay2 = (rule_candidate[2] > 0) ? time_delay_memo[2] : 0;
                                    rule_pool[rule_count].time_delay3 = (rule_candidate[3] > 0) ? time_delay_memo[3] : 0;
                                    rule_pool[rule_count].time_delay4 = (rule_candidate[4] > 0) ? time_delay_memo[4] : 0;
                                    rule_pool[rule_count].time_delay5 = (rule_candidate[5] > 0) ? time_delay_memo[5] : 0;
                                    rule_pool[rule_count].time_delay6 = (rule_candidate[6] > 0) ? time_delay_memo[6] : 0;
                                    rule_pool[rule_count].time_delay7 = (rule_candidate[7] > 0) ? time_delay_memo[7] : 0;

                                    /* 統計情報を保存 */
                                    rule_pool[rule_count].x_mean = x_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].x_sigma = x_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_mean = y_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_sigma = y_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].support_count = matched_antecedent_count;
                                    rule_pool[rule_count].negative_count = negative_count[individual_id][k][loop_j];

                                    /* ★ファイルへの出力（時間遅延付き） */
                                    file_rule = fopen(filename_rule, "a");

                                    /* 各属性とその時間遅延を出力
                                       形式: 属性番号(t-遅延値) */
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

                                    /* 高サポートマーカー */
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

                                    /* 低分散マーカー */
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

                                    /* ★★Phase 3: 遅延パターンの学習★★
                                       発見されたルールの遅延値を統計に追加
                                       これにより成功パターンを学習 */
                                    for (k3 = 0; k3 < j2; k3++)
                                    {
                                        if (time_delay_memo[k3] >= 0 && time_delay_memo[k3] <= MAX_TIME_DELAY_PHASE3)
                                        {
                                            /* この遅延値の使用頻度を増加 */
                                            delay_usage_history[0][time_delay_memo[k3]] += 1;

                                            /* 高品質ルールの場合は追加ボーナス
                                               良いルールのパターンをより強く学習 */
                                            if (high_support_marker || low_variance_marker)
                                            {
                                                delay_usage_history[0][time_delay_memo[k3]] += 2;
                                            }
                                        }
                                    }

                                    /* ルール数と統計を更新 */
                                    rule_count = rule_count + 1;
                                    rules_by_attribute_count[j2] = rules_by_attribute_count[j2] + 1;

                                    /* 適応度を大幅に増加（新規ルール発見ボーナス） */
                                    fitness_value[individual_id] =
                                        fitness_value[individual_id] + j2 + support_value * 10 + 2 / (sigma_x + 0.1) + 2 / (sigma_y + 0.1) + 20; /* 新規ルールボーナス */

                                    /* 属性使用頻度の更新 */
                                    for (k3 = 0; k3 < j2; k3++)
                                    {
                                        i5 = rule_memo[k3] - 1;
                                        attribute_usage_history[0][i5] =
                                            attribute_usage_history[0][i5] + 1;
                                    }
                                }

                                /* ルール数上限チェック */
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

            /* 世代のルール抽出が終了 */
            if (rule_count > (Nrulemax - 2))
            {
                break;
            }

            /* ================================================================================
               ★Phase 3: 遅延統計の更新
               世代終了時に遅延使用統計を集計・更新
               ================================================================================ */

            /* 遅延使用頻度の集計 */
            total_delay_usage = 0;
            for (i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
            {
                delay_tracking[i] = 0;

                /* 過去5世代分の履歴を合計 */
                for (j = 0; j < 5; j++)
                {
                    delay_tracking[i] += delay_usage_history[j][i];
                }

                /* 全体の使用頻度を計算 */
                total_delay_usage += delay_tracking[i];
            }

            /* 履歴配列をシフト（古い情報を削除） */
            for (i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
            {
                /* 古い世代の情報を後ろにシフト */
                for (j = 4; j > 0; j--)
                {
                    delay_usage_history[j][i] = delay_usage_history[j - 1][i];
                }

                /* 現世代をリセット（最小値を保証） */
                delay_usage_history[0][i] = 1;

                /* 5世代ごとにリフレッシュ（探索の多様性を維持） */
                if (generation_id % 5 == 0)
                {
                    delay_usage_history[0][i] = 2;
                }
            }

            /* 属性使用統計の更新（遅延統計と同様の処理） */
            total_attribute_usage = 0;
            for (i4 = 0; i4 < Nzk; i4++)
            {
                attribute_usage_count[i4] = 0;
                attribute_tracking[i4] = 0;
            }

            /* 過去5世代分の属性使用頻度を集計 */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 0; j4 < 5; j4++)
                {
                    attribute_tracking[i4] = attribute_tracking[i4] +
                                             attribute_usage_history[j4][i4];
                }
            }

            /* 属性履歴配列をシフト */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 4; j4 > 0; j4--)
                {
                    attribute_usage_history[j4][i4] = attribute_usage_history[j4 - 1][i4];
                }

                attribute_usage_history[0][i4] = 0;

                /* 5世代ごとにリフレッシュ */
                if (generation_id % 5 == 0)
                {
                    attribute_usage_history[0][i4] = 1;
                }
            }

            /* 総属性使用頻度を計算 */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                total_attribute_usage = total_attribute_usage + attribute_tracking[i4];
            }

            /* 現世代の属性使用をカウント */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    attribute_usage_count[(gene_attribute[i4][j4])] =
                        attribute_usage_count[(gene_attribute[i4][j4])] + 1;
                }
            }

            /* ================================================================================
               適応度によるランキング計算
               選択のための個体順位付け
               ================================================================================ */
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

            /* ================================================================================
               選択処理
               上位1/3の個体を3倍に複製して次世代を作成
               ================================================================================ */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                if (fitness_ranking[i4] < 40) /* 上位40個体（120個体の1/3） */
                {
                    for (j4 = 0; j4 < (Npn + Njg); j4++)
                    {
                        /* 元の位置（0-39）にコピー */
                        gene_attribute[fitness_ranking[i4]][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4]][j4] = node_structure[i4][j4][1];
                        gene_time_delay[fitness_ranking[i4]][j4] = node_structure[i4][j4][2];

                        /* 複製1（40-79）にコピー */
                        gene_attribute[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][1];
                        gene_time_delay[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][2];

                        /* 複製2（80-119）にコピー */
                        gene_attribute[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][1];
                        gene_time_delay[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][2];
                    }
                }
            }

            /* ================================================================================
               交叉処理
               上位個体間で遺伝子を交換
               ================================================================================ */
            for (i4 = 0; i4 < 20; i4++)
            {
                for (j4 = 0; j4 < Nkousa; j4++)
                {
                    /* ランダムに交叉点を選択 */
                    crossover_node1 = rand() % Njg + Npn;

                    /* 接続遺伝子を交換 */
                    temp_memory = gene_connection[i4 + 20][crossover_node1];
                    gene_connection[i4 + 20][crossover_node1] = gene_connection[i4][crossover_node1];
                    gene_connection[i4][crossover_node1] = temp_memory;

                    /* 属性遺伝子を交換 */
                    temp_memory = gene_attribute[i4 + 20][crossover_node1];
                    gene_attribute[i4 + 20][crossover_node1] = gene_attribute[i4][crossover_node1];
                    gene_attribute[i4][crossover_node1] = temp_memory;

                    /* 時間遅延遺伝子も交換 */
                    temp_memory = gene_time_delay[i4 + 20][crossover_node1];
                    gene_time_delay[i4 + 20][crossover_node1] = gene_time_delay[i4][crossover_node1];
                    gene_time_delay[i4][crossover_node1] = temp_memory;
                }
            }

            /* ================================================================================
               突然変異処理
               遺伝的多様性を維持するためのランダムな変更
               ================================================================================ */

            /* 突然変異0: 処理ノードの接続変異 */
            for (i4 = 0; i4 < 120; i4++)
            {
                for (j4 = 0; j4 < Npn; j4++)
                {
                    mutation_random = rand() % Muratep;
                    if (mutation_random == 0)
                    {
                        /* ランダムな判定ノードへ接続を変更 */
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                    }
                }
            }

            /* 突然変異1: 判定ノード接続の変異（個体40-79） */
            for (i4 = 40; i4 < 80; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    /* 接続の突然変異 */
                    mutation_random = rand() % Muratej;
                    if (mutation_random == 0)
                    {
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                    }

                    /* ★★Phase 3: 適応的時間遅延の突然変異★★
                       成功パターンに基づいて遅延値を選択 */
                    mutation_random = rand() % Muratedelay;
                    if (mutation_random == 0)
                    {
                        if (ADAPTIVE_DELAY && total_delay_usage > 0)
                        {
                            /* ★ルーレット選択による適応的遅延選択
                               使用頻度が高い遅延値ほど選ばれやすい */
                            mutation_delay1 = rand() % total_delay_usage;
                            mutation_delay2 = 0;
                            selected_delay_value = 0;

                            /* ルーレット選択アルゴリズム */
                            for (i5 = 0; i5 <= MAX_TIME_DELAY_PHASE3; i5++)
                            {
                                mutation_delay2 += delay_tracking[i5];
                                if (mutation_delay1 < mutation_delay2)
                                {
                                    selected_delay_value = i5;
                                    break;
                                }
                            }

                            /* 選択された遅延値を設定 */
                            gene_time_delay[i4][j4] = selected_delay_value;
                        }
                        else
                        {
                            /* 通常のランダム選択（適応学習無効時） */
                            gene_time_delay[i4][j4] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
                        }
                    }
                }
            }

            /* 突然変異2: 属性の適応的変異（個体80-119） */
            for (i4 = 80; i4 < 120; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    /* 属性の適応的突然変異 */
                    mutation_random = rand() % Muratea;
                    if (mutation_random == 0)
                    {
                        /* よく使われる属性を優先的に選択 */
                        mutation_attr1 = rand() % total_attribute_usage;
                        mutation_attr2 = 0;
                        mutation_attr3 = 0;

                        /* ルーレット選択で属性を決定 */
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

                    /* ★Phase 3: 適応的時間遅延の突然変異（第2グループ） */
                    mutation_random = rand() % Muratedelay;
                    if (mutation_random == 0)
                    {
                        if (ADAPTIVE_DELAY && total_delay_usage > 0)
                        {
                            /* ルーレット選択による適応的遅延選択 */
                            mutation_delay1 = rand() % total_delay_usage;
                            mutation_delay2 = 0;
                            selected_delay_value = 0;

                            for (i5 = 0; i5 <= MAX_TIME_DELAY_PHASE3; i5++)
                            {
                                mutation_delay2 += delay_tracking[i5];
                                if (mutation_delay1 < mutation_delay2)
                                {
                                    selected_delay_value = i5;
                                    break;
                                }
                            }

                            gene_time_delay[i4][j4] = selected_delay_value;
                        }
                        else
                        {
                            /* 通常のランダム選択 */
                            gene_time_delay[i4][j4] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
                        }
                    }
                }
            }

            /* ================================================================================
               進捗報告
               定期的に世代の統計情報を出力
               ================================================================================ */
            if (generation_id % 5 == 0)
            {
                /* レポートファイルに統計を書き込み */
                file_report = fopen(filename_report, "a");
                fitness_average_all = 0;

                /* 平均適応度を計算 */
                for (i4 = 0; i4 < Nkotai; i4++)
                {
                    fitness_average_all = fitness_average_all + fitness_value[i4];
                }
                fitness_average_all = fitness_average_all / Nkotai;

                /* 統計情報をファイルに記録 */
                fprintf(file_report, "%5d\t%5d\t%5d\t%5d\t%9.3f\n",
                        generation_id,
                        (rule_count - 1),
                        (high_support_rule_count - 1),
                        (low_variance_rule_count - 1),
                        fitness_average_all);
                fclose(file_report);

                /* コンソールに進捗表示 */
                printf("  Gen.=%5d: %5d rules (%5d high-sup, %5d low-var)\n",
                       generation_id, (rule_count - 1),
                       (high_support_rule_count - 1), (low_variance_rule_count - 1));

                /* ★Phase 3: 遅延統計の表示（デバッグ用）
                   どの遅延値がよく使われているかを確認 */
                if (generation_id % 20 == 0 && ADAPTIVE_DELAY)
                {
                    printf("    Delay usage: ");
                    for (i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
                    {
                        printf("t-%d:%d ", i, delay_tracking[i]);
                    }
                    printf("\n");
                }
            }

        } /* generation_id: 世代ループ終了 */

        /* この試行でのルール数を記録 */
        rules_per_trial[trial_index - Nstart] = rule_count - 1;

        /* 試行完了メッセージ */
        printf("\nTrial %d completed:\n", trial_index - Nstart + 1);
        printf("  File: %s\n", filename_rule);
        printf("  Rules: %d (High-support: %d, Low-variance: %d)\n",
               rules_per_trial[trial_index - Nstart],
               (high_support_rule_count - 1), (low_variance_rule_count - 1));

        /* ================================================================================
           ローカル出力（時間遅延付き）
           この試行の全ルールをバックアップファイルに保存
           ================================================================================ */
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

            /* 品質フラグを出力 */
            fprintf(file_local, "%2d\t%2d\n",
                    rule_pool[i].high_support_flag,
                    rule_pool[i].low_variance_flag);
        }
        fclose(file_local);

        /* ================================================================================
           グローバルルールプールへの統合処理
           全試行を通しての累積ルールプールを構築
           ================================================================================= */
        if (trial_index == Nstart)
        {
            /* 最初の試行：新規作成 */

            /* 属性ID版プール作成（時間遅延付き） */
            file_pool_attribute = fopen(fpoola, "w");
            fprintf(file_pool_attribute, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tX_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");

            /* 全ルールを出力 */
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

                /* 統計情報を出力 */
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

            /* 全ルールを出力 */
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

                /* 統計情報を出力 */
                fprintf(file_pool_numeric,
                        "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"
                        "%d\t%d\t%d\t%d\n",
                        rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                        rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                        rule_pool[i2].support_count, rule_pool[i2].negative_count,
                        rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
            }
            fclose(file_pool_numeric);

            /* 累積カウンタを更新 */
            total_rule_count = total_rule_count + rule_count - 1;
            total_high_support = total_high_support + high_support_rule_count - 1;
            total_low_variance = total_low_variance + low_variance_rule_count - 1;

            printf("Global pool created: %s, %s\n", fpoola, fpoolb);
        }

        if (trial_index > Nstart)
        {
            /* 2回目以降の試行：既存プールに追加（重複チェック処理は省略） */
            printf("  New rules added to global pool.\n");

            total_rule_count = total_rule_count + rule_count - 1;
            total_high_support = total_high_support + high_support_rule_count - 1;
            total_low_variance = total_low_variance + low_variance_rule_count - 1;
        }

        /* 累積統計を表示 */
        printf("  Cumulative total: %d rules (High-support: %d, Low-variance: %d)\n",
               total_rule_count, total_high_support, total_low_variance);

        /* ドキュメントファイルに統計を記録 */
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
       実行結果のサマリーを表示
       ================================================================================ */
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

    return 0;
}