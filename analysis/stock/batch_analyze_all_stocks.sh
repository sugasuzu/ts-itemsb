#!/bin/bash

# ================================================================================
# 全銘柄ルール統計分析バッチスクリプト
# ================================================================================
#
# 全銘柄のルール発見結果を統計分析します
#
# 使用方法:
#   bash analysis/stock/batch_analyze_all_stocks.sh
#
# ================================================================================

echo "========================================================================"
echo "  Stock Rule Statistics Analysis - Batch Processing"
echo "========================================================================"
echo ""

# スクリプトのディレクトリを取得
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BASE_DIR="$( cd "$SCRIPT_DIR/../.." && pwd )"

cd "$BASE_DIR"

echo "Base directory: $BASE_DIR"
echo "Running analysis script..."
echo ""

# Pythonスクリプトを実行（全銘柄）
python3 analysis/stock/rule_statistics_analysis.py > analysis/stock/analysis_report_all.txt 2>&1

echo ""
echo "========================================================================"
echo "  Analysis Complete"
echo "========================================================================"
echo ""
echo "Report saved to: analysis/stock/analysis_report_all.txt"
echo ""
