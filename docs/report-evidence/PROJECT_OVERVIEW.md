# Project Overview

- **Project:** CITESPECT — Research Paper Citation Analysis System
- **Institution:** Vishwakarma Institute of Technology, Pune
- **Department:** Computer Science & Engineering (Artificial Intelligence)
- **Course:** Data Structures (ML2301)
- **Academic Year:** 2026–27
- **Guide:** Mrs. Amruta Bhawarthi *(official designation not verified in repository — student must confirm)*
- **Team:**
  1. Vishwas Vala — `1251030056`
  2. Siddhant Gangwal — `1251030383`
  3. Ved Rane — `1251030369`
  4. Sushant Zanwar — `1251030386`

## Problem Statement
Evaluating citation relationships manually across academic reference lists is slow and error-prone. Researchers need to distinguish self-citations, reciprocal citations, bibliographic coupling, and co-citation while avoiding false negatives when citation metadata is incomplete.

## Objectives
1. Implement core Data Structures (Hash Table with Separate Chaining, Directed Graph via Adjacency List, Singly Linked List) from scratch in **C99** without external DSA libraries.
2. Integrate live **OpenAlex** metadata retrieval and local **CSV** caching.
3. Provide a **Flask** HTTP/JSON bridge and an accessible **HTML/CSS/JS** web workspace with an interactive **SVG** citation graph.
4. Handle incomplete, truncated, or failed API responses honestly (`UNKNOWN` / `UNVERIFIED` instead of `0%` or false `NO`).

## Scope
- **In Scope:** Single-paper DOI/Work ID lookup, self-citation and reciprocal citation analysis, two-paper Bibliographic Coupling and Co-Citation comparison, interactive SVG graph inspection, and local CSV caching.
- **Out of Scope:** Full-text PDF parsing, multi-user authentication, database servers, or external DSA/UI frameworks.
