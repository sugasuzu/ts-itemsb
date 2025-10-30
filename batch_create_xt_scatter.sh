#!/bin/bash
# X-T散布図一括作成スクリプト
# 全20通貨ペア × 2方向 = 40個の散布図セットを生成

echo "================================================================================"
echo "X-T Scatter Plot Batch Generator"
echo "================================================================================"
echo "Processing 20 currency pairs × 2 directions = 40 plot sets"
echo "This may take 10-15 minutes..."
echo ""

# 処理する通貨ペアリスト
PAIRS=(
  GBPAUD EURAUD USDJPY GBPJPY EURUSD
  AUDJPY NZDJPY GBPNZD EURGBP AUDNZD
  CADJPY CHFJPY EURNZD NZDCAD NZDCHF
  GBPCAD EURCAD EURJPY AUDCAD AUDCHF
)

# カウンター
total=${#PAIRS[@]}
current=0
success=0
failed=0

# 各通貨ペアを処理
for pair in "${PAIRS[@]}"; do
  current=$((current + 1))

  echo "[$current/$total] Processing $pair..."

  # Positive方向
  echo "  → positive..."
  if python3 analysis/fx/rule_scatter_plot_xt.py "$pair" positive > /dev/null 2>&1; then
    echo "    ✓ positive done"
    success=$((success + 1))
  else
    echo "    ✗ positive failed"
    failed=$((failed + 1))
  fi

  # Negative方向
  echo "  → negative..."
  if python3 analysis/fx/rule_scatter_plot_xt.py "$pair" negative > /dev/null 2>&1; then
    echo "    ✓ negative done"
    success=$((success + 1))
  else
    echo "    ✗ negative failed"
    failed=$((failed + 1))
  fi

  echo ""
done

echo "================================================================================"
echo "Batch Processing Complete"
echo "================================================================================"
echo "Total pairs processed: $total"
echo "Total directions:      $((total * 2))"
echo "Success:               $success"
echo "Failed:                $failed"
echo ""
echo "Output directory: analysis/fx/visualizations_xt/"
echo "================================================================================"
