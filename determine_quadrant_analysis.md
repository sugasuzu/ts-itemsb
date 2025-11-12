# 象限判定ロジックの改善提案

## 現在の問題点

### 現在の実装（main.c lines 432-479）
```c
static int determine_quadrant(double min_t1, double max_t1, double min_t2, double max_t2)
{
    double center_t1 = (min_t1 + max_t1) / 2.0;
    double center_t2 = (min_t2 + max_t2) / 2.0;

    // 中心点ベースの判定 + またぎ許容
    if (center_t1 > 0.0) {
        if (min_t1 < -QUADRANT_THRESHOLD)
            return 0;
    }

    // 中心点で象限を判定
    if (center_t1 > 0.0 && center_t2 > 0.0)
        return 1; // Q1
    // ...
}
```

### 問題例（QUADRANT_THRESHOLD = 0.1%）

| min_t1 | max_t1 | center_t1 | 現在の判定 | 問題点 |
|--------|--------|-----------|-----------|--------|
| **-0.08%** | +0.50% | +0.21% | Q1 ✓ | 一部が負のリターン |
| +0.12% | +0.50% | +0.31% | Q1 ✓ | 正常 |

**問題:**
- 一部のマッチが負のリターンを含んでいても、中心が正ならQ1と判定される
- 「ある象限にたくさんある」という要件を満たさない

---

## 改善案: Min/Maxベースの厳密な判定

### 新しい実装（推奨）

```c
static int determine_quadrant_strict(double min_t1, double max_t1,
                                      double min_t2, double max_t2)
{
    // Q1: すべてのマッチが正（minが閾値以上なら、maxも必ず閾値以上）
    if (min_t1 >= QUADRANT_THRESHOLD && min_t2 >= QUADRANT_THRESHOLD)
        return 1;

    // Q2: t+1がすべて負、t+2がすべて正
    if (max_t1 <= -QUADRANT_THRESHOLD && min_t2 >= QUADRANT_THRESHOLD)
        return 2;

    // Q3: すべて負
    if (max_t1 <= -QUADRANT_THRESHOLD && max_t2 <= -QUADRANT_THRESHOLD)
        return 3;

    // Q4: t+1がすべて正、t+2がすべて負
    if (min_t1 >= QUADRANT_THRESHOLD && max_t2 <= -QUADRANT_THRESHOLD)
        return 4;

    return 0; // どの象限にも一貫していない
}
```

### 判定ロジックの違い

#### Q1象限の例（QUADRANT_THRESHOLD = 0.1%）

| min_t1 | max_t1 | min_t2 | max_t2 | 旧ロジック | **新ロジック** | 理由 |
|--------|--------|--------|--------|-----------|--------------|------|
| -0.08% | +0.50% | +0.10% | +0.60% | Q1 | **0 (却下)** | min_t1 < 閾値 |
| +0.12% | +0.50% | +0.15% | +0.60% | Q1 | **Q1** | すべて閾値以上 |
| +0.05% | +0.50% | +0.10% | +0.60% | Q1 | **0 (却下)** | min_t1 < 閾値 |

**新ロジックの利点:**
- **すべて**のマッチが閾値以上であることを保証
- 「ある象限にたくさんある」という要件を厳密に満たす
- 象限の一貫性が確実に保たれる

---

## 4象限すべての判定条件

### Q1（両方とも上昇）
```c
min_t1 >= +QUADRANT_THRESHOLD AND min_t2 >= +QUADRANT_THRESHOLD
```
- すべてのマッチが t+1, t+2 ともに閾値以上の正のリターン

### Q2（t+1下落、t+2上昇：リバウンド）
```c
max_t1 <= -QUADRANT_THRESHOLD AND min_t2 >= +QUADRANT_THRESHOLD
```
- すべてのマッチが t+1 で閾値以下の負のリターン、t+2 で閾値以上の正のリターン

### Q3（両方とも下落）
```c
max_t1 <= -QUADRANT_THRESHOLD AND max_t2 <= -QUADRANT_THRESHOLD
```
- すべてのマッチが t+1, t+2 ともに閾値以下の負のリターン

### Q4（t+1上昇、t+2下落：反転）
```c
min_t1 >= +QUADRANT_THRESHOLD AND max_t2 <= -QUADRANT_THRESHOLD
```
- すべてのマッチが t+1 で閾値以上の正のリターン、t+2 で閾値以下の負のリターン

---

## 実装上の変更点

### main.c の修正箇所

1. **関数名の変更（または新関数の追加）**
   - `determine_quadrant()` → `determine_quadrant_strict()`
   - または既存関数を書き換え

2. **Lines 432-479 を置き換え**

3. **呼び出し元の確認**
   - Line 1960: `quadrant = determine_quadrant(min_t1, max_t1, min_t2, max_t2);`
   - この1箇所のみ

---

## 期待される効果

### Before（現在の実装）

```
Total Evaluated: 1,929,600 rules

Rejected by Quadrant: 485,557 (25.16%)
Passed All Filters: 0 (0.00%)
```

**問題:**
- 0ルールも通過しない（閾値が厳しすぎる）
- しかし、一貫性の低いルールを除外できていない

### After（新しい実装）

期待される結果:
- **一貫性の高い**ルールのみが通過
- QUADRANT_THRESHOLDの調整により、通過数を制御可能
- 「ある象限にたくさんある」という要件を厳密に満たす

### 閾値の推奨値

| QUADRANT_THRESHOLD | 意味 | 推奨用途 |
|-------------------|------|---------|
| 0.05% | 非常に厳密 | 確実な方向性が必要な場合 |
| 0.10% | 厳密 | バランス型 |
| 0.20% | やや緩和 | より多くのルールを発見したい場合 |
| 0.50% | 緩和 | 探索的分析 |

**現在の設定（0.1%）は適切**ですが、新ロジックでは意味が変わります：
- 旧: 「中心から0.1%のまたぎ許容」
- **新: 「すべてのマッチが0.1%以上/以下」**

---

## Sigmaフィルタについて

### 結論: **Sigmaフィルタは不要**

**理由:**
1. **目的との不一致**
   - ユーザーの目的: 「ある象限にたくさんあるルールを発見」
   - Sigmaの役割: 予測の安定性（バラつきの小ささ）
   - これらは別の基準

2. **有用なルールを排除する可能性**
   ```
   ルールA: 100マッチ、すべてQ1、sigma = 0.5%（大きい）
   → sigmaフィルタで却下されるが、「Q1にたくさんある」という点では有用
   ```

3. **現在既に無効化されている**
   - Lines 1974-1988 はコメントアウト済み
   - ユーザーは既に正しい判断をしている

### 推奨: Sigmaフィルタを完全削除

コメントアウトではなく、コード自体を削除してシンプルにする。

---

## まとめ

### ユーザーの指摘は完全に正しい

1. ✅ **Sigmaは標準偏差を正しく計算している**
2. ✅ **象限にたくさんあるルールを見つける場合、sigmaは不要**
3. ✅ **MIN_SUPPORT_COUNTとQUADRANT_THRESHOLDだけで十分**

### ただし、実装に問題がある

- ❌ 現在の`determine_quadrant()`は中心点ベースで「またぎ」を許容しすぎる
- ✅ Min/Maxベースの厳密な判定に変更すべき

### 推奨される変更

1. **`determine_quadrant()`をMin/Maxベースに書き換え**
2. **Sigmaフィルタのコメントアウト部分を完全削除**
3. **QUADRANT_THRESHOLDの意味を明確化**（0.1% = すべてのマッチが0.1%以上/以下）

これにより、「ある象限にたくさんあるルール」を確実に発見できるようになります。
