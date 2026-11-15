#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "author_hash.h"
#include "citation_analysis.h"
#include "citation_graph.h"
#include "csv_store.h"
#include "openalex.h"
#include "paper.h"

static int passed = 0;
static int failed = 0;

static void check(const char *name, int condition)
{
    if (condition) {
        printf("[PASS] %s\n", name);
        passed++;
    } else {
        printf("[FAIL] %s\n", name);
        failed++;
    }
}

static void fill_id(char *buf, char prefix, size_t len)
{
    size_t i;

    buf[0] = prefix;
    for (i = 1; i < len; i++) {
        buf[i] = (char)('0' + (i % 10));
    }
    buf[len] = '\0';
}

static void test_null_and_empty_inputs(void)
{
    CitationGraph *g = citation_graph_create("W100");
    AuthorHash ht;
    Paper p;

    author_hash_init(&ht);
    paper_init(&p, "W100", NULL, 2024, NULL);

    check("T1: NULL graph pointer handled safely",
          citation_graph_add_reference(NULL, "W200") == 0 &&
          citation_graph_has_reference(NULL, "W200") == 0);
    citation_graph_free(NULL);

    check("T2: NULL paper ID rejected across modules",
          citation_graph_create(NULL) == NULL &&
          citation_graph_add_reference(g, NULL) == 0 &&
          citation_graph_has_reference(g, NULL) == 0 &&
          hash_insert(&ht, NULL, "Alice") == 0 &&
          hash_search(&ht, NULL) == NULL &&
          paper_already_saved(NULL) == 0 &&
          paper_has_citations_saved(NULL) == 0 &&
          openalex_fetch_by_id(NULL, &p) == OPENALEX_ERR_BAD_ID);

    check("T3: Empty paper ID rejected across modules",
          citation_graph_create("") == NULL &&
          citation_graph_add_reference(g, "") == 0 &&
          citation_graph_has_reference(g, "") == 0 &&
          hash_insert(&ht, "", "Alice") == 0 &&
          hash_search(&ht, "") == NULL &&
          paper_add_author(&p, "", "Alice") == 0 &&
          paper_add_reference(&p, "") == 0 &&
          paper_already_saved("") == 0 &&
          paper_has_citations_saved("") == 0 &&
          openalex_fetch_by_id("", &p) == OPENALEX_ERR_BAD_ID);

    check("T4: paper_init handles NULL title and DOI safely",
          p.title[0] == '\0' && p.doi[0] == '\0' &&
          strcmp(p.paper_id, "W100") == 0);

    citation_graph_free(g);
    hash_free(&ht);
}

static void test_id_length_boundaries(void)
{
    char max_work[PAPER_ID_LEN];
    char over_work[PAPER_ID_LEN + 16];
    char max_author[PAPER_ID_LEN];
    char over_author[PAPER_ID_LEN + 2];
    CitationGraph *g;
    AuthorHash ht;
    Paper p;

    fill_id(max_work, 'W', PAPER_ID_LEN - 1);
    fill_id(over_work, 'W', PAPER_ID_LEN);
    fill_id(max_author, 'A', PAPER_ID_LEN - 1);
    fill_id(over_author, 'A', PAPER_ID_LEN);

    g = citation_graph_create(max_work);
    check("T5: Max length Work ID (31 chars) accepted in graph",
          g != NULL &&
          citation_graph_add_reference(g, max_work) == 1 &&
          citation_graph_has_reference(g, max_work) == 1);

    check("T6: Oversized Work ID (32+ chars) rejected in graph (no truncation)",
          citation_graph_create(over_work) == NULL &&
          citation_graph_add_reference(g, over_work) == 0 &&
          citation_graph_has_reference(g, over_work) == 0);

    author_hash_init(&ht);
    check("T7: Max length Author ID (31 chars) accepted in hash table",
          hash_insert(&ht, max_author, "Max Author") == 1 &&
          hash_search(&ht, max_author) != NULL);

    check("T8: Oversized Author ID sharing 31-char prefix does NOT alias",
          hash_insert(&ht, over_author, "Over Author") == 0 &&
          hash_search(&ht, over_author) == NULL);

    paper_init(&p, max_work, "Title", 2024, "10.1/x");
    check("T9: Oversized IDs rejected by paper_add_author and paper_add_reference",
          paper_add_author(&p, max_author, "Valid") == 1 &&
          paper_add_author(&p, over_author, "TooLong") == 0 &&
          paper_add_reference(&p, max_work) == 1 &&
          paper_add_reference(&p, over_work) == 0 &&
          openalex_fetch_by_id(over_work, &p) == OPENALEX_ERR_BAD_ID);

    citation_graph_free(g);
    hash_free(&ht);
}

static void test_array_bounds(void)
{
    static Paper p;
    AuthorHash ht;
    char id[PAPER_ID_LEN];
    int i;
    int all_ok = 1;

    paper_init(&p, "W999", "Bounds Paper", 2024, "10.1/b");
    for (i = 0; i < PAPER_MAX_AUTHORS; i++) {
        sprintf(id, "A%d", i + 1);
        if (!paper_add_author(&p, id, "Author"))
            all_ok = 0;
    }
    check("T10: paper_add_author stops cleanly at PAPER_MAX_AUTHORS",
          all_ok && p.author_count == PAPER_MAX_AUTHORS &&
          paper_add_author(&p, "A9999", "Overflow") == 0 &&
          p.author_count == PAPER_MAX_AUTHORS);

    all_ok = 1;
    for (i = 0; i < PAPER_MAX_REFS; i++) {
        sprintf(id, "W%d", i + 1);
        if (!paper_add_reference(&p, id))
            all_ok = 0;
    }
    check("T11: paper_add_reference stops cleanly at PAPER_MAX_REFS",
          all_ok && p.reference_count == PAPER_MAX_REFS &&
          paper_add_reference(&p, "W99999") == 0 &&
          p.reference_count == PAPER_MAX_REFS);

    /* Simulate corrupted out-of-bounds author_count safely capped by build */
    p.author_count = PAPER_MAX_AUTHORS + 25;
    author_hash_init(&ht);
    author_hash_build(&ht, &p);
    check("T12: author_hash_build caps out-of-bounds author_count safely",
          hash_search(&ht, "A1") != NULL &&
          hash_search(&ht, "A50") != NULL);
    hash_free(&ht);
}

static void test_csv_boundaries_isolated(void)
{
    char over_id[PAPER_ID_LEN + 10];
    char cited_ids[4][PAPER_ID_LEN];
    FILE *f;
    Paper loaded;
    int count;

    fill_id(over_id, 'W', PAPER_ID_LEN + 4);

    /* Ensure temp data directory exists in current working directory */
    f = fopen("data/citations.csv", "w");
    if (f == NULL) {
        check("T13: Isolated CSV boundary test setup (data/citations.csv)", 0);
        return;
    }
    fprintf(f, "citing_paper_id,cited_paper_id\n");
    fprintf(f, "W100,W200\n");
    fprintf(f, "W100,%s\n", over_id);
    fclose(f);

    count = csv_load_citations("W100", cited_ids, 4);
    paper_init(&loaded, "W100", "", 0, "");
    check("T13: csv_store rejects oversized cited_id and oversized query IDs",
          count == 1 &&
          strcmp(cited_ids[0], "W200") == 0 &&
          paper_already_saved(over_id) == 0 &&
          paper_has_citations_saved(over_id) == 0 &&
          csv_load_citations(over_id, cited_ids, 4) == 0 &&
          csv_load_authors(over_id, &loaded) == 0);
}

int main(void)
{
    printf("==============================================\n");
    printf("  CITESPECT MEMORY & BOUNDARY SAFETY TESTS    \n");
    printf("==============================================\n\n");

    test_null_and_empty_inputs();
    test_id_length_boundaries();
    test_array_bounds();
    test_csv_boundaries_isolated();

    printf("\n----------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("----------------------------------------------\n");

    return failed > 0 ? 1 : 0;
}

