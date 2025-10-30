import wfdb

# 1. 読み込むレコード名（ファイル拡張子は不要）
record_name = '../arrhythmia-data/100'

try:
    # 2. 波形データ(.dat)とヘッダー(.hea)を読み込む
    #    signals: 心電図の波形データ (NumPy配列)
    #    fields:  ヘッダー情報 (辞書)
    signals, fields = wfdb.rdsamp(record_name)

    # 3. アノテーション(.atr)を読み込む
    #    annotation.sample: イベントが発生したサンプル番号
    #    annotation.symbol: イベントのラベル ('N', 'V'など)
    annotation = wfdb.rdann(record_name, 'atr')

    # --- 読み込んだ内容の確認 ---

    print("--- 1. 波形データ (signals) ---")
    print(f"信号の形状 (サンプル数, チャネル数): {signals.shape}")
    print("先頭5サンプルのデータ:\n", signals[:5])

    print("\n--- 2. ヘッダー情報 (fields) ---")
    print(f"サンプリング周波数 (fs): {fields['fs']} Hz")
    print(f"信号チャネル名: {fields['sig_name']}")

    print("\n--- 3. アノテーション (annotation) ---")
    print(f"アノテーションの総数: {len(annotation.sample)}")
    print("先頭5個のラベル:", annotation.symbol[:5])
    print("上記ラベルの発生時間 (サンプル番号):", annotation.sample[:5])

except FileNotFoundError:
    print(f"エラー: '{record_name}.dat'等の3ファイルが見つかりません。")
    print("スクリプトをファイルと同じディレクトリで実行しているか確認してください。")
except Exception as e:
    print(f"エラーが発生しました: {e}")