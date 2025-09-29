#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ================================================================================
   GNP-based Association Rule Mining System
   遺伝的ネットワークプログラミングによる2次元連続値局所分布ルールマイニング

   【システム概要】
   このプログラムは、95個の離散属性から2つの連続値（X,Y）への関連を発見する。
   「ある属性の組み合わせが成立すると、X,Yの値が狭い範囲に集中する」という
   局所分布ルールを進化計算により自動発見する。

   【主要な特徴】
   1. GNP（有向グラフ構造）による柔軟なルール表現
   2. 2次元連続値の分散を考慮した興味深さ評価
   3. 100回の独立試行による多様なルール発見
   4. 属性使用頻度に基づく適応的進化
   5. 出力ファイルの体系的なディレクトリ管理
   ================================================================================ */

/* ================================================================================
   開発履歴とバージョン情報
   ================================================================================ */
/* 2012-1-30 初版開発 */
/* N_(x)<=6 制約条件：前件部の属性数は最大6個まで（計算量とルール解釈性のバランス） */
/* 2021-1-12 第2版：評価関数の改良 */
/* 2021-1-21 機能追加：複数評価指標の導入 */
/* 2021-1-25 変数X,Yの標準偏差を独立に評価（従来は同一閾値） */
/* 2023-5-09 軽微な修正：メモリ管理の最適化 */
/* 2024-1-15 ディレクトリ構造による出力ファイル管理機能追加 */

/* ================================================================================
   ディレクトリ構造定義
   出力ファイルを整理するためのディレクトリパス
   ================================================================================ */
#define OUTPUT_DIR "output"           /* メイン出力ディレクトリ */
#define OUTPUT_DIR_IL "output/IL"     /* ILファイル用（ルールリスト） */
#define OUTPUT_DIR_IA "output/IA"     /* IAファイル用（分析レポート） */
#define OUTPUT_DIR_IT "output/IT"     /* ITファイル用（一時ファイル） */
#define OUTPUT_DIR_IB "output/IB"     /* IBファイル用（バックアップ） */
#define OUTPUT_DIR_POOL "output/pool" /* グローバルルールプール用 */
#define OUTPUT_DIR_DOC "output/doc"   /* ドキュメント用 */

/* ================================================================================
   ファイル定義セクション
   各ファイルの役割と形式を定義
   ================================================================================ */
#define DATANAME "testdata1s.txt" /* 入力: 学習データファイル \
                                     形式: 159行×97列    \
                                     各行: 95個の離散属性(0/1/2) + 2個の実数値(X,Y) */
#define DICNAME "testdic1.txt"    /* 入力: 属性名辞書ファイル                     \
                                     形式: ID番号 属性名（日本語可） \
                                     用途: ルール出力時の可読性向上 */

/* グローバルプールファイル（output/pool/に配置） */
#define fpoola "output/pool/zrp01a.txt" /* 出力: グローバルルールプール（属性名版） \
                                           人間が読みやすい形式でルールを記録 */
#define fpoolb "output/pool/zrp01b.txt" /* 出力: グローバルルールプール（数値版） \
                                           プログラム処理用の数値形式 */

/* ドキュメントファイル（output/doc/に配置） */
#define fcont "output/doc/zrd01.txt"         /* 出力: 実行統計ドキュメント \
                                               各試行でのルール発見数等を記録 */
#define RESULTNAME "output/doc/zrmemo01.txt" /* 出力: デバッグ用メモファイル（現在未使用） */

/* ================================================================================
   データ構造パラメータ定義
   データセットのサイズを定義
   ================================================================================ */
#define Nrd 159 /* 総レコード数（訓練データのインスタンス数） \
                   統計的有意性を保つための最小サンプル数 */
#define Nzk 95  /* 前件部用の属性数                      \
                   各属性は離散値(0/1/2)を取る \
                   これらの組み合わせがルールの条件部となる */

/* ================================================================================
   ルールマイニング制約パラメータ
   発見するルールの品質基準
   ================================================================================ */
#define Nrulemax 2002 /* ルールプールの最大サイズ                               \
                         メモリ制約とルール管理の観点から設定 \
                         2000+予備2 */
#define Minsup 0.04   /* 最小サポート値（4%）                        \
                         159レコード中最低6レコード必要 \
                         統計的信頼性の下限 */
#define Maxsigx 0.5   /* X変数の最大標準偏差                        \
                         これ以下なら「局所的」と判定 \
                         データの正規化を前提 */
#define Maxsigy 0.5   /* Y変数の最大標準偏差 \
                         X,Y独立に評価することで柔軟性向上 */

/* ================================================================================
   実験実行パラメータ
   複数試行による安定した結果取得
   ================================================================================ */
#define Nstart 1000 /* ファイルID開始番号                \
                       出力ファイル名の管理用 \
                       IL1000.txt から開始 */
#define Ntry 100    /* 独立試行回数                               \
                       異なる初期個体群から100回実行 \
                       多様なルールを発見するため */

/* ================================================================================
   GNP（Genetic Network Programming）パラメータ
   進化計算の核心設定
   ================================================================================ */
#define Generation 201 /* 最終世代数                     \
                          収束と探索のバランス \
                          経験的に200世代で十分収束 */
#define Nkotai 120     /* 母集団サイズ（個体数）                              \
                          多様性維持と計算時間のトレードオフ \
                          40×3構造（後述の選択戦略）に対応 */
#define Npn 10         /* 処理ノード数/個体                                \
                          各ノードが異なるルール探索の起点 \
                          並列的に複数ルールを探索 */
#define Njg 100        /* 判定ノード数/個体              \
                          属性判定を行うノード \
                          ネットワークの表現力を決定 */
#define Nmx 7          /* Yes側遷移の最大数+1                               \
                          ルールの最大長を制限（実質6属性） \
                          長すぎるルールは解釈困難 */
#define Muratep 1      /* 処理ノードの突然変異率 \
                          1/1=100%で常に変異（探索強化） */
#define Muratej 6      /* 接続突然変異率の分母 \
                          1/6≈16.7%の確率で接続変更 */
#define Muratea 6      /* 属性突然変異率の分母 \
                          1/6≈16.7%の確率で属性変更 */
#define Nkousa 20      /* 交叉回数/個体                    \
                          各個体ペアで20箇所交叉 \
                          遺伝的多様性の促進 */

/* ================================================================================
   ディレクトリ作成関数
   必要なディレクトリ構造を作成
   ================================================================================ */
void create_directories()
{
    /* メイン出力ディレクトリ作成 */
    mkdir(OUTPUT_DIR, 0755);
    /* 0755: オーナー読み書き実行可、グループ/その他読み実行可 */

    /* サブディレクトリ作成 */
    mkdir(OUTPUT_DIR_IL, 0755);   /* ILファイル用 */
    mkdir(OUTPUT_DIR_IA, 0755);   /* IAファイル用 */
    mkdir(OUTPUT_DIR_IT, 0755);   /* ITファイル用 */
    mkdir(OUTPUT_DIR_IB, 0755);   /* IBファイル用 */
    mkdir(OUTPUT_DIR_POOL, 0755); /* ルールプール用 */
    mkdir(OUTPUT_DIR_DOC, 0755);  /* ドキュメント用 */

    printf("=== Directory Structure Created ===\n");
    printf("output/\n");
    printf("├── IL/        (Rule Lists)\n");
    printf("├── IA/        (Analysis Reports)\n");
    printf("├── IT/        (Temporary Files)\n");
    printf("├── IB/        (Backup Files)\n");
    printf("├── pool/      (Global Rule Pool)\n");
    printf("└── doc/       (Documentation)\n");
    printf("===================================\n\n");
}

int main()
{
    /* ================================================================================
       ルール構造体定義
       発見されたルールの完全な情報を保持
       ================================================================================ */
    struct asrule
    {
        /* --- 前件部（ルールの条件部分）の属性ID --- */
        int antecedent_attr0; /* 前件部属性1: 0は「この位置に属性なし」を意味
                                 1〜95が実際の属性番号 */
        int antecedent_attr1; /* 前件部属性2: ルールは可変長
                                 使用しない部分は0で埋める */
        int antecedent_attr2; /* 前件部属性3 */
        int antecedent_attr3; /* 前件部属性4 */
        int antecedent_attr4; /* 前件部属性5 */
        int antecedent_attr5; /* 前件部属性6: Nmxの制約により最大6属性 */
        int antecedent_attr6; /* 前件部属性7: 拡張用（現在は実質未使用） */
        int antecedent_attr7; /* 前件部属性8: 拡張用（現在は実質未使用） */

        /* --- 後件部（ルールが示す結果）の統計情報 --- */
        double x_mean;  /* X変数の平均値: 前件部を満たすレコードでのX平均
                          このルールが示すX値の中心位置 */
        double x_sigma; /* X変数の標準偏差: 分布の広がり
                          小さいほど「局所的」で価値が高い */
        double y_mean;  /* Y変数の平均値: 前件部を満たすレコードでのY平均 */
        double y_sigma; /* Y変数の標準偏差: Y方向の分布の広がり */

        /* --- ルールの評価指標 --- */
        int support_count;     /* サポート数: 前件部を満たすレコード数
                                 統計的信頼性の指標 */
        int negative_count;    /* 負例数: サポート計算の分母
                                 欠損値を除いた有効レコード数 */
        int high_support_flag; /* 高サポートフラグ: 1=特に信頼性が高い
                                 Minsup+0.02以上で設定 */
        int low_variance_flag; /* 低分散フラグ: 1=特に局所的
                                 Maxsig-0.1以下で設定 */
    };
    struct asrule rule_pool[Nrulemax]; /* ルールプール本体: 最大2002個のルールを保持 */

    /* ================================================================================
       比較用ルール構造体
       重複チェック専用の軽量構造体
       ================================================================================ */
    struct cmrule
    {
        /* 前件部のみを保持（統計情報は不要） */
        int antecedent_attr0;
        int antecedent_attr1;
        int antecedent_attr2;
        int antecedent_attr3;
        int antecedent_attr4;
        int antecedent_attr5;
        int antecedent_attr6;
        int antecedent_attr7;
    };
    struct cmrule compare_rules[Nrulemax]; /* 比較用ルール配列 */

    /* ================================================================================
       作業用変数群の宣言
       様々な処理で使用される変数
       ================================================================================ */

    /* --- 基本的な作業変数とカウンタ --- */
    int work_array[5];           /* 汎用作業配列（現在未使用） */
    int input_value;             /* ファイル読込用の一時変数 */
    int rule_count;              /* 現在のルール数（1から開始） */
    int rule_count2;             /* 予備ルールカウンタ（現在未使用） */
    int high_support_rule_count; /* 高サポートルールの数 */
    int low_variance_rule_count; /* 低分散ルールの数 */

    /* --- ループカウンタ群（多重ループで使用） --- */
    int i, j, k;                /* 汎用ループカウンタ */
    int loop_i, loop_j, loop_k; /* 明示的なループ用カウンタ */
    int i2, j2, k2;             /* 第2階層のループ用 */
    int i3, j3, k3;             /* 第3階層のループ用 */
    int i4, j4, k4;             /* 第4階層のループ用（主に個体操作） */
    int i5, j5;                 /* 第5階層のループ用（属性操作） */
    int i6, j6;                 /* 第6階層のループ用（統計処理） */
    int i7, j7, k7, k8;         /* 第7階層のループ用（ファイル処理） */
    int attr_index;             /* 属性インデックス専用 */

    /* --- GNP関連の制御変数 --- */
    int current_node_id; /* 現在処理中のノードID */
    int loop_trial;      /* 試行ループ制御（未使用） */
    int generation_id;   /* 現在の世代番号 */
    int individual_id;   /* 現在処理中の個体ID */

    /* --- ルール評価関連変数 --- */
    int matched_antecedent_count; /* 前件部マッチ数 */
    int process_id;               /* プロセスID（属性辞書読込用） */
    int new_flag;                 /* 新規性判定フラグ（0:既存, 1:新規） */
    int same_flag1, same_flag2;   /* 同一性判定フラグ */
    int match_flag;               /* マッチング状態フラグ */

    /* --- 進化操作関連変数 --- */
    int mutation_random;                                /* 突然変異判定用乱数 */
    int mutation_attr1, mutation_attr2, mutation_attr3; /* 属性突然変異用作業変数 */
    int total_attribute_usage;                          /* 全属性の使用頻度合計 */
    int difference_count;                               /* 差分カウント（未使用） */

    /* --- 交叉操作関連変数 --- */
    int crossover_parent;                 /* 交叉親個体ID（未使用） */
    int crossover_node1, crossover_node2; /* 交叉対象ノード */
    int temp_memory;                      /* 一時記憶（遺伝子交換用） */
    int save_memory;                      /* 保存用メモリ（未使用） */
    int node_memory;                      /* ノード情報保存（未使用） */
    int node_save1, node_save2;           /* ノード保存用（未使用） */

    /* --- 評価マーカー関連 --- */
    int backup_counter2;     /* バックアップカウンタ（未使用） */
    int high_support_marker; /* 高サポート判定用 */
    int low_variance_marker; /* 低分散判定用 */

    /* --- 統計量関連の実数変数 --- */
    double input_x, input_y;               /* 読み込んだX,Y値 */
    double threshold_x, threshold_sigma_x; /* X変数の閾値 */
    double threshold_y, threshold_sigma_y; /* Y変数の閾値 */

    /* --- グローバル統計変数 --- */
    int total_rule_count;   /* 全試行での総ルール数 */
    int new_rule_count;     /* 新規ルール数 */
    int total_high_support; /* 全高サポートルール数 */
    int total_low_variance; /* 全低分散ルール数 */
    int new_high_support;   /* 新規高サポートルール数 */
    int new_low_variance;   /* 新規低分散ルール数 */

    /* --- ファイル名生成用変数 --- */
    int file_index;                                                      /* ファイルインデックス */
    int file_digit;                                                      /* ファイル番号（未使用） */
    int trial_index;                                                     /* 現在の試行番号 */
    int compare_trial_index;                                             /* 比較対象試行番号 */
    int file_digit1, file_digit2, file_digit3, file_digit4, file_digit5; /* 5桁分解用 */
    int compare_digit1, compare_digit2, compare_digit3, compare_digit4, compare_digit5;

    /* --- 評価値関連 --- */
    double confidence_max;                   /* 最大信頼度（未使用） */
    double fitness_average_all;              /* 全個体の平均適応度 */
    double support_value;                    /* サポート値（0.0〜1.0） */
    double sigma_x, sigma_y;                 /* 標準偏差の作業変数 */
    double current_x_value, current_y_value; /* 現在のX,Y値 */

    /* ================================================================================
       大規模静的配列の宣言
       GNPネットワークと統計情報を保持
       ================================================================================ */

    /* --- GNPネットワーク構造 --- */
    static int node_structure[Nkotai][Npn + Njg][2];
    /* 3次元配列: [個体番号][ノード番号][属性/接続]
       [0]: ノードの属性ID（判定ノードが見る属性）
       [1]: 次の接続先ノードID
       静的確保で約120×110×2×4bytes = 105KB */

    /* --- データ読込バッファ --- */
    static int record_attributes[Nzk + 2];
    /* 1レコード分の属性値を保持
       +2は連続値格納用の予備（実際は別変数使用） */

    /* --- 統計情報配列群（各個体×各処理ノード×各深さ） --- */
    static int match_count[Nkotai][Npn][10];
    /* 各ノード連鎖でマッチしたレコード数
       ルールのサポート計算の基礎データ */

    static int negative_count[Nkotai][Npn][10];
    /* 負例数（マッチしなかったレコード数）
       サポート値の分母計算に使用 */

    static int evaluation_count[Nkotai][Npn][10];
    /* 評価されたレコード数（欠損値を除く）
       統計的有効性の判定に使用 */

    static int attribute_chain[Nkotai][Npn][10];
    /* 属性連鎖の記録
       どの属性がどの順序で判定されたか */

    /* --- X,Y変数の統計量配列 --- */
    static double x_sum[Nkotai][Npn][10];
    /* X変数の累積和（平均計算用）
       各深さでのX値の合計 */

    static double x_sigma_array[Nkotai][Npn][10];
    /* X変数の2乗和（分散計算用）
       Σ(x^2)を保持、後で分散を計算 */

    static double y_sum[Nkotai][Npn][10];
    /* Y変数の累積和（平均計算用） */

    static double y_sigma_array[Nkotai][Npn][10];
    /* Y変数の2乗和（分散計算用） */

    /* --- 実行管理配列 --- */
    int rules_per_trial[Ntry];
    /* 各試行で発見されたルール数
       重複チェック時に参照 */

    /* --- 属性使用統計 --- */
    int attribute_usage_history[5][Nzk];
    /* 過去5世代分の属性使用履歴
       適応的突然変異で使用頻度の高い属性を優先 */

    int attribute_usage_count[Nzk];
    /* 現世代での属性使用回数 */

    int attribute_tracking[Nzk];
    /* 属性使用の追跡（5世代分の合計） */

    /* --- 適応度関連 --- */
    double fitness_value[Nkotai];
    /* 各個体の適応度値
       新規ルール発見数と品質で評価 */

    int fitness_ranking[Nkotai];
    /* 適応度順位（0が最良） */

    int new_rule_flag[Nkotai];
    /* 新規ルール発見フラグ（未使用） */

    /* --- GNP遺伝子プール --- */
    static int gene_connection[Nkotai][Npn + Njg];
    /* ノード接続遺伝子
       次世代に引き継がれる接続情報 */

    static int gene_attribute[Nkotai][Npn + Njg];
    /* 属性遺伝子
       各ノードが判定する属性ID */

    /* --- 作業用配列 --- */
    int selected_attributes[10];
    /* 選択された属性の一時保存 */

    int read_attributes[10];
    /* ファイルから読んだ属性 */

    int rules_by_attribute_count[10];
    /* 属性数別のルール数統計
       [i]: i個の属性を持つルール数 */

    int attribute_set[Nzk + 1];
    /* 属性セット（0〜Nzk） */

    /* --- ルール候補管理 --- */
    int rule_candidate_pre[10];
    /* ルール候補の前処理用 */

    int rule_candidate[10];
    /* ルール候補（整理後） */

    int rule_memo[10];
    /* ルール記憶用 */

    /* --- 新規性管理 --- */
    int new_rule_marker[Nrulemax];
    /* 新規ルールマーカー
       0:新規, 1:既存と重複 */

    /* ================================================================================
       ファイル名文字列配列
       動的に生成される出力ファイル名（パス付き）
       ================================================================================ */
    char filename_rule[256];   /* ILファイルパス（拡張） */
    char filename_report[256]; /* IAファイルパス（拡張） */
    char filename_temp[256];   /* ITファイルパス（拡張） */
    char filename_local[256];  /* IBファイルパス（拡張） */

    /* --- 属性名管理 --- */
    char attribute_name_buffer[31];
    /* 属性名読込バッファ
       最大30文字+終端 */

    char attribute_dictionary[Nzk + 3][31];
    /* 属性名辞書
       全属性の名前を保持 */

    /* ================================================================================
       ファイルポインタ宣言
       入出力ファイルのハンドル管理
       ================================================================================ */
    FILE *file_data1;          /* 汎用データファイル1（未使用） */
    FILE *file_data2;          /* データ読込用（DATANAME） */
    FILE *file_dictionary;     /* 属性辞書読込用（DICNAME） */
    FILE *file_rule;           /* ルール出力用 */
    FILE *file_report;         /* レポート出力用 */
    FILE *file_compare;        /* 比較用ルールファイル */
    FILE *file_local;          /* ローカル出力用 */
    FILE *file_pool_attribute; /* グローバルプール（属性名版） */
    FILE *file_pool_numeric;   /* グローバルプール（数値版） */
    FILE *file_document;       /* ドキュメント出力用 */

    /* ================================================================================
       メイン処理開始
       プログラムのエントリポイント
       ================================================================================ */

    /* ディレクトリ構造を作成 */
    create_directories();

    srand(1);
    /* 乱数シード固定: 再現性確保のため
       実験では固定、実用では time(NULL) 推奨 */

    /* ================================================================================
       初期化フェーズ1: 属性辞書の読み込み
       ルールの可読性向上のため属性名を事前ロード
       ================================================================================ */

    /* 属性辞書配列を空文字列で初期化 */
    for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
    {
        strcpy(attribute_dictionary[attr_index], "\0");
        /* 各要素を明示的に空文字列化
           後の文字列操作での事故防止 */
    }

    /* 属性辞書ファイルを開いて全属性名を読込 */
    file_dictionary = fopen(DICNAME, "r");
    if (file_dictionary == NULL)
    {
        printf("Error: Cannot open dictionary file: %s\n", DICNAME);
        printf("Please ensure the file exists in the current directory.\n");
        return 0;
    }
    for (attr_index = 0; attr_index < Nzk + 3; attr_index++)
    {
        fscanf(file_dictionary, "%d%s", &process_id, attribute_name_buffer);
        /* フォーマット: "番号 属性名"
           番号は確認用、実際の格納は配列順序に依存 */
        strcpy(attribute_dictionary[attr_index], attribute_name_buffer);
        /* 属性名を辞書にコピー
           最大30文字まで対応 */
    }
    fclose(file_dictionary);
    printf("Dictionary loaded: %d attributes\n", Nzk);

    /* ================================================================================
       初期化フェーズ2: グローバルカウンタ初期化
       全試行を通じた統計情報の準備
       ================================================================================ */
    total_rule_count = 0;
    /* 全試行での累積ルール数
       最終的な成果を評価 */

    total_high_support = 0;
    /* 高品質ルール（高サポート）の総数
       システムの有効性指標 */

    total_low_variance = 0;
    /* 高品質ルール（低分散）の総数
       局所性の達成度指標 */

    /* ドキュメントファイルの初期化（ヘッダー出力） */
    file_document = fopen(fcont, "w");
    if (file_document == NULL)
    {
        printf("Error: Cannot create document file: %s\n", fcont);
        return 0;
    }
    fprintf(file_document, "Trial\tRules\tHighSup\tLowVar\tTotal\tTotalHigh\tTotalLow\n");
    /* ヘッダー行を追加 */
    fclose(file_document);
    printf("Document file initialized: %s\n", fcont);

    /* ================================================================================
       ★★★ メイン試行ループ開始 ★★★
       Ntry(100)回の独立した進化計算を実行
       各試行は異なる初期個体群から開始し、多様なルールを発見
       ================================================================================ */
    for (trial_index = Nstart; trial_index < (Nstart + Ntry); trial_index++)
    {
        printf("\n========== Trial %d/%d Started ==========\n",
               trial_index - Nstart + 1, Ntry);

        /* ================================================================================
           試行別初期化フェーズ
           各試行を完全に独立させるため全データ構造をリセット
           ================================================================================ */

        /* --- ルールプールの完全初期化 --- */
        for (i = 0; i < Nrulemax; i++)
        {
            /* 前件部属性を全クリア（0=未使用） */
            rule_pool[i].antecedent_attr0 = 0;
            rule_pool[i].antecedent_attr1 = 0;
            rule_pool[i].antecedent_attr2 = 0;
            rule_pool[i].antecedent_attr3 = 0;
            rule_pool[i].antecedent_attr4 = 0;
            rule_pool[i].antecedent_attr5 = 0;
            rule_pool[i].antecedent_attr6 = 0;
            rule_pool[i].antecedent_attr7 = 0;

            /* 統計量を全て0に初期化 */
            rule_pool[i].x_mean = 0;
            rule_pool[i].x_sigma = 0;
            rule_pool[i].y_mean = 0;
            rule_pool[i].y_sigma = 0;

            /* 評価指標も初期化 */
            rule_pool[i].support_count = 0;
            rule_pool[i].negative_count = 0;
            rule_pool[i].high_support_flag = 0;
            rule_pool[i].low_variance_flag = 0;
        }

        /* --- 比較用ルール配列の初期化 --- */
        for (i = 0; i < Nrulemax; i++)
        {
            /* 前件部のみ初期化（統計情報は不要） */
            compare_rules[i].antecedent_attr0 = 0;
            compare_rules[i].antecedent_attr1 = 0;
            compare_rules[i].antecedent_attr2 = 0;
            compare_rules[i].antecedent_attr3 = 0;
            compare_rules[i].antecedent_attr4 = 0;
            compare_rules[i].antecedent_attr5 = 0;
            compare_rules[i].antecedent_attr6 = 0;
            compare_rules[i].antecedent_attr7 = 0;
        }

        /* --- 新規ルールマーカーの初期化 --- */
        for (i = 0; i < Nrulemax; i++)
        {
            new_rule_marker[i] = 0;
            /* 0:新規（デフォルト）
               1:既存ルールと重複 */
        }

        /* ================================================================================
           ファイルパス生成処理
           試行番号から5桁の数字を抽出し、ディレクトリ付きのファイルパスを作成
           例: trial_index=12345 → output/IL/IL12345.txt
           ================================================================================ */

        /* 5桁の数字を各桁に分解 */
        file_digit5 = trial_index / 10000;                                                                           /* 万の位: 12345→1 */
        file_digit4 = trial_index / 1000 - file_digit5 * 10;                                                         /* 千の位: 12345→2 */
        file_digit3 = trial_index / 100 - file_digit5 * 100 - file_digit4 * 10;                                      /* 百の位: 12345→3 */
        file_digit2 = trial_index / 10 - file_digit5 * 1000 - file_digit4 * 100 - file_digit3 * 10;                  /* 十の位: 12345→4 */
        file_digit1 = trial_index - file_digit5 * 10000 - file_digit4 * 1000 - file_digit3 * 100 - file_digit2 * 10; /* 一の位: 12345→5 */

        /* ルールファイルパス生成: output/IL/IL#####.txt */
        sprintf(filename_rule, "%s/IL%d%d%d%d%d.txt",
                OUTPUT_DIR_IL,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        /* レポートファイルパス生成: output/IA/IA#####.txt */
        sprintf(filename_report, "%s/IA%d%d%d%d%d.txt",
                OUTPUT_DIR_IA,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        /* 一時ファイルパス生成: output/IT/IT#####.txt */
        sprintf(filename_temp, "%s/IT%d%d%d%d%d.txt",
                OUTPUT_DIR_IT,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        /* ローカルファイルパス生成: output/IB/IB#####.txt */
        sprintf(filename_local, "%s/IB%d%d%d%d%d.txt",
                OUTPUT_DIR_IB,
                file_digit5, file_digit4, file_digit3, file_digit2, file_digit1);

        /* ================================================================================
           試行別変数の初期化
           ================================================================================ */

        /* 属性セットの初期化（0〜Nzkの連番） */
        for (file_index = 0; file_index < Nzk + 1; file_index++)
        {
            attribute_set[file_index] = file_index;
            /* 属性番号の対応表
               後の処理で属性の並び替えに使用可能 */
        }

        /* ルールカウンタの初期化 */
        rule_count = 1;              /* ルール番号は1から開始（0は未使用） */
        high_support_rule_count = 1; /* 高サポートルール数 */
        low_variance_rule_count = 1; /* 低分散ルール数 */

        /* 属性数別ルール統計の初期化 */
        for (i = 0; i < 10; i++)
        {
            rules_by_attribute_count[i] = 0;
            /* rules_by_attribute_count[3]=10 なら
               3属性のルールが10個発見された */
        }

        /* 属性使用管理の初期化 */
        total_attribute_usage = Nzk; /* 初期は全属性が等価 */

        for (i = 0; i < Nzk; i++)
        {
            attribute_usage_count[i] = 0;      /* 現世代での使用回数 */
            attribute_tracking[i] = 0;         /* 累積使用回数 */
            attribute_usage_history[0][i] = 1; /* 第0世代は全属性使用可 */
            attribute_usage_history[1][i] = 0; /* 過去世代は0初期化 */
            attribute_usage_history[2][i] = 0;
            attribute_usage_history[3][i] = 0;
            attribute_usage_history[4][i] = 0;
        }

        /* ================================================================================
           GNP初期個体群の生成
           完全ランダムな120個体を作成
           ================================================================================ */
        for (i4 = 0; i4 < Nkotai; i4++) /* 各個体について */
        {
            for (j4 = 0; j4 < (Npn + Njg); j4++) /* 全ノードについて */
            {
                gene_connection[i4][j4] = rand() % Njg + Npn;
                /* 接続先をランダム設定
                   Npn〜(Npn+Njg-1)の範囲
                   必ず判定ノードを指す */

                gene_attribute[i4][j4] = rand() % Nzk;
                /* 判定属性をランダム選択
                   0〜(Nzk-1)の範囲 */
            }
        }

        /* ================================================================================
           出力ファイルの初期化
           各試行用のファイルを新規作成（ディレクトリ付き）
           ================================================================================ */

        /* ルールファイル作成（空ファイル） */
        file_rule = fopen(filename_rule, "w");
        if (file_rule == NULL)
        {
            printf("Error: Cannot create rule file: %s\n", filename_rule);
            return 0;
        }
        fprintf(file_rule, "");
        fclose(file_rule);
        printf("Created: %s\n", filename_rule);

        /* レポートファイル作成（ヘッダー行） */
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
        fprintf(file_local, "# Local backup file for trial %d\n", trial_index);
        fclose(file_local);

        /* ================================================================================
           ★★★ 進化計算メインループ開始 ★★★
           201世代の進化を実行
           ================================================================================ */
        for (generation_id = 0; generation_id < Generation; generation_id++)
        {
            /* ルール数上限チェック */
            if (rule_count > (Nrulemax - 2))
            {
                /* バッファオーバーフロー防止
                   残り2個を予備として確保 */
                printf("Rule limit reached. Stopping evolution.\n");
                break;
            }

            /* ================================================================================
               世代開始: GNP遺伝子を個体構造にコピー
               進化操作で更新された遺伝子を実行用構造体に反映
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (j4 = 0; j4 < (Npn + Njg); j4++)
                {
                    node_structure[individual_id][j4][0] = gene_attribute[individual_id][j4];
                    /* [0]: ノードが判定する属性ID */

                    node_structure[individual_id][j4][1] = gene_connection[individual_id][j4];
                    /* [1]: 次の接続先ノードID */
                }
            }

            /* ================================================================================
               評価準備: 統計配列の初期化
               各個体の評価結果を格納する配列をクリア
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                fitness_value[individual_id] = (double)individual_id * (-0.00001);
                /* 適応度に微小な差をつける
                   完全同一適応度による順位付け問題を回避
                   individual_id=0が-0.00000, 1が-0.00001... */

                fitness_ranking[individual_id] = 0;
                /* ランキング初期化（後で計算） */

                /* 各処理ノードの統計情報初期化 */
                for (k = 0; k < Npn; k++)
                {
                    for (i = 0; i < 10; i++) /* 最大深さ10まで */
                    {
                        match_count[individual_id][k][i] = 0;
                        /* マッチしたレコード数 */

                        attribute_chain[individual_id][k][i] = 0;
                        /* どの属性が使われたか */

                        negative_count[individual_id][k][i] = 0;
                        /* マッチしなかったレコード数 */

                        evaluation_count[individual_id][k][i] = 0;
                        /* 評価されたレコード数 */

                        x_sum[individual_id][k][i] = 0;
                        /* X値の累積（平均計算用） */

                        x_sigma_array[individual_id][k][i] = 0;
                        /* X値の2乗累積（分散計算用） */

                        y_sum[individual_id][k][i] = 0;
                        /* Y値の累積 */

                        y_sigma_array[individual_id][k][i] = 0;
                        /* Y値の2乗累積 */
                    }
                }
            }

            /* ================================================================================
               ★ データセット評価フェーズ ★
               全159レコードを各個体のGNPネットワークで評価
               これがGNMinerの中核処理
               ================================================================================ */
            file_data2 = fopen(DATANAME, "r");
            if (file_data2 == NULL)
            {
                printf("Error: Cannot open data file: %s\n", DATANAME);
                return 0;
            }

            for (i = 0; i < Nrd; i++) /* 各レコード（インスタンス）について */
            {
                /* --- Step1: レコードデータの読み込み --- */
                for (j7 = 0; j7 < Nzk; j7++)
                {
                    fscanf(file_data2, "%d", &input_value);
                    record_attributes[j7] = input_value;
                    /* 95個の属性値（0/1/2）を配列に格納
                       0: 属性が不成立
                       1: 属性が成立
                       2: 欠損値または中間値 */
                }

                /* X,Y連続値の読み込み */
                fscanf(file_data2, "%lf%lf", &input_x, &input_y);
                current_x_value = input_x; /* 作業変数にコピー */
                current_y_value = input_y;

                /* ================================================================================
                   ★★ GNPネットワーク評価の核心部分 ★★
                   各個体のネットワークでレコードを判定
                   ================================================================================ */
                for (individual_id = 0; individual_id < Nkotai; individual_id++)
                {
                    for (k = 0; k < Npn; k++) /* 各処理ノードを起点として */
                    {
                        /* --- ネットワーク探索の初期化 --- */
                        current_node_id = k; /* 処理ノードkから開始 */
                        j = 0;               /* 深さカウンタ（ルールの属性数） */
                        match_flag = 1;      /* マッチフラグ（1:有効） */

                        /* ルートノード（深さ0）の統計更新 */
                        match_count[individual_id][k][j] = match_count[individual_id][k][j] + 1;
                        evaluation_count[individual_id][k][j] = evaluation_count[individual_id][k][j] + 1;

                        /* 最初の判定ノードへ遷移 */
                        current_node_id = node_structure[individual_id][current_node_id][1];

                        /* --- 判定ノードチェーンを辿る --- */
                        while (current_node_id > (Npn - 1) && j < Nmx)
                        /* 条件1: current_node_id>(Npn-1) → 判定ノードである
                           条件2: j<Nmx → 最大深さ未達 */
                        {
                            j = j + 1; /* 深さインクリメント */

                            /* 現在のノードが判定する属性を記録 */
                            attribute_chain[individual_id][k][j] =
                                node_structure[individual_id][current_node_id][0] + 1;
                            /* +1する理由: 内部では0開始、記録は1開始 */

                            /* ★ 属性値による分岐処理（GNPの核心） ★ */
                            if (record_attributes[node_structure[individual_id][current_node_id][0]] == 1)
                            {
                                /* === 属性値が1（成立）の場合 === */
                                if (match_flag == 1) /* まだ有効な経路なら */
                                {
                                    /* マッチ数をインクリメント */
                                    match_count[individual_id][k][j] =
                                        match_count[individual_id][k][j] + 1;

                                    /* X,Y値を累積（統計計算用） */
                                    x_sum[individual_id][k][j] =
                                        x_sum[individual_id][k][j] + current_x_value;
                                    x_sigma_array[individual_id][k][j] =
                                        x_sigma_array[individual_id][k][j] +
                                        current_x_value * current_x_value;
                                    /* 2乗和: 分散=E[X^2]-E[X]^2 で後で計算 */

                                    y_sum[individual_id][k][j] =
                                        y_sum[individual_id][k][j] + current_y_value;
                                    y_sigma_array[individual_id][k][j] =
                                        y_sigma_array[individual_id][k][j] +
                                        current_y_value * current_y_value;
                                }

                                /* 評価カウントは無条件で増加 */
                                evaluation_count[individual_id][k][j] =
                                    evaluation_count[individual_id][k][j] + 1;

                                /* Yes側（継続）へ遷移 */
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                            else if (record_attributes[node_structure[individual_id][current_node_id][0]] == 0)
                            {
                                /* === 属性値が0（不成立）の場合 === */
                                /* No側へ遷移→処理ノードに戻る */
                                current_node_id = k;
                                /* 判定失敗でルート探索終了 */
                            }
                            else /* 属性値が2（欠損値）の場合 */
                            {
                                /* === 欠損値の処理 === */
                                evaluation_count[individual_id][k][j] =
                                    evaluation_count[individual_id][k][j] + 1;

                                match_flag = 0; /* 以降は無効化 */

                                /* 一応継続するが統計には含めない */
                                current_node_id = node_structure[individual_id][current_node_id][1];
                            }
                        } /* while: 判定ノードチェーン終了 */
                    } /* k: 処理ノードループ終了 */
                } /* individual_id: 個体ループ終了 */
            } /* i: レコードループ終了 */
            fclose(file_data2);

            /* ================================================================================
               統計後処理1: 負例数の計算
               サポート値計算の分母となる重要な値
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (k = 0; k < Npn; k++)
                {
                    for (i = 0; i < Nmx; i++)
                    {
                        negative_count[individual_id][k][i] =
                            match_count[individual_id][k][0]        /* 全体数 */
                            - evaluation_count[individual_id][k][i] /* 評価外を引く */
                            + match_count[individual_id][k][i];     /* 正例を足し戻す */
                                                                    /* 結果: 有効レコード数（欠損値除外） */
                    }
                }
            }

            /* ================================================================================
               統計後処理2: 平均と標準偏差の計算
               累積値から統計量を算出
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                for (k = 0; k < Npn; k++)
                {
                    for (j6 = 1; j6 < 9; j6++) /* 深さ1〜8について */
                    {
                        if (match_count[individual_id][k][j6] != 0) /* データが存在する場合のみ */
                        {
                            /* === X変数の統計量計算 === */
                            /* 平均値: sum/count */
                            x_sum[individual_id][k][j6] =
                                x_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];

                            /* 分散: E[X^2] - E[X]^2 */
                            x_sigma_array[individual_id][k][j6] =
                                x_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - x_sum[individual_id][k][j6] * x_sum[individual_id][k][j6];
                            /* 分散の公式を使用して一度の走査で計算 */

                            /* === Y変数の統計量計算 === */
                            y_sum[individual_id][k][j6] =
                                y_sum[individual_id][k][j6] / (double)match_count[individual_id][k][j6];

                            y_sigma_array[individual_id][k][j6] =
                                y_sigma_array[individual_id][k][j6] / (double)match_count[individual_id][k][j6] - y_sum[individual_id][k][j6] * y_sum[individual_id][k][j6];

                            /* === 数値誤差の補正 === */
                            /* 浮動小数点演算誤差で負になる場合がある */
                            if (x_sigma_array[individual_id][k][j6] < 0)
                            {
                                x_sigma_array[individual_id][k][j6] = 0;
                                /* 理論上は0以上だが、計算誤差で微小な負値になることがある */
                            }
                            if (y_sigma_array[individual_id][k][j6] < 0)
                            {
                                y_sigma_array[individual_id][k][j6] = 0;
                            }

                            /* === 標準偏差への変換 === */
                            x_sigma_array[individual_id][k][j6] = sqrt(x_sigma_array[individual_id][k][j6]);
                            y_sigma_array[individual_id][k][j6] = sqrt(y_sigma_array[individual_id][k][j6]);
                            /* sqrt(分散) = 標準偏差 */
                        }
                    }
                }
            }

            /* ================================================================================
               ★★★ ルール抽出フェーズ ★★★
               統計結果から興味深いルールを抽出
               これが本システムの最重要処理
               ================================================================================ */
            for (individual_id = 0; individual_id < Nkotai; individual_id++)
            {
                if (rule_count > (Nrulemax - 2))
                {
                    break; /* ルール数上限チェック */
                }

                for (k = 0; k < Npn; k++) /* 各処理ノードチェーンから */
                {
                    if (rule_count > (Nrulemax - 2))
                    {
                        break;
                    }

                    /* ルール候補配列の初期化 */
                    for (i = 0; i < 10; i++)
                    {
                        rule_candidate_pre[i] = 0;
                        rule_candidate[i] = 0;
                        rule_memo[i] = 0;
                    }

                    /* --- 各深さでルールを評価 --- */
                    for (loop_j = 1; loop_j < 9; loop_j++) /* 深さ1〜8 */
                    {
                        /* 統計値の取得 */
                        threshold_x = x_sum[individual_id][k][loop_j]; /* X平均 */
                        threshold_sigma_x = Maxsigx;                   /* X標準偏差の閾値 */

                        threshold_y = y_sum[individual_id][k][loop_j]; /* Y平均 */
                        threshold_sigma_y = Maxsigy;                   /* Y標準偏差の閾値 */

                        matched_antecedent_count = match_count[individual_id][k][loop_j];
                        /* このルールにマッチしたレコード数 */

                        support_value = 0;                                 /* サポート値（初期化） */
                        sigma_x = x_sigma_array[individual_id][k][loop_j]; /* X標準偏差 */
                        sigma_y = y_sigma_array[individual_id][k][loop_j]; /* Y標準偏差 */

                        /* サポート値の計算 */
                        if (negative_count[individual_id][k][loop_j] != 0)
                        {
                            support_value = (double)matched_antecedent_count /
                                            (double)negative_count[individual_id][k][loop_j];
                            /* サポート = 正例数 / 有効レコード数 */
                        }

                        /* ================================================================================
                           ★ 興味深いルールの判定条件 ★
                           3つの条件を全て満たすルールのみ抽出
                           ================================================================================ */
                        if (sigma_x <= threshold_sigma_x && /* 条件1: X方向が局所的 */
                            sigma_y <= threshold_sigma_y && /* 条件2: Y方向が局所的 */
                            support_value >= Minsup)        /* 条件3: 十分なサポート */
                        {
                            /* === ルール前件部の構築 === */
                            /* 使用された属性を取得 */
                            for (i2 = 1; i2 < 9; i2++)
                            {
                                rule_candidate_pre[i2 - 1] = attribute_chain[individual_id][k][i2];
                                /* 判定ノードチェーンから属性列を取得 */
                            }

                            /* === 有効属性の抽出 === */
                            j2 = 0;                            /* 有効属性数カウンタ */
                            for (k2 = 1; k2 < (1 + Nzk); k2++) /* 全属性をチェック */
                            {
                                new_flag = 0;
                                for (i2 = 0; i2 < loop_j; i2++) /* 現在の深さまで */
                                {
                                    if (rule_candidate_pre[i2] == k2)
                                    {
                                        new_flag = 1; /* この属性は使用されている */
                                    }
                                }
                                if (new_flag == 1)
                                {
                                    rule_candidate[j2] = k2; /* 候補配列に追加 */
                                    rule_memo[j2] = k2;      /* メモ配列にも保存 */
                                    j2 = j2 + 1;             /* 有効属性数増加 */
                                }
                            }

                            if (j2 < 9) /* 属性数が上限内（実質6個まで） */
                            {
                                /* ================================================================================
                                   新規性チェック: 既存ルールとの重複判定
                                   O(n)の線形探索だが、ルール数が少ないため問題なし
                                   ================================================================================ */
                                new_flag = 0; /* 新規性フラグ初期化 */

                                for (i2 = 0; i2 < rule_count; i2++) /* 既存の全ルールと比較 */
                                {
                                    same_flag1 = 1; /* 同一性フラグ（1:異なる） */

                                    /* 8属性全てが一致するかチェック */
                                    if (rule_pool[i2].antecedent_attr0 == rule_candidate[0] &&
                                        rule_pool[i2].antecedent_attr1 == rule_candidate[1] &&
                                        rule_pool[i2].antecedent_attr2 == rule_candidate[2] &&
                                        rule_pool[i2].antecedent_attr3 == rule_candidate[3] &&
                                        rule_pool[i2].antecedent_attr4 == rule_candidate[4] &&
                                        rule_pool[i2].antecedent_attr5 == rule_candidate[5] &&
                                        rule_pool[i2].antecedent_attr6 == rule_candidate[6] &&
                                        rule_pool[i2].antecedent_attr7 == rule_candidate[7])
                                    {
                                        same_flag1 = 0; /* 完全一致 */
                                    }

                                    if (same_flag1 == 0) /* 既存ルールと一致 */
                                    {
                                        new_flag = 1; /* 新規ではない */

                                        /* 既存ルール再発見ボーナス */
                                        fitness_value[individual_id] =
                                            fitness_value[individual_id] + j2 /* 属性数ボーナス */
                                            + support_value * 10              /* サポートボーナス */
                                            + 2 / (sigma_x + 0.1)             /* X局所性ボーナス */
                                            + 2 / (sigma_y + 0.1);            /* Y局所性ボーナス */
                                                                              /* 既存の良いルールを再発見する個体も評価 */
                                    }

                                    if (new_flag == 1)
                                    {
                                        break; /* 一致が見つかったら探索終了 */
                                    }
                                }

                                if (new_flag == 0) /* 新規ルールの場合 */
                                {
                                    /* ================================================================================
                                       新規ルールの登録処理
                                       ルールプールへの追加と統計更新
                                       ================================================================================ */

                                    /* ルールプールに追加 */
                                    rule_pool[rule_count].antecedent_attr0 = rule_candidate[0];
                                    rule_pool[rule_count].antecedent_attr1 = rule_candidate[1];
                                    rule_pool[rule_count].antecedent_attr2 = rule_candidate[2];
                                    rule_pool[rule_count].antecedent_attr3 = rule_candidate[3];
                                    rule_pool[rule_count].antecedent_attr4 = rule_candidate[4];
                                    rule_pool[rule_count].antecedent_attr5 = rule_candidate[5];
                                    rule_pool[rule_count].antecedent_attr6 = rule_candidate[6];
                                    rule_pool[rule_count].antecedent_attr7 = rule_candidate[7];

                                    /* 統計情報の保存 */
                                    rule_pool[rule_count].x_mean = x_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].x_sigma = x_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_mean = y_sum[individual_id][k][loop_j];
                                    rule_pool[rule_count].y_sigma = y_sigma_array[individual_id][k][loop_j];
                                    rule_pool[rule_count].support_count = matched_antecedent_count;
                                    rule_pool[rule_count].negative_count = negative_count[individual_id][k][loop_j];

                                    /* === ファイルへの即座の出力 === */
                                    /* メモリクラッシュ対策: 即座に永続化 */
                                    file_rule = fopen(filename_rule, "a"); /* 追記モード */
                                    fprintf(file_rule, "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                                            rule_candidate[0], rule_candidate[1],
                                            rule_candidate[2], rule_candidate[3],
                                            rule_candidate[4], rule_candidate[5],
                                            rule_candidate[6], rule_candidate[7]);
                                    fclose(file_rule);

                                    /* === 品質マーカーの設定 === */

                                    /* 高サポートマーカー */
                                    high_support_marker = 0;
                                    if (support_value >= (Minsup + 0.02)) /* 基準値+2% */
                                    {
                                        high_support_marker = 1;
                                        /* 特に信頼性の高いルール */
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
                                        /* 特に局所的なルール */
                                    }
                                    rule_pool[rule_count].low_variance_flag = low_variance_marker;

                                    if (rule_pool[rule_count].low_variance_flag == 1)
                                    {
                                        low_variance_rule_count = low_variance_rule_count + 1;
                                    }

                                    /* ルールカウンタ更新 */
                                    rule_count = rule_count + 1;
                                    rules_by_attribute_count[j2] = rules_by_attribute_count[j2] + 1;
                                    /* 属性数別の統計更新 */

                                    /* === 新規ルール発見の大ボーナス === */
                                    fitness_value[individual_id] =
                                        fitness_value[individual_id] + j2 /* 属性数（複雑さ） */
                                        + support_value * 10              /* サポート×10 */
                                        + 2 / (sigma_x + 0.1)             /* X局所性 */
                                        + 2 / (sigma_y + 0.1)             /* Y局所性 */
                                        + 20;                             /* 新規発見ボーナス！ */

                                    /* === 属性使用頻度の更新 === */
                                    /* 発見に貢献した属性の使用回数を増やす */
                                    for (k3 = 0; k3 < j2; k3++)
                                    {
                                        i5 = rule_memo[k3] - 1; /* 属性番号（0開始） */
                                        attribute_usage_history[0][i5] =
                                            attribute_usage_history[0][i5] + 1;
                                        /* 現世代での使用回数増加 */
                                    }
                                } /* if new_flag==0 */

                                if (rule_count > (Nrulemax - 2))
                                {
                                    break;
                                }
                            } /* if j2<9 */
                        } /* if 興味深いルール条件 */
                    } /* loop_j: 深さループ */

                    if (rule_count > (Nrulemax - 2))
                    {
                        break;
                    }
                } /* k: 処理ノード */

                if (rule_count > (Nrulemax - 2))
                {
                    break;
                }
            } /* individual_id: 個体 */

            if (rule_count > (Nrulemax - 2))
            {
                break;
            }

            /* ================================================================================
               属性使用統計の更新
               適応的突然変異のための重要な処理
               ================================================================================ */

            /* 初期化 */
            total_attribute_usage = 0;
            for (i4 = 0; i4 < Nzk; i4++)
            {
                attribute_usage_count[i4] = 0;
                attribute_tracking[i4] = 0;
            }

            /* 過去5世代分の使用頻度を合計 */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 0; j4 < 5; j4++)
                {
                    attribute_tracking[i4] = attribute_tracking[i4] +
                                             attribute_usage_history[j4][i4];
                    /* 5世代分の累積で長期傾向を把握 */
                }
            }

            /* 履歴配列を1世代分シフト（古い情報を削除） */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                for (j4 = 4; j4 > 0; j4--)
                {
                    attribute_usage_history[j4][i4] = attribute_usage_history[j4 - 1][i4];
                    /* [0]→[1], [1]→[2], ... [3]→[4] */
                }

                /* 現世代をリセット */
                attribute_usage_history[0][i4] = 0;

                if (generation_id % 5 == 0) /* 5世代ごとに */
                {
                    attribute_usage_history[0][i4] = 1;
                    /* 全属性に最小使用頻度を与える
                       完全に使われない属性の復活チャンス */
                }
            }

            /* 全属性の使用頻度合計を計算 */
            for (i4 = 0; i4 < Nzk; i4++)
            {
                total_attribute_usage = total_attribute_usage + attribute_tracking[i4];
                /* 後でルーレット選択に使用 */
            }

            /* 現世代の個体群での属性使用頻度を計算 */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++) /* 判定ノードのみ */
                {
                    attribute_usage_count[(gene_attribute[i4][j4])] =
                        attribute_usage_count[(gene_attribute[i4][j4])] + 1;
                    /* 各属性が何回使われているか */
                }
            }

            /* ================================================================================
               適応度によるランキング計算
               選択の準備
               ================================================================================ */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                for (j4 = 0; j4 < Nkotai; j4++)
                {
                    if (fitness_value[j4] > fitness_value[i4])
                    {
                        fitness_ranking[i4] = fitness_ranking[i4] + 1;
                        /* 自分より良い個体の数 = 順位 */
                    }
                }
                /* fitness_ranking[i4]=0なら最優秀個体 */
            }

            /* ================================================================================
               選択処理: エリート保存型選択（上位1/3を3倍複製）
               多様性を保ちつつ優良個体を増殖
               ================================================================================ */
            for (i4 = 0; i4 < Nkotai; i4++)
            {
                if (fitness_ranking[i4] < 40) /* 上位40個体（120の1/3） */
                {
                    for (j4 = 0; j4 < (Npn + Njg); j4++)
                    {
                        /* 優良個体を3箇所にコピー */
                        /* 位置0-39: オリジナル保存 */
                        gene_attribute[fitness_ranking[i4]][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4]][j4] = node_structure[i4][j4][1];

                        /* 位置40-79: コピー1（後で突然変異1） */
                        gene_attribute[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4] + 40][j4] = node_structure[i4][j4][1];

                        /* 位置80-119: コピー2（後で突然変異2） */
                        gene_attribute[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][0];
                        gene_connection[fitness_ranking[i4] + 80][j4] = node_structure[i4][j4][1];
                    }
                }
            }

            /* ================================================================================
               交叉処理: 一様交叉
               上位個体間で遺伝子を交換し、新しい組み合わせを生成
               ================================================================================ */
            for (i4 = 0; i4 < 20; i4++) /* 上位20個体ペア */
            {
                for (j4 = 0; j4 < Nkousa; j4++) /* 各ペアで20回交叉 */
                {
                    crossover_node1 = rand() % Njg + Npn;
                    /* ランダムに交叉点を選択（判定ノードのみ） */

                    /* 個体i4と個体(i4+20)の遺伝子を交換 */

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

            /* ================================================================================
               突然変異0: 処理ノードの変異（全個体対象）
               探索の起点を変更
               ================================================================================ */
            for (i4 = 0; i4 < 120; i4++)
            {
                for (j4 = 0; j4 < Npn; j4++)
                {
                    mutation_random = rand() % Muratep;
                    if (mutation_random == 0) /* 100%の確率（Muratep=1） */
                    {
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                        /* 処理ノードの接続先を完全ランダム化 */
                    }
                }
            }

            /* ================================================================================
               突然変異1: 判定ノード接続の変異（40-79の個体）
               ネットワーク構造を変更
               ================================================================================ */
            for (i4 = 40; i4 < 80; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    mutation_random = rand() % Muratej;
                    if (mutation_random == 0) /* 約16.7%の確率 */
                    {
                        gene_connection[i4][j4] = rand() % Njg + Npn;
                        /* 接続先をランダムに変更 */
                    }
                }
            }

            /* ================================================================================
               突然変異2: 属性の適応的変異（80-119の個体）
               使用頻度の高い属性を優先的に選択
               ================================================================================ */
            for (i4 = 80; i4 < 120; i4++)
            {
                for (j4 = Npn; j4 < (Npn + Njg); j4++)
                {
                    mutation_random = rand() % Muratea;
                    if (mutation_random == 0) /* 約16.7%の確率 */
                    {
                        /* === ルーレット選択による属性決定 === */
                        mutation_attr1 = rand() % total_attribute_usage;
                        /* 0〜総使用数の範囲で乱数生成 */

                        mutation_attr2 = 0; /* 累積カウンタ */
                        mutation_attr3 = 0; /* 選択属性 */

                        /* 累積和で属性を選択 */
                        for (i5 = 0; i5 < Nzk; i5++)
                        {
                            mutation_attr2 = mutation_attr2 + attribute_tracking[i5];
                            if (mutation_attr1 > mutation_attr2)
                            {
                                mutation_attr3 = i5 + 1;
                                /* 使用頻度が高い属性ほど選ばれやすい */
                            }
                        }
                        gene_attribute[i4][j4] = mutation_attr3;
                    }
                }
            }

            /* ================================================================================
               進捗報告: 5世代ごとに状況を出力
               ================================================================================ */
            if (generation_id % 5 == 0)
            {
                file_report = fopen(filename_report, "a");
                fitness_average_all = 0;

                /* 全個体の平均適応度を計算 */
                for (i4 = 0; i4 < Nkotai; i4++)
                {
                    fitness_average_all = fitness_average_all + fitness_value[i4];
                }
                fitness_average_all = fitness_average_all / Nkotai;

                /* 統計情報をファイルに記録 */
                fprintf(file_report, "%5d\t%5d\t%5d\t%5d\t%9.3f\n",
                        generation_id,                 /* 現世代 */
                        (rule_count - 1),              /* 総ルール数 */
                        (high_support_rule_count - 1), /* 高サポート数 */
                        (low_variance_rule_count - 1), /* 低分散数 */
                        fitness_average_all);          /* 平均適応度 */
                fclose(file_report);

                /* コンソールにも出力 */
                printf("  Gen.=%5d: %5d rules (%5d high-sup, %5d low-var)\n",
                       generation_id, (rule_count - 1),
                       (high_support_rule_count - 1), (low_variance_rule_count - 1));
            }

        } /* generation_id: 世代ループ終了 */

        /* ================================================================================
           試行終了処理
           1試行分の結果をまとめる
           ================================================================================ */

        /* この試行でのルール数を記録 */
        rules_per_trial[trial_index - Nstart] = rule_count - 1;

        printf("\nTrial %d completed:\n", trial_index - Nstart + 1);
        printf("  File: %s\n", filename_rule);
        printf("  Rules: %d (High-support: %d, Low-variance: %d)\n",
               rules_per_trial[trial_index - Nstart],
               (high_support_rule_count - 1), (low_variance_rule_count - 1));

        /* ================================================================================
           ローカル出力: 簡易形式でルールを保存
           ================================================================================ */
        file_local = fopen(filename_local, "w");
        fprintf(file_local, "# Backup for trial %d\n", trial_index);
        fprintf(file_local, "# Format: 8 antecedent attributes + (high_support low_variance) flags\n");
        for (i = 1; i < rule_count; i++)
        {
            fprintf(file_local, "%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t%3d\t( %2d %2d )\n",
                    rule_pool[i].antecedent_attr0, rule_pool[i].antecedent_attr1,
                    rule_pool[i].antecedent_attr2, rule_pool[i].antecedent_attr3,
                    rule_pool[i].antecedent_attr4, rule_pool[i].antecedent_attr5,
                    rule_pool[i].antecedent_attr6, rule_pool[i].antecedent_attr7,
                    rule_pool[i].high_support_flag, rule_pool[i].low_variance_flag);
        }
        fclose(file_local);

        /* ================================================================================
           グローバルルールプールへの統合処理
           全試行を通じた最終結果の構築
           ================================================================================ */

        if (trial_index == Nstart) /* 最初の試行の場合 */
        {
            /* === 新規作成モード === */

            /* 属性名版ルールプール作成 */
            file_pool_attribute = fopen(fpoola, "w");
            fprintf(file_pool_attribute, "# Global Rule Pool (Attribute Names)\n");
            fprintf(file_pool_attribute, "# Attributes(1-8)\tX_mean\tX_sigma\tY_mean\tY_sigma\tSupport\tNegative\tHighSup\tLowVar\n");
            for (i2 = 1; i2 < rule_count; i2++)
            {
                fprintf(file_pool_attribute,
                        "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t" /* 属性名 */
                        "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"     /* 統計量 */
                        "%d\t%d\t%d\t%d\n",                /* カウント */
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

            /* 数値版ルールプール作成 */
            file_pool_numeric = fopen(fpoolb, "w");
            fprintf(file_pool_numeric, "# Global Rule Pool (Numeric)\n");
            for (i2 = 1; i2 < rule_count; i2++)
            {
                fprintf(file_pool_numeric,
                        "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
                        "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"
                        "%d\t%d\t%d\t%d\n",
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

            /* グローバル統計の更新 */
            total_rule_count = total_rule_count + rule_count - 1;
            total_high_support = total_high_support + high_support_rule_count - 1;
            total_low_variance = total_low_variance + low_variance_rule_count - 1;

            printf("Global pool created: %s, %s\n", fpoola, fpoolb);
        }

        if (trial_index > Nstart) /* 2回目以降の試行 */
        {
            /* ================================================================================
               重複チェック処理: 過去の全試行と比較
               O(試行数×ルール数^2)だが、実用上問題なし
               ================================================================================ */

            printf("Checking duplicates with previous trials...\n");

            for (compare_trial_index = Nstart; compare_trial_index < trial_index; compare_trial_index++)
            {
                /* 比較対象ファイル名の生成 */
                compare_digit5 = compare_trial_index / 10000;
                compare_digit4 = compare_trial_index / 1000 - compare_digit5 * 10;
                compare_digit3 = compare_trial_index / 100 - compare_digit5 * 100 - compare_digit4 * 10;
                compare_digit2 = compare_trial_index / 10 - compare_digit5 * 1000 - compare_digit4 * 100 - compare_digit3 * 10;
                compare_digit1 = compare_trial_index - compare_digit5 * 10000 - compare_digit4 * 1000 - compare_digit3 * 100 - compare_digit2 * 10;

                /* ファイルパス再構築 */
                sprintf(filename_rule, "%s/IL%d%d%d%d%d.txt",
                        OUTPUT_DIR_IL,
                        compare_digit5, compare_digit4, compare_digit3, compare_digit2, compare_digit1);

                /* 過去のルールファイルを読込 */
                file_compare = fopen(filename_rule, "r");
                if (file_compare == NULL)
                {
                    printf("Warning: Cannot open comparison file: %s\n", filename_rule);
                    continue;
                }

                for (i7 = 1; i7 < (rules_per_trial[compare_trial_index - Nstart] + 1); i7++)
                {
                    fscanf(file_compare, "%d%d%d%d%d%d%d%d",
                           &read_attributes[0], &read_attributes[1],
                           &read_attributes[2], &read_attributes[3],
                           &read_attributes[4], &read_attributes[5],
                           &read_attributes[6], &read_attributes[7]);

                    /* 比較用配列に格納 */
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

                /* 現在のルールと過去のルールを比較 */
                for (i2 = 1; i2 < rule_count; i2++)
                {
                    for (i7 = 1; i7 < (rules_per_trial[compare_trial_index - Nstart] + 1); i7++)
                    {
                        /* 8属性全て一致するかチェック */
                        if (rule_pool[i2].antecedent_attr0 == compare_rules[i7].antecedent_attr0 &&
                            rule_pool[i2].antecedent_attr1 == compare_rules[i7].antecedent_attr1 &&
                            rule_pool[i2].antecedent_attr2 == compare_rules[i7].antecedent_attr2 &&
                            rule_pool[i2].antecedent_attr3 == compare_rules[i7].antecedent_attr3 &&
                            rule_pool[i2].antecedent_attr4 == compare_rules[i7].antecedent_attr4 &&
                            rule_pool[i2].antecedent_attr5 == compare_rules[i7].antecedent_attr5 &&
                            rule_pool[i2].antecedent_attr6 == compare_rules[i7].antecedent_attr6 &&
                            rule_pool[i2].antecedent_attr7 == compare_rules[i7].antecedent_attr7)
                        {
                            new_rule_marker[i2] = 1; /* 重複マーク */
                        }
                    }
                }

                printf("  Trial %d: %d rules checked\n",
                       compare_trial_index - Nstart + 1,
                       rules_per_trial[compare_trial_index - Nstart]);
            }

            /* 新規ルール数のカウント */
            new_rule_count = 0;
            new_high_support = 0;
            new_low_variance = 0;

            /* === 新規ルールのみをグローバルプールに追加 === */
            for (i2 = 1; i2 < rule_count; i2++)
            {
                if (new_rule_marker[i2] == 0) /* 新規の場合のみ */
                {
                    /* 属性番号を取得 */
                    selected_attributes[0] = attribute_set[rule_pool[i2].antecedent_attr0];
                    selected_attributes[1] = attribute_set[rule_pool[i2].antecedent_attr1];
                    selected_attributes[2] = attribute_set[rule_pool[i2].antecedent_attr2];
                    selected_attributes[3] = attribute_set[rule_pool[i2].antecedent_attr3];
                    selected_attributes[4] = attribute_set[rule_pool[i2].antecedent_attr4];
                    selected_attributes[5] = attribute_set[rule_pool[i2].antecedent_attr5];
                    selected_attributes[6] = attribute_set[rule_pool[i2].antecedent_attr6];
                    selected_attributes[7] = attribute_set[rule_pool[i2].antecedent_attr7];

                    /* 属性名版に追記 */
                    file_pool_attribute = fopen(fpoola, "a");
                    fprintf(file_pool_attribute,
                            "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t"
                            "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"
                            "%d\t%d\t%d\t%d\n",
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

                    /* 数値版に追記 */
                    file_pool_numeric = fopen(fpoolb, "a");
                    fprintf(file_pool_numeric,
                            "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
                            "%8.3f\t%5.3f\t%8.3f\t%5.3f\t"
                            "%d\t%d\t%d\t%d\n",
                            selected_attributes[0], selected_attributes[1],
                            selected_attributes[2], selected_attributes[3],
                            selected_attributes[4], selected_attributes[5],
                            selected_attributes[6], selected_attributes[7],
                            rule_pool[i2].x_mean, rule_pool[i2].x_sigma,
                            rule_pool[i2].y_mean, rule_pool[i2].y_sigma,
                            rule_pool[i2].support_count, rule_pool[i2].negative_count,
                            rule_pool[i2].high_support_flag, rule_pool[i2].low_variance_flag);
                    fclose(file_pool_numeric);

                    /* 新規ルール統計の更新 */
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

            printf("  New rules: %d (High-support: %d, Low-variance: %d)\n",
                   new_rule_count, new_high_support, new_low_variance);

            /* グローバル統計の更新 */
            total_rule_count = total_rule_count + new_rule_count;
            total_high_support = total_high_support + new_high_support;
            total_low_variance = total_low_variance + new_low_variance;
        }

        /* 最終統計の出力 */
        printf("  Cumulative total: %d rules (High-support: %d, Low-variance: %d)\n",
               total_rule_count, total_high_support, total_low_variance);

        /* ================================================================================
           実行統計をドキュメントファイルに記録
           ================================================================================ */
        file_document = fopen(fcont, "a");
        fprintf(file_document, "%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
                (trial_index - Nstart + 1),    /* 試行番号 */
                (rule_count - 1),              /* この試行のルール数 */
                (high_support_rule_count - 1), /* 高サポート数 */
                (low_variance_rule_count - 1), /* 低分散数 */
                total_rule_count,              /* 累積総ルール数 */
                total_high_support,            /* 累積高サポート数 */
                total_low_variance);           /* 累積低分散数 */
        fclose(file_document);

    } /* trial_index: メイン試行ループ終了 */

    /* ================================================================================
       プログラム終了メッセージ
       ================================================================================ */
    printf("\n========================================\n");
    printf("GNMiner Execution Complete!\n");
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