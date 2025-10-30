#!/bin/bash
# Batch X-T Scatter Plot Generation Script
# Generates X-T scatter plots for all currency pairs and directions

echo "=================================="
echo "Batch X-T Scatter Plot Generation"
echo "=================================="
echo ""

# Currency pairs
PAIRS=(
    "AUDCAD" "AUDJPY" "AUDNZD" "AUDUSD"
    "CADJPY" "CHFJPY"
    "EURAUD" "EURCHF" "EURGBP" "EURJPY" "EURUSD"
    "GBPAUD" "GBPCAD" "GBPJPY" "GBPUSD"
    "NZDJPY" "NZDUSD"
    "USDCAD" "USDCHF" "USDJPY"
)

# Directions
DIRECTIONS=("positive" "negative")

# Parameters
TOP_RULES=10
MIN_SAMPLES=5
SORT_BY="support"

total_pairs=$((${#PAIRS[@]} * ${#DIRECTIONS[@]}))
current=0
success=0
failed=0

echo "Total combinations to process: $total_pairs"
echo "Top rules per combination: $TOP_RULES"
echo "Sort by: $SORT_BY"
echo "Minimum samples: $MIN_SAMPLES"
echo ""
echo "=================================="
echo ""

for pair in "${PAIRS[@]}"; do
    for direction in "${DIRECTIONS[@]}"; do
        current=$((current + 1))

        echo "[$current/$total_pairs] Processing: $pair $direction"

        # Check if rule file exists
        rule_file="output/$pair/$direction/pool/zrp01a.txt"
        if [ ! -f "$rule_file" ]; then
            echo "  ⚠️  Rule file not found: $rule_file"
            failed=$((failed + 1))
            echo ""
            continue
        fi

        # Run the script
        python3 analysis/fx/rule_scatter_plot_xt.py "$pair" "$direction" \
            --top $TOP_RULES \
            --min-samples $MIN_SAMPLES \
            --sort-by $SORT_BY > /dev/null 2>&1

        if [ $? -eq 0 ]; then
            echo "  ✓ Success"
            success=$((success + 1))
        else
            echo "  ✗ Failed"
            failed=$((failed + 1))
        fi

        echo ""
    done
done

echo "=================================="
echo "Batch Processing Complete"
echo "=================================="
echo "Total processed: $total_pairs"
echo "Success:         $success"
echo "Failed:          $failed"
echo ""
echo "Output directory: analysis/fx/visualizations_xt/"
echo "=================================="
