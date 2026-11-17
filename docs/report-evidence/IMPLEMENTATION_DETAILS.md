# Implementation Details

## Experimental Setup (Hardware & Software)
- **OS:** Windows (tested on Windows 10/11 x64)
- **Compiler:** GCC (MinGW `gcc -Wall -Wextra -std=c99 -Iinclude src/*.c -o citespect.exe`)
- **HTTP Client:** `curl` CLI invoked by `openalex.c`
- **Bridge Runtime:** Python 3 with `Flask` (`python bridge/app.py` on `127.0.0.1:5000`)
- **Frontend:** Vanilla HTML5, CSS3, ES5/ES6 JavaScript, inline SVG (no React, no build bundler)

## OpenAlex Endpoints Used
- **By DOI:** `https://api.openalex.org/works/https://doi.org/<DOI>?select=id,doi,title,publication_year,authorships,referenced_works`
- **By Work ID:** `https://api.openalex.org/works/<WORK_ID>?select=id,doi,title,publication_year,authorships,referenced_works`
- **Incoming Citers (Co-Citation):** `https://api.openalex.org/works?filter=cites:<WORK_ID>&select=id&per-page=200&page=<N>` (up to `5` pages = `1000` citers)

## CSV Storage Schemas (`backend/data/`)
- **`papers.csv`:** `paper_id,title,year,doi,openalex_id`
- **`authors.csv`:** `paper_id,author_id,author_name`
- **`citations.csv`:** `citing_paper_id,cited_paper_id` (uses `<paper_id>,` sentinel row to mark verified 0-reference papers)

## Flask API Endpoints (`bridge/app.py`)
- `POST /api/analyze` — accepts `{"doi": "<DOI_or_WorkID>"}`, checks local CSVs first, falls back to `citespect.exe --doi`.
- `POST /api/citation-analysis` — accepts `{"paper_id": "<W...>"}`, runs CLI option `4` via `citespect.exe`.
- `POST /api/coupling` — accepts `{"paper_a": "<W...>", "paper_b": "<W...>"}`, runs `citespect.exe --coupling`.
- `POST /api/cocitation` — accepts `{"paper_a": "<W...>", "paper_b": "<W...>"}`, runs `citespect.exe --cocitation`.
