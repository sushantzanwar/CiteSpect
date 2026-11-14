#include <stdio.h>
#include <string.h>
#include "citation_analysis.h"
#include "author_hash.h"
#include "citation_graph.h"
#include "csv_store.h"
#include "openalex.h"

/*
 * CitationSummary counts outcomes for the final report.
 * self_count        - confirmed self-citations (shared author found).
 * not_self_count    - citations not identified as self.
 * recip_conf_count  - confirmed reciprocal citations (B also cites A).
 * recip_not_count   - not reciprocal (B's refs available and A not found).
 * recip_unk_count   - reciprocal status unknown (B's refs unavailable/failed).
 * self_ref_count    - the citing paper references itself by Work ID (skipped).
 * dup_count         - duplicate cited-ID records in citations.csv (skipped).
 */
typedef struct {
    int self_count;
    int not_self_count;
    int recip_conf_count;
    int recip_not_count;
    int recip_unk_count;
    int self_ref_count;
    int dup_count;
} CitationSummary;

/* ------------------------------------------------------------------
 * Hash-table-based shared-author search
 * ------------------------------------------------------------------ */

/*
 * find_self_authors_hash
 *
 * Looks up each non-empty author ID of *cited* in the pre-built hash
 * table of the citing paper's authors.
 *
 * Parameters:
 *   citing_ht   - hash table built from the citing paper's author IDs.
 *   cited       - the cited paper whose authors we are checking.
 *   out_ids     - caller-supplied array: receives matched author IDs.
 *   out_names   - caller-supplied array: receives matched author names.
 *   max_matches - capacity of out_ids / out_names arrays.
 *
 * Returns the number of shared authors found (0 = no match = not self).
 */
static int find_self_authors_hash(const AuthorHash *citing_ht,
                                  const Paper *cited,
                                  char out_ids[][PAPER_ID_LEN],
                                  char out_names[][AUTHOR_NAME_LEN],
                                  int max_matches)
{
    int i, k;
    int count = 0;
    const AuthorNode *hit;

    for (i = 0; i < cited->author_count && count < max_matches; i++) {
        if (cited->author_ids[i][0] == '\0')
            continue;
        hit = hash_search(citing_ht, cited->author_ids[i]);
        if (hit != NULL) {
            /* Avoid reporting the same shared author more than once */
            int already_added = 0;
            for (k = 0; k < count; k++) {
                if (strcmp(out_ids[k], hit->author_id) == 0) {
                    already_added = 1;
                    break;
                }
            }
            if (!already_added) {
                strncpy(out_ids  [count], hit->author_id,   PAPER_ID_LEN   - 1);
                strncpy(out_names[count], hit->author_name, AUTHOR_NAME_LEN - 1);
                out_ids  [count][PAPER_ID_LEN   - 1] = '\0';
                out_names[count][AUTHOR_NAME_LEN - 1] = '\0';
                count++;
            }
        }
    }
    return count;
}

/* ------------------------------------------------------------------
 * Output helpers
 * ------------------------------------------------------------------ */

static void print_citation_result(const char *cited_id,
                                  int is_self,
                                  const char shared_ids[][PAPER_ID_LEN],
                                  const char shared_names[][AUTHOR_NAME_LEN],
                                  int shared_count,
                                  int recip_status)
{
    int i;

    printf("Cited Paper: %s\n", cited_id);

    /* SELF-citation line */
    if (is_self) {
        printf("Self Citation: SELF\n");
        for (i = 0; i < shared_count; i++) {
            if (shared_names[i][0] != '\0')
                printf("Shared Author: %s (%s)\n", shared_ids[i], shared_names[i]);
            else
                printf("Shared Author: %s\n", shared_ids[i]);
        }
    } else {
        printf("Self Citation: NOT IDENTIFIED AS SELF\n");
    }

    /* Reciprocal Cross-Citation line */
    if (recip_status == RECIPROCAL_CONFIRMED) {
        printf("Reciprocal Citation: CONFIRMED RECIPROCAL\n");
    } else if (recip_status == RECIPROCAL_NOT) {
        printf("Reciprocal Citation: NOT RECIPROCAL\n");
    } else {
        printf("Reciprocal Citation: UNKNOWN\n");
    }

    printf("\n---------------------------------------\n\n");
}

/* ------------------------------------------------------------------
 * Load a cited paper (from CSV cache or OpenAlex)
 * ------------------------------------------------------------------ */

/*
 * fetch_missing_references
 *
 * Fetches a cached paper from OpenAlex to obtain its outgoing references
 * when they have not yet been recorded in citations.csv.
 * Returns 1 if references were retrieved and verified, 0 on failure.
 */
static int fetch_missing_references(const char *paper_id, Paper *out)
{
    static Paper fetched;
    int i;

    printf("Fetching references for %s from OpenAlex...\n", paper_id);
    if (openalex_fetch_by_id(paper_id, &fetched) != OPENALEX_OK)
        return 0;

    for (i = 0; i < fetched.reference_count; i++)
        paper_add_reference(out, fetched.references[i]);

    if (csv_save_citations(out) == CSV_ERR_FILE)
        printf("Warning: could not save references for %s.\n", paper_id);
    return 1;
}

/*
 * load_cached_cited_paper
 *
 * Loads authors from authors.csv, then loads references from citations.csv
 * if available, or fetches missing references from OpenAlex once.
 */
static int load_cached_cited_paper(const char *paper_id, Paper *out, int *refs_available)
{
    paper_init(out, paper_id, "", 0, "");
    csv_load_authors(paper_id, out);

    if (paper_has_citations_saved(paper_id)) {
        out->reference_count = csv_load_citations(paper_id, out->references, PAPER_MAX_REFS);
        *refs_available = 1;
        return OPENALEX_OK;
    }

    *refs_available = fetch_missing_references(paper_id, out);
    return OPENALEX_OK;
}

/*
 * load_cited_paper
 *
 * Populates *out* with authors and references.
 * Sets *refs_available to 1 if references were successfully retrieved
 * or verified, or 0 if unavailable.
 */
static int load_cited_paper(const char *paper_id, Paper *out, int *refs_available)
{
    int code;
    *refs_available = 0;

    if (paper_already_saved(paper_id))
        return load_cached_cited_paper(paper_id, out, refs_available);

    printf("Fetching %s from OpenAlex...\n", paper_id);
    code = openalex_fetch_by_id(paper_id, out);
    if (code != OPENALEX_OK)
        return code;

    /* Referenced works were fetched directly by openalex_fetch_by_id */
    *refs_available = 1;

    if (csv_save_paper(out) == CSV_ERR_FILE)
        printf("Warning: could not save %s to the CSV files.\n", paper_id);
    return OPENALEX_OK;
}

/* ------------------------------------------------------------------
 * Check reciprocal citation using CitationGraph
 * ------------------------------------------------------------------ */

static int check_reciprocal(const Paper *cited, const char *citing_id, int refs_available)
{
    CitationGraph *graph;
    int has_rev;
    int i;

    if (!refs_available)
        return RECIPROCAL_UNKNOWN;

    graph = citation_graph_create(cited->paper_id);
    if (graph == NULL)
        return RECIPROCAL_UNKNOWN;

    for (i = 0; i < cited->reference_count; i++) {
        citation_graph_add_reference(graph, cited->references[i]);
    }

    has_rev = citation_graph_has_reference(graph, citing_id);
    citation_graph_free(graph);

    return has_rev ? RECIPROCAL_CONFIRMED : RECIPROCAL_NOT;
}

/* ------------------------------------------------------------------
 * Per-citation analysis
 * ------------------------------------------------------------------ */

/*
 * analyze_one
 *
 * Uses the pre-built AuthorHash of the citing paper to check one cited
 * paper for shared authors, and checks for reciprocal cross-citation
 * (B -> A) using CitationGraph.
 */
static void analyze_one(const char *citing_id,
                        const AuthorHash *citing_ht,
                        const char *cited_id,
                        CitationSummary *summary)
{
    static Paper cited;
    char shared_ids  [PAPER_MAX_AUTHORS][PAPER_ID_LEN];
    char shared_names[PAPER_MAX_AUTHORS][AUTHOR_NAME_LEN];
    int  shared_count;
    int  refs_available = 0;
    int  code = load_cited_paper(cited_id, &cited, &refs_available);
    int  recip_status;
    int  is_self;

    if (code != OPENALEX_OK) {
        printf("Cited Paper: %s\n", cited_id);
        printf("Self Citation: NOT IDENTIFIED AS SELF (metadata retrieval failed: %s)\n",
               openalex_error_message(code));
        printf("Reciprocal Citation: UNKNOWN\n");
        printf("\n---------------------------------------\n\n");
        summary->not_self_count++;
        summary->recip_unk_count++;
        return;
    }

    /* 1. SELF-citation detection via Hash Table */
    shared_count = find_self_authors_hash(citing_ht, &cited,
                                          shared_ids, shared_names,
                                          PAPER_MAX_AUTHORS);
    is_self = (shared_count > 0);
    if (is_self)
        summary->self_count++;
    else
        summary->not_self_count++;

    /* 2. Reciprocal Cross-Citation detection via CitationGraph */
    recip_status = check_reciprocal(&cited, citing_id, refs_available);
    if (recip_status == RECIPROCAL_CONFIRMED)
        summary->recip_conf_count++;
    else if (recip_status == RECIPROCAL_NOT)
        summary->recip_not_count++;
    else
        summary->recip_unk_count++;

    print_citation_result(cited_id, is_self, shared_ids, shared_names, shared_count, recip_status);
}

/* ------------------------------------------------------------------
 * Duplicate detection
 * ------------------------------------------------------------------ */

static int is_duplicate(char cited_ids[][PAPER_ID_LEN], int index)
{
    int i;

    for (i = 0; i < index; i++) {
        if (strcmp(cited_ids[i], cited_ids[index]) == 0)
            return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Loop over all cited papers using a single hash table
 * ------------------------------------------------------------------ */

static void analyze_all(const char *citing_id,
                        const AuthorHash *citing_ht,
                        char cited_ids[][PAPER_ID_LEN],
                        int total,
                        CitationSummary *summary)
{
    int i;

    for (i = 0; i < total; i++) {
        if (strcmp(cited_ids[i], citing_id) == 0) {
            summary->self_ref_count++;
        } else if (is_duplicate(cited_ids, i)) {
            summary->dup_count++;
        } else {
            analyze_one(citing_id, citing_ht, cited_ids[i], summary);
        }
    }
}

/* ------------------------------------------------------------------
 * Summary printout
 * ------------------------------------------------------------------ */

static void print_summary(const CitationSummary *summary)
{
    int analyzed = summary->self_count + summary->not_self_count;

    printf("========== SUMMARY ==========\n\n");
    printf("Citations Checked: %d\n", analyzed);
    printf("Self Citations: %d\n", summary->self_count);
    printf("Confirmed Reciprocal Citations: %d\n", summary->recip_conf_count);
    printf("Not Reciprocal: %d\n", summary->recip_not_count);
    printf("Reciprocal Unknown: %d\n", summary->recip_unk_count);
    if (summary->self_ref_count > 0 || summary->dup_count > 0) {
        printf("\nSkipped:\n");
        if (summary->self_ref_count > 0)
            printf("Self-references   : %d (citing paper references itself)\n",
                   summary->self_ref_count);
        if (summary->dup_count > 0)
            printf("Duplicate records : %d\n", summary->dup_count);
    }
}

/* ------------------------------------------------------------------
 * Public entry point
 * ------------------------------------------------------------------ */

/*
 * analyze_paper_citations
 *
 * 1. Load the citing paper's author IDs from CSV.
 * 2. Build an AuthorHash table once from those IDs.
 * 3. Load the list of cited paper IDs from citations.csv.
 * 4. For each cited paper, call analyze_one() which reuses the same
 *    hash table (no rebuild per cited paper) and checks reciprocal
 *    cross-citation via CitationGraph.
 * 5. Free the hash table, print the summary, return.
 */
int analyze_paper_citations(const char *citing_id)
{
    static Paper citing;
    static char cited_ids[PAPER_MAX_REFS][PAPER_ID_LEN];
    CitationSummary summary = {0, 0, 0, 0, 0, 0, 0};
    AuthorHash citing_ht;
    int total;

    if (!paper_already_saved(citing_id)) {
        printf("Paper %s is not in %s. Add it first with option 3.\n",
               citing_id, PAPERS_CSV);
        return 0;
    }

    paper_init(&citing, citing_id, "", 0, "");
    csv_load_authors(citing_id, &citing);

    /* Build the hash table once from the citing paper's author IDs. */
    author_hash_init(&citing_ht);
    author_hash_build(&citing_ht, &citing);

    total = csv_load_citations(citing_id, cited_ids, PAPER_MAX_REFS);
    if (total == 0) {
        printf("No citation records found for %s in %s.\n",
               citing_id, CITATIONS_CSV);
        author_hash_free(&citing_ht);
        return 0;
    }

    printf("\n========== CITATION ANALYSIS ==========\n\n");
    printf("Citing Paper: %s\n\n", citing_id);
    analyze_all(citing_id, &citing_ht, cited_ids, total, &summary);
    print_summary(&summary);

    /* Free all hash-table nodes. */
    author_hash_free(&citing_ht);
    return 1;
}

/* ------------------------------------------------------------------
 * classify_citation — kept for API compatibility
 *
 * Still performs its original O(n*m) nested-loop comparison so that
 * any code calling this function directly continues to work unchanged.
 * The citation-analysis workflow above uses the hash table instead.
 * ------------------------------------------------------------------ */

static int find_shared_authors(const Paper *citing, const Paper *cited,
                               int shared_index[])
{
    int i, j, count = 0;

    for (i = 0; i < citing->author_count; i++) {
        if (citing->author_ids[i][0] == '\0')
            continue;
        for (j = 0; j < cited->author_count; j++) {
            if (strcmp(citing->author_ids[i], cited->author_ids[j]) == 0) {
                shared_index[count++] = i;
                break;
            }
        }
    }
    return count;
}

static int authors_are_complete(const Paper *paper)
{
    int i;

    if (paper->author_count == 0 || paper->author_count >= PAPER_MAX_AUTHORS)
        return 0;
    for (i = 0; i < paper->author_count; i++) {
        if (paper->author_ids[i][0] == '\0')
            return 0;
    }
    return 1;
}

int classify_citation(const Paper *citing, const Paper *cited,
                      int shared_index[], int *shared_count)
{
    *shared_count = find_shared_authors(citing, cited, shared_index);

    if (*shared_count > 0)
        return CITATION_SELF;
    if (authors_are_complete(citing) && authors_are_complete(cited))
        return CITATION_CROSS;
    return CITATION_UNKNOWN;
}
