#!/bin/bash

# ================================================================================
# 全銘柄可視化バッチスクリプト
# ================================================================================
#
# このスクリプトは全銘柄の上位ルールを可視化します
#
# 使用方法:
#   bash analysis/stock/batch_visualize_all_stocks.sh
#
# ================================================================================

echo "========================================================================"
echo "  Stock Rule Visualization - Batch Processing"
echo "========================================================================"
echo ""

# スクリプトのディレクトリを取得
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BASE_DIR="$( cd "$SCRIPT_DIR/../.." && pwd )"

cd "$BASE_DIR"

echo "Base directory: $BASE_DIR"
echo "Running visualization script..."
echo ""

# Pythonスクリプトを実行
python3 analysis/stock/xt_scatter_actual_data_visualization.py

echo ""
echo "========================================================================"
echo "  Visualization Complete"
echo "========================================================================"
echo ""
echo "Output directory: analysis/stock/visualizations_xt_actual/"
echo ""
