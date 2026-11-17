# Module Descriptions

Only files that actually exist in the repository are listed below.

| Module | File(s) | Purpose & Key Functions |
|---|---|---|
| **CLI & Entry Point** | `backend/src/main.c` | Parses `--doi`, `--coupling`, `--cocitation` flags or runs the 7-option interactive menu (`run_menu`, `run_doi_mode`). |
| **Paper Model** | `backend/include/paper.h`, `backend/src/paper.c` | Defines `Paper` struct (`PAPER_MAX_AUTHORS=50`, `PAPER_MAX_REFS=1000`, `PAPER_ID_LEN=32`) and `paper_init`, `paper_add_author`, `paper_add_reference`, `paper_display`. |
| **Paper Linked List** | `backend/include/paper_list.h`, `backend/src/paper_list.c` | Singly linked list `PaperNode` for CLI paper loading and lookup (`paper_list_insert`, `paper_list_search`, `paper_list_count`, `paper_list_load`, `paper_list_free`). |
| **Author Hash Table** | `backend/include/author_hash.h`, `backend/src/author_hash.c` | 61-bucket separate-chaining hash table (`author_hash_init`, `hash_author_id`, `hash_insert`, `hash_search`, `author_hash_build`, `hash_free`). |
| **Citation Graph** | `backend/include/citation_graph.h`, `backend/src/citation_graph.c` | Adjacency-list directed graph (`citation_graph_create`, `citation_graph_add_reference`, `citation_graph_has_reference`, `citation_graph_display`, `citation_graph_free`). |
| **Citation Analysis** | `backend/include/citation_analysis.h`, `backend/src/citation_analysis.c` | Orchestrates all 4 analyses: `analyze_paper_citations`, `citation_check_reciprocal`, `citation_compute_coupling`, `compare_papers_coupling`, `citation_compute_cocitation`, `compare_papers_cocitation`. |
| **OpenAlex Client** | `backend/include/openalex.h`, `backend/src/openalex.c` | Executes `curl` requests and hand-written JSON parsing (`openalex_parse_work`, `openalex_fetch_by_doi`, `openalex_fetch_by_id`, `openalex_parse_citing_page`, `openalex_collect_citing_pages`, `openalex_fetch_citing_ids`). |
| **CSV Storage** | `backend/include/csv_store.h`, `backend/src/csv_store.c` | Reads/writes `data/papers.csv`, `data/authors.csv`, `data/citations.csv` (`csv_save_paper`, `csv_load_authors`, `csv_load_citations`, `csv_load_citing_papers`, `paper_already_saved`, `paper_has_citations_saved`). |
| **Config** | `backend/include/config.h`, `backend/src/config.c` | Reads optional `OPENALEX_API_KEY` from environment (`config_get_openalex_key`). |
| **Flask Bridge** | `bridge/app.py` | Serves `frontend/` and exposes `POST /api/analyze`, `/api/citation-analysis`, `/api/coupling`, `/api/cocitation` with thread-safe `c_program_lock`. |
| **Frontend UI** | `frontend/index.html`, `frontend/style.css`, `frontend/script.js` | Research workspace UI, 3-counter summary, 4-column table, adaptive SVG citation graph with keyboard panel, and Bibliographic Coupling / Co-Citation comparison. |
