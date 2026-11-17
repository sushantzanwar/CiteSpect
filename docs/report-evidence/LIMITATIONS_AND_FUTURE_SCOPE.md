# Limitations and Future Scope

## Current Limitations
1. **Fixed Capacity Bounds:** `Paper` structs cap authors at `50` (`PAPER_MAX_AUTHORS`), references at `1000` (`PAPER_MAX_REFS`), and IDs at `31` characters (`PAPER_ID_LEN - 1`).
2. **Co-Citation Pagination Cap:** `openalex_fetch_citing_ids()` fetches at most `5` pages (`200` citers/page = `1000` citers max). Papers exceeding `1000` citations are marked incomplete (`UNVERIFIED` similarity) to prevent false negatives.
3. **Network & `curl` Dependency:** Live OpenAlex metadata requires `curl` on `PATH` and internet access; offline mode is limited to cached CSVs.
4. **Local CSV Storage:** Flat CSV files (`papers.csv`, `authors.csv`, `citations.csv`) are suitable for single-user academic demos, not concurrent multi-user writes.
5. **MinGW Sanitizers:** Windows MinGW GCC 6.3.0 lacks `-lasan` and `-lubsan`.

## Future Scope
1. Replace flat CSV storage with SQLite or a persistent graph database for larger datasets.
2. Add cursor-based OpenAlex pagination or async background fetching for papers with `> 1000` citations.
3. Support batch BibTeX / RIS import and export of citation analysis reports.
4. Add multi-hop citation-chain visualization (depth $\ge 2$) in the SVG graph.
