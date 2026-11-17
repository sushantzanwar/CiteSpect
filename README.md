# CITESPECT — Research Paper Citation Analysis System

CITESPECT is a research paper citation analysis application developed for the **Data Structures (ML2301)** course at **Vishwakarma Institute of Technology, Pune** (Department of Computer Science & Engineering — Artificial Intelligence, Academic Year 2026–27).

It combines a **C99** backend (implementing all core data structures from scratch without external DSA libraries), a **Flask** HTTP/JSON bridge, an **HTML/CSS/JavaScript** frontend with an interactive **SVG** citation graph, **OpenAlex API** metadata retrieval, and local **CSV** caching.

---

## Problem Statement

Researchers and students often need to evaluate how academic papers relate to one another beyond simple reference counts:
- Whether an author cites their own prior work (**Self-Citation**).
- Whether two papers mutually cite each other (**Reciprocal Citation**).
- How strongly two papers overlap in their outgoing bibliographies (**Bibliographic Coupling**).
- How frequently two distinct papers are cited together by third-party works (**Co-Citation**).

Manual inspection across reference lists is slow and error-prone. CITESPECT automates these four analyses using explicit, inspectable data structures and clearly distinguishes verified results from incomplete or unavailable metadata.

---

## Main Features

1. **Self-Citation Detection** — Identifies shared OpenAlex author IDs between a citing paper and each cited paper (`SELF` vs. `NOT IDENTIFIED AS SELF`).
2. **Reciprocal Citation Detection** — Checks whether a cited paper also cites back to the original citing paper (`CONFIRMED RECIPROCAL`, `NOT RECIPROCAL`, or `UNKNOWN`).
3. **Bibliographic Coupling** — Compares two papers' outgoing reference sets to find common references, union size, and Jaccard similarity (`YES`, `NO`, or `UNKNOWN`).
4. **Co-Citation Detection** — Compares two distinct papers' incoming citing-paper sets from OpenAlex (supplemented with local CSV reverse edges) to find third-party papers citing both (`YES`, `NO`, or `UNKNOWN`).
5. **Interactive SVG Citation Graph** — Visualizes directed citation edges (`Citing -> Cited`) with keyboard navigation (`Tab`, `Enter`/`Space`, `Escape`) and a node detail panel.
6. **OpenAlex & Local CSV Caching** — Fetches paper metadata by DOI or OpenAlex Work ID via `curl` and caches papers, authors, and citation edges in `backend/data/*.csv`.

---

## Data Structures & Algorithms Used

- **Hash Table with Separate Chaining (`AuthorHash`, 61 buckets)** — `author_hash.c`: Stores the citing paper's author IDs using a djb2-style multiply-and-XOR hash (`h = h * 31 ^ c`) for $O(1)$ average-time self-citation lookups.
- **Directed Graph via Adjacency List (`CitationGraph`)** — `citation_graph.c`: Stores outgoing citation edges (`paper_id -> cited_paper_id`) and traverses them to verify reciprocal edges.
- **Hash Table with Separate Chaining (`RefHash`, 1009 buckets)** — `citation_analysis.c`: Deduplicates and intersects outgoing reference sets (Bibliographic Coupling) and incoming citing-paper sets (Co-Citation) using `h = h * 31 + c` and computes Jaccard similarity $\frac{|A \cap B|}{|A \cup B|} \times 100\%$.
- **Singly Linked List (`PaperNode`)** — `paper_list.c`: Loads and searches cached paper records in `data/papers.csv` for the CLI menu.

---

## System Architecture

```text
User (Browser / CLI)
  │
  ▼
Frontend (HTML / CSS / Vanilla JS + SVG Graph)
  │  POST /api/analyze, /api/citation-analysis, /api/coupling, /api/cocitation
  ▼
Flask Bridge (bridge/app.py)
  │  subprocess.run(backend/citespect.exe ...)
  ▼
C99 Backend (backend/src/*.c)
  ├──► Local CSV Cache (backend/data/papers.csv, authors.csv, citations.csv)
  └──► OpenAlex REST API (via curl)
```

---

## Technology Stack

- **Backend:** C99 (`gcc -Wall -Wextra -std=c99`)
- **Bridge:** Python 3, Flask
- **Frontend:** HTML5, CSS3, Vanilla JavaScript, inline SVG
- **External API:** OpenAlex (`https://api.openalex.org/works`) via `curl`
- **Storage:** Local CSV files (`papers.csv`, `authors.csv`, `citations.csv`)

---

## Repository Structure

```text
CiteSpect/
├── DESIGN.md                  Frontend design system specification
├── README.md                  Project overview and setup guide
├── backend/
│   ├── include/               C99 headers (paper.h, author_hash.h, citation_graph.h, ...)
│   ├── src/                   C99 source files (main.c, citation_analysis.c, openalex.c, ...)
│   ├── data/                  Local CSV cache (papers.csv, authors.csv, citations.csv)
│   ├── tests/                 7 C99 unit & boundary test suites
│   └── README.md              Backend-focused technical notes
├── bridge/
│   ├── app.py                 Flask HTTP/JSON bridge
│   └── test_app.py            14 Flask integration tests (unittest)
├── docs/
│   └── report-evidence/       Academic report evidence package
└── frontend/
    ├── index.html             Workspace layout
    ├── style.css              Responsive Editorial Academic styles
    ├── script.js              API calls, table/summary rendering, SVG graph
    ├── test_cocitation_ui.js  6 Co-Citation frontend JS tests
    └── test_redesign_ui.js    12 UI presentation & accessibility JS tests
```

---

## Installation & Run Instructions

### Prerequisites
- `gcc` (C99 compiler, e.g. MinGW on Windows)
- `curl` (available on `PATH`)
- Python 3 with `flask` (`pip install flask`)
- Optional: Node.js (to run frontend JS DOM tests)

### 1. Compile the C99 Backend
From `backend/`:
```powershell
gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o citespect.exe
```

### 2. Optional: Configure `OPENALEX_API_KEY`
For higher OpenAlex rate limits (never hard-code keys):
- **PowerShell:** `$env:OPENALEX_API_KEY = "your-key"`
- **Bash:** `export OPENALEX_API_KEY=your-key`

### 3. Run the Flask Web Application
From the repository root:
```powershell
pip install flask
python bridge/app.py
```
Open `http://127.0.0.1:5000` in your browser.

### 4. CLI Usage (Optional)
From `backend/`:
```powershell
.\citespect.exe --doi 10.1038/nature14539
.\citespect.exe --coupling W2919115771 W1901616594
.\citespect.exe --cocitation W4395000590 W4401955319
```
Running `.\citespect.exe` with no arguments opens the interactive CLI menu (Options 1–7).

---

## Example Inputs & How to Use Each Feature

- **Paper Search (Self-Citation & Reciprocal Citation):** Enter a DOI such as `10.1038/nature14539` or a cached Work ID such as `W2919115771`, then click **Analyze**. Inspect the summary band, 4-column citation table, and interactive SVG graph (click or `Tab` + `Enter`/`Space` on any node).
- **Bibliographic Coupling:** Under **Compare Papers**, select **Bibliographic Coupling (outgoing references)**, enter `W2919115771` and `W1901616594`, and click **Compare**.
- **Co-Citation Detection:** Under **Compare Papers**, select **Co-Citation (incoming citations)**, enter `W4395000590` and `W4401955319`, and click **Compare** (verified common citer: `W7129575785`).

---

## Testing Instructions & Verified Results

- **C99 Unit & Boundary Suites (`backend/tests/`):**
  - `test_author_hash.c` (19 passed)
  - `test_reciprocal_graph.c` (14 passed)
  - `test_reciprocal_detection.c` (25 passed)
  - `test_memory_boundary.c` (13 passed)
  - `test_openalex_parser.c` (12 passed)
  - `test_bibliographic_coupling.c` (12 passed)
  - `test_cocitation.c` (23 passed)
- **Flask Integration Tests:** `python -m unittest -v bridge/test_app.py` (14 passed)
- **Frontend JS DOM Tests:**
  - `node frontend/test_cocitation_ui.js` (6 passed)
  - `node frontend/test_redesign_ui.js` (12 passed)

---

## Known Limitations

- **Fixed Capacity Bounds:** `Paper` stores up to `50` authors (`PAPER_MAX_AUTHORS`), `1000` references (`PAPER_MAX_REFS`), and `31`-character IDs (`PAPER_ID_LEN - 1`).
- **Co-Citation Pagination Cap:** Fetches up to `5` OpenAlex pages (`200` citers/page = `1000` citers max per paper); papers exceeding `1000` citations are flagged incomplete (`UNVERIFIED` similarity) to prevent false negatives.
- **Network Dependency:** Live OpenAlex queries require `curl` and internet access; offline mode relies on `backend/data/*.csv`.
- **Windows MinGW Sanitizers:** Local MinGW GCC 6.3.0 does not ship `-lasan`/`-lubsan`.

---

## Team & Guide

- **Guide:** Mrs. Amruta Bhawarthi *(official designation to be verified by student)*
- **Team Members (Vishwakarma Institute of Technology, Pune — CSE AI):**
  1. Vishwas Vala — `1251030056`
  2. Siddhant Gangwal — `1251030383`
  3. Ved Rane — `1251030369`
  4. Sushant Zanwar — `1251030386`

---

## References & Acknowledgements

- **OpenAlex API:** [https://docs.openalex.org/](https://docs.openalex.org/)
- **Flask Documentation:** [https://flask.palletsprojects.com/](https://flask.palletsprojects.com/)
- **Course:** Data Structures (ML2301), Vishwakarma Institute of Technology, Pune.
