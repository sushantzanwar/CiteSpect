# CITESPECT — Research Paper Citation Analysis System

A college FODS (Fundamentals of Data Structures) project written in **C99** with a **Flask** bridge and interactive **HTML/CSS/JavaScript** frontend. CITESPECT analyses citation relationships between research papers using data structures implemented from scratch (no external DSA libraries), backed by OpenAlex metadata and local CSV caching.

## Architecture

```
CiteSpect/
├── DESIGN.md          Frontend visual system, tokens, typography, and motion rules
├── backend/
│   ├── include/       C99 headers (paper.h, author_hash.h, citation_graph.h, citation_analysis.h, csv_store.h, openalex.h, config.h, paper_list.h)
│   ├── src/           C99 implementation (main.c, paper.c, paper_list.c, author_hash.c, citation_graph.c, citation_analysis.c, csv_store.c, openalex.c, config.c)
│   ├── data/          Local CSV cache (papers.csv, authors.csv, citations.csv)
│   └── tests/         C99 unit & boundary test suites
├── bridge/
│   ├── app.py         Flask HTTP/JSON bridge invoking backend/citespect.exe
│   └── test_app.py    Flask integration test suite (unittest)
└── frontend/
    ├── index.html     UI layout, citation summary, and interactive graph container
    ├── script.js      API calls, citation table, comparison workflows, and SVG graph
    ├── style.css      Responsive styling and graph legend/highlighting
    ├── test_cocitation_ui.js  Node JS DOM integration tests for comparison workflows
    └── test_redesign_ui.js    Node JS DOM presentation, adaptive graph, and a11y tests
```

## Core Algorithms & Data Structures

1. **Hash-Table SELF-Citation Detection (`author_hash.c`):**
   Builds a 61-bucket hash table with separate chaining from the citing paper's author IDs, then looks up each cited paper's author IDs in $O(1)$ average time to identify shared authors (`SELF` vs. `NOT IDENTIFIED AS SELF`).
2. **Adjacency-List Reciprocal Citation Detection (`citation_graph.c`, `citation_analysis.c`):**
   Constructs a directed `CitationGraph` adjacency list for each cited paper's references and searches for a reverse edge back to the citing paper:
   - `CONFIRMED RECIPROCAL` — cited paper's references include the citing paper.
   - `NOT RECIPROCAL` — cited paper's references were verified (including verified 0-reference papers) and do not include the citing paper.
   - `UNKNOWN` — references could not be loaded or fetched from OpenAlex.
3. **Bibliographic Coupling — Two-Paper Reference Overlap (`citation_analysis.c`):**
   Builds a dedicated 1009-bucket separate-chaining `RefHash` table for Paper A's deduplicated outgoing references (ignoring self-references) and intersects them with Paper B's deduplicated outgoing references to compute common references $|A \cap B|$, union size $|A \cup B|$, and Jaccard similarity percentage:
   $$\text{Jaccard Similarity} = \frac{|A \cap B|}{|A \cup B|} \times 100\%$$
   - `YES` — at least one common reference found ($|A \cap B| > 0$).
   - `NO` — reference lists for both papers were verified (including verified 0-reference papers) and have zero overlap.
   - `UNKNOWN` — reference metadata for one or both papers is unavailable (never reported as `0%`).
4. **Co-Citation Detection — Shared Incoming Citations (`citation_analysis.c`, `openalex.c`, `csv_store.c`):**
   Identifies third-party papers $C$ ($C \neq A, B$) that cite both Paper A and Paper B by retrieving paginated incoming citing Work IDs via OpenAlex (`?filter=cites:<WORK_ID>`) supplemented with local `citations.csv` reverse edges, deduplicating and intersecting both sets with `RefHash` (`1009` buckets):
   - `YES` — at least one verified third-party paper cites both A and B.
   - `NO` — complete incoming citing sets for both A and B were checked and share zero common papers (`0.00%`).
   - `UNKNOWN` — incomplete, truncated, or failed citing-paper data prevents ruling out overlap (exact Jaccard is reported as `N/A` in CLI and `UNVERIFIED` in the web UI whenever either citing set is incomplete).
   - Identical Work IDs (`A == B`) are rejected cleanly.
- **Duplicate & Self-Reference Handling:**
  Duplicate cited-paper records and self-referencing edges (citing paper citing its own Work ID) are tracked separately in the summary without double-counting.

## OpenAlex Integration & Reference Caching

- `openalex.c` fetches paper metadata by DOI or OpenAlex Work ID via `curl` and parses JSON using lightweight hand-written helpers (storing up to `PAPER_MAX_AUTHORS` = 50 authors and `PAPER_MAX_REFS` = 1000 references; rejecting IDs `>= PAPER_ID_LEN` = 32 bytes).
- For Co-Citation Detection, `openalex_fetch_citing_ids()` queries `https://api.openalex.org/works?filter=cites:<WORK_ID>&select=id&per-page=200` up to `OPENALEX_MAX_CITER_PAGES` (`5` pages / `1000` citers) and explicitly flags truncated, short, or incomplete pagination responses.
- `config.c` reads an optional `OPENALEX_API_KEY` from the environment (never hard-coded).
- `csv_store.c` caches papers, authors, and citation edges in `backend/data/*.csv`. A sentinel row (`<paper_id>,`) in `citations.csv` records papers whose empty reference list (0 references) was verified from OpenAlex, distinguishing verified empty lists from papers whose references have not yet been fetched. Reverse-edge scanning (`csv_load_citing_papers()`) uses local `citations.csv` only as supplementary evidence for Co-Citation.

## Flask Bridge & Interactive Frontend

- `bridge/app.py` serves `frontend/` and exposes:
  - `POST /api/analyze` — looks up a paper by DOI or Work ID (using local CSVs first, otherwise running `citespect.exe --doi <DOI>`).
  - `POST /api/citation-analysis` — runs `citespect.exe` option 4 for a Work ID and returns structured JSON with per-citation classifications, shared authors, local titles, and summary counts.
  - `POST /api/coupling` — runs `citespect.exe --coupling <PAPER_A_ID> <PAPER_B_ID>` and returns structured JSON with coupling status (`YES` / `NO` / `UNKNOWN`), unique/common/union reference counts, Jaccard similarity percentage, and shared Work IDs with local titles.
  - `POST /api/cocitation` — runs `citespect.exe --cocitation <PAPER_A_ID> <PAPER_B_ID>` and returns structured JSON with co-citation status (`YES` / `NO` / `UNKNOWN`), `data_complete` flag, unique citing-paper counts, union count, exact Jaccard similarity (or `null` when incomplete), and common co-citing Work IDs with local titles.
- `frontend/` (visual system documented in root `DESIGN.md`) renders:
  - A simplified 3-counter dashboard (`Total Citations`, `Self-Citations`, `Reciprocal Citations`) and citation details table with clear labels (`SELF`, `OTHER / UNCONFIRMED SELF`, `RECIPROCAL`, `ONE-WAY`, `UNVERIFIED`, `FETCH FAILED`).
  - An interactive SVG citation graph with directional arrowheads, color-coded nodes, legend, click and keyboard navigation (`Tab` / `Enter` / `Space`), and a node detail panel showing Work ID, title, SELF/reciprocal status, shared authors, and OpenAlex link.
  - A **Compare two papers** section with mode selection between **Bibliographic Coupling (outgoing references)** and **Co-Citation (incoming citations)**, clickable OpenAlex links for shared/co-citing Work IDs, and `UNVERIFIED` display when similarity cannot be reliably calculated.

## Build and Run

From `backend/`:

```powershell
gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o citespect.exe
.\citespect.exe 10.1038/nature14539
.\citespect.exe --coupling W2919115771 W1901616594
.\citespect.exe --cocitation W4395000590 W4401955319
```

Interactive CLI menu includes Option `6. Compare two papers (Bibliographic Coupling)` and Option `7. Compare two papers (Co-Citation)`.

Optional API key:

- PowerShell: `$env:OPENALEX_API_KEY = "your-key"`
- Bash: `export OPENALEX_API_KEY=your-key`

Run the web UI from the repository root:

```powershell
pip install flask
python bridge/app.py
```

Open `http://127.0.0.1:5000`.

## Test Suites

C99 tests (compile with `-Wall -Wextra -std=c99 -Ibackend/include`; run CSV-writing tests in an isolated temporary directory or let self-isolating suites use `%TEMP%`):

- `backend/tests/test_author_hash.c` — 19 hash-table unit tests.
- `backend/tests/test_reciprocal_graph.c` — 14 adjacency-list graph tests.
- `backend/tests/test_reciprocal_detection.c` — 25 reciprocal detection, cache, and end-to-end production tests.
- `backend/tests/test_memory_boundary.c` — 13 memory-safety and ID/array boundary tests.
- `backend/tests/test_openalex_parser.c` — 12 offline OpenAlex JSON parser unit tests.
- `backend/tests/test_bibliographic_coupling.c` — 12 Bibliographic Coupling and Jaccard similarity tests.
- `backend/tests/test_cocitation.c` — 23 offline C99 Co-Citation, pagination, truncation, and isolated CSV integration tests.

Flask and frontend JavaScript integration tests (isolated via `tempfile.TemporaryDirectory` and `CITESPECT_WORKDIR` / Node VM DOM harness):

```powershell
python -m unittest -v bridge/test_app.py   # 14 Flask integration tests
node frontend/test_cocitation_ui.js       # 6 Co-Citation frontend JS tests
node frontend/test_redesign_ui.js         # 12 UI presentation & a11y JS tests
```

## Known Limitations

- **Sanitizers on Windows MinGW 6.3.0:** The installed `mingw32` GCC 6.3.0 toolchain does not ship `-lasan` or `-lubsan`, so AddressSanitizer and UndefinedBehaviorSanitizer cannot be linked locally.
- **OpenAlex Network Dependency:** Live fetches require `curl` and internet access; without them, only cached CSV papers can be analyzed (uncached references or unverified incoming citations resolve to `UNKNOWN`).
- **Fixed Bounds & Pagination Cap:** `Paper` structs store up to 50 authors, 1000 references, and 31-character IDs (`PAPER_ID_LEN - 1`). Co-Citation fetches up to 5 OpenAlex pages (1000 citing papers per target paper); papers exceeding 1000 citations are flagged incomplete (`UNVERIFIED` similarity) rather than reporting false negatives or inaccurate percentages.
