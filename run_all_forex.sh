#!/bin/bash

# ================================================================================
# 全為替ペア自動処理スクリプト
# ================================================================================
#
# このスクリプトは20通貨ペアを個別に順次処理します
# 各通貨ペアごとに独立した出力ディレクトリが作成されます
#
# 使用方法:
#   bash run_all_forex.sh
#
# ================================================================================

# 色設定
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ログディレクトリの作成
LOG_DIR="logs"
mkdir -p "$LOG_DIR"

# バッチログファイル
BATCH_LOG="$LOG_DIR/batch_$(date +%Y%m%d_%H%M%S).log"

# 為替ペアリスト（main.cと同じ順序）
FOREX_PAIRS=(
    # 対円ペア (7ペア)
    "USDJPY" "EURJPY" "GBPJPY" "AUDJPY" "NZDJPY" "CADJPY" "CHFJPY"
    # 主要クロスペア (6ペア)
    "EURUSD" "GBPUSD" "AUDUSD" "NZDUSD" "USDCAD" "USDCHF"
    # その他の主要ペア (7ペア)
    "EURGBP" "EURAUD" "EURCHF" "GBPAUD" "GBPCAD" "AUDCAD" "AUDNZD"
)

TOTAL_PAIRS=${#FOREX_PAIRS[@]}
SUCCESS_COUNT=0
FAILED_COUNT=0
START_TIME=$(date +%s)

echo ""
echo "========================================================================"
echo "  為替ペア一括処理開始"
echo "========================================================================"
echo "総通貨ペア数: $TOTAL_PAIRS"
echo "ログファイル: $BATCH_LOG"
echo "開始時刻: $(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================================================"
echo ""

# ログファイルにヘッダーを書き込み
{
    echo "========================================================================"
    echo "為替ペア一括処理ログ"
    echo "開始時刻: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "総通貨ペア数: $TOTAL_PAIRS"
    echo "========================================================================"
    echo ""
} > "$BATCH_LOG"

# 各通貨ペアを順次処理
for idx in "${!FOREX_PAIRS[@]}"; do
    PAIR="${FOREX_PAIRS[$idx]}"
    PAIR_NUM=$((idx + 1))

    echo ""
    echo "========================================================================"
    printf "${BLUE}  [%d/%d] 処理中: %s${NC}\n" "$PAIR_NUM" "$TOTAL_PAIRS" "$PAIR"
    echo "========================================================================"

    # ログファイルにも記録
    {
        echo ""
        echo "========================================================================"
        echo "[$PAIR_NUM/$TOTAL_PAIRS] 通貨ペア: $PAIR"
        echo "開始: $(date '+%Y-%m-%d %H:%M:%S')"
        echo "------------------------------------------------------------------------"
    } >> "$BATCH_LOG"

    # 処理開始時刻
    PAIR_START=$(date +%s)

    # データファイルの存在確認（極値データセット）
    DATA_FILE="forex_data/extreme_gnminer/${PAIR}.txt"
    if [ ! -f "$DATA_FILE" ]; then
        printf "${RED}✗ エラー: データファイルが見つかりません: %s${NC}\n" "$DATA_FILE"
        echo "Result: FAILED (データファイル未発見)" >> "$BATCH_LOG"
        FAILED_COUNT=$((FAILED_COUNT + 1))
        continue
    fi

    # 処理実行
    PAIR_LOG="$LOG_DIR/${PAIR}_$(date +%Y%m%d_%H%M%S).log"
    echo "  個別ログ: $PAIR_LOG"

    if ./main "$PAIR" > "$PAIR_LOG" 2>&1; then
        # 処理終了時刻
        PAIR_END=$(date +%s)
        PAIR_ELAPSED=$((PAIR_END - PAIR_START))

        printf "${GREEN}✓ 成功: %s (%.0f秒)${NC}\n" "$PAIR" "$PAIR_ELAPSED"

        # ログに記録
        {
            echo "Result: SUCCESS"
            echo "処理時間: ${PAIR_ELAPSED}秒"
            echo "終了: $(date '+%Y-%m-%d %H:%M:%S')"
        } >> "$BATCH_LOG"

        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        # 処理終了時刻
        PAIR_END=$(date +%s)
        PAIR_ELAPSED=$((PAIR_END - PAIR_START))

        printf "${RED}✗ 失敗: %s (%.0f秒)${NC}\n" "$PAIR" "$PAIR_ELAPSED"

        # ログに記録
        {
            echo "Result: FAILED"
            echo "処理時間: ${PAIR_ELAPSED}秒"
            echo "終了: $(date '+%Y-%m-%d %H:%M:%S')"
        } >> "$BATCH_LOG"

        FAILED_COUNT=$((FAILED_COUNT + 1))
    fi

    # 進捗状況
    echo ""
    echo "------------------------------------------------------------------------"
    printf "進捗: %d/%d (%.1f%%)\n" "$PAIR_NUM" "$TOTAL_PAIRS" "$(echo "scale=1; $PAIR_NUM * 100 / $TOTAL_PAIRS" | bc)"
    printf "成功: ${GREEN}%d${NC} | 失敗: ${RED}%d${NC}\n" "$SUCCESS_COUNT" "$FAILED_COUNT"
    echo "------------------------------------------------------------------------"
done

# 全体の処理時間
END_TIME=$(date +%s)
TOTAL_ELAPSED=$((END_TIME - START_TIME))
TOTAL_MINUTES=$(echo "scale=1; $TOTAL_ELAPSED / 60" | bc)

echo ""
echo "========================================================================"
echo "  処理完了"
echo "========================================================================"
echo "総処理時間: ${TOTAL_ELAPSED}秒 (${TOTAL_MINUTES}分)"
printf "成功: ${GREEN}%d${NC} / %d 通貨ペア\n" "$SUCCESS_COUNT" "$TOTAL_PAIRS"
printf "失敗: ${RED}%d${NC} / %d 通貨ペア\n" "$FAILED_COUNT" "$TOTAL_PAIRS"
echo "成功率: $(echo "scale=1; $SUCCESS_COUNT * 100 / $TOTAL_PAIRS" | bc)%"
echo "バッチログ: $BATCH_LOG"
echo "========================================================================"
echo ""

# ログファイルにサマリーを追記
{
    echo ""
    echo "========================================================================"
    echo "処理完了サマリー"
    echo "========================================================================"
    echo "終了時刻: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "総処理時間: ${TOTAL_ELAPSED}秒 (${TOTAL_MINUTES}分)"
    echo "成功: $SUCCESS_COUNT / $TOTAL_PAIRS 通貨ペア"
    echo "失敗: $FAILED_COUNT / $TOTAL_PAIRS 通貨ペア"
    echo "成功率: $(echo "scale=1; $SUCCESS_COUNT * 100 / $TOTAL_PAIRS" | bc)%"
    echo "========================================================================"
} >> "$BATCH_LOG"

# 終了コード
if [ "$FAILED_COUNT" -eq 0 ]; then
    exit 0
else
    exit 1
fi
