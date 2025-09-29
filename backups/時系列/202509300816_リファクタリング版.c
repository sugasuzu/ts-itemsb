#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   時系列対応版GNMiner Phase 3 - 詳細コメント版

   このプログラムは、Genetic Network Programming (GNP)を用いて
   時系列データから関連ルールを自動的に発見するシステムです。

   Phase 3の特徴：
   - 適応的な時間遅延学習メカニズム
   - 成功パターンの記憶と再利用
   - ルーレット選択による確率的な進化

   ルール形式：
   A1(t-d1) ∧ A2(t-d2) ∧ ... → (X,Y)(t+1)
   各属性Aiは異なる過去時点(t-di)を参照し、未来(t+1)の値を予測します
   ================================================================================
*/

/* ================================================================================
   定数定義セクション
   プログラム全体で使用される定数を定義
   ================================================================================
*/

/* ---------- 時系列パラメータ ----------
   時系列処理の核心となるパラメータ群 */
#define TIMESERIES_MODE 1       /* 時系列モード有効化（1:有効, 0:無効） */
#define MAX_TIME_DELAY 10       /* 理論上の最大時間遅延（将来拡張用） */
#define MAX_TIME_DELAY_PHASE3 3 /* Phase 3で実際に使用する最大遅延（t-0〜t-3） */
#define MIN_TIME_DELAY 0        /* 最小時間遅延（0は現在時刻） */
#define PREDICTION_SPAN 1       /* 予測時点（1は1時刻先を予測） */
#define ADAPTIVE_DELAY 1        /* 適応的遅延学習の有効化（1:有効） */

/* ---------- ディレクトリ構造定義 ----------
   出力ファイルを整理するためのディレクトリパス */
#define OUTPUT_DIR "output"           /* メイン出力ディレクトリ */
#define OUTPUT_DIR_IL "output/IL"     /* Individual List: 個体別ルールリスト */
#define OUTPUT_DIR_IA "output/IA"     /* Individual Analysis: 分析レポート */
#define OUTPUT_DIR_IB "output/IB"     /* Individual Backup: バックアップ */
#define OUTPUT_DIR_POOL "output/pool" /* グローバルルールプール */
#define OUTPUT_DIR_DOC "output/doc"   /* ドキュメント・統計情報 */

/* ---------- ファイル名定義 ----------
   入出力ファイルのパス */
#define DATANAME "testdata1s.txt"             /* 時系列データファイル */
#define DICNAME "testdic1.txt"                /* 属性辞書ファイル */
#define POOL_FILE_A "output/pool/zrp01a.txt"  /* ルールプール（属性名版） */
#define POOL_FILE_B "output/pool/zrp01b.txt"  /* ルールプール（数値版） */
#define CONT_FILE "output/doc/zrd01.txt"      /* 実行統計ファイル */
#define RESULT_FILE "output/doc/zrmemo01.txt" /* 結果メモファイル */

/* ---------- データ構造パラメータ ----------
   データセットのサイズと構造 */
#define Nrd 159 /* 総レコード数（時系列データの長さ） */
#define Nzk 95  /* 属性数（前件部で使用可能な属性の数） */

/* ---------- ルールマイニング制約 ----------
   発見するルールの品質基準 */
#define Nrulemax 2002 /* 最大ルール数（メモリ制約） */
#define Minsup 0.04   /* 最小サポート値（4%以上の頻度が必要） */
#define Maxsigx 0.5   /* X方向の最大標準偏差（分布の狭さ） */
#define Maxsigy 0.5   /* Y方向の最大標準偏差（分布の狭さ） */

/* ---------- 実験パラメータ ----------
   実験の規模と繰り返し設定 */
#define Nstart 1000 /* 開始試行番号（ファイル名に使用） */
#define Ntry 100    /* 試行回数（独立実験の回数） */

/* ---------- GNPパラメータ ----------
   進化計算のパラメータ設定 */
#define Generation 201 /* 世代数（進化の繰り返し回数） */
#define Nkotai 120     /* 個体数（個体群のサイズ） */
#define Npn 10         /* 処理ノード数（ルール探索の開始点数） */
#define Njg 100        /* 判定ノード数（条件判定を行うノード数） */
#define Nmx 7          /* 最大ルール長（前件部の最大属性数+1） */
#define Muratep 1      /* 処理ノード突然変異率（1/Muratep） */
#define Muratej 6      /* 判定ノード接続突然変異率（1/6の確率） */
#define Muratea 6      /* 属性突然変異率（1/6の確率） */
#define Muratedelay 6  /* 時間遅延突然変異率（1/6の確率） */
#define Nkousa 20      /* 交叉点数（一度に交換するノード数） */

/* ================================================================================
   構造体定義セクション
   プログラムで使用する主要なデータ構造
   ================================================================================
*/

/* ---------- ルール構造体 ----------
   発見された時系列ルールを格納する構造体 */
struct asrule
{
    /* 前件部属性（最大8個）
       値が0の場合はその位置に属性がないことを示す */
    int antecedent_attr0;
    int antecedent_attr1;
    int antecedent_attr2;
    int antecedent_attr3;
    int antecedent_attr4;
    int antecedent_attr5;
    int antecedent_attr6;
    int antecedent_attr7;

    /* 各属性の時間遅延（0〜3）
       例: time_delay0=2 → attr0は2時刻前(t-2)を参照 */
    int time_delay0;
    int time_delay1;
    int time_delay2;
    int time_delay3;
    int time_delay4;
    int time_delay5;
    int time_delay6;
    int time_delay7;

    /* 後件部の統計情報（予測対象の分布） */
    double x_mean;  /* X値の平均（予測の中心値） */
    double x_sigma; /* X値の標準偏差（予測の確からしさ） */
    double y_mean;  /* Y値の平均 */
    double y_sigma; /* Y値の標準偏差 */

    /* ルール評価指標 */
    int support_count;     /* サポート数（ルールが成立した回数） */
    int negative_count;    /* 負例数（評価母数） */
    int high_support_flag; /* 高サポートフラグ（1:優良ルール） */
    int low_variance_flag; /* 低分散フラグ（1:精度の高いルール） */
};

/* ---------- 比較用ルール構造体 ----------
   ルールの重複チェック用（前件部のみ） */
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

/* ---------- 試行状態管理構造体 ----------
   各試行の状態を管理するための構造体 */
struct trial_state
{
    int trial_id;                /* 試行ID */
    int rule_count;              /* 現在のルール数 */
    int high_support_rule_count; /* 高サポートルール数 */
    int low_variance_rule_count; /* 低分散ルール数 */
    int generation;              /* 現在の世代番号 */
    char filename_rule[256];     /* ルールファイル名 */
    char filename_report[256];   /* レポートファイル名 */
    char filename_local[256];    /* ローカルバックアップファイル名 */
};

/* ================================================================================
   グローバル変数セクション
   プログラム全体で共有される主要なデータ
   ================================================================================
*/

/* ---------- ルールプール ----------
   発見された全ルールを格納 */
struct asrule rule_pool[Nrulemax];
struct cmrule compare_rules[Nrulemax];

/* ---------- データバッファ ----------
   時系列データを事前読み込みして高速アクセス */
static int data_buffer[Nrd][Nzk]; /* 全レコードの属性値 */
static double x_buffer[Nrd];      /* 全レコードのX値 */
static double y_buffer[Nrd];      /* 全レコードのY値 */

/* ---------- 時間遅延統計（Phase 3の核心） ----------
   適応的遅延学習のための統計情報 */
int delay_usage_history[5][MAX_TIME_DELAY_PHASE3 + 1]; /* 5世代分の使用履歴 */
int delay_usage_count[MAX_TIME_DELAY_PHASE3 + 1];      /* 現世代での使用回数 */
int delay_tracking[MAX_TIME_DELAY_PHASE3 + 1];         /* 累積使用頻度 */
double delay_success_rate[MAX_TIME_DELAY_PHASE3 + 1];  /* 成功率（将来拡張用） */

/* ---------- GNPノード構造 ----------
   個体のネットワーク構造を表現
   [個体番号][ノード番号][0:属性ID, 1:接続先, 2:時間遅延] */
static int node_structure[Nkotai][Npn + Njg][3];

/* ---------- 評価統計配列 ----------
   ルール候補の評価中に使用する統計情報 */
static int match_count[Nkotai][Npn][10];      /* マッチ回数 */
static int negative_count[Nkotai][Npn][10];   /* 負例数 */
static int evaluation_count[Nkotai][Npn][10]; /* 評価回数 */
static int attribute_chain[Nkotai][Npn][10];  /* 属性チェーン */
static int time_delay_chain[Nkotai][Npn][10]; /* 時間遅延チェーン */

/* X,Y値の統計（平均・分散計算用） */
static double x_sum[Nkotai][Npn][10];         /* X値の合計 */
static double x_sigma_array[Nkotai][Npn][10]; /* X値の二乗和 */
static double y_sum[Nkotai][Npn][10];         /* Y値の合計 */
static double y_sigma_array[Nkotai][Npn][10]; /* Y値の二乗和 */

/* ---------- 属性使用統計 ----------
   どの属性がよく使われるかを追跡 */
int attribute_usage_history[5][Nzk]; /* 5世代分の使用履歴 */
int attribute_usage_count[Nzk];      /* 現世代での使用カウント */
int attribute_tracking[Nzk];         /* 累積使用頻度 */

/* ---------- 適応度関連 ---------- */
double fitness_value[Nkotai]; /* 各個体の適応度 */
int fitness_ranking[Nkotai];  /* 適応度順位 */

/* ---------- 遺伝子プール ----------
   進化操作で変更される遺伝情報 */
static int gene_connection[Nkotai][Npn + Njg]; /* 接続遺伝子 */
static int gene_attribute[Nkotai][Npn + Njg];  /* 属性遺伝子 */
static int gene_time_delay[Nkotai][Npn + Njg]; /* 時間遅延遺伝子 */

/* ---------- 属性辞書 ---------- */
char attribute_dictionary[Nzk + 3][31]; /* 属性名の辞書 */

/* ---------- グローバル統計カウンタ ---------- */
int total_rule_count = 0;   /* 全試行での総ルール数 */
int total_high_support = 0; /* 全試行での高サポートルール数 */
int total_low_variance = 0; /* 全試行での低分散ルール数 */

/* ---------- その他の作業用配列 ---------- */
int rules_per_trial[Ntry];        /* 各試行でのルール数 */
int new_rule_marker[Nrulemax];    /* 新規ルールマーカー */
int rules_by_attribute_count[10]; /* 属性数別ルールカウント */
int attribute_set[Nzk + 1];       /* 属性セット */

/* ================================================================================
   1. 初期化・設定系関数
   システムの初期化とディレクトリ構造の作成
   ================================================================================
*/

/**
 * create_output_directories()
 * 出力用のディレクトリ構造を作成する関数
 * 各種ファイルを整理して保存するための階層構造を構築
 */
void create_output_directories()
{
    /* メインディレクトリを作成（既存の場合はエラーにならない） */
    mkdir(OUTPUT_DIR, 0755);

    /* サブディレクトリを作成 */
    mkdir(OUTPUT_DIR_IL, 0755);   /* 個体別ルールリスト用 */
    mkdir(OUTPUT_DIR_IA, 0755);   /* 分析レポート用 */
    mkdir(OUTPUT_DIR_IB, 0755);   /* バックアップ用 */
    mkdir(OUTPUT_DIR_POOL, 0755); /* グローバルプール用 */
    mkdir(OUTPUT_DIR_DOC, 0755);  /* ドキュメント用 */

    /* ディレクトリ構造を表示 */
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

/**
 * load_attribute_dictionary()
 * 属性辞書ファイルを読み込む関数
 * 属性IDと属性名の対応関係を設定
 * 辞書ファイルがない場合は番号で代替
 */
void load_attribute_dictionary()
{
    FILE *file_dictionary;
    int process_id;
    char attribute_name_buffer[31];

    /* 辞書配列を初期化（空文字列で埋める） */
    for (int i = 0; i < Nzk + 3; i++)
    {
        strcpy(attribute_dictionary[i], "\0");
    }

    /* 辞書ファイルを開く */
    file_dictionary = fopen(DICNAME, "r");
    if (file_dictionary == NULL)
    {
        /* ファイルが開けない場合は番号で代替 */
        printf("Warning: Cannot open dictionary file: %s\n", DICNAME);
        printf("Using attribute numbers instead of names.\n");
        for (int i = 0; i < Nzk + 3; i++)
        {
            sprintf(attribute_dictionary[i], "Attr%d", i);
        }
    }
    else
    {
        /* 辞書ファイルから属性名を読み込み */
        for (int i = 0; i < Nzk + 3; i++)
        {
            if (fscanf(file_dictionary, "%d%s", &process_id, attribute_name_buffer) == 2)
            {
                strcpy(attribute_dictionary[i], attribute_name_buffer);
            }
            else
            {
                /* 読み込めない場合はデフォルト名を生成 */
                sprintf(attribute_dictionary[i], "Attr%d", i);
            }
        }
        fclose(file_dictionary);
        printf("Dictionary loaded: %d attributes\n", Nzk);
    }
}

/**
 * initialize_rule_pool()
 * ルールプールを初期化する関数
 * 全ルール構造体と比較用構造体をゼロクリア
 */
void initialize_rule_pool()
{
    for (int i = 0; i < Nrulemax; i++)
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

        /* 比較用構造体も初期化 */
        compare_rules[i].antecedent_attr0 = 0;
        compare_rules[i].antecedent_attr1 = 0;
        compare_rules[i].antecedent_attr2 = 0;
        compare_rules[i].antecedent_attr3 = 0;
        compare_rules[i].antecedent_attr4 = 0;
        compare_rules[i].antecedent_attr5 = 0;
        compare_rules[i].antecedent_attr6 = 0;
        compare_rules[i].antecedent_attr7 = 0;

        /* 新規ルールマーカーを初期化 */
        new_rule_marker[i] = 0;
    }
}

/**
 * initialize_delay_statistics()
 * 時間遅延統計を初期化する関数（Phase 3の核心）
 * 適応的遅延学習のための統計情報を初期設定
 */
void initialize_delay_statistics()
{
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        delay_usage_count[i] = 0;    /* 現世代での使用回数をクリア */
        delay_tracking[i] = 1;       /* 最小値1で初期化（ゼロ除算回避） */
        delay_success_rate[i] = 0.5; /* 中立的な初期成功率 */

        /* 過去5世代分の履歴を初期化 */
        for (int j = 0; j < 5; j++)
        {
            delay_usage_history[j][i] = 1; /* 全遅延値に等しいチャンスを与える */
        }
    }
}

/**
 * initialize_attribute_statistics()
 * 属性使用統計を初期化する関数
 * どの属性が有効かを学習するための統計情報を初期設定
 */
void initialize_attribute_statistics()
{
    /* 各属性の使用カウントを初期化 */
    for (int i = 0; i < Nzk; i++)
    {
        attribute_usage_count[i] = 0;
        attribute_tracking[i] = 0;

        /* 過去5世代分の履歴を初期化 */
        attribute_usage_history[0][i] = 1; /* 現世代は最小値1 */
        attribute_usage_history[1][i] = 0;
        attribute_usage_history[2][i] = 0;
        attribute_usage_history[3][i] = 0;
        attribute_usage_history[4][i] = 0;
    }

    /* 属性数別ルールカウントを初期化 */
    for (int i = 0; i < 10; i++)
    {
        rules_by_attribute_count[i] = 0;
    }

    /* 属性セットを初期化（0〜Nzk） */
    for (int i = 0; i < Nzk + 1; i++)
    {
        attribute_set[i] = i;
    }
}

/**
 * initialize_global_counters()
 * グローバルカウンタを初期化する関数
 * 全試行を通しての累積統計をリセット
 */
void initialize_global_counters()
{
    total_rule_count = 0;
    total_high_support = 0;
    total_low_variance = 0;
}

/* ================================================================================
   2. データ管理系関数
   時系列データの読み込みと管理
   ================================================================================
*/

/**
 * load_timeseries_data()
 * 時系列データをバッファに読み込む関数
 * 全データをメモリに展開して高速アクセスを実現
 */
void load_timeseries_data()
{
    FILE *file_data;
    int input_value;
    double input_x, input_y;

    printf("Loading data into buffer for time-series processing...\n");

    /* データファイルを開く */
    file_data = fopen(DATANAME, "r");
    if (file_data == NULL)
    {
        printf("Error: Cannot open data file: %s\n", DATANAME);
        exit(1);
    }

    /* 全レコードを読み込んでバッファに格納 */
    for (int i = 0; i < Nrd; i++)
    {
        /* 各属性値を読み込み（0または1） */
        for (int j = 0; j < Nzk; j++)
        {
            fscanf(file_data, "%d", &input_value);
            data_buffer[i][j] = input_value;
        }

        /* X,Y値（連続値）を読み込み */
        fscanf(file_data, "%lf%lf", &input_x, &input_y);
        x_buffer[i] = input_x;
        y_buffer[i] = input_y;
    }
    fclose(file_data);
    printf("Data loaded: %d records with %d attributes each\n", Nrd, Nzk);
}

/**
 * get_safe_data_range()
 * 安全なデータ範囲を取得する関数
 * 時系列処理で過去・未来参照時の範囲外アクセスを防ぐ
 *
 * @param start_index: 開始インデックス（出力）
 * @param end_index: 終了インデックス（出力）
 */
void get_safe_data_range(int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        /* 時系列モード：過去参照と未来参照のマージンを確保 */
        *start_index = MAX_TIME_DELAY_PHASE3; /* 最大遅延分のマージン */
        *end_index = Nrd - PREDICTION_SPAN;   /* 予測時点分のマージン */
    }
    else
    {
        /* 通常モード：全データを使用 */
        *start_index = 0;
        *end_index = Nrd;
    }
}

/**
 * get_past_attribute_value()
 * 過去の属性値を取得する関数
 * 指定された時間遅延を考慮して過去のデータを参照
 *
 * @param current_time: 現在時刻
 * @param time_delay: 時間遅延（0〜3）
 * @param attribute_id: 属性ID
 * @return: 属性値（0または1、範囲外の場合は-1）
 */
int get_past_attribute_value(int current_time, int time_delay, int attribute_id)
{
    /* 過去の時刻を計算 */
    int data_index = current_time - time_delay;

    /* 範囲チェック */
    if (data_index < 0 || data_index >= Nrd)
    {
        return -1; /* 範囲外エラー */
    }

    return data_buffer[data_index][attribute_id];
}

/**
 * get_future_target_values()
 * 未来の目標値を取得する関数
 * 予測対象となる未来時点のX,Y値を取得
 *
 * @param current_time: 現在時刻
 * @param x_value: X値（出力）
 * @param y_value: Y値（出力）
 */
void get_future_target_values(int current_time, double *x_value, double *y_value)
{
    /* 未来の時刻を計算（PREDICTION_SPAN分先） */
    int future_index = current_time + PREDICTION_SPAN;

    /* 範囲チェック */
    if (future_index >= Nrd)
    {
        *x_value = 0.0;
        *y_value = 0.0;
        return;
    }

    /* 未来時点の値を返す */
    *x_value = x_buffer[future_index];
    *y_value = y_buffer[future_index];
}

/* ================================================================================
   3. GNP個体管理系関数
   進化計算の個体群を管理
   ================================================================================
*/

/**
 * create_initial_population()
 * 初期個体群を生成する関数
 * ランダムな遺伝子を持つ個体群を作成
 */
void create_initial_population()
{
    for (int i = 0; i < Nkotai; i++)
    {
        for (int j = 0; j < (Npn + Njg); j++)
        {
            /* ランダムな接続先（判定ノードの範囲内） */
            gene_connection[i][j] = rand() % Njg + Npn;

            /* ランダムな属性（0〜Nzk-1） */
            gene_attribute[i][j] = rand() % Nzk;

            /* ランダムな時間遅延（0〜3） */
            gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
        }
    }
}

/**
 * copy_genes_to_nodes()
 * 遺伝子情報をノード構造にコピーする関数
 * 進化操作後の遺伝子を実際の評価用構造に変換
 */
void copy_genes_to_nodes()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        for (int node = 0; node < (Npn + Njg); node++)
        {
            /* 属性、接続、時間遅延をコピー */
            node_structure[individual][node][0] = gene_attribute[individual][node];
            node_structure[individual][node][1] = gene_connection[individual][node];
            node_structure[individual][node][2] = gene_time_delay[individual][node];
        }
    }
}

/**
 * initialize_individual_statistics()
 * 個体統計を初期化する関数
 * 各個体の評価前に統計情報をクリア
 */
void initialize_individual_statistics()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        /* 適応度を初期化（個体番号による微小な差をつける） */
        fitness_value[individual] = (double)individual * (-0.00001);
        fitness_ranking[individual] = 0;

        /* 各処理ノードの統計を初期化 */
        for (int k = 0; k < Npn; k++)
        {
            for (int i = 0; i < 10; i++)
            {
                match_count[individual][k][i] = 0;      /* マッチ回数 */
                attribute_chain[individual][k][i] = 0;  /* 属性チェーン */
                time_delay_chain[individual][k][i] = 0; /* 時間遅延チェーン */
                negative_count[individual][k][i] = 0;   /* 負例数 */
                evaluation_count[individual][k][i] = 0; /* 評価回数 */
                x_sum[individual][k][i] = 0;            /* X値の合計 */
                x_sigma_array[individual][k][i] = 0;    /* X値の二乗和 */
                y_sum[individual][k][i] = 0;            /* Y値の合計 */
                y_sigma_array[individual][k][i] = 0;    /* Y値の二乗和 */
            }
        }
    }
}

/* ================================================================================
   4. ルール評価系関数
   GNPネットワークによるルール候補の評価
   ================================================================================
*/

/**
 * evaluate_single_instance()
 * 単一時刻のデータを評価する関数
 * 各個体のネットワークに従ってルール候補を評価
 *
 * @param time_index: 評価する時刻
 */
void evaluate_single_instance(int time_index)
{
    double future_x, future_y;
    int current_node_id, depth, match_flag;
    int time_delay, data_index;

    /* この時刻に対する未来の目標値を取得 */
    get_future_target_values(time_index, &future_x, &future_y);

    /* 全個体に対して評価を実行 */
    for (int individual = 0; individual < Nkotai; individual++)
    {
        /* 各処理ノードから探索開始 */
        for (int k = 0; k < Npn; k++)
        {
            current_node_id = k; /* 処理ノードから開始 */
            depth = 0;           /* 深さ0から開始 */
            match_flag = 1;      /* マッチフラグを立てる */

            /* ルートノード（深さ0）の統計更新 */
            match_count[individual][k][depth]++;
            evaluation_count[individual][k][depth]++;

            /* 最初の判定ノードへ遷移 */
            current_node_id = node_structure[individual][current_node_id][1];

            /* 判定ノードチェーンを辿る */
            while (current_node_id > (Npn - 1) && depth < Nmx)
            {
                depth++; /* 深さを増やす */

                /* 現在のノードの属性を記録（+1は表示用） */
                attribute_chain[individual][k][depth] =
                    node_structure[individual][current_node_id][0] + 1;

                /* 時間遅延を取得して記録 */
                time_delay = node_structure[individual][current_node_id][2];
                time_delay_chain[individual][k][depth] = time_delay;

                /* 時間遅延を考慮したデータインデックスを計算 */
                data_index = time_index - time_delay;

                /* 過去参照の安全性チェック */
                if (data_index < 0)
                {
                    current_node_id = k; /* 処理ノードに戻る */
                    break;
                }

                /* 過去の属性値を取得 */
                int attr_value = data_buffer[data_index][node_structure[individual][current_node_id][0]];

                if (attr_value == 1)
                {
                    /* 条件成立（Yes側）*/
                    if (match_flag == 1)
                    {
                        /* まだマッチ中の場合、統計を更新 */
                        match_count[individual][k][depth]++;

                        /* 未来時点のX,Y値を累積（予測対象） */
                        x_sum[individual][k][depth] += future_x;
                        x_sigma_array[individual][k][depth] += future_x * future_x;
                        y_sum[individual][k][depth] += future_y;
                        y_sigma_array[individual][k][depth] += future_y * future_y;
                    }
                    evaluation_count[individual][k][depth]++;

                    /* 次の判定ノードへ進む */
                    current_node_id = node_structure[individual][current_node_id][1];
                }
                else if (attr_value == 0)
                {
                    /* 条件不成立（No側）：処理ノードに戻る */
                    current_node_id = k;
                }
                else
                {
                    /* 欠損値の場合 */
                    evaluation_count[individual][k][depth]++;
                    match_flag = 0; /* マッチフラグを下ろす */

                    /* 一応次のノードへ進む */
                    current_node_id = node_structure[individual][current_node_id][1];
                }
            }
        }
    }
}

/**
 * evaluate_all_individuals()
 * 全データに対して個体群を評価する関数
 * 安全な範囲内の全時刻でevaluate_single_instanceを呼び出す
 */
void evaluate_all_individuals()
{
    int safe_start, safe_end;

    /* 安全なデータ範囲を取得 */
    get_safe_data_range(&safe_start, &safe_end);

    /* 全時刻で評価を実行 */
    for (int i = safe_start; i < safe_end; i++)
    {
        evaluate_single_instance(i);
    }
}

/**
 * calculate_negative_counts()
 * 負例数を計算する関数
 * 負例数 = 全体数 - 評価数 + マッチ数
 */
void calculate_negative_counts()
{
    for (int individual = 0; individual < Nkotai; individual++)
    {
        for (int k = 0; k < Npn; k++)
        {
            for (int i = 0; i < Nmx; i++)
            {
                /* 負例数の計算式 */
                negative_count[individual][k][i] =
                    match_count[individual][k][0] -      /* ルートのマッチ数 */
                    evaluation_count[individual][k][i] + /* この深さの評価数を引く */
                    match_count[individual][k][i];       /* この深さのマッチ数を足す */
            }
        }
    }
}

/**
 * calculate_rule_statistics()
 * ルール統計（平均・標準偏差）を計算する関数
 * 累積した値から統計量を算出
 */
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
                    /* X方向の平均を計算 */
                    x_sum[individual][k][j] /= (double)match_count[individual][k][j];

                    /* X方向の分散を計算（E[X^2] - E[X]^2） */
                    x_sigma_array[individual][k][j] =
                        x_sigma_array[individual][k][j] / (double)match_count[individual][k][j] -
                        x_sum[individual][k][j] * x_sum[individual][k][j];

                    /* Y方向の平均を計算 */
                    y_sum[individual][k][j] /= (double)match_count[individual][k][j];

                    /* Y方向の分散を計算（E[Y^2] - E[Y]^2） */
                    y_sigma_array[individual][k][j] =
                        y_sigma_array[individual][k][j] / (double)match_count[individual][k][j] -
                        y_sum[individual][k][j] * y_sum[individual][k][j];

                    /* 分散が負になった場合の補正（数値誤差対策） */
                    if (x_sigma_array[individual][k][j] < 0)
                    {
                        x_sigma_array[individual][k][j] = 0;
                    }
                    if (y_sigma_array[individual][k][j] < 0)
                    {
                        y_sigma_array[individual][k][j] = 0;
                    }

                    /* 標準偏差を計算（分散の平方根） */
                    x_sigma_array[individual][k][j] = sqrt(x_sigma_array[individual][k][j]);
                    y_sigma_array[individual][k][j] = sqrt(y_sigma_array[individual][k][j]);
                }
            }
        }
    }
}

/**
 * calculate_support_value()
 * サポート値を計算する関数
 * サポート値 = マッチ数 / 負例数
 *
 * @param matched_count: マッチ数
 * @param negative_count: 負例数
 * @return: サポート値（0.0〜1.0）
 */
double calculate_support_value(int matched_count, int negative_count)
{
    if (negative_count == 0)
    {
        return 0.0; /* ゼロ除算を回避 */
    }
    return (double)matched_count / (double)negative_count;
}

/* ================================================================================
   5. ルール抽出系関数
   評価結果から興味深いルールを抽出
   ================================================================================
*/

/**
 * check_rule_quality()
 * ルール品質をチェックする関数
 * 標準偏差とサポート値の閾値判定
 *
 * @param sigma_x: X方向の標準偏差
 * @param sigma_y: Y方向の標準偏差
 * @param support: サポート値
 * @return: 1:品質基準を満たす, 0:満たさない
 */
int check_rule_quality(double sigma_x, double sigma_y, double support)
{
    /* 標準偏差が閾値以下（分布が狭い）かつ
       サポート値が閾値以上（頻度が高い）なら品質良好 */
    return (sigma_x <= Maxsigx && sigma_y <= Maxsigy && support >= Minsup);
}

/**
 * check_rule_duplication()
 * ルールの重複をチェックする関数
 * 既存のルールプールに同じ前件部が存在するか確認
 *
 * @param rule_candidate: チェック対象のルール候補
 * @param rule_count: 現在のルール数
 * @return: 1:重複あり, 0:新規ルール
 */
int check_rule_duplication(int *rule_candidate, int rule_count)
{
    for (int i = 0; i < rule_count; i++)
    {
        /* 全8属性が一致するか確認 */
        if (rule_pool[i].antecedent_attr0 == rule_candidate[0] &&
            rule_pool[i].antecedent_attr1 == rule_candidate[1] &&
            rule_pool[i].antecedent_attr2 == rule_candidate[2] &&
            rule_pool[i].antecedent_attr3 == rule_candidate[3] &&
            rule_pool[i].antecedent_attr4 == rule_candidate[4] &&
            rule_pool[i].antecedent_attr5 == rule_candidate[5] &&
            rule_pool[i].antecedent_attr6 == rule_candidate[6] &&
            rule_pool[i].antecedent_attr7 == rule_candidate[7])
        {
            return 1; /* 重複あり */
        }
    }
    return 0; /* 新規ルール */
}

/**
 * register_new_rule()
 * 新規ルールを登録する関数
 * ルールプールに新しいルールを追加し、品質フラグを設定
 *
 * @param state: 試行状態
 * @param rule_candidate: ルール候補（前件部属性）
 * @param time_delays: 各属性の時間遅延
 * @param x_mean, x_sigma, y_mean, y_sigma: 統計情報
 * @param support_count, negative_count: 評価指標
 * @param support_value: サポート値
 */
void register_new_rule(struct trial_state *state, int *rule_candidate, int *time_delays,
                       double x_mean, double x_sigma, double y_mean, double y_sigma,
                       int support_count, int negative_count, double support_value)
{
    int idx = state->rule_count;

    /* 前件部属性を保存 */
    rule_pool[idx].antecedent_attr0 = rule_candidate[0];
    rule_pool[idx].antecedent_attr1 = rule_candidate[1];
    rule_pool[idx].antecedent_attr2 = rule_candidate[2];
    rule_pool[idx].antecedent_attr3 = rule_candidate[3];
    rule_pool[idx].antecedent_attr4 = rule_candidate[4];
    rule_pool[idx].antecedent_attr5 = rule_candidate[5];
    rule_pool[idx].antecedent_attr6 = rule_candidate[6];
    rule_pool[idx].antecedent_attr7 = rule_candidate[7];

    /* 時間遅延を保存（属性がある場合のみ） */
    rule_pool[idx].time_delay0 = (rule_candidate[0] > 0) ? time_delays[0] : 0;
    rule_pool[idx].time_delay1 = (rule_candidate[1] > 0) ? time_delays[1] : 0;
    rule_pool[idx].time_delay2 = (rule_candidate[2] > 0) ? time_delays[2] : 0;
    rule_pool[idx].time_delay3 = (rule_candidate[3] > 0) ? time_delays[3] : 0;
    rule_pool[idx].time_delay4 = (rule_candidate[4] > 0) ? time_delays[4] : 0;
    rule_pool[idx].time_delay5 = (rule_candidate[5] > 0) ? time_delays[5] : 0;
    rule_pool[idx].time_delay6 = (rule_candidate[6] > 0) ? time_delays[6] : 0;
    rule_pool[idx].time_delay7 = (rule_candidate[7] > 0) ? time_delays[7] : 0;

    /* 統計情報を保存 */
    rule_pool[idx].x_mean = x_mean;
    rule_pool[idx].x_sigma = x_sigma;
    rule_pool[idx].y_mean = y_mean;
    rule_pool[idx].y_sigma = y_sigma;
    rule_pool[idx].support_count = support_count;
    rule_pool[idx].negative_count = negative_count;

    /* 高サポートフラグの設定 */
    int high_support_marker = 0;
    if (support_value >= (Minsup + 0.02))
    { /* 閾値より2%高い */
        high_support_marker = 1;
    }
    rule_pool[idx].high_support_flag = high_support_marker;

    if (rule_pool[idx].high_support_flag == 1)
    {
        state->high_support_rule_count++;
    }

    /* 低分散フラグの設定 */
    int low_variance_marker = 0;
    if (x_sigma <= (Maxsigx - 0.1) && y_sigma <= (Maxsigy - 0.1))
    { /* 閾値より0.1低い */
        low_variance_marker = 1;
    }
    rule_pool[idx].low_variance_flag = low_variance_marker;

    if (rule_pool[idx].low_variance_flag == 1)
    {
        state->low_variance_rule_count++;
    }

    /* ルールカウントを増加 */
    state->rule_count++;
}

/**
 * update_delay_learning_from_rule()
 * ルールから時間遅延パターンを学習する関数（Phase 3の核心）
 * 成功したルールの遅延パターンを統計に反映
 *
 * @param time_delays: 時間遅延配列
 * @param num_attributes: 属性数
 * @param high_support: 高サポートフラグ
 * @param low_variance: 低分散フラグ
 */
void update_delay_learning_from_rule(int *time_delays, int num_attributes,
                                     int high_support, int low_variance)
{
    for (int k = 0; k < num_attributes; k++)
    {
        if (time_delays[k] >= 0 && time_delays[k] <= MAX_TIME_DELAY_PHASE3)
        {
            /* この遅延値の使用頻度を増加 */
            delay_usage_history[0][time_delays[k]] += 1;

            /* 高品質ルールの場合は追加ボーナス */
            if (high_support || low_variance)
            {
                delay_usage_history[0][time_delays[k]] += 2; /* 良いパターンを強化 */
            }
        }
    }
}

/**
 * extract_rules_from_individual()
 * 個体からルールを抽出する関数
 * 一つの個体の全処理ノードチェーンを調べてルールを抽出
 *
 * @param state: 試行状態
 * @param individual: 個体番号
 */
void extract_rules_from_individual(struct trial_state *state, int individual)
{
    int rule_candidate_pre[10];   /* ルール候補（前処理用） */
    int rule_candidate[10];       /* ルール候補（整理後） */
    int rule_memo[10];            /* ルールメモ */
    int time_delay_candidate[10]; /* 時間遅延候補 */
    int time_delay_memo[10];      /* 時間遅延メモ */

    /* 各処理ノードチェーンを調査 */
    for (int k = 0; k < Npn; k++)
    {
        /* ルール数上限チェック */
        if (state->rule_count > (Nrulemax - 2))
        {
            break;
        }

        /* 候補配列を初期化 */
        for (int i = 0; i < 10; i++)
        {
            rule_candidate_pre[i] = 0;
            rule_candidate[i] = 0;
            rule_memo[i] = 0;
            time_delay_candidate[i] = 0;
            time_delay_memo[i] = 0;
        }

        /* 各深さでルール候補をチェック */
        for (int loop_j = 1; loop_j < 9; loop_j++)
        {
            /* 統計値を取得 */
            double sigma_x = x_sigma_array[individual][k][loop_j];
            double sigma_y = y_sigma_array[individual][k][loop_j];
            int matched_count = match_count[individual][k][loop_j];
            double support = calculate_support_value(matched_count,
                                                     negative_count[individual][k][loop_j]);

            /* ルール品質をチェック */
            if (check_rule_quality(sigma_x, sigma_y, support))
            {
                /* 前件部と時間遅延を構築 */
                for (int i2 = 1; i2 < 9; i2++)
                {
                    rule_candidate_pre[i2 - 1] = attribute_chain[individual][k][i2];
                    time_delay_candidate[i2 - 1] = time_delay_chain[individual][k][i2];
                }

                /* 重複する属性を除去して整理 */
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

                /* 属性数が上限未満の場合のみ処理 */
                if (j2 < 9)
                {
                    /* 重複チェック */
                    if (!check_rule_duplication(rule_candidate, state->rule_count))
                    {
                        /* 新規ルールを登録 */
                        register_new_rule(state, rule_candidate, time_delay_memo,
                                          x_sum[individual][k][loop_j], sigma_x,
                                          y_sum[individual][k][loop_j], sigma_y,
                                          matched_count, negative_count[individual][k][loop_j],
                                          support);

                        /* 属性数別カウントを更新 */
                        rules_by_attribute_count[j2]++;

                        /* 適応度を大幅に増加（新規ルール発見ボーナス） */
                        fitness_value[individual] += j2 + support * 10 +
                                                     2 / (sigma_x + 0.1) +
                                                     2 / (sigma_y + 0.1) + 20;

                        /* 属性使用頻度を更新 */
                        for (int k3 = 0; k3 < j2; k3++)
                        {
                            int i5 = rule_memo[k3] - 1;
                            attribute_usage_history[0][i5]++;
                        }

                        /* 遅延パターンを学習（Phase 3の重要部分） */
                        update_delay_learning_from_rule(time_delay_memo, j2,
                                                        rule_pool[state->rule_count - 1].high_support_flag,
                                                        rule_pool[state->rule_count - 1].low_variance_flag);
                    }
                    else
                    {
                        /* 既存ルールでも適応度は増加（重複でも価値はある） */
                        fitness_value[individual] += j2 + support * 10 +
                                                     2 / (sigma_x + 0.1) +
                                                     2 / (sigma_y + 0.1);
                    }

                    /* ルール数上限チェック */
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
 * extract_rules_from_population()
 * 全個体からルールを抽出する関数
 * 個体群全体を調査してルールプールを構築
 *
 * @param state: 試行状態
 */
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
   6. 進化計算系関数
   遺伝的アルゴリズムによる最適化
   ================================================================================
*/

/**
 * calculate_fitness_rankings()
 * 適応度順位を計算する関数
 * 各個体の適応度から順位付けを行う
 */
void calculate_fitness_rankings()
{
    for (int i = 0; i < Nkotai; i++)
    {
        fitness_ranking[i] = 0;
        /* 自分より適応度が高い個体の数を数える */
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
 * 選択処理を実行する関数
 * 上位1/3の個体を3倍に複製して次世代を作成
 */
void perform_selection()
{
    for (int i = 0; i < Nkotai; i++)
    {
        if (fitness_ranking[i] < 40)
        { /* 上位40個体（120の1/3） */
            for (int j = 0; j < (Npn + Njg); j++)
            {
                /* 元の位置にコピー（0-39） */
                gene_attribute[fitness_ranking[i]][j] = node_structure[i][j][0];
                gene_connection[fitness_ranking[i]][j] = node_structure[i][j][1];
                gene_time_delay[fitness_ranking[i]][j] = node_structure[i][j][2];

                /* 複製1（40-79） */
                gene_attribute[fitness_ranking[i] + 40][j] = node_structure[i][j][0];
                gene_connection[fitness_ranking[i] + 40][j] = node_structure[i][j][1];
                gene_time_delay[fitness_ranking[i] + 40][j] = node_structure[i][j][2];

                /* 複製2（80-119） */
                gene_attribute[fitness_ranking[i] + 80][j] = node_structure[i][j][0];
                gene_connection[fitness_ranking[i] + 80][j] = node_structure[i][j][1];
                gene_time_delay[fitness_ranking[i] + 80][j] = node_structure[i][j][2];
            }
        }
    }
}

/**
 * perform_crossover()
 * 交叉処理を実行する関数
 * 上位個体間で遺伝子を交換して多様性を生成
 */
void perform_crossover()
{
    for (int i = 0; i < 20; i++)
    {
        for (int j = 0; j < Nkousa; j++)
        {
            /* ランダムに交叉点を選択 */
            int crossover_point = rand() % Njg + Npn;
            int temp;

            /* 接続遺伝子を交換 */
            temp = gene_connection[i + 20][crossover_point];
            gene_connection[i + 20][crossover_point] = gene_connection[i][crossover_point];
            gene_connection[i][crossover_point] = temp;

            /* 属性遺伝子を交換 */
            temp = gene_attribute[i + 20][crossover_point];
            gene_attribute[i + 20][crossover_point] = gene_attribute[i][crossover_point];
            gene_attribute[i][crossover_point] = temp;

            /* 時間遅延遺伝子も交換 */
            temp = gene_time_delay[i + 20][crossover_point];
            gene_time_delay[i + 20][crossover_point] = gene_time_delay[i][crossover_point];
            gene_time_delay[i][crossover_point] = temp;
        }
    }
}

/**
 * perform_mutation_processing_nodes()
 * 処理ノードの突然変異を実行する関数
 * 処理ノードの接続先をランダムに変更
 */
void perform_mutation_processing_nodes()
{
    for (int i = 0; i < 120; i++)
    {
        for (int j = 0; j < Npn; j++)
        {
            if (rand() % Muratep == 0)
            { /* 1/Muratepの確率で突然変異 */
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

/**
 * perform_mutation_judgment_nodes()
 * 判定ノードの突然変異を実行する関数
 * 判定ノードの接続先をランダムに変更
 */
void perform_mutation_judgment_nodes()
{
    for (int i = 40; i < 80; i++)
    { /* 個体40-79のみ対象 */
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratej == 0)
            { /* 1/6の確率で突然変異 */
                gene_connection[i][j] = rand() % Njg + Npn;
            }
        }
    }
}

/**
 * roulette_wheel_selection_delay()
 * 遅延値のルーレット選択を行う関数（Phase 3の核心）
 * 使用頻度に基づいて確率的に遅延値を選択
 *
 * @param total_usage: 総使用頻度
 * @return: 選択された遅延値（0〜3）
 */
int roulette_wheel_selection_delay(int total_usage)
{
    if (total_usage <= 0)
    {
        /* 使用頻度がない場合はランダム選択 */
        return rand() % (MAX_TIME_DELAY_PHASE3 + 1);
    }

    /* ルーレット選択：使用頻度が高いほど選ばれやすい */
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

/**
 * roulette_wheel_selection_attribute()
 * 属性のルーレット選択を行う関数
 * 使用頻度に基づいて確率的に属性を選択
 *
 * @param total_usage: 総使用頻度
 * @return: 選択された属性ID
 */
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
 * 適応的時間遅延突然変異を実行する関数（Phase 3の核心）
 * 成功パターンに基づいて遅延値を変更
 */
void perform_adaptive_delay_mutation()
{
    /* 総遅延使用頻度を計算 */
    int total_delay_usage = 0;
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        total_delay_usage += delay_tracking[i];
    }

    /* 個体40-79に対して適応的突然変異 */
    for (int i = 40; i < 80; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratedelay == 0)
            { /* 1/6の確率で突然変異 */
                if (ADAPTIVE_DELAY && total_delay_usage > 0)
                {
                    /* 適応的選択：成功パターンを優先 */
                    gene_time_delay[i][j] = roulette_wheel_selection_delay(total_delay_usage);
                }
                else
                {
                    /* 通常のランダム選択 */
                    gene_time_delay[i][j] = rand() % (MAX_TIME_DELAY_PHASE3 + 1);
                }
            }
        }
    }

    /* 個体80-119に対しても同様の処理 */
    for (int i = 80; i < 120; i++)
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
 * 適応的属性突然変異を実行する関数
 * よく使われる属性を優先的に選択
 */
void perform_adaptive_attribute_mutation()
{
    /* 総属性使用頻度を計算 */
    int total_attribute_usage = 0;
    for (int i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    /* 個体80-119に対して適応的突然変異 */
    for (int i = 80; i < 120; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            if (rand() % Muratea == 0)
            { /* 1/6の確率で突然変異 */
                /* よく使われる属性を優先的に選択 */
                gene_attribute[i][j] = roulette_wheel_selection_attribute(total_attribute_usage);
            }
        }
    }
}

/* ================================================================================
   7. 統計・履歴管理系関数
   適応的学習のための統計管理
   ================================================================================
*/

/**
 * update_delay_statistics()
 * 遅延統計を更新する関数（Phase 3の核心）
 * 世代ごとの遅延使用パターンを記録・集計
 *
 * @param generation: 現在の世代番号
 */
void update_delay_statistics(int generation)
{
    int total_delay_usage = 0;

    /* 過去5世代分の履歴を合計 */
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        delay_tracking[i] = 0;

        for (int j = 0; j < 5; j++)
        {
            delay_tracking[i] += delay_usage_history[j][i];
        }

        total_delay_usage += delay_tracking[i];
    }

    /* 履歴配列をシフト（古い情報を削除） */
    for (int i = 0; i <= MAX_TIME_DELAY_PHASE3; i++)
    {
        /* 古い世代を後ろにシフト */
        for (int j = 4; j > 0; j--)
        {
            delay_usage_history[j][i] = delay_usage_history[j - 1][i];
        }

        /* 現世代をリセット */
        delay_usage_history[0][i] = 1;

        /* 5世代ごとにリフレッシュ（探索の多様性維持） */
        if (generation % 5 == 0)
        {
            delay_usage_history[0][i] = 2;
        }
    }
}

/**
 * update_attribute_statistics()
 * 属性統計を更新する関数
 * 世代ごとの属性使用パターンを記録・集計
 *
 * @param generation: 現在の世代番号
 */
void update_attribute_statistics(int generation)
{
    int total_attribute_usage = 0;

    /* 属性使用カウントをリセット */
    for (int i = 0; i < Nzk; i++)
    {
        attribute_usage_count[i] = 0;
        attribute_tracking[i] = 0;
    }

    /* 過去5世代分の履歴を合計 */
    for (int i = 0; i < Nzk; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            attribute_tracking[i] += attribute_usage_history[j][i];
        }
    }

    /* 履歴配列をシフト */
    for (int i = 0; i < Nzk; i++)
    {
        for (int j = 4; j > 0; j--)
        {
            attribute_usage_history[j][i] = attribute_usage_history[j - 1][i];
        }

        attribute_usage_history[0][i] = 0;

        /* 5世代ごとにリフレッシュ */
        if (generation % 5 == 0)
        {
            attribute_usage_history[0][i] = 1;
        }
    }

    /* 総使用頻度を計算 */
    for (int i = 0; i < Nzk; i++)
    {
        total_attribute_usage += attribute_tracking[i];
    }

    /* 現世代の属性使用をカウント */
    for (int i = 0; i < Nkotai; i++)
    {
        for (int j = Npn; j < (Npn + Njg); j++)
        {
            attribute_usage_count[gene_attribute[i][j]]++;
        }
    }
}

/* ================================================================================
   8. ファイル入出力系関数
   結果の記録と出力
   ================================================================================
*/

/**
 * create_trial_files()
 * 試行別ファイルを作成する関数
 * 各試行の開始時に必要なファイルを初期化
 *
 * @param state: 試行状態
 */
void create_trial_files(struct trial_state *state)
{
    FILE *file;

    /* ルールファイルを作成 */
    file = fopen(state->filename_rule, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\n");
        fclose(file);
    }

    /* レポートファイルを作成 */
    file = fopen(state->filename_report, "w");
    if (file != NULL)
    {
        fprintf(file, "Generation\tRules\tHighSup\tLowVar\tAvgFitness\n");
        fclose(file);
    }

    /* ローカルファイルを作成 */
    file = fopen(state->filename_local, "w");
    if (file != NULL)
    {
        fprintf(file, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\tHighSup\tLowVar\n");
        fclose(file);
    }
}

/**
 * write_rule_to_file()
 * ルールをファイルに書き込む関数
 * 時間遅延付きでルールを出力
 *
 * @param state: 試行状態
 * @param rule_index: ルールインデックス
 */
void write_rule_to_file(struct trial_state *state, int rule_index)
{
    FILE *file = fopen(state->filename_rule, "a");
    if (file == NULL)
        return;

    /* 各属性とその時間遅延を出力 */
    for (int i = 0; i < 8; i++)
    {
        int attr = 0;
        int delay = 0;

        /* 属性と遅延を取得 */
        switch (i)
        {
        case 0:
            attr = rule_pool[rule_index].antecedent_attr0;
            delay = rule_pool[rule_index].time_delay0;
            break;
        case 1:
            attr = rule_pool[rule_index].antecedent_attr1;
            delay = rule_pool[rule_index].time_delay1;
            break;
        case 2:
            attr = rule_pool[rule_index].antecedent_attr2;
            delay = rule_pool[rule_index].time_delay2;
            break;
        case 3:
            attr = rule_pool[rule_index].antecedent_attr3;
            delay = rule_pool[rule_index].time_delay3;
            break;
        case 4:
            attr = rule_pool[rule_index].antecedent_attr4;
            delay = rule_pool[rule_index].time_delay4;
            break;
        case 5:
            attr = rule_pool[rule_index].antecedent_attr5;
            delay = rule_pool[rule_index].time_delay5;
            break;
        case 6:
            attr = rule_pool[rule_index].antecedent_attr6;
            delay = rule_pool[rule_index].time_delay6;
            break;
        case 7:
            attr = rule_pool[rule_index].antecedent_attr7;
            delay = rule_pool[rule_index].time_delay7;
            break;
        }

        /* 属性がある場合は「属性(t-遅延)」形式で出力 */
        if (attr > 0)
        {
            fprintf(file, "%d(t-%d)", attr, delay);
        }
        else
        {
            fprintf(file, "0");
        }

        if (i < 7)
            fprintf(file, "\t");
    }

    fprintf(file, "\n");
    fclose(file);
}

/**
 * write_progress_report()
 * 進捗レポートを出力する関数
 * 定期的に世代の統計情報を記録
 *
 * @param state: 試行状態
 * @param generation: 世代番号
 */
void write_progress_report(struct trial_state *state, int generation)
{
    FILE *file = fopen(state->filename_report, "a");
    if (file == NULL)
        return;

    /* 平均適応度を計算 */
    double fitness_average = 0;
    for (int i = 0; i < Nkotai; i++)
    {
        fitness_average += fitness_value[i];
    }
    fitness_average /= Nkotai;

    /* 統計情報をファイルに記録 */
    fprintf(file, "%5d\t%5d\t%5d\t%5d\t%9.3f\n",
            generation,
            state->rule_count - 1,
            state->high_support_rule_count - 1,
            state->low_variance_rule_count - 1,
            fitness_average);

    fclose(file);

    /* コンソールに進捗表示 */
    printf("  Gen.=%5d: %5d rules (%5d high-sup, %5d low-var)\n",
           generation, state->rule_count - 1,
           state->high_support_rule_count - 1,
           state->low_variance_rule_count - 1);

    /* 遅延統計を定期的に表示（デバッグ用） */
    if (generation % 20 == 0 && ADAPTIVE_DELAY)
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
 * ローカル出力を書き込む関数
 * 試行のバックアップファイルに全ルールを保存
 *
 * @param state: 試行状態
 */
void write_local_output(struct trial_state *state)
{
    FILE *file = fopen(state->filename_local, "a");
    if (file == NULL)
        return;

    /* 全ルールを出力 */
    for (int i = 1; i < state->rule_count; i++)
    {
        /* 各属性とその時間遅延を出力 */
        for (int j = 0; j < 8; j++)
        {
            int attr = 0;
            int delay = 0;

            switch (j)
            {
            case 0:
                attr = rule_pool[i].antecedent_attr0;
                delay = rule_pool[i].time_delay0;
                break;
            case 1:
                attr = rule_pool[i].antecedent_attr1;
                delay = rule_pool[i].time_delay1;
                break;
            case 2:
                attr = rule_pool[i].antecedent_attr2;
                delay = rule_pool[i].time_delay2;
                break;
            case 3:
                attr = rule_pool[i].antecedent_attr3;
                delay = rule_pool[i].time_delay3;
                break;
            case 4:
                attr = rule_pool[i].antecedent_attr4;
                delay = rule_pool[i].time_delay4;
                break;
            case 5:
                attr = rule_pool[i].antecedent_attr5;
                delay = rule_pool[i].time_delay5;
                break;
            case 6:
                attr = rule_pool[i].antecedent_attr6;
                delay = rule_pool[i].time_delay6;
                break;
            case 7:
                attr = rule_pool[i].antecedent_attr7;
                delay = rule_pool[i].time_delay7;
                break;
            }

            if (attr > 0)
            {
                fprintf(file, "%d(t-%d)\t", attr, delay);
            }
            else
            {
                fprintf(file, "0\t");
            }
        }

        /* 品質フラグを出力 */
        fprintf(file, "%2d\t%2d\n",
                rule_pool[i].high_support_flag,
                rule_pool[i].low_variance_flag);
    }

    fclose(file);
}

/**
 * write_global_pool()
 * グローバルルールプールを出力する関数
 * 全試行で共有されるルールプールを作成
 *
 * @param state: 試行状態
 */
void write_global_pool(struct trial_state *state)
{
    FILE *file_a = fopen(POOL_FILE_A, "w");
    FILE *file_b = fopen(POOL_FILE_B, "w");

    if (file_a != NULL)
    {
        /* ヘッダー行を出力 */
        fprintf(file_a, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file_a, "X_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");

        /* 全ルールを出力 */
        for (int i = 1; i < state->rule_count; i++)
        {
            /* 前件部属性と時間遅延を出力 */
            for (int j = 0; j < 8; j++)
            {
                int attr = 0;
                int delay = 0;

                switch (j)
                {
                case 0:
                    attr = rule_pool[i].antecedent_attr0;
                    delay = rule_pool[i].time_delay0;
                    break;
                case 1:
                    attr = rule_pool[i].antecedent_attr1;
                    delay = rule_pool[i].time_delay1;
                    break;
                case 2:
                    attr = rule_pool[i].antecedent_attr2;
                    delay = rule_pool[i].time_delay2;
                    break;
                case 3:
                    attr = rule_pool[i].antecedent_attr3;
                    delay = rule_pool[i].time_delay3;
                    break;
                case 4:
                    attr = rule_pool[i].antecedent_attr4;
                    delay = rule_pool[i].time_delay4;
                    break;
                case 5:
                    attr = rule_pool[i].antecedent_attr5;
                    delay = rule_pool[i].time_delay5;
                    break;
                case 6:
                    attr = rule_pool[i].antecedent_attr6;
                    delay = rule_pool[i].time_delay6;
                    break;
                case 7:
                    attr = rule_pool[i].antecedent_attr7;
                    delay = rule_pool[i].time_delay7;
                    break;
                }

                if (attr > 0)
                {
                    fprintf(file_a, "%d(t-%d)\t", attr, delay);
                }
                else
                {
                    fprintf(file_a, "0\t");
                }
            }

            /* 統計情報を出力 */
            fprintf(file_a, "%8.3f\t%5.3f\t%8.3f\t%5.3f\t%d\t%d\t%d\t%d\n",
                    rule_pool[i].x_mean, rule_pool[i].x_sigma,
                    rule_pool[i].y_mean, rule_pool[i].y_sigma,
                    rule_pool[i].support_count, rule_pool[i].negative_count,
                    rule_pool[i].high_support_flag, rule_pool[i].low_variance_flag);
        }

        fclose(file_a);
    }

    /* 数値版も同様に出力 */
    if (file_b != NULL)
    {
        fprintf(file_b, "Attr1\tAttr2\tAttr3\tAttr4\tAttr5\tAttr6\tAttr7\tAttr8\t");
        fprintf(file_b, "X_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");

        for (int i = 1; i < state->rule_count; i++)
        {
            for (int j = 0; j < 8; j++)
            {
                int attr = 0;
                int delay = 0;

                switch (j)
                {
                case 0:
                    attr = rule_pool[i].antecedent_attr0;
                    delay = rule_pool[i].time_delay0;
                    break;
                case 1:
                    attr = rule_pool[i].antecedent_attr1;
                    delay = rule_pool[i].time_delay1;
                    break;
                case 2:
                    attr = rule_pool[i].antecedent_attr2;
                    delay = rule_pool[i].time_delay2;
                    break;
                case 3:
                    attr = rule_pool[i].antecedent_attr3;
                    delay = rule_pool[i].time_delay3;
                    break;
                case 4:
                    attr = rule_pool[i].antecedent_attr4;
                    delay = rule_pool[i].time_delay4;
                    break;
                case 5:
                    attr = rule_pool[i].antecedent_attr5;
                    delay = rule_pool[i].time_delay5;
                    break;
                case 6:
                    attr = rule_pool[i].antecedent_attr6;
                    delay = rule_pool[i].time_delay6;
                    break;
                case 7:
                    attr = rule_pool[i].antecedent_attr7;
                    delay = rule_pool[i].time_delay7;
                    break;
                }

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

/**
 * write_document_stats()
 * ドキュメント統計を出力する関数
 * 試行ごとの累積統計を記録
 *
 * @param state: 試行状態
 */
void write_document_stats(struct trial_state *state)
{
    FILE *file = fopen(CONT_FILE, "a");
    if (file != NULL)
    {
        fprintf(file, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                state->trial_id - Nstart + 1,       /* 試行番号 */
                state->rule_count - 1,              /* この試行のルール数 */
                state->high_support_rule_count - 1, /* 高サポート数 */
                state->low_variance_rule_count - 1, /* 低分散数 */
                total_rule_count,                   /* 累積総ルール数 */
                total_high_support,                 /* 累積高サポート数 */
                total_low_variance);                /* 累積低分散数 */
        fclose(file);
    }
}

/* ================================================================================
   9. ユーティリティ系関数
   補助的な機能を提供
   ================================================================================
*/

/**
 * generate_filename()
 * ファイル名を生成する関数
 * 試行番号から5桁のファイル名を作成
 *
 * @param filename: 出力バッファ
 * @param prefix: ファイル接頭辞
 * @param trial_id: 試行ID
 */
void generate_filename(char *filename, const char *prefix, int trial_id)
{
    /* 5桁の数値を分解 */
    int digit5 = trial_id / 10000;
    int digit4 = trial_id / 1000 - digit5 * 10;
    int digit3 = trial_id / 100 - digit5 * 100 - digit4 * 10;
    int digit2 = trial_id / 10 - digit5 * 1000 - digit4 * 100 - digit3 * 10;
    int digit1 = trial_id - digit5 * 10000 - digit4 * 1000 - digit3 * 100 - digit2 * 10;

    /* ファイル名を生成 */
    sprintf(filename, "%s/%s%d%d%d%d%d.txt",
            (strcmp(prefix, "IL") == 0) ? OUTPUT_DIR_IL : (strcmp(prefix, "IA") == 0) ? OUTPUT_DIR_IA
                                                                                      : OUTPUT_DIR_IB,
            prefix, digit5, digit4, digit3, digit2, digit1);
}

/**
 * initialize_document_file()
 * ドキュメントファイルを初期化する関数
 * 統計記録用ファイルのヘッダーを作成
 */
void initialize_document_file()
{
    FILE *file = fopen(CONT_FILE, "w");
    if (file != NULL)
    {
        fprintf(file, "Trial\tRules\tHighSup\tLowVar\tTotal\tTotalHigh\tTotalLow\n");
        fclose(file);
    }
}

/**
 * print_final_statistics()
 * 最終統計を表示する関数
 * プログラム終了時にサマリーを出力
 */
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
   プログラムのエントリーポイント
   ================================================================================
*/

int main()
{
    /* ========== システム初期化フェーズ ========== */
    srand(1);                     /* 乱数シード設定（再現性のため固定） */
    create_output_directories();  /* ディレクトリ構造作成 */
    load_attribute_dictionary();  /* 属性辞書読み込み */
    load_timeseries_data();       /* 時系列データ読み込み */
    initialize_global_counters(); /* グローバルカウンタ初期化 */
    initialize_document_file();   /* ドキュメントファイル初期化 */

    /* ========== メイン試行ループ ========== */
    for (int trial = Nstart; trial < (Nstart + Ntry); trial++)
    {
        /* ---------- 試行状態の初期化 ---------- */
        struct trial_state state;
        state.trial_id = trial;
        state.rule_count = 1; /* 1から開始（0は使用しない） */
        state.high_support_rule_count = 1;
        state.low_variance_rule_count = 1;
        state.generation = 0;

        /* ---------- ファイル名生成 ---------- */
        generate_filename(state.filename_rule, "IL", trial);
        generate_filename(state.filename_report, "IA", trial);
        generate_filename(state.filename_local, "IB", trial);

        printf("\n========== Trial %d/%d Started ==========\n", trial - Nstart + 1, Ntry);
        if (TIMESERIES_MODE && ADAPTIVE_DELAY)
        {
            printf("  [Time-Series Mode Phase 3: Adaptive delays]\n");
        }

        /* ---------- 試行別初期化 ---------- */
        initialize_rule_pool();            /* ルールプール初期化 */
        initialize_delay_statistics();     /* 遅延統計初期化 */
        initialize_attribute_statistics(); /* 属性統計初期化 */
        create_initial_population();       /* 初期個体群生成 */
        create_trial_files(&state);        /* 出力ファイル作成 */

        /* ========== 進化計算ループ ========== */
        for (int gen = 0; gen < Generation; gen++)
        {
            /* ルール数上限チェック */
            if (state.rule_count > (Nrulemax - 2))
            {
                printf("Rule limit reached. Stopping evolution.\n");
                break;
            }

            /* ---------- 個体評価フェーズ ---------- */
            copy_genes_to_nodes();              /* 遺伝子→ノード構造変換 */
            initialize_individual_statistics(); /* 個体統計初期化 */
            evaluate_all_individuals();         /* 全データで評価 */
            calculate_negative_counts();        /* 負例数計算 */
            calculate_rule_statistics();        /* 統計量計算 */

            /* ---------- ルール抽出フェーズ ---------- */
            extract_rules_from_population(&state); /* ルール抽出 */

            /* ---------- 新規ルールの出力 ---------- */
            int prev_count = rules_per_trial[trial - Nstart];
            for (int i = prev_count + 1; i < state.rule_count; i++)
            {
                write_rule_to_file(&state, i);
            }
            rules_per_trial[trial - Nstart] = state.rule_count - 1;

            /* ---------- 統計更新フェーズ ---------- */
            update_delay_statistics(gen);     /* 遅延統計更新 */
            update_attribute_statistics(gen); /* 属性統計更新 */

            /* ---------- 進化操作フェーズ ---------- */
            calculate_fitness_rankings();          /* 適応度順位計算 */
            perform_selection();                   /* 選択 */
            perform_crossover();                   /* 交叉 */
            perform_mutation_processing_nodes();   /* 処理ノード突然変異 */
            perform_mutation_judgment_nodes();     /* 判定ノード突然変異 */
            perform_adaptive_delay_mutation();     /* 適応的遅延突然変異 */
            perform_adaptive_attribute_mutation(); /* 適応的属性突然変異 */

            /* ---------- 進捗報告 ---------- */
            if (gen % 5 == 0)
            {
                write_progress_report(&state, gen);
            }
        }

        /* ---------- 試行終了処理 ---------- */
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

        /* 累積統計更新 */
        total_rule_count += state.rule_count - 1;
        total_high_support += state.high_support_rule_count - 1;
        total_low_variance += state.low_variance_rule_count - 1;

        printf("  Cumulative total: %d rules (High-support: %d, Low-variance: %d)\n",
               total_rule_count, total_high_support, total_low_variance);

        /* ドキュメント更新 */
        write_document_stats(&state);
    }

    /* ========== プログラム終了 ========== */
    print_final_statistics();

    return 0;
}