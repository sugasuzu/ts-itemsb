# 論文準拠サポート率計算の実装方針

**作成日:** 2025-11-13
**対象:** main.c - Inter-transaction Association Rule Mining
**参考論文:** "A New Method for Mining Intertransaction Association Rules" (Example 3.1, 式(3))

---

## 背景

現在の実装では、サポート率の計算が論文の定義と完全に一致していない。各ルールの実際の時間遅延を考慮した正確なサポート率計算を実装する必要がある。

### 論文の定義（式(3)）

```
support'(X) = t'(X) / (N - S_max(X∪Y))
```

**where:**
- `t'(X)`: Xを含むトランザクション数（Y範囲も考慮）
- `N`: 全トランザクション数
- `S_max(X∪Y)`: ルール全体（X∪Y）の最大時間間隔スパン

### Example 3.1からの重要な洞察

論文のExample 3.1より：

> "although A₁=1 in T₄ and A₂=1 in T₅, it is impossible to compose the rule because there is no T₇ in the database"

→ **Xがマッチしても、Yが範囲外なら無効**
→ 分母は常に `N - S_max(X∪Y)` を使用

---

## 現状の問題点

### 問題1: 固定値の使用

```c
// 現在の実装（main.c:2034, 2052, 2095, 1773）
double support_rate = (double)matched_count / (double)(Nrd - FUTURE_SPAN);
```

**問題点:**
- 全ルールで同じ分母（`Nrd - FUTURE_SPAN`）を使用
- 各ルールの実際の時間遅延（max_delay）を無視
- 論文の式(3)と不整合

### 問題2: ルール固有の時間スパンが未考慮

**ルールA:** `USDJPY_Up(t-1) ⇒ X(t+1), X(t+2)`
- max_delay = 1
- S_max(X∪Y) = 1 + 2 = 3

**ルールB:** `GBPJPY_Up(t-5), EURJPY_Down(t-2) ⇒ X(t+1), X(t+2)`
- max_delay = 5
- S_max(X∪Y) = 5 + 2 = 7

**現状:** 両方とも `Nrd - 2` で割っている → 不正確

---

## 論文準拠の正しい計算

### S_max(X∪Y)の定義

本システムでは：

```
X (antecedent): 過去の属性パターン
  - 時間範囲: t-max_delay ~ t-0

Y (consequent): 未来の予測値
  - 時間範囲: t+1 ~ t+FUTURE_SPAN

S_max(X∪Y): ルール全体の時間スパン
  - 開始: t - max_delay
  - 終了: t + FUTURE_SPAN
  - スパン = max_delay + FUTURE_SPAN
```

### 有効レコード数

```
effective_records = N - S_max(X∪Y)
                  = Nrd - (max_delay + FUTURE_SPAN)
```

**理由:**
- 最初の `max_delay` レコード: 過去参照不可
- 最後の `FUTURE_SPAN` レコード: 未来予測不可
- 有効範囲: `[max_delay, Nrd - FUTURE_SPAN)`

---

## 実装仕様

### 1. ヘルパー関数の追加

**場所:** `main.c:1700` 付近（`calculate_support_value`の後）

```c
/**
 * 論文準拠のサポート率計算（式(3)準拠）
 *
 * support'(X) = t'(X) / (N - S_max(X∪Y))
 *
 * S_max(X∪Y) = max(time_delays) + FUTURE_SPAN
 *
 * @param matched_count: マッチ数 t'(X)
 * @param time_delays: 時間遅延配列
 * @param num_attributes: 属性数
 * @return サポート率 support'(X)
 */
static double calculate_accurate_support_rate(int matched_count, int *time_delays, int num_attributes)
{
    int max_delay;
    int S_max_XY;
    int effective_records;
    int i;

    /* ルール固有の最大時間遅延を取得 */
    max_delay = 0;
    for (i = 0; i < num_attributes; i++)
    {
        if (time_delays[i] > max_delay)
        {
            max_delay = time_delays[i];
        }
    }

    /* 論文の式(3): S_max(X∪Y) = max_delay + FUTURE_SPAN */
    S_max_XY = max_delay + FUTURE_SPAN;

    /* 有効レコード数 = N - S_max(X∪Y) */
    effective_records = Nrd - S_max_XY;

    /* サポート率を返す */
    return (double)matched_count / (double)effective_records;
}
```

---

## 修正箇所（4箇所）

### 修正1: check_rule_quality関数

**場所:** `main.c:1771-1773`

**修正前:**
```c
int effective_records = Nrd - FUTURE_SPAN;
double support_rate = (double)actual_matched_count / (double)effective_records;
```

**修正後:**
```c
/* 論文準拠: support'(X) = t'(X) / (N - S_max(X∪Y)) */
double support_rate = calculate_accurate_support_rate(actual_matched_count, time_delays, num_attributes);
```

**理由:** フィルタリング判定で正確なサポート率を使用

---

### 修正2: register_new_rule呼び出し

**場所:** `main.c:2033-2034`

**修正前:**
```c
// サポート率を計算（有効レコード数ベース：未来予測可能な範囲）
double correct_support_rate = (double)matched_count / (double)(Nrd - FUTURE_SPAN);
```

**修正後:**
```c
/* 論文準拠: support'(X) = t'(X) / (N - S_max(X∪Y)) */
double correct_support_rate = calculate_accurate_support_rate(matched_count, time_delay_memo, j2);
```

**理由:** ルールプールに保存するサポート率の正確性

---

### 修正3: フィットネス計算（新規ルール）

**場所:** `main.c:2051-2052`

**修正前:**
```c
// サポート率を計算（論文準拠：ルール固有の最大遅延を考慮）
double support_rate = (double)matched_count / (double)(Nrd - FUTURE_SPAN);
```

**修正後:**
```c
/* 論文準拠: support'(X) = t'(X) / (N - S_max(X∪Y)) */
double support_rate = calculate_accurate_support_rate(matched_count, time_delay_memo, j2);
```

**理由:** 適応度評価の正確性向上

---

### 修正4: フィットネス計算（重複ルール）

**場所:** `main.c:2094-2095`

**修正前:**
```c
// サポート率を計算（未来予測可能なレコード数ベース）
double support_rate = (double)matched_count / (double)(Nrd - FUTURE_SPAN);
```

**修正後:**
```c
/* 論文準拠: support'(X) = t'(X) / (N - S_max(X∪Y)) */
double support_rate = calculate_accurate_support_rate(matched_count, time_delay_memo, j2);
```

**理由:** 新規ルールと重複ルールで同じ計算基準を使用

---

## 実装効果

### 1. 理論的正確性

| ルール例 | max_delay | S_max(X∪Y) | 有効レコード数 | サポート率 |
|----------|-----------|------------|----------------|------------|
| `USDJPY_Up(t-1)` | 1 | 3 | Nrd-3 | matched/4497 |
| `GBPJPY_Up(t-5), EURJPY_Down(t-2)` | 5 | 7 | Nrd-7 | matched/4493 |

**差:** ルールごとに異なる分母 → 論文の式(3)に完全準拠

### 2. 進化への影響

- **時間遅延が小さいルール:** サポート率がわずかに高く評価される
- **進化の方向性:** より少ない時間遅延で予測できるパターンを優先
- **理論的裏付け:** 論文の定義に従った正確な評価

### 3. 数値例（Nrd=4500の場合）

```
matched_count = 45

ルールA (max_delay=1):
  effective_records = 4500 - 1 - 2 = 4497
  support_rate = 45 / 4497 = 0.010007 (1.0007%)

ルールB (max_delay=5):
  effective_records = 4500 - 5 - 2 = 4493
  support_rate = 45 / 4493 = 0.010018 (1.0018%)

差: 0.0011% (わずかだが理論的に正確)
```

---

## 実装手順

### Step 1: ヘルパー関数の追加
- [ ] `calculate_accurate_support_rate()` を実装
- [ ] `main.c:1700` 付近に配置
- [ ] コメントで論文の式(3)を明記

### Step 2: 修正箇所の置き換え
- [ ] `check_rule_quality()` 内（main.c:1771-1773）
- [ ] `register_new_rule()` 呼び出し（main.c:2034）
- [ ] フィットネス計算・新規（main.c:2052）
- [ ] フィットネス計算・重複（main.c:2095）

### Step 3: コンパイル・テスト
- [ ] `make clean && make`
- [ ] 警告・エラーなし確認
- [ ] 動作テスト（GBPJPY等）

---

## 注意事項

### 前提条件
- データセットは十分に大きい（Nrd >> S_max）
- ゼロ除算は発生しない想定（実データで確認済み）

### コメント規約
各修正箇所に以下のコメントを追加：
```c
/* 論文準拠: support'(X) = t'(X) / (N - S_max(X∪Y)) */
```

### 後方互換性
- 出力フォーマットは変更なし
- ルールの品質評価がより正確になる
- 既存の閾値（Minsup）は影響を受ける可能性あり（わずか）

---

## コード量

- **追加コード**: 約25行（1つの関数）
- **修正箇所**: 4箇所（1-2行ずつ）
- **削除コード**: 4行程度
- **正味増加**: 約25行

---

## 参考文献

**論文:** "A New Method for Mining Intertransaction Association Rules"

**関連する式:**
- 式(1): `support(X) = t(X) / (N - S_max(X))`
- 式(3): `support'(X) = t'(X) / (N - S_max(X∪Y))` ← **本実装で使用**
- 式(4): `support'(Y) = t'(Y) / (N - S_max(X∪Y))`

**Example 3.1:**
- ルール: `A⁰₁ ∧ A¹₂ ⇒ A³₄`
- t'(X)の計算で、Yが範囲外の場合を除外する重要性を示す

---

## 実装完了後の検証項目

- [ ] サポート率が論文の定義と一致することを確認
- [ ] max_delayが異なるルールで分母が異なることを確認
- [ ] 出力ファイル（zrp01a.txt）のsupport_rate列が正確な値になることを確認
- [ ] フィットネス値の推移が理論的に妥当であることを確認

---

**実装者:** Claude Code
**レビュー必要:** 実装後に数値検証を実施
