#!/bin/bash

################################################################################
# run_stock.sh
#
# 単一銘柄を処理する簡易スクリプト
# 使用例: ./run_stock.sh 7203
################################################################################

# 色付き出力用
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 引数チェック
if [ $# -eq 0 ]; then
    echo "Usage: $0 <stock_code>"
    echo ""
    echo "Examples:"
    echo "  $0 7203      # Toyota"
    echo "  $0 9984      # SoftBank"
    echo ""
    echo "Available stocks:"
    ls nikkei225_data/gnminer_individual/*.txt 2>/dev/null | head -10 | xargs -n 1 basename | sed 's/\.txt$/  /'
    if [ $(ls nikkei225_data/gnminer_individual/*.txt 2>/dev/null | wc -l) -gt 10 ]; then
        echo "  ... and $(($(ls nikkei225_data/gnminer_individual/*.txt 2>/dev/null | wc -l) - 10)) more"
    fi
    exit 1
fi

STOCK_CODE="$1"

# 実行ファイルの確認
if [ ! -f "./main" ]; then
    echo -e "${RED}Error: ./main not found. Please compile first with:${NC}"
    echo "  gcc -o main main.c -lm"
    exit 1
fi

# データファイルの確認
DATA_FILE="nikkei225_data/gnminer_individual/${STOCK_CODE}.txt"
if [ ! -f "$DATA_FILE" ]; then
    echo -e "${RED}Error: Data file not found: $DATA_FILE${NC}"
    echo ""
    echo "Please check if the stock code is correct."
    exit 1
fi

echo -e "${BLUE}=========================================="
echo "  GNMiner Phase 2 - Single Stock"
echo "==========================================${NC}"
echo ""
echo "Stock Code: $STOCK_CODE"
echo "Data File:  $DATA_FILE"
echo "Output Dir: output_${STOCK_CODE}/"
echo ""

# 開始時刻
START_TIME=$(date +%s)

echo -e "${GREEN}Starting analysis...${NC}"
echo ""

# プログラムを実行
if ./main "$STOCK_CODE"; then
    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))

    echo ""
    echo -e "${GREEN}=========================================="
    echo "  ✓ Analysis Complete"
    echo "==========================================${NC}"
    echo "Elapsed time: ${ELAPSED}s ($(($ELAPSED / 60))m $(($ELAPSED % 60))s)"
    echo ""
    echo "Results saved to: output_${STOCK_CODE}/"
    echo "  - Rule lists:      output_${STOCK_CODE}/IL/"
    echo "  - Analysis:        output_${STOCK_CODE}/IA/"
    echo "  - Backups:         output_${STOCK_CODE}/IB/"
    echo "  - Rule pools:      output_${STOCK_CODE}/pool/"
    echo "  - Documentation:   output_${STOCK_CODE}/doc/"
    echo "  - Visualization:   output_${STOCK_CODE}/vis/"
    echo ""

    exit 0
else
    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))

    echo ""
    echo -e "${RED}=========================================="
    echo "  ✗ Analysis Failed"
    echo "==========================================${NC}"
    echo "Elapsed time: ${ELAPSED}s"
    echo ""

    exit 1
fi
