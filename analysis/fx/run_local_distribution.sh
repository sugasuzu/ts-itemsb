#!/bin/bash
# ================================================================================
# 局所分布分析実行スクリプト
# ================================================================================
#
# 使用方法:
#   bash run_local_distribution.sh [通貨ペア]
#
# 例:
#   bash run_local_distribution.sh USDJPY
#   bash run_local_distribution.sh  # 全通貨ペア
#
# ================================================================================

# 色設定
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo ""
echo "========================================================================"
echo "  局所分布分析 - Local Distribution Analysis"
echo "========================================================================"
echo ""

# Pythonスクリプトのパス
SCRIPT_PATH="local_distribution_analysis.py"

# 通貨ペアが指定されている場合
if [ $# -gt 0 ]; then
    FOREX_PAIR=$1
    echo -e "${BLUE}Analyzing single forex pair: ${FOREX_PAIR}${NC}"
    echo ""

    python3 "$SCRIPT_PATH" "$FOREX_PAIR"

else
    # 全通貨ペア
    echo -e "${BLUE}Analyzing all available forex pairs${NC}"
    echo ""

    python3 "$SCRIPT_PATH"
fi

# 実行結果の確認
if [ $? -eq 0 ]; then
    echo ""
    echo "========================================================================"
    echo -e "${GREEN}✓ Analysis completed successfully${NC}"
    echo "========================================================================"
    echo ""
    echo "Output files are saved in:"
    echo "  - analysis/fx/<PAIR>/local_distribution_rule_*.png"
    echo "  - analysis/fx/<PAIR>/local_distribution_summary.csv"
    echo "  - analysis/fx/<PAIR>/local_distribution_summary_viz.png"
    echo ""
else
    echo ""
    echo "========================================================================"
    echo -e "${RED}✗ Analysis failed${NC}"
    echo "========================================================================"
    echo ""
fi
