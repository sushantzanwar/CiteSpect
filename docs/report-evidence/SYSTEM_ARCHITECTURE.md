# System Architecture and Flowcharts

## End-to-End Data Flow (Verified Against Code)
`User (Browser/CLI)` -> `Frontend (index.html, script.js)` -> `Flask Bridge (bridge/app.py)` -> `C99 Backend (citespect.exe)` -> `OpenAlex API (curl) / Local CSVs (backend/data/*.csv)` -> `C99 Algorithms (AuthorHash, CitationGraph, RefHash)` -> `Stdout / JSON` -> `Frontend UI & SVG Graph`

```mermaid
flowchart LR
    U[User] --> FE[Frontend HTML/JS/SVG]
    FE -->|POST /api/*| FL[Flask Bridge app.py]
    FL -->|subprocess citespect.exe| C99[C99 Backend]
    C99 -->|Read/Write| CSV[(backend/data/*.csv)]
    C99 -->|curl HTTPS| OA[OpenAlex API]
    C99 --> DSA[AuthorHash / CitationGraph / RefHash]
    DSA --> FL --> FE
```

---

## Overall System Workflow
```mermaid
flowchart TD
    S([Start]) --> M{Mode}
    M -->|Search DOI or Work ID| A1[POST /api/analyze]
    A1 --> C1{Cached in CSV?}
    C1 -->|Yes| P1[Return Paper JSON]
    C1 -->|No| O1[citespect.exe --doi]
    O1 --> P1
    P1 --> A2[POST /api/citation-analysis]
    A2 --> R1[Render Summary, Table, SVG Graph]
    M -->|Compare Papers| C2{Comparison Mode}
    C2 -->|Coupling| K1[POST /api/coupling]
    C2 -->|Co-Citation| K2[POST /api/cocitation]
    K1 --> R2[Render Comparison Results]
    K2 --> R2
```

---

## Self-Citation Detection Flowchart
```mermaid
flowchart TD
    S([Start]) --> B[Build AuthorHash 61 buckets from Citing Paper]
    B --> L{More Cited Papers?}
    L -->|No| E([Print Summary])
    L -->|Yes| F{Metadata Loaded?}
    F -->|No| U[NOT IDENTIFIED AS SELF + FETCH FAILED] --> L
    F -->|Yes| H[Lookup Cited Author IDs in AuthorHash]
    H --> M{Match Found?}
    M -->|Yes| Y[Classify SELF] --> L
    M -->|No| N[Classify NOT IDENTIFIED AS SELF] --> L
```

---

## Reciprocal Citation Detection Flowchart
```mermaid
flowchart TD
    S([Start]) --> R{Refs Available?}
    R -->|No| U[RECIPROCAL_UNKNOWN]
    R -->|Yes| G[Build CitationGraph Adjacency List for Cited Paper]
    G --> E{Edge Cited -> Citing?}
    E -->|Yes| Y[RECIPROCAL_CONFIRMED]
    E -->|No| N[RECIPROCAL_NOT]
```

---

## Bibliographic Coupling Flowchart
```mermaid
flowchart TD
    S([Start]) --> V{Both Refs Available?}
    V -->|No| U[COUPLING_UNKNOWN / UNVERIFIED]
    V -->|Yes| H1[Insert Unique Non-Self Refs of A into RefHash]
    H1 --> H2[Scan Refs of B, Count Overlap in RefHash]
    H2 --> C{Common > 0?}
    C -->|Yes| Y[COUPLING_YES + Jaccard %]
    C -->|No| N[COUPLING_NO + 0.00%]
```

---

## Co-Citation Detection Flowchart
```mermaid
flowchart TD
    S([Start]) --> D{IDs Valid & Distinct?}
    D -->|No| INV[Reject: COCITATION_INVALID]
    D -->|Yes| F[Fetch Paginated Citers from OpenAlex + CSV Reverse Edges]
    F --> H[Deduplicate & Intersect Third-Party Citers via RefHash]
    H --> C{Common > 0?}
    C -->|Yes| Y[COCITATION_YES]
    Y --> J{Both Sets Complete?}
    J -->|Yes| JE[Show Exact Jaccard %]
    J -->|No| JU[Show UNVERIFIED / N/A]
    C -->|No| K{Both Sets Complete?}
    K -->|Yes| N[COCITATION_NO + 0.00%]
    K -->|No| U[COCITATION_UNKNOWN + UNVERIFIED]
```
