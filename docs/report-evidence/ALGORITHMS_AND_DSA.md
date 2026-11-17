# Algorithms and Data Structures

All core data structures are implemented from scratch in C99 without external DSA libraries.

---

## A. Self-Citation Detection (`AuthorHash`)

- **Confirmed from source code (`backend/include/author_hash.h`, `backend/src/author_hash.c`, `backend/src/citation_analysis.c`):**
  - **Structure:** `AuthorHash` — array of `61` buckets (`AUTHOR_HASH_BUCKETS = 61`) with separate chaining (`AuthorNode` singly linked list storing `author_id`, `author_name`, `next`).
  - **Hash Function (`hash_author_id`):** djb2-style multiply-and-XOR:
    `h = 5381u; for each byte c: h = h * 31u ^ (unsigned int)c; return h % 61;`
  - **Insertion (`hash_insert` / `author_hash_build`):** Skips `NULL` or empty (`""`) author IDs and duplicates (`strcmp`), prepending new nodes to `buckets[idx]`.
  - **Comparison (`find_self_authors_hash`):** Builds `AuthorHash` once for the citing paper, then looks up each cited paper's non-empty `author_ids[i]` via `hash_search` (`strcmp`).
  - **Missing Metadata:** Empty author IDs are never inserted or matched; failed OpenAlex fetches report `NOT IDENTIFIED AS SELF (metadata retrieval failed: ...)` and increment `not_self_count`.
  - **Complexity:** Build $O(A_1)$, per-cited-paper lookup $O(A_2)$ average ($O(A_1 \cdot A_2)$ worst case), Space $O(A_1)$ where $A_1, A_2 \le 50$.
- **Confirmed by existing tests:** `test_author_hash.c` (19 passed), `test_reciprocal_detection.c` (25 passed).
- **Reproducible Example:** `W1001` (author `A1001`) citing `W2001` (author `A1001`) -> `SELF` (`Shared Author: A1001 (Alice)`).

### Pseudocode: Self-Citation Detection
```text
author_hash_init(ht)
for each author in citing_paper:
    if author.id != "": hash_insert(ht, author.id, author.name)
for each cited_paper:
    matches = []
    for each author in cited_paper:
        if author.id != "" and hash_search(ht, author.id) != NULL:
            add unique match to matches
    if matches is non-empty: classify SELF else NOT IDENTIFIED AS SELF
hash_free(ht)
```

---

## B. Reciprocal Citation Detection (`CitationGraph`)

- **Confirmed from source code (`backend/include/citation_graph.h`, `backend/src/citation_graph.c`, `backend/src/citation_analysis.c`):**
  - **Representation:** Directed graph via adjacency list (`CitationGraph` node with `paper_id` and linked list `CitationEdge *references`).
  - **Edge Check (`citation_check_reciprocal`):** Creates a `CitationGraph` for `cited->paper_id`, appends each outgoing reference (`citation_graph_add_reference`), and searches for `citing_id` (`citation_graph_has_reference` using `strcmp`).
  - **Missing/Incomplete Data:** If `refs_available == 0` (fetch failed or uncached), returns `RECIPROCAL_UNKNOWN`. Verified 0-reference papers (recorded in `citations.csv` via `<paper_id>,` sentinel) set `refs_available = 1` and return `RECIPROCAL_NOT`.
  - **Complexity:** Time $O(R_B)$ per cited paper ($R_B \le 1000$), Space $O(R_B)$.
- **Confirmed by existing tests:** `test_reciprocal_graph.c` (14 passed), `test_reciprocal_detection.c` (25 passed).
- **Reproducible Example:** `W1001 -> W2001` and `W2001 -> W1001` -> `CONFIRMED RECIPROCAL`.

### Pseudocode: Reciprocal Citation Detection
```text
if not refs_available: return RECIPROCAL_UNKNOWN
graph = citation_graph_create(cited.paper_id)
for each ref in cited.references:
    citation_graph_add_reference(graph, ref)
found = citation_graph_has_reference(graph, citing_id)
citation_graph_free(graph)
return RECIPROCAL_CONFIRMED if found else RECIPROCAL_NOT
```

---

## C. Bibliographic Coupling (`RefHash`)

- **Confirmed from source code (`backend/src/citation_analysis.c`):**
  - **Structure:** `RefHash` — `1009`-bucket (`REF_HASH_BUCKETS = 1009`) hash table with separate chaining (`RefNode`), hash function `h = h * 31u + c; return h % 1009;`.
  - **Deduplication & Self-Ref Filtering:** `populate_ref_hash` inserts A's outgoing references into `ht_a` (ignoring self-references `ref == paper_a->paper_id` and duplicates). `scan_common_refs` inserts B's references into `ht_b` and checks `ref_hash_contains(ht_a, ref)` only on newly seen references.
  - **Exact Jaccard Formula:**
    `union_count = unique_refs_a + unique_refs_b - common_count`
    `jaccard_pct = (union_count > 0) ? (common_count * 100.0 / union_count) : 0.0`
  - **Unknown Data:** If `!refs_a_available || !refs_b_available`, returns `COUPLING_UNKNOWN` (never `0%`).
  - **Complexity:** Time $O(R_A + R_B)$ average, Space $O(R_A + R_B)$ where $R_A, R_B \le 1000$.
- **Confirmed by existing tests:** `test_bibliographic_coupling.c` (12 passed).
- **Reproducible Example:** `W2001` (cites `W1001`) vs `W2002` (cites `W1001`) -> `YES`, `common=1`, `union=1`, `100.00%`.

### Pseudocode: Bibliographic Coupling
```text
if not refs_a_available or not refs_b_available: return COUPLING_UNKNOWN
insert unique non-self refs of A into ht_a (count unique_a)
for each ref of B:
    if ref != B.id and insert_new(ht_b, ref):
        unique_b++
        if contains(ht_a, ref): common++
union_count = unique_a + unique_b - common
jaccard = (union_count > 0) ? (common * 100.0 / union_count) : 0.0
return COUPLING_YES if common > 0 else COUPLING_NO
```

---

## D. Co-Citation Detection (`RefHash` + Paginated OpenAlex `filter=cites:`)

- **Confirmed from source code (`backend/src/openalex.c`, `backend/src/csv_store.c`, `backend/src/citation_analysis.c`):**
  - **Retrieval & Pagination:** `openalex_fetch_citing_ids` queries `https://api.openalex.org/works?filter=cites:<WORK_ID>&select=id&per-page=200&page=<N>` up to `OPENALEX_MAX_CITER_PAGES = 5` (`1000` citers max). `csv_load_citing_papers` scans `citations.csv` for reverse edges as **supplementary evidence only** (never proof of completeness).
  - **Intersection & Guard:** `citation_compute_cocitation` rejects identical IDs (`COCITATION_INVALID`), ignores direct pair IDs (`A` and `B`) so only third-party papers $C \neq A, B$ are counted, and deduplicates/intersects via `RefHash` (`1009` buckets).
  - **Incomplete Data & Jaccard:** `exact_jaccard = (a_complete && b_complete)`. If `common_count > 0`, returns `COCITATION_YES` (with `jaccard_pct = -1.0` / `N/A` if `!exact_jaccard`). If `common_count == 0` and `!exact_jaccard`, returns `COCITATION_UNKNOWN`.
  - **Complexity:** Time $O(C_A + C_B)$ average, Space $O(C_A + C_B)$.
- **Confirmed by existing tests:** `test_cocitation.c` (23 passed), `bridge/test_app.py` (`test_12`, `test_13`), `frontend/test_cocitation_ui.js` (6 passed).
- **Reproducible Example:** `W4395000590` vs `W4401955319` -> `YES`, `common=1` (`W7129575785`), `union=29`, `3.45%` (verified via Flask `test_client()`).
