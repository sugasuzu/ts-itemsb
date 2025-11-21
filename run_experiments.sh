#!/bin/bash
################################################################################
# 閾値パラメータ実験自動実行スクリプト（v3.0 - MAX_TIME_DELAY追加）
#
# 4つの閾値の全組み合わせ（3×3×3×3 = 81通り）で実験を実行
# - MAX_TIME_DELAY: 3, 4, 5
# - Minsup: 0.005, 0.0075, 0.01
# - MIN_CONCENTRATION: 0.50, 0.60, 0.70
# - MAX_DEVIATION: 0.50, 0.75, 1.0
#
# 全20通貨ペアに対して実行（make run使用）
################################################################################

set -e  # エラーで停止

# パラメータ設定（更新版 - MAX_TIME_DELAY追加）
DELAY_VALUES=(3 4 5)
MINSUP_VALUES=(0.005 0.0075 0.01)
CONC_VALUES=(0.50 0.60 0.70)
DEV_VALUES=(0.50 0.75 1.0)

# 実験結果保存ディレクトリ
EXP_BASE_DIR="experiments"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
EXP_ROOT="${EXP_BASE_DIR}/${TIMESTAMP}"

# main.cのバックアップ
MAIN_C="main.c"
MAIN_C_BACKUP="${MAIN_C}.backup_${TIMESTAMP}"

echo "========================================"
echo "  閾値パラメータ実験スクリプト v3.0"
echo "========================================"
echo "実験数: 81通り (3×3×3×3)"
echo "通貨ペア: 全20ペア (make run使用)"
echo "結果保存先: ${EXP_ROOT}"
echo ""
echo "パラメータ範囲:"
echo "  MAX_TIME_DELAY:    3, 4, 5"
echo "  Minsup:            0.005, 0.0075, 0.01"
echo "  MIN_CONCENTRATION: 0.50, 0.60, 0.70"
echo "  MAX_DEVIATION:     0.50, 0.75, 1.0"
echo ""

# ディレクトリ作成
mkdir -p "${EXP_ROOT}"

# main.cをバックアップ
cp "${MAIN_C}" "${MAIN_C_BACKUP}"
echo "✓ main.cをバックアップ: ${MAIN_C_BACKUP}"
echo ""

# サマリーファイル初期化
SUMMARY_FILE="${EXP_ROOT}/experiment_summary.csv"
echo "Experiment,MAX_TIME_DELAY,Minsup,MIN_CONCENTRATION,MAX_DEVIATION,Rules_Discovered,Exit_Code,Duration_Sec" > "${SUMMARY_FILE}"

# 実験開始時刻記録
EXPERIMENT_START_TIME=$(date +%s)

# 実験カウンタ
exp_num=0
total_exp=81

# 時間推定用の変数
total_duration=0
avg_duration=0

echo "========================================"
echo "  実験開始: $(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================"
echo ""

# パラメータの全組み合わせをループ（4重ループ）
for delay in "${DELAY_VALUES[@]}"; do
  for minsup in "${MINSUP_VALUES[@]}"; do
    for conc in "${CONC_VALUES[@]}"; do
      for dev in "${DEV_VALUES[@]}"; do
        exp_num=$((exp_num + 1))

        # 実験名
        exp_name="exp$(printf '%03d' ${exp_num})_d${delay}_ms${minsup}_c${conc}_dv${dev}"
        exp_dir="${EXP_ROOT}/${exp_name}"

        # 進捗率計算
        progress_pct=$((exp_num * 100 / total_exp))

        # 残り時間推定
        if [ $exp_num -gt 1 ]; then
          avg_duration=$((total_duration / (exp_num - 1)))
          remaining_exp=$((total_exp - exp_num + 1))
          estimated_remaining=$((avg_duration * remaining_exp))
          estimated_remaining_hour=$((estimated_remaining / 3600))
          estimated_remaining_min=$(((estimated_remaining % 3600) / 60))
          estimated_remaining_sec=$((estimated_remaining % 60))

          if [ $estimated_remaining_hour -gt 0 ]; then
            eta_str="ETA: ${estimated_remaining_hour}時間${estimated_remaining_min}分${estimated_remaining_sec}秒"
          else
            eta_str="ETA: ${estimated_remaining_min}分${estimated_remaining_sec}秒"
          fi
        else
          eta_str="ETA: 計算中..."
        fi

        echo "========================================"
        echo "📊 進捗: [${exp_num}/${total_exp}] (${progress_pct}%) | ${eta_str}"
        echo "========================================"
        echo "🧪 実験名: ${exp_name}"
        echo "   MAX_TIME_DELAY:    ${delay}     (過去参照スパン t-${delay} まで)"
        echo "   Minsup:            ${minsup}  (最小サポート率)"
        echo "   MIN_CONCENTRATION: ${conc}  (最小象限集中率)"
        echo "   MAX_DEVIATION:     ${dev}   (最大逸脱率)"
        echo ""

        # 実験ディレクトリ作成
        mkdir -p "${exp_dir}"

        # パラメータをログファイルに記録
        cat > "${exp_dir}/parameters.txt" <<EOF
Experiment: ${exp_name}
Timestamp: $(date)
Progress: ${exp_num}/${total_exp} (${progress_pct}%)
MAX_TIME_DELAY: ${delay}
Minsup: ${minsup}
MIN_CONCENTRATION: ${conc}
MAX_DEVIATION: ${dev}
Currency Pairs: All 20 pairs (via make run)
EOF

        # main.cのdefineを書き換え
        echo "  ⚙️  main.cの閾値を書き換え中..."
        sed -i.tmp \
          -e "s/^#define MAX_TIME_DELAY .*/#define MAX_TIME_DELAY ${delay} \/\/ 過去スパン/" \
          -e "s/^#define Minsup .*/#define Minsup ${minsup}           \/\/ 最小支持度/" \
          -e "s/^#define MIN_CONCENTRATION .*/#define MIN_CONCENTRATION ${conc} \/\/ 最小集中率/" \
          -e "s/^#define MAX_DEVIATION .*/#define MAX_DEVIATION ${dev}      \/\/ 最大逸脱率/" \
          "${MAIN_C}"
        rm -f "${MAIN_C}.tmp"
        echo "  ✓ 閾値書き換え完了"

        # コンパイル
        echo "  🔨 コンパイル中..."
        if ! make clean > /dev/null 2>&1; then
          echo "  ✗ make clean失敗"
          continue
        fi

        if ! make > "${exp_dir}/compile.log" 2>&1; then
          echo "  ✗ コンパイル失敗"
          echo "${exp_name},${delay},${minsup},${conc},${dev},0,COMPILE_ERROR,0" >> "${SUMMARY_FILE}"
          continue
        fi

        echo "  ✓ コンパイル成功"

        # 実行（make run で全20通貨ペアを実行）
        echo "  🚀 実行中: make run (全20通貨ペア)"
        echo "     [進行状況は execution.log に記録されます]"
        start_time=$(date +%s)

        # バックグラウンドで実行し、進捗をリアルタイム表示
        make run > "${exp_dir}/execution.log" 2>&1 &
        make_pid=$!

        # 実行中の進捗を表示
        echo -n "     実行中"
        while kill -0 $make_pid 2>/dev/null; do
          echo -n "."
          sleep 5
        done
        echo ""

        # プロセス終了を待機
        wait $make_pid
        exit_code=$?

        if [ $exit_code -eq 0 ]; then
          echo "  ✓ 実行成功"
        else
          echo "  ✗ 実行失敗 (exit code: ${exit_code})"
        fi

        end_time=$(date +%s)
        duration=$((end_time - start_time))
        total_duration=$((total_duration + duration))

        duration_min=$((duration / 60))
        duration_sec=$((duration % 60))

        # 結果をコピー（全通貨ペア）
        if [ -d "1-deta-enginnering/forex_data_daily/output" ]; then
          echo "  📂 結果をコピー中..."
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
          echo "  ✅ 総発見ルール数: ${rule_count} (全20通貨ペア合計)"
        else
          rule_count=0
          echo "  ⚠️  出力ディレクトリが見つかりません"
        fi

        # サマリーに記録
        echo "${exp_name},${delay},${minsup},${conc},${dev},${rule_count},${exit_code},${duration}" >> "${SUMMARY_FILE}"

        echo "  ⏱️  所要時間: ${duration_min}分${duration_sec}秒"
        echo ""
      done
    done
  done
done

# 実験終了時刻
EXPERIMENT_END_TIME=$(date +%s)
TOTAL_ELAPSED=$((EXPERIMENT_END_TIME - EXPERIMENT_START_TIME))
TOTAL_ELAPSED_HOUR=$((TOTAL_ELAPSED / 3600))
TOTAL_ELAPSED_MIN=$(((TOTAL_ELAPSED % 3600) / 60))
TOTAL_ELAPSED_SEC=$((TOTAL_ELAPSED % 60))

# main.cを元に戻す
echo "========================================"
echo "  全実験完了 🎉"
echo "========================================"
echo ""
if [ $TOTAL_ELAPSED_HOUR -gt 0 ]; then
  echo "⏱️  総実行時間: ${TOTAL_ELAPSED_HOUR}時間${TOTAL_ELAPSED_MIN}分${TOTAL_ELAPSED_SEC}秒"
else
  echo "⏱️  総実行時間: ${TOTAL_ELAPSED_MIN}分${TOTAL_ELAPSED_SEC}秒"
fi
echo "📅 完了時刻: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""
echo "🔧 main.cを元に戻しています..."
cp "${MAIN_C_BACKUP}" "${MAIN_C}"
make clean > /dev/null 2>&1
make > /dev/null 2>&1
echo "✓ main.cを復元しました"
echo ""

# サマリー表示
echo "========================================"
echo "  実験結果サマリー"
echo "========================================"
echo ""
column -t -s',' "${SUMMARY_FILE}" | head -30
if [ $(wc -l < "${SUMMARY_FILE}") -gt 30 ]; then
  echo "..."
  echo "[残り $(($(wc -l < "${SUMMARY_FILE}") - 30)) 行は省略]"
fi
echo ""
echo "📁 詳細結果: ${EXP_ROOT}"
echo "📊 サマリーCSV: ${SUMMARY_FILE}"
echo ""
echo "✅ 全81実験が完了しました"
echo ""
echo "次のステップ:"
echo "  python3 analyze_experiments.py ${EXP_ROOT}"
echo ""
