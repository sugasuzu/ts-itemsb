#!/bin/bash

################################################################################
# run_all_stocks.sh
#
# 全銘柄を一括処理するバッチスクリプト
# nikkei225_data/gnminer_individual/ 内の全.txtファイルを処理
################################################################################

# 色付き出力用
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ログファイル
LOG_FILE="batch_run_$(date +%Y%m%d_%H%M%S).log"

echo "=========================================="
echo "  GNMiner Phase 2 - Batch Processing"
echo "=========================================="
echo ""
echo "Log file: $LOG_FILE"
echo ""

# 実行ファイルの確認
if [ ! -f "./main" ]; then
    echo -e "${RED}Error: ./main not found. Please compile first with:${NC}"
    echo "  gcc -o main main.c -lm"
    exit 1
fi

# データディレクトリの確認
DATA_DIR="nikkei225_data/gnminer_individual"
if [ ! -d "$DATA_DIR" ]; then
    echo -e "${RED}Error: Data directory not found: $DATA_DIR${NC}"
    exit 1
fi

# 処理対象ファイルのリストを取得
STOCK_FILES=($DATA_DIR/*.txt)
TOTAL_STOCKS=${#STOCK_FILES[@]}

echo -e "${GREEN}Found $TOTAL_STOCKS stock data files${NC}"
echo ""

# ユーザーに確認
read -p "Process all $TOTAL_STOCKS stocks? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled by user."
    exit 0
fi

echo ""
echo "Starting batch processing..."
echo "Started at: $(date)" | tee -a "$LOG_FILE"
echo "=========================================="
echo ""

# 成功・失敗カウンター
SUCCESS_COUNT=0
FAILED_COUNT=0
FAILED_STOCKS=()

# 開始時刻
START_TIME=$(date +%s)

# 各銘柄を処理
COUNTER=0
for STOCK_FILE in "${STOCK_FILES[@]}"; do
    COUNTER=$((COUNTER + 1))

    # ファイル名から銘柄コードを抽出（拡張子を除く）
    STOCK_CODE=$(basename "$STOCK_FILE" .txt)

    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}[$COUNTER/$TOTAL_STOCKS] Processing: $STOCK_CODE${NC}"
    echo -e "${BLUE}================================================${NC}"

    # 開始時刻を記録
    STOCK_START=$(date +%s)

    # プログラムを実行
    echo "Command: ./main $STOCK_CODE" | tee -a "$LOG_FILE"

    if ./main "$STOCK_CODE" >> "$LOG_FILE" 2>&1; then
        STOCK_END=$(date +%s)
        ELAPSED=$((STOCK_END - STOCK_START))

        echo -e "${GREEN}✓ SUCCESS: $STOCK_CODE (${ELAPSED}s)${NC}" | tee -a "$LOG_FILE"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        STOCK_END=$(date +%s)
        ELAPSED=$((STOCK_END - STOCK_START))

        echo -e "${RED}✗ FAILED: $STOCK_CODE (${ELAPSED}s)${NC}" | tee -a "$LOG_FILE"
        FAILED_COUNT=$((FAILED_COUNT + 1))
        FAILED_STOCKS+=("$STOCK_CODE")
    fi

    echo ""

    # 進捗状況を表示
    PROGRESS=$((COUNTER * 100 / TOTAL_STOCKS))
    echo -e "${YELLOW}Progress: $COUNTER/$TOTAL_STOCKS ($PROGRESS%)${NC}"
    echo -e "${GREEN}Success: $SUCCESS_COUNT${NC} | ${RED}Failed: $FAILED_COUNT${NC}"
    echo ""
done

# 終了時刻
END_TIME=$(date +%s)
TOTAL_ELAPSED=$((END_TIME - START_TIME))

# 結果サマリを出力
echo ""
echo "=========================================="
echo "  Batch Processing Complete"
echo "=========================================="
echo "Finished at: $(date)" | tee -a "$LOG_FILE"
echo "Total time: ${TOTAL_ELAPSED}s ($(($TOTAL_ELAPSED / 60))m $(($TOTAL_ELAPSED % 60))s)" | tee -a "$LOG_FILE"
echo ""
echo "Results:" | tee -a "$LOG_FILE"
echo "  Total stocks: $TOTAL_STOCKS" | tee -a "$LOG_FILE"
echo -e "  ${GREEN}Successful: $SUCCESS_COUNT${NC}" | tee -a "$LOG_FILE"
echo -e "  ${RED}Failed: $FAILED_COUNT${NC}" | tee -a "$LOG_FILE"
echo ""

# 失敗した銘柄をリスト表示
if [ $FAILED_COUNT -gt 0 ]; then
    echo -e "${RED}Failed stocks:${NC}" | tee -a "$LOG_FILE"
    for STOCK in "${FAILED_STOCKS[@]}"; do
        echo "  - $STOCK" | tee -a "$LOG_FILE"
    done
    echo ""
fi

echo "Log saved to: $LOG_FILE"
echo "=========================================="

# 出力ディレクトリ一覧
echo ""
echo "Output directories created:"
ls -d output_* 2>/dev/null | head -10
if [ $(ls -d output_* 2>/dev/null | wc -l) -gt 10 ]; then
    echo "... and $(($(ls -d output_* | wc -l) - 10)) more"
fi
echo ""

exit 0
