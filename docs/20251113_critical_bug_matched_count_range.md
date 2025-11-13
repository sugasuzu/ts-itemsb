# 重大バグ: マッチカウント範囲の不整合

**発見日:** 2025-11-13
**重要度:** Critical
**影響:** 論文準拠性違反、サポート率計算の不正確さ

---

## 問題の概要

rematch_rule_pattern()とcollect_matched_indices()が固定的な`MAX_TIME_DELAY`を使用しているため、ルール固有の`max_delay`が小さい場合にマッチカウントが過小評価される。

これにより、サポート率計算の分子（matched_count）と分母（effective_records）の範囲が不整合となり、論文の式(3)に違反している。

---

## 理論的背景

### 論文の定義（式(3)）

```
support'(X) = t'(X) / (N - S_max(X∪Y))
```

**where:**
- `t'(X)`: Xを含むトランザクション数（Yの範囲も考慮）
- `N - S_max(X∪Y)`: ルール全体が適用可能なトランザクション数
- `S_max(X∪Y)`: ルール全体の時間スパン = max_delay + FUTURE_SPAN

### Example 3.1の教訓

> "although A₁=1 in T₄ and A₂=1 in T₅, it is impossible to compose the rule because there is no T₇ in the database"

**重要:** Xがマッチしても、Yが範囲外なら無効。つまり、分子と分母は**同じ有効範囲**に基づく必要がある。

---

## 現在の実装の問題

### get_safe_data_range (main.c:1414-1430)

```c
void get_safe_data_range(int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        *start_index = MAX_TIME_DELAY;      // 常に 5（固定）
        *end_index = Nrd - FUTURE_SPAN;     // Nrd - 2
    }
}
```

**問題:** すべてのルールが同じ範囲[5, Nrd-2)を走査する

### rematch_rule_pattern (main.c:527-576)

```c
static int rematch_rule_pattern(int *rule_attributes, int *time_delays, int num_attributes, int *matched_indices_out)
{
    int safe_start, safe_end;
    get_safe_data_range(&safe_start, &safe_end);  // 固定範囲！

    for (time_index = safe_start; time_index < safe_end; time_index++)
    {
        // マッチング処理
    }
}
```

**問題:** ルール固有のmax_delayを考慮していない

### calculate_accurate_support_rate (main.c:1724-1749)

```c
static double calculate_accurate_support_rate(int matched_count, int *time_delays, int num_attributes)
{
    int max_delay = 0;
    for (i = 0; i < num_attributes; i++)
    {
        if (time_delays[i] > max_delay)
        {
            max_delay = time_delays[i];
        }
    }

    int S_max_XY = max_delay + FUTURE_SPAN;
    int effective_records = Nrd - S_max_XY;  // ルール固有の分母

    return (double)matched_count / (double)effective_records;
}
```

**問題:** 分母はルール固有だが、分子（matched_count）は固定範囲で計算されている

---

## 具体例（Nrd = 4132の場合）

### ルールA: USDJPY_Up(t-1) のみ

- **max_delay**: 1
- **S_max(X∪Y)**: 1 + 2 = 3
- **理論上の有効範囲**: [1, 4130) → 4129レコード
- **現在の走査範囲**: [5, 4130) → 4125レコード
- **失われるマッチ**: index 1, 2, 3, 4 でのマッチ
- **分母**: 4132 - 3 = 4129（正しい）
- **分子**: [5, 4130)のマッチ数（4レコード分少ない）

**サポート率への影響:**
```
理論値: 仮に実際は25マッチ → 25/4129 = 0.00605 (0.605%)
現在値: [5,4130)で21マッチ → 21/4129 = 0.00508 (0.508%)
差: 0.097% (相対誤差 16%)
```

### ルールB: GBPJPY_Up(t-5), EURJPY_Down(t-2)

- **max_delay**: 5
- **S_max(X∪Y)**: 5 + 2 = 7
- **理論上の有効範囲**: [5, 4130) → 4125レコード
- **現在の走査範囲**: [5, 4130) → 4125レコード
- **分母**: 4132 - 7 = 4125（正しい）
- **分子**: [5, 4130)のマッチ数（正しい）

**サポート率:** 整合性あり ✓

---

## 影響範囲

### 論文準拠性

- ❌ 式(3)の定義に違反
- ❌ Example 3.1の精神に反する（範囲の不整合）

### 数値への影響

- **max_delay < MAX_TIME_DELAY のルール:** サポート率が過小評価される
- **max_delay = MAX_TIME_DELAY のルール:** 正しく計算される
- **進化への影響:** 短い遅延のルールが不当に低評価される可能性

### 実際の影響度

- **小さいデータセット（Nrd < 1000）:** 相対誤差が大きい
- **大きいデータセット（Nrd > 4000）:** 相対誤差は小さいが、理論的には不正確

---

## 修正方針

### Option 1: rematch_rule_pattern内でmax_delayを計算（推奨）

**利点:**
- 最小限の変更
- 関数の責任が明確

**実装:**

```c
static int rematch_rule_pattern(int *rule_attributes, int *time_delays, int num_attributes, int *matched_indices_out)
{
    int matched_count = 0;
    int time_index, attr_idx, is_match;
    int safe_start, safe_end;
    int i;

    /* ルール固有の最大時間遅延を計算 */
    int max_delay = 0;
    for (i = 0; i < num_attributes; i++)
    {
        if (time_delays[i] > max_delay)
        {
            max_delay = time_delays[i];
        }
    }

    /* ルール固有の安全な範囲を設定 */
    if (TIMESERIES_MODE)
    {
        safe_start = max_delay;              // ルール固有の開始点
        safe_end = Nrd - FUTURE_SPAN;        // 未来予測可能な終了点
    }
    else
    {
        safe_start = 0;
        safe_end = Nrd;
    }

    /* 安全範囲のデータポイントのみをスキャン */
    for (time_index = safe_start; time_index < safe_end; time_index++)
    {
        is_match = 1;

        /* 各属性をチェック */
        for (attr_idx = 0; attr_idx < num_attributes; attr_idx++)
        {
            if (rule_attributes[attr_idx] > 0)
            {
                int attr_id = rule_attributes[attr_idx] - 1;
                int delay = time_delays[attr_idx];
                int data_index = time_index - delay;

                /* 範囲チェック */
                if (data_index < 0 || data_index >= Nrd)
                {
                    is_match = 0;
                    break;
                }

                /* 属性値チェック */
                if (data_buffer[data_index][attr_id] != 1)
                {
                    is_match = 0;
                    break;
                }
            }
        }

        /* マッチした場合、インデックスを保存 */
        if (is_match == 1)
        {
            matched_indices_out[matched_count] = time_index;
            matched_count++;
        }
    }

    return matched_count;
}
```

### Option 2: 新しいヘルパー関数を追加

**実装:**

```c
void get_safe_data_range_for_rule(int max_delay, int *start_index, int *end_index)
{
    if (TIMESERIES_MODE)
    {
        *start_index = max_delay;            // ルール固有の最大遅延
        *end_index = Nrd - FUTURE_SPAN;
    }
    else
    {
        *start_index = 0;
        *end_index = Nrd;
    }
}
```

そして rematch_rule_pattern で使用：

```c
int max_delay = 0;
for (i = 0; i < num_attributes; i++)
{
    if (time_delays[i] > max_delay)
    {
        max_delay = time_delays[i];
    }
}
get_safe_data_range_for_rule(max_delay, &safe_start, &safe_end);
```

---

## 修正箇所

### 1. rematch_rule_pattern (main.c:527-576)

- [x] max_delayの計算を追加
- [x] safe_startをmax_delayに設定
- [x] get_safe_data_range()呼び出しを削除または条件分岐

### 2. collect_matched_indices (main.c:1909-1980)

- [x] 同様の修正を適用

### 3. evaluate_all_individuals (main.c:1621-1634)

- [ ] 修正不要（MAX_TIME_DELAYの使用が適切）

---

## テスト計画

### 検証項目

1. **数値検証:**
   - max_delay=1のルールのマッチ数が増加することを確認
   - max_delay=5のルールのマッチ数が変わらないことを確認

2. **理論的検証:**
   - matched_countの範囲とeffective_recordsの範囲が一致することを確認
   - サポート率が論文の式(3)に準拠していることを確認

3. **回帰テスト:**
   - コンパイル成功
   - 既存のルールプールと比較（数値の変化を確認）

---

## 実装優先度

**Priority:** P0 (Critical - 論文準拠性違反)

**理由:**
1. 論文の式(3)に明確に違反している
2. 数値の正確性が損なわれている
3. 進化の方向性に影響を与える可能性

**推奨:** 即座に修正すべき

---

## 参考文献

**論文:** "A New Method for Mining Intertransaction Association Rules"

**関連する式:**
- 式(3): `support'(X) = t'(X) / (N - S_max(X∪Y))`

**Example 3.1:**
- ルール: `A⁰₁ ∧ A¹₂ ⇒ A³₄`
- 重要な教訓: Xがマッチしても、Yが範囲外なら無効

---

**発見者:** Claude Code (Code Reviewer)
**レビュー:** 論文査読者の視点から、理論的整合性を厳密に検証

