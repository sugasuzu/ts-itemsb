#!/bin/bash
# 現在のパラメータ設定を表示するスクリプト

MAIN_C="../main.c"

if [ ! -f "$MAIN_C" ]; then
    echo "Error: main.c が見つかりません"
    exit 1
fi

echo "================================================================================"
echo "  現在のパラメータ設定（main.c）"
echo "================================================================================"
echo ""

echo "【基本フィルタパラメータ】"
echo "-------------------------------------------"
grep -n "^#define Nrulemax" $MAIN_C
grep -n "^#define Minsup" $MAIN_C
grep -n "^#define Maxsigx" $MAIN_C
grep -n "^#define Min_Mean" $MAIN_C
grep -n "^#define MIN_ATTRIBUTES" $MAIN_C
grep -n "^#define MIN_SUPPORT_COUNT" $MAIN_C
grep -n "^#define MIN_CONCENTRATION" $MAIN_C
echo ""

echo "【適応度関数パラメータ】"
echo "-------------------------------------------"
grep -n "^#define SUPPORT_WEIGHT" $MAIN_C
echo ""

echo "【集中度ボーナス閾値】"
echo "-------------------------------------------"
grep -n "^#define CONCENTRATION_THRESHOLD" $MAIN_C | head -7
echo ""

echo "【集中度ボーナスポイント】"
echo "-------------------------------------------"
grep -n "^#define CONCENTRATION_BONUS" $MAIN_C | head -7
echo ""

echo "================================================================================"
echo ""
echo "パラメータを変更するには:"
echo "  vim main.c +<行番号>"
echo ""
echo "例: MIN_CONCENTRATIONを編集"
LINE_NUM=$(grep -n "^#define MIN_CONCENTRATION" $MAIN_C | cut -d: -f1)
echo "  vim main.c +$LINE_NUM"
echo ""
echo "================================================================================"
