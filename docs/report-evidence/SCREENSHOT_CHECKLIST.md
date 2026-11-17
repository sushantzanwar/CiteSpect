# Screenshot Checklist

Browser automation (`chrome_devtools` MCP) is unavailable in this environment, so screenshots were **not** captured automatically. Use the steps below to capture real screenshots using a **separate copy** of `backend/data/` so real CSVs are never modified.

## Safe Demo Setup (Separate Data Copy)
```powershell
$demo = Join-Path $env:TEMP "citespect_demo"
New-Item -ItemType Directory -Force -Path (Join-Path $demo "data") | Out-Null
Copy-Item backend/data/*.csv (Join-Path $demo "data")
$env:CITESPECT_WORKDIR = $demo
python bridge/app.py
```
Open `http://127.0.0.1:5000` in Chrome/Edge.

## Required Screenshots (9)
1. **Home page** — Initial workspace at `1440px` desktop width (`Understand how research connects.`, search form, Compare Papers).
2. **DOI / Work ID analysis** — Search input filled with `10.1038/nature14539` or `W2919115771` and status/result loaded.
3. **Paper metadata** — Serif paper title, Year, DOI, clickable Work ID, reference count, and author list.
4. **Citation summary** — 3-figure summary band (`Total Citations`, `Self-Citations`, `Reciprocal Citations`).
5. **Citation details** — 4-column table showing `SELF`, `RECIPROCAL`, `ONE-WAY`, `OTHER / UNCONFIRMED SELF`, `UNVERIFIED`, or `FETCH FAILED` labels.
6. **Interactive citation graph** — SVG radial graph with directional arrows and legend.
7. **Selected graph node details** — Side detail panel open after clicking or pressing `Enter`/`Space` on a cited node.
8. **Bibliographic coupling results** — Compare Papers (`W2919115771` vs. `W1901616594`) showing `YES`, counts, Jaccard %, and shared reference links.
9. **Co-citation results** — Compare Papers in Co-Citation mode (`W4395000590` vs. `W4401955319`) showing `YES`, `data_complete`, `3.45%` Jaccard, and clickable co-citing paper `W7129575785`.
