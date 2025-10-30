#!/bin/bash
# Count X-T scatter plots for each pair and direction

echo "X-T Scatter Plot Summary"
echo "========================"
echo ""

for dir in analysis/fx/visualizations_xt/*/; do
    pair_dir=$(basename "$dir")
    count=$(ls "$dir"*.png 2>/dev/null | wc -l)
    printf "%-25s %3d plots\n" "$pair_dir" "$count"
done | sort

echo ""
echo "========================"
total=$(find analysis/fx/visualizations_xt -name "*.png" | wc -l)
echo "Total: $total plots"
