# Testing Evidence

All results below are **historical verified results** from prior completed phases of this project. No full test suite was rerun during this audit/evidence-collection task.

---

## 1. C99 Unit & Boundary Test Suites (`backend/tests/`)

Compiled with `gcc -Wall -Wextra -std=c99 -Ibackend/include` (`0` warnings, `0` errors). CSV-writing tests run in isolated `%TEMP%` directories.

| Test Suite | Scope | Result |
|---|---|---|
| `test_author_hash.c` | `AuthorHash` 61-bucket insert, lookup, collision, duplicate/empty-ID guards | **19 passed, 0 failed** |
| `test_reciprocal_graph.c` | `CitationGraph` adjacency-list creation, edge insertion, traversal, free | **14 passed, 0 failed** |
| `test_reciprocal_detection.c` | Reciprocal detection, CSV cache, verified-empty sentinel, end-to-end analysis | **25 passed, 0 failed** |
| `test_memory_boundary.c` | Memory safety, `PAPER_ID_LEN` (`32`), `PAPER_MAX_AUTHORS` (`50`), `PAPER_MAX_REFS` (`1000`) bounds | **13 passed, 0 failed** |
| `test_openalex_parser.c` | Offline OpenAlex JSON parser (`openalex_parse_work`), malformed JSON, UTF-8 escapes | **12 passed, 0 failed** |
| `test_bibliographic_coupling.c` | `RefHash` (`1009` buckets), Jaccard similarity, duplicates, `UNKNOWN` handling, 1000-ref scale | **12 passed, 0 failed** |
| `test_cocitation.c` | Co-Citation intersection, pagination, truncation, `max_pages` cap, malformed JSON, CSV edges | **23 passed, 0 failed** |
| **Total C99 Cases** | **7 suites** | **118 passed, 0 failed** |

---

## 2. Flask & Frontend JavaScript Integration Suites

| Suite | Scope | Result |
|---|---|---|
| `bridge/test_app.py` | `/api/analyze`, `/api/citation-analysis`, `/api/coupling`, `/api/cocitation`, error/timeout handling, frontend HTML/JS routes (isolated via `CITESPECT_WORKDIR`) | **14 passed, 0 failed** |
| `frontend/test_cocitation_ui.js` | Node VM DOM integration tests for mode switching, identical-ID guard, `UNVERIFIED` display, clickable OpenAlex links | **6 passed, 0 failed** |
| `frontend/test_redesign_ui.js` | Node VM DOM presentation tests for paper header, author collapse, 4-col table, adaptive graph labels, keyboard focus/Escape, reduced motion | **12 passed, 0 failed** |

---

## 3. Code Coverage & CSV Integrity

- **Coverage (`gcov -b`):** Core offline C99 modules achieved **76.35%** line coverage; `openalex.c` achieved **74.18%** line coverage after adding `test_openalex_parser.c`.
- **Real CSV SHA-256 Integrity:** Verified identical before and after isolated test runs:
  - `authors.csv`: `D495C8925BB5A6D33B4647CA88BA73EB84DE0D26E6F9C713C24E8A75F1B5CA9D`
  - `citations.csv`: `BC49482838414D2435599435464CCCF1735A87BDF3B2C4850C36754D5FC706AF`
  - `papers.csv`: `6CE36C314CADD6FB0D4BD65337CB5FB3CCAEB61730F8E3BBC63BFE071A87E095`
- **Environment Limitation:** Windows MinGW GCC 6.3.0 does not ship `-lasan` or `-lubsan`, so AddressSanitizer / UndefinedBehaviorSanitizer could not be linked locally.
