#!/bin/bash
################################################################################
# 閾値パラメータ実験自動実行スクリプト
#
# 3つの閾値の全組み合わせ（3×3×3 = 27通り）で実験を実行
# - Minsup: 0.003, 0.004, 0.005
# - MIN_CONCENTRATION: 0.30, 0.40, 0.50
# - MAX_DEVIATION: 0.50, 0.75, 1.0
#
# 全20通貨ペアに対して実行（make run使用）
################################################################################

set -e  # エラーで停止

# パラメータ設定
MINSUP_VALUES=(0.003 0.004 0.005)
CONC_VALUES=(0.30 0.40 0.50)
DEV_VALUES=(0.50 0.75 1.0)

# 実験結果保存ディレクトリ
EXP_BASE_DIR="experiments"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
EXP_ROOT="${EXP_BASE_DIR}/${TIMESTAMP}"

# main.cのバックアップ
MAIN_C="main.c"
MAIN_C_BACKUP="${MAIN_C}.backup_${TIMESTAMP}"

echo "========================================"
echo "  閾値パラメータ実験スクリプト"
echo "========================================"
echo "実験数: 27通り (3×3×3)"
echo "通貨ペア: 全20ペア (make run使用)"
echo "結果保存先: ${EXP_ROOT}"
echo ""

# ディレクトリ作成
mkdir -p "${EXP_ROOT}"

# main.cをバックアップ
cp "${MAIN_C}" "${MAIN_C_BACKUP}"
echo "✓ main.cをバックアップ: ${MAIN_C_BACKUP}"
echo ""

# サマリーファイル初期化
SUMMARY_FILE="${EXP_ROOT}/experiment_summary.csv"
echo "Experiment,Minsup,MIN_CONCENTRATION,MAX_DEVIATION,Rules_Discovered,Exit_Code,Duration_Sec" > "${SUMMARY_FILE}"

# 実験カウンタ
exp_num=0
total_exp=27

# パラメータの全組み合わせをループ
for minsup in "${MINSUP_VALUES[@]}"; do
  for conc in "${CONC_VALUES[@]}"; do
    for dev in "${DEV_VALUES[@]}"; do
      exp_num=$((exp_num + 1))

      # 実験名
      exp_name="exp$(printf '%02d' ${exp_num})_ms${minsup}_c${conc}_d${dev}"
      exp_dir="${EXP_ROOT}/${exp_name}"

      echo "========================================"
      echo "[${exp_num}/${total_exp}] ${exp_name}"
      echo "========================================"
      echo "  Minsup:            ${minsup}"
      echo "  MIN_CONCENTRATION: ${conc}"
      echo "  MAX_DEVIATION:     ${dev}"
      echo ""

      # 実験ディレクトリ作成
      mkdir -p "${exp_dir}"

      # パラメータをログファイルに記録
      cat > "${exp_dir}/parameters.txt" <<EOF
Experiment: ${exp_name}
Timestamp: $(date)
Minsup: ${minsup}
MIN_CONCENTRATION: ${conc}
MAX_DEVIATION: ${dev}
Currency Pairs: All 20 pairs (via make run)
EOF

      # main.cのdefineを書き換え
      echo "  → main.cの閾値を書き換え中..."
      sed -i.tmp \
        -e "s/^#define Minsup .*/#define Minsup ${minsup}           \/\/ 最小支持度/" \
        -e "s/^#define MIN_CONCENTRATION .*/#define MIN_CONCENTRATION ${conc} \/\/ 最小集中率/" \
        -e "s/^#define MAX_DEVIATION .*/#define MAX_DEVIATION ${dev}      \/\/ 最大逸脱率/" \
        "${MAIN_C}"
      rm -f "${MAIN_C}.tmp"

      # コンパイル
      echo "  → コンパイル中..."
      if ! make clean > /dev/null 2>&1; then
        echo "  ✗ make clean失敗"
        continue
      fi

      if ! make > "${exp_dir}/compile.log" 2>&1; then
        echo "  ✗ コンパイル失敗"
        echo "${exp_name},${minsup},${conc},${dev},0,COMPILE_ERROR,0" >> "${SUMMARY_FILE}"
        continue
      fi

      echo "  ✓ コンパイル成功"

      # 実行（make run で全20通貨ペアを実行）
      echo "  → 実行中: make run (全20通貨ペア)"
      start_time=$(date +%s)

      if make run > "${exp_dir}/execution.log" 2>&1; then
        exit_code=0
        echo "  ✓ 実行成功"
      else
        exit_code=$?
        echo "  ✗ 実行失敗 (exit code: ${exit_code})"
      fi

      end_time=$(date +%s)
      duration=$((end_time - start_time))

      # 結果をコピー（全通貨ペア）
      if [ -d "1-deta-enginnering/forex_data_daily/output" ]; then
        echo "  → 結果をコピー中..."
        cp -r "1-deta-enginnering/forex_data_daily/output" "${exp_dir}/"

        # 全通貨ペアのルール数をカウント
        rule_count=0
        for pair_dir in "${exp_dir}/output/"*/; do
          if [ -f "${pair_dir}pool/zrp01a.txt" ]; then
            # ヘッダー行を除いてカウント
            pair_rules=$(($(wc -l < "${pair_dir}pool/zrp01a.txt") - 1))
            rule_count=$((rule_count + pair_rules))
          fi
        done
        echo "  ✓ 総発見ルール数: ${rule_count} (全20通貨ペア合計)"
      else
        rule_count=0
        echo "  ! 出力ディレクトリが見つかりません"
      fi

      # サマリーに記録
      echo "${exp_name},${minsup},${conc},${dev},${rule_count},${exit_code},${duration}" >> "${SUMMARY_FILE}"

      echo "  完了 (所要時間: ${duration}秒)"
      echo ""
    done
  done
done

# main.cを元に戻す
echo "========================================"
echo "全実験完了"
echo "========================================"
echo ""
echo "main.cを元に戻しています..."
cp "${MAIN_C_BACKUP}" "${MAIN_C}"
make clean > /dev/null 2>&1
make > /dev/null 2>&1
echo "✓ main.cを復元しました"
echo ""

# サマリー表示
echo "========================================"
echo "実験結果サマリー"
echo "========================================"
echo ""
column -t -s',' "${SUMMARY_FILE}"
echo ""
echo "詳細結果: ${EXP_ROOT}"
echo "サマリーCSV: ${SUMMARY_FILE}"
echo ""
echo "✓ 全27実験が完了しました"
