# CITESPECT Design System

Direction: **Editorial Academic Software.** A quiet, light, text-first reading tool for citation data. Structure comes from spacing, hairline dividers and type hierarchy, not from boxes, shadows or color.

This file describes the frontend in `frontend/`. It is original to this project. It borrows two general ideas from public design analyses (one accent color used sparingly, hairline borders) and copies no brand's palette, type or marks.

## Principles

1. The paper being read is the strongest thing on the page.
2. One accent color. Everything else is neutral or a functional status color.
3. Information is separated by space and fine rules. A bordered container is used only where the boundary means something: the table, the selected-paper panel and the compare form.
4. Every status is a word first. Color and marker support the word and never replace it.
5. Nothing moves unless the movement explains a change. The interface is fully usable with animation off.
6. No decoration: no gradients, glass, glow, shadows, icons-for-their-own-sake, hero sections or marketing copy.

## Color

Light only.

| Token | Value | Use | Contrast |
|---|---|---|---|
| `--bg` | `#F8F8F6` | page | |
| `--surface` | `#FFFFFF` | table, panels, inputs | |
| `--text` | `#202420` | body text | 14.8:1 on bg |
| `--text-2` | `#5A645D` | metadata, captions, hints | 5.8:1 on bg |
| `--line` | `#E2E5E1` | dividers and row rules (decorative) | |
| `--line-strong` | `#8A938C` | borders of inputs and controls | 3.2:1 on white |
| `--accent` | `#246B57` | primary button, links, focus ring, selection, SELF | 6.3:1 on white |
| `--accent-dark` | `#1B5444` | primary button hover | |

### Relationship labels

Each label is the word, plus one marker so it never depends on color alone.

| Label | Color | Marker | Meaning |
|---|---|---|---|
| SELF | `--accent` | filled square | Shared author found |
| RECIPROCAL | `#2B5C9E` | filled circle | Cited paper also cites back |
| ONE-WAY | `#4A544E` | hollow square | Reverse citation not found in verified data |
| OTHER / UNCONFIRMED SELF | `--text-2` | none | No shared author established |
| UNVERIFIED | `#8A5A00` | dashed outline | Required data unavailable |
| FETCH FAILED | `#A23B2F` | solid outline | Metadata retrieval failed |
| PENDING | `--text-2` | none | Not analyzed yet |

All label colors are at least 5.8:1 on white. SELF and RECIPROCAL are independent, so a citation can show both. In the graph, SELF + RECIPROCAL is a green dot with a blue ring.

## Typography

- Sans: `"Segoe UI Variable Text", "Segoe UI", system-ui, -apple-system, "Helvetica Neue", Arial, sans-serif`. No downloads.
- Serif, for research-paper titles only: `Charter, "Iowan Old Style", "Palatino Linotype", Georgia, serif`.

| Role | Size | Weight |
|---|---|---|
| App name | 1.375rem | 700 |
| Paper title (serif) | 1.5–2.125rem, line-height 1.25 | 600 |
| Section heading | 1.375–1.625rem | 650 |
| Body | 1rem, line-height 1.6 | 400 |
| Table text | 0.9375rem | 400 |
| Metadata label | 0.75rem, sentence case | 600 |
| Caption and hint | 0.8125rem | 400 |
| Summary figure | 1.75rem, tabular numerals | 600 |

Work IDs and DOIs wrap anywhere (`overflow-wrap: anywhere`) so nothing is clipped at narrow widths or high zoom. No wide-tracked uppercase headings.

## Spacing, shape, layout

- Scale: 4, 8, 12, 16, 24, 32, 40, 64 px.
- Workspace: full available width with fluid 16–36 px side gutters. Supporting prose uses a shorter measure.
- Sections are separated by 64 px (32 px margin plus 32 px padding) and a hairline.
- Radius: 4 px for controls, 6 px for panels. No pills, no shadows.
- Minimum interactive target: 44 px.
- Breakpoints: 600 px for stacked mobile controls and 1080 px for the side-by-side graph and detail panel. The graph keeps its square SVG viewBox and caps at 680 px. The citation table scrolls within its own region on narrow screens.

## Components

- **Top bar:** app name, one-line descriptor (hidden below 600 px). A skip link precedes it.
- **Search:** a 48 px input with an attached primary button.
- **Paper header:** serif title, then a compact metadata row (year, DOI, Work ID, reference count), then authors inline. Beyond eight authors the rest collapse behind "and N more authors".
- **Summary:** three figures in one band separated by hairlines: Total citations, Self-citations, Reciprocal citations. A one-line note explains that SELF and RECIPROCAL are independent.
- **Citation table:** columns are Cited paper (ID link plus local title when known), Self-citation, Reciprocal, Shared authors and notes. The citing paper is stated once above the table rather than repeated in every row.
- **Graph:** the existing SVG. Labels appear only when they fit (see below). Selection is shown with a ring and a heavier edge. The details panel sits beside the graph on wide screens and under it on narrow ones.
- **Compare:** a two-option segmented control (Bibliographic Coupling, Co-Citation), a one-sentence explanation, two ID fields, one button, then a three-figure band and a list of OpenAlex links. Similarity that cannot be calculated reliably reads **UNVERIFIED**, never `0%`.
- **About these analyses:** a collapsed `<details>` holding the label definitions and a short description of each method. Explanations that matter for reading a result stay visible next to it.

### Graph label rule

Labels are drawn only if both hold: the rendered label size is at least 9 px, and all labels fit around the ring without touching (`count <= ring circumference / label width`). Otherwise labels are hidden and every node is still reachable by keyboard, hover or focus, showing its ID in a pill and its full details in the panel. The decision is re-evaluated when the window is resized.

## Motion

Only CSS transitions and the native Web Animations API (`element.animate`). No libraries.

| Event | Treatment | Duration |
|---|---|---|
| Hover and focus on links, buttons, rows | color and border-color | 140 ms |
| Button press | 1 px translate | 100 ms |
| A results section appears | fade plus 6 px rise, once | 220 ms |
| Selected-paper panel opens | fade plus 8 px slide | 200 ms |
| Mode switch hint | fade | 160 ms |
| Loading | text only, disabled button | none |

`prefers-reduced-motion: reduce` removes all transitions and scripted animation. If `element.animate` is missing or throws, the content simply appears.

## Accessibility

- Visible focus ring on every control (2 px accent, 2 px offset).
- Skip link, labeled inputs, `aria-live` status lines, table caption, `scope` on headers.
- Graph nodes are buttons (`tabindex=0`, `aria-pressed`) operable with Enter and Space; keyboard selection moves focus to the details panel, and Escape or Close returns focus to the node.
- Text reflows at 200 percent zoom and at 320 px width.

## Do not

Purple or pink gradients, glass, glow, shadows on everything, large radii, decorative icons or illustrations, fake statistics or testimonials, identical cards for unrelated content, hero banners, em dashes in interface text, animation that runs without a trigger.
