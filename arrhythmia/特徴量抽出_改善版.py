import wfdb
import numpy as np
import pandas as pd
from scipy.signal import find_peaks
from scipy.stats import skew, kurtosis
from datetime import datetime, timedelta

"""
MIT-BIH心電図データから形態学的・時間的特徴量を抽出（改善版）

【重要な改善点】
1. 波形の形態学的特徴を追加（QRS幅、R波高さ、波形の歪みなど）
2. R波周辺の波形から統計量を計算
3. 医学的に重要な特徴を網羅
4. 1行 = 1心拍 として適切にデータ化
"""

def extract_waveform_segment(ecg_signal, r_peak, fs=360, window_before=0.2, window_after=0.4):
    """
    R波周辺の波形セグメントを抽出

    Args:
        ecg_signal: 心電図信号
        r_peak: R波の位置（サンプル番号）
        fs: サンプリング周波数
        window_before: R波前の窓幅（秒）
        window_after: R波後の窓幅（秒）

    Returns:
        segment: 抽出した波形セグメント
        segment_indices: セグメントのインデックス範囲
    """
    before_samples = int(window_before * fs)
    after_samples = int(window_after * fs)

    start = max(0, r_peak - before_samples)
    end = min(len(ecg_signal), r_peak + after_samples)

    segment = ecg_signal[start:end]
    return segment, (start, end)


def calculate_morphological_features(ecg_signal, r_peak, fs=360):
    """
    波形の形態学的特徴量を計算

    Args:
        ecg_signal: 心電図信号
        r_peak: R波の位置
        fs: サンプリング周波数

    Returns:
        features: 形態学的特徴量の辞書
    """
    features = {}

    # === 1. R波周辺の波形セグメントを抽出 ===
    segment, (start, end) = extract_waveform_segment(ecg_signal, r_peak, fs)

    # R波の位置（セグメント内）
    r_pos_in_segment = r_peak - start

    # === 2. R波の振幅（高さ） ===
    features['R_amplitude'] = ecg_signal[r_peak]

    # === 3. QRS複合波の検出と幅の計算 ===
    # QRS開始点を探す（R波の前で振幅が小さくなる点）
    qrs_search_start = max(0, r_pos_in_segment - int(0.08 * fs))  # R波の80ms前から探索
    qrs_pre_segment = segment[qrs_search_start:r_pos_in_segment]

    baseline = np.median(segment)
    threshold = 0.15 * (features['R_amplitude'] - baseline)

    # QRS開始点（R波より前で閾値を下回る最後の点）
    qrs_start_candidates = np.where(np.abs(qrs_pre_segment - baseline) < threshold)[0]
    if len(qrs_start_candidates) > 0:
        qrs_start = qrs_search_start + qrs_start_candidates[-1]
    else:
        qrs_start = qrs_search_start

    # QRS終了点を探す（R波の後で振幅が小さくなる点）
    qrs_search_end = min(len(segment), r_pos_in_segment + int(0.12 * fs))  # R波の120ms後まで探索
    qrs_post_segment = segment[r_pos_in_segment:qrs_search_end]

    qrs_end_candidates = np.where(np.abs(qrs_post_segment - baseline) < threshold)[0]
    if len(qrs_end_candidates) > 0:
        qrs_end = r_pos_in_segment + qrs_end_candidates[0]
    else:
        qrs_end = qrs_search_end

    # QRS幅（秒）
    features['QRS_duration'] = (qrs_end - qrs_start) / fs

    # === 4. Q波とS波の検出 ===
    # Q波（R波の前の谷）
    q_search_region = segment[qrs_start:r_pos_in_segment]
    if len(q_search_region) > 0:
        q_depth_idx = np.argmin(q_search_region)
        features['Q_depth'] = baseline - q_search_region[q_depth_idx]
    else:
        features['Q_depth'] = 0.0

    # S波（R波の後の谷）
    s_search_region = segment[r_pos_in_segment:qrs_end]
    if len(s_search_region) > 0:
        s_depth_idx = np.argmin(s_search_region)
        features['S_depth'] = baseline - s_search_region[s_depth_idx]
    else:
        features['S_depth'] = 0.0

    # === 5. 波形の統計量（R波周辺 ±100ms） ===
    stat_window = int(0.1 * fs)  # 100ms
    stat_start = max(0, r_pos_in_segment - stat_window)
    stat_end = min(len(segment), r_pos_in_segment + stat_window)
    stat_segment = segment[stat_start:stat_end]

    features['waveform_mean'] = np.mean(stat_segment)
    features['waveform_std'] = np.std(stat_segment)
    features['waveform_skewness'] = skew(stat_segment)
    features['waveform_kurtosis'] = kurtosis(stat_segment)

    # === 6. QRS複合波のエネルギー ===
    qrs_segment = segment[qrs_start:qrs_end]
    features['QRS_energy'] = np.sum(qrs_segment ** 2)

    # === 7. R波の鋭さ（2次微分の最大値） ===
    if len(segment) > 2:
        second_derivative = np.diff(segment, n=2)
        features['R_sharpness'] = np.max(np.abs(second_derivative))
    else:
        features['R_sharpness'] = 0.0

    return features


def detect_r_peaks(ecg_signal, fs=360):
    """R波（心拍のピーク）を検出"""
    min_distance = int(0.4 * fs)
    threshold = np.percentile(ecg_signal, 90)
    r_peaks, _ = find_peaks(ecg_signal, height=threshold, distance=min_distance)
    return r_peaks


def calculate_rr_features(r_peaks, current_idx, fs=360, history_window=10):
    """
    RR間隔関連の時間的特徴量を計算

    Args:
        r_peaks: 全R波の位置
        current_idx: 現在の心拍のインデックス
        fs: サンプリング周波数
        history_window: 過去何心拍分の統計を取るか

    Returns:
        features: 時間的特徴量の辞書
    """
    features = {}

    # 現在のRR間隔
    if current_idx > 0:
        rr_current = (r_peaks[current_idx] - r_peaks[current_idx - 1]) / fs
    else:
        rr_current = 1.0
    features['RR_current'] = rr_current

    # 前のRR間隔
    if current_idx > 1:
        rr_previous = (r_peaks[current_idx - 1] - r_peaks[current_idx - 2]) / fs
    else:
        rr_previous = 1.0
    features['RR_previous'] = rr_previous

    # 次のRR間隔（存在する場合）
    if current_idx < len(r_peaks) - 1:
        rr_next = (r_peaks[current_idx + 1] - r_peaks[current_idx]) / fs
    else:
        rr_next = 1.0
    features['RR_next'] = rr_next

    # === 局所的なRR統計（過去N心拍） ===
    start_idx = max(0, current_idx - history_window)
    if current_idx > start_idx:
        rr_intervals = []
        for i in range(start_idx + 1, current_idx + 1):
            rr = (r_peaks[i] - r_peaks[i - 1]) / fs
            rr_intervals.append(rr)

        if len(rr_intervals) > 0:
            features['RR_local_mean'] = np.mean(rr_intervals)
            features['RR_local_std'] = np.std(rr_intervals)

            # RR間隔の比率（重要！）
            features['RR_ratio'] = rr_current / features['RR_local_mean'] if features['RR_local_mean'] > 0 else 1.0
        else:
            features['RR_local_mean'] = rr_current
            features['RR_local_std'] = 0.0
            features['RR_ratio'] = 1.0
    else:
        features['RR_local_mean'] = rr_current
        features['RR_local_std'] = 0.0
        features['RR_ratio'] = 1.0

    # RR間隔の変化率
    features['RR_change'] = abs(rr_current - rr_previous) / rr_previous if rr_previous > 0 else 0.0

    # 心拍数（bpm）
    features['heart_rate'] = 60.0 / rr_current if rr_current > 0 else 60.0

    return features


def get_anomaly_label(annotation_symbol):
    """アノテーションラベルから異常度スコア（X値）を付与"""
    label_map = {
        'N': 0, 'L': 1, 'R': 1,
        'A': 3, 'a': 3, 'J': 2, 'S': 2,
        'V': 7, 'F': 8, '[': 4, '!': 9, ']': 4,
        'e': 5, 'j': 4, 'E': 6,
        '/': 1, 'f': 2, 'x': 5, 'Q': 8, '|': 0,
    }
    return label_map.get(annotation_symbol, 0)


def extract_comprehensive_features(record_path, max_beats=2000):
    """
    包括的な特徴量抽出（形態学的 + 時間的）

    Args:
        record_path: レコードファイルのパス
        max_beats: 処理する最大心拍数

    Returns:
        df: 特徴量DataFrame（1行 = 1心拍）
    """
    print(f"Loading record: {record_path}")

    # データ読み込み
    signals, fields = wfdb.rdsamp(record_path)
    annotation = wfdb.rdann(record_path, 'atr')

    fs = fields['fs']
    ecg_signal = signals[:, 0]  # 第1チャネル（MLII）

    print(f"Sampling rate: {fs} Hz")
    print(f"Signal length: {len(ecg_signal)} samples ({len(ecg_signal)/fs:.1f} seconds)")
    print(f"Total annotations: {len(annotation.sample)}")

    # R波検出
    print("Detecting R-peaks...")
    r_peaks = detect_r_peaks(ecg_signal, fs)
    print(f"Detected {len(r_peaks)} R-peaks")

    # アノテーションとR波の対応付け
    annotation_indices = []
    for ann_sample in annotation.sample[:max_beats]:
        distances = np.abs(r_peaks - ann_sample)
        closest_r_peak_idx = np.argmin(distances)
        if distances[closest_r_peak_idx] > 0.1 * fs:
            continue
        annotation_indices.append(closest_r_peak_idx)

    print(f"Processing {len(annotation_indices)} beats...")

    # 特徴量リスト
    features_list = []

    for i, r_peak_idx in enumerate(annotation_indices):
        if i >= len(annotation.symbol):
            break

        r_peak = r_peaks[r_peak_idx]

        # === 形態学的特徴量 ===
        morph_features = calculate_morphological_features(ecg_signal, r_peak, fs)

        # === 時間的特徴量 ===
        rr_features = calculate_rr_features(r_peaks, r_peak_idx, fs)

        # === アノテーションラベル ===
        annotation_label = annotation.symbol[i]
        anomaly_score = get_anomaly_label(annotation_label)

        # === タイムスタンプ（仮想） ===
        timestamp = datetime(2024, 1, 1) + timedelta(seconds=r_peak/fs)

        # 全特徴量を統合
        feature_dict = {
            'beat_number': i,
            'sample_index': r_peak,
            'timestamp': timestamp.strftime('%Y-%m-%d %H:%M:%S'),
            'annotation': annotation_label,
            'X': anomaly_score,
        }
        feature_dict.update(morph_features)
        feature_dict.update(rr_features)

        features_list.append(feature_dict)

    df = pd.DataFrame(features_list)

    print(f"\nExtracted {len(df)} heartbeats with comprehensive features")
    print(f"\nAnomaly distribution:")
    print(df['annotation'].value_counts())

    return df


def binarize_comprehensive_features(df):
    """
    包括的特徴量を二値化（GNP用）
    """
    df_binary = df.copy()

    # === 時間的特徴量の二値化 ===
    df_binary['RR_long'] = (df['RR_current'] > 1.2).astype(int)
    df_binary['RR_short'] = (df['RR_current'] < 0.6).astype(int)
    df_binary['RR_unstable'] = (df['RR_change'] > 0.2).astype(int)
    df_binary['RR_ratio_high'] = (df['RR_ratio'] > 1.15).astype(int)  # 重要！
    df_binary['RR_ratio_low'] = (df['RR_ratio'] < 0.85).astype(int)
    df_binary['HR_high'] = (df['heart_rate'] > 100).astype(int)
    df_binary['HR_low'] = (df['heart_rate'] < 60).astype(int)

    # === 形態学的特徴量の二値化 ===
    df_binary['QRS_wide'] = (df['QRS_duration'] > 0.12).astype(int)
    df_binary['QRS_narrow'] = (df['QRS_duration'] < 0.06).astype(int)
    df_binary['R_amp_high'] = (df['R_amplitude'] > df['R_amplitude'].quantile(0.75)).astype(int)
    df_binary['R_amp_low'] = (df['R_amplitude'] < df['R_amplitude'].quantile(0.25)).astype(int)
    df_binary['Q_deep'] = (df['Q_depth'] > df['Q_depth'].quantile(0.75)).astype(int)
    df_binary['S_deep'] = (df['S_depth'] > df['S_depth'].quantile(0.75)).astype(int)

    # 波形の統計量
    df_binary['waveform_irregular'] = (df['waveform_std'] > df['waveform_std'].quantile(0.75)).astype(int)
    df_binary['waveform_asymmetric'] = (np.abs(df['waveform_skewness']) > 0.5).astype(int)

    # X値（異常度）
    df_binary['X'] = df['X'].astype(int)

    # GNP用の列を選択
    gnp_cols = ['timestamp',
                'RR_long', 'RR_short', 'RR_unstable', 'RR_ratio_high', 'RR_ratio_low',
                'HR_high', 'HR_low',
                'QRS_wide', 'QRS_narrow',
                'R_amp_high', 'R_amp_low',
                'Q_deep', 'S_deep',
                'waveform_irregular', 'waveform_asymmetric',
                'X']

    return df_binary[gnp_cols]


def main():
    record_path = '../arrhythmia-data/100'
    output_dir = '../arrhythmia-data'

    print("=" * 70)
    print("MIT-BIH心電図データ → 包括的特徴量抽出（改善版）")
    print("=" * 70)

    # 1. 包括的特徴量抽出
    df_features = extract_comprehensive_features(record_path, max_beats=2000)

    # 連続値の特徴量を保存
    output_features = f'{output_dir}/ecg_features_100_v2.csv'
    df_features.to_csv(output_features, index=False)
    print(f"\n✓ Continuous features saved: {output_features}")

    # 2. 二値化
    print("\nBinarizing features...")
    df_binary = binarize_comprehensive_features(df_features)

    # 3. GNP形式で保存
    output_gnp = f'{output_dir}/ecg_gnp_100_v2.csv'
    df_binary.to_csv(output_gnp, index=False)
    print(f"✓ GNP format saved: {output_gnp}")

    # 4. データサマリ
    print("\n" + "=" * 70)
    print("データサマリ")
    print("=" * 70)
    print(f"総心拍数: {len(df_binary)}")
    print(f"属性数: {len(df_binary.columns) - 2}")  # timestamp, X除く

    print(f"\n異常度分布:")
    print(df_binary['X'].value_counts().sort_index())

    print(f"\n【重要な改善点】")
    print("✅ 波形の形態学的特徴を追加（QRS幅、Q/S波深さ、波形統計量）")
    print("✅ RR_ratio（リズムの乱れ）を追加 - 異常検知で最重要")
    print("✅ 波形の歪み・非対称性を定量化")
    print("✅ 1行 = 1心拍 として医学的に意味のあるデータ構造")

    print(f"\n属性の発生頻度:")
    attr_cols = [col for col in df_binary.columns if col not in ['timestamp', 'X']]
    for col in attr_cols:
        freq = df_binary[col].sum()
        pct = 100 * freq / len(df_binary)
        print(f"  {col:20s}: {freq:4d} ({pct:5.1f}%)")

    print("\n" + "=" * 70)
    print("変換完了！")
    print("=" * 70)
    print(f"\nGNP入力ファイル: {output_gnp}")
    print("\n【期待される発見ルール例】")
    print("IF RR_ratio_high=1 (t-2) AND    # 2心拍前にリズムが乱れ")
    print("   QRS_wide=1 (t-1) AND         # 1心拍前にQRS延長")
    print("   waveform_irregular=1 (t)     # 現在波形が不規則")
    print("THEN X(t+1) = 7                 # 次の心拍で心室性期外収縮")


if __name__ == '__main__':
    main()
