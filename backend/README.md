# CITESPECT — Research Paper Citation Analysis System

A FODS college project written in C. CITESPECT will analyse citation relationships between research papers using data structures and algorithms implemented from scratch (no external DSA libraries). Real paper data comes from the OpenAlex API.

## Architecture (current)

```
CITESPECT/
├── src/       C source files
├── include/   header files
├── data/      local CSV dataset (papers, authors, citations) — empty for now
├── tests/     test programs (later)
├── docs/      documentation (later)
└── README.md
```

Planned data flow:
PDF/uploaded paper -> identify title/authors -> OpenAlex API -> paper metadata + referenced works -> C data structures.

## Current stage

Stage 3: Paper data model, OpenAlex lookup by DOI, and saving the results to CSV files in `data/`. No DSA algorithms yet (no search, sort, graph, BFS/DFS, hashing, BST).

## Files

- `include/paper.h` — the `Paper` struct (id, title, year, DOI, author list, referenced-work ID list) and its function declarations.
- `src/paper.c` — `paper_init`, `paper_add_author`, `paper_add_reference`, `paper_display`. Only Paper-related code lives here.
- `include/openalex.h`, `src/openalex.c` — `openalex_fetch_by_doi`: downloads the work with the `curl` command, then reads the JSON with small hand-written parsing helpers and fills a `Paper`.
- `include/csv_store.h`, `src/csv_store.c` — `csv_save_paper`: appends the paper to `data/papers.csv` (`paper_id,title,year,doi,openalex_id`), its authors to `data/authors.csv` (`paper_id,author_id,author_name`) and its references to `data/citations.csv` (`citing_paper_id,cited_paper_id`). Header rows are written when a file is empty, and a paper already in `papers.csv` is skipped. The OpenAlex work ID is used as `paper_id`.
- `include/config.h`, `src/config.c` — reads the OpenAlex API key from the `OPENALEX_API_KEY` environment variable (never hard-coded).
- `src/main.c` — takes a DOI (argument or prompt), looks it up through OpenAlex and displays the paper.

## Build and run

```
gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o citespect
./citespect 10.1038/nature14539   # run from this folder so data/ is found
```

Requires the `curl` command (included with Windows 10/11). Limits: first 50 authors and first 1000 referenced works are stored.

Set the API key (optional; without it requests are anonymous):

- PowerShell: `$env:OPENALEX_API_KEY = "your-key"`
- Bash: `export OPENALEX_API_KEY=your-key`

## Web demo (frontend + bridge)

```
cd backend && gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o citespect
cd ../bridge && pip install flask && python app.py
```
Open http://127.0.0.1:5000. The page sends the DOI to `bridge/app.py`, which runs `citespect.exe --doi <DOI>`; the C program does the OpenAlex lookup, CSV save and citation graph, and prints `KEY=value` lines that the bridge turns into JSON.
