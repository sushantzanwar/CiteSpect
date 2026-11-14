/*
 * test_author_hash.c
 *
 * Unit tests for author_hash.c and SELF-citation detection algorithm.
 *
 * Tests all 8 required cases from Step 6:
 *   1. Shared author ID -> SELF
 *   2. No shared author ID -> NOT IDENTIFIED AS SELF
 *   3. Empty author IDs -> No false match
 *   4. Hash collision -> Correct author ID comparison via strcmp
 *   5. Duplicate author IDs -> No incorrect counting
 *   6. Multiple cited papers -> Same hash table reused
 *   7. Missing cited-paper metadata -> Handled safely
 *   8. Comparison: Nested-loop vs Hash-table -> Exactly identical results
 *
 * Compile and run:
 *   gcc -Wall -Wextra -std=c99 -Iinclude src/author_hash.c src/paper.c tests/test_author_hash.c -o tests/test_author_hash.exe
 *   .\tests\test_author_hash.exe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "author_hash.h"
#include "paper.h"

static int passed = 0;
static int failed = 0;

#define CHECK(desc, expr)                                          \
    do {                                                           \
        if (expr) {                                                \
            printf("[PASS] %s\n", desc);                          \
            passed++;                                              \
        } else {                                                   \
            printf("[FAIL] %s  (line %d)\n", desc, __LINE__);    \
            failed++;                                              \
        }                                                          \
    } while (0)

/* Helper: original nested-loop search for comparison testing */
static int nested_loop_has_shared_author(const Paper *citing, const Paper *cited)
{
    int i, j;
    for (i = 0; i < citing->author_count; i++) {
        if (citing->author_ids[i][0] == '\0')
            continue;
        for (j = 0; j < cited->author_count; j++) {
            if (cited->author_ids[j][0] == '\0')
                continue;
            if (strcmp(citing->author_ids[i], cited->author_ids[j]) == 0)
                return 1; /* Shared author found */
        }
    }
    return 0;
}

/* Helper: hash-table search for cited paper authors against citing table */
static int hash_table_has_shared_author(const AuthorHash *ht, const Paper *cited)
{
    int i;
    for (i = 0; i < cited->author_count; i++) {
        if (cited->author_ids[i][0] == '\0')
            continue;
        if (hash_search(ht, cited->author_ids[i]) != NULL)
            return 1; /* Match found */
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Test 1: Shared author ID -> SELF
 * ------------------------------------------------------------------ */
static void test_1_shared_author_self(void)
{
    AuthorHash ht;
    const AuthorNode *node;

    author_hash_init(&ht);
    hash_insert(&ht, "A5001226970", "Yann LeCun");

    node = hash_search(&ht, "A5001226970");
    CHECK("Test 1: Shared author ID found in hash table -> SELF", node != NULL);
    if (node != NULL) {
        CHECK("Test 1: Author ID matches exactly", strcmp(node->author_id, "A5001226970") == 0);
        CHECK("Test 1: Author name matches exactly", strcmp(node->author_name, "Yann LeCun") == 0);
    }
    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 2: No shared author ID -> NOT IDENTIFIED AS SELF
 * ------------------------------------------------------------------ */
static void test_2_no_shared_author(void)
{
    AuthorHash ht;
    const AuthorNode *node;

    author_hash_init(&ht);
    hash_insert(&ht, "A5001226970", "Yann LeCun");

    node = hash_search(&ht, "A5004270950"); /* Different author */
    CHECK("Test 2: Non-matching author returns NULL -> NOT IDENTIFIED AS SELF", node == NULL);

    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 3: Empty author IDs -> No false match
 * ------------------------------------------------------------------ */
static void test_3_empty_author_ids(void)
{
    AuthorHash ht;
    const AuthorNode *node;
    int ins_ok;

    author_hash_init(&ht);
    ins_ok = hash_insert(&ht, "", "Empty ID Author");
    CHECK("Test 3a: Empty author ID insertion rejected (returns 0)", ins_ok == 0);

    node = hash_search(&ht, "");
    CHECK("Test 3b: Searching empty author ID returns NULL", node == NULL);

    node = hash_search(&ht, NULL);
    CHECK("Test 3c: Searching NULL author ID returns NULL", node == NULL);

    /* Also verify hash_author_id on NULL/empty */
    CHECK("Test 3d: hash_author_id(NULL) returns 0", hash_author_id(NULL) == 0);

    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 4: Hash collision -> Correct author ID comparison via strcmp
 * ------------------------------------------------------------------ */
static void test_4_hash_collision(void)
{
    AuthorHash ht;
    char id_a[PAPER_ID_LEN] = "W0000000001";
    char id_b[PAPER_ID_LEN];
    unsigned int ha = hash_author_id(id_a);
    int found_collision = 0;
    int i;
    const AuthorNode *node_a;
    const AuthorNode *node_b;
    const AuthorNode *node_miss;

    for (i = 2; i < 10000; i++) {
        snprintf(id_b, sizeof(id_b), "W%010d", i);
        if (hash_author_id(id_b) == ha) {
            found_collision = 1;
            break;
        }
    }

    CHECK("Test 4a: Found colliding ID for bucket testing", found_collision == 1);

    author_hash_init(&ht);
    hash_insert(&ht, id_a, "Author A");
    hash_insert(&ht, id_b, "Author B");

    node_a = hash_search(&ht, id_a);
    CHECK("Test 4b: In collision bucket, search id_a retrieves id_a",
          node_a != NULL && strcmp(node_a->author_id, id_a) == 0);

    node_b = hash_search(&ht, id_b);
    CHECK("Test 4c: In collision bucket, search id_b retrieves id_b",
          node_b != NULL && strcmp(node_b->author_id, id_b) == 0);

    node_miss = hash_search(&ht, "W9999999999");
    CHECK("Test 4d: Absent ID in table returns NULL even with collisions", node_miss == NULL);

    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 5: Duplicate author IDs -> No incorrect counting
 * ------------------------------------------------------------------ */
static void test_5_duplicate_author_ids(void)
{
    AuthorHash ht;
    unsigned int idx;
    const AuthorNode *node;
    int chain_count = 0;

    author_hash_init(&ht);
    hash_insert(&ht, "A5001226970", "Yann LeCun");
    hash_insert(&ht, "A5001226970", "Yann LeCun"); /* Duplicate insertion */

    idx = hash_author_id("A5001226970");
    node = ht.buckets[idx];
    while (node != NULL) {
        chain_count++;
        node = node->next;
    }

    CHECK("Test 5: Duplicate ID not inserted twice into bucket chain", chain_count == 1);
    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 6: Multiple cited papers -> Same hash table reused
 * ------------------------------------------------------------------ */
static void test_6_multiple_cited_papers_reuse_table(void)
{
    Paper citing;
    Paper cited_1, cited_2, cited_3;
    AuthorHash ht;

    /* Citing paper: Yann LeCun and Yoshua Bengio */
    paper_init(&citing, "W_CITING", "Deep Learning", 2015, "10.1038/nature14539");
    paper_add_author(&citing, "A5001226970", "Yann LeCun");
    paper_add_author(&citing, "A5086198262", "Yoshua Bengio");

    /* Cited Paper 1: shares Yann LeCun */
    paper_init(&cited_1, "W_CITED_1", "Farabet Paper", 2013, "10.0/1");
    paper_add_author(&cited_1, "A5023316826", "Clement Farabet");
    paper_add_author(&cited_1, "A5001226970", "Yann LeCun");

    /* Cited Paper 2: shares no authors */
    paper_init(&cited_2, "W_CITED_2", "Deva Paper", 2012, "10.0/2");
    paper_add_author(&cited_2, "A5004270950", "D Deva");
    paper_add_author(&cited_2, "A5070638542", "Jose Manoel Seixas");

    /* Cited Paper 3: shares Yoshua Bengio */
    paper_init(&cited_3, "W_CITED_3", "Bengio Paper", 2014, "10.0/3");
    paper_add_author(&cited_3, "A5086198262", "Yoshua Bengio");

    /* Create ONE hash table for the citing paper */
    author_hash_init(&ht);
    author_hash_build(&ht, &citing);

    /* Reuse for Paper 1 */
    CHECK("Test 6a: Reused table classifies Cited 1 as SELF",
          hash_table_has_shared_author(&ht, &cited_1) == 1);

    /* Reuse for Paper 2 */
    CHECK("Test 6b: Reused table classifies Cited 2 as NOT SELF",
          hash_table_has_shared_author(&ht, &cited_2) == 0);

    /* Reuse for Paper 3 */
    CHECK("Test 6c: Reused table classifies Cited 3 as SELF",
          hash_table_has_shared_author(&ht, &cited_3) == 1);

    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 7: Missing cited-paper metadata -> Handled safely
 * ------------------------------------------------------------------ */
static void test_7_missing_metadata_handled_safely(void)
{
    Paper citing;
    Paper cited_empty;
    AuthorHash ht;

    paper_init(&citing, "W_CITING", "Citing Paper", 2020, "10.0/test");
    paper_add_author(&citing, "A5001226970", "Yann LeCun");

    /* Cited paper with 0 authors (e.g. metadata failed or missing authors) */
    paper_init(&cited_empty, "W_CITED_EMPTY", "No Authors", 2019, "");

    author_hash_init(&ht);
    author_hash_build(&ht, &citing);

    CHECK("Test 7a: Missing metadata cited paper safely returns NOT SELF",
          hash_table_has_shared_author(&ht, &cited_empty) == 0);

    /* NULL table check */
    CHECK("Test 7b: NULL table search safely returns NULL",
          hash_search(NULL, "A5001226970") == NULL);

    hash_free(&ht);
}

/* ------------------------------------------------------------------
 * Test 8: Comparison against nested-loop algorithm
 * ------------------------------------------------------------------ */
static void test_8_compare_nested_loop_vs_hash(void)
{
    Paper citing;
    Paper test_papers[6];
    AuthorHash ht;
    int i;
    int match_count = 0;

    paper_init(&citing, "W_CITING", "Deep Learning", 2015, "10.1038/nature14539");
    paper_add_author(&citing, "A5001226970", "Yann LeCun");
    paper_add_author(&citing, "A5086198262", "Yoshua Bengio");
    paper_add_author(&citing, "A5108093963", "Geoffrey E. Hinton");

    /* Paper 0: shares LeCun */
    paper_init(&test_papers[0], "W0", "P0", 2010, "");
    paper_add_author(&test_papers[0], "A5001226970", "Yann LeCun");

    /* Paper 1: shares nobody */
    paper_init(&test_papers[1], "W1", "P1", 2011, "");
    paper_add_author(&test_papers[1], "A1111111111", "Author 1");
    paper_add_author(&test_papers[1], "A2222222222", "Author 2");

    /* Paper 2: shares Hinton */
    paper_init(&test_papers[2], "W2", "P2", 2012, "");
    paper_add_author(&test_papers[2], "A5108093963", "Geoffrey E. Hinton");

    /* Paper 3: shares Bengio and LeCun */
    paper_init(&test_papers[3], "W3", "P3", 2013, "");
    paper_add_author(&test_papers[3], "A5086198262", "Yoshua Bengio");
    paper_add_author(&test_papers[3], "A5001226970", "Yann LeCun");

    /* Paper 4: empty author IDs */
    paper_init(&test_papers[4], "W4", "P4", 2014, "");
    paper_add_author(&test_papers[4], "", "Unknown");

    /* Paper 5: completely empty */
    paper_init(&test_papers[5], "W5", "P5", 2015, "");

    /* Build hash table */
    author_hash_init(&ht);
    author_hash_build(&ht, &citing);

    for (i = 0; i < 6; i++) {
        int res_nested = nested_loop_has_shared_author(&citing, &test_papers[i]);
        int res_hash   = hash_table_has_shared_author(&ht, &test_papers[i]);

        if (res_nested == res_hash) {
            match_count++;
        }
    }

    CHECK("Test 8: Hash-table detection produces 100% identical results to nested loop across all test papers",
          match_count == 6);

    hash_free(&ht);
}

int main(void)
{
    printf("=========================================\n");
    printf("   CITESPECT FODS HASH TABLE UNIT TESTS  \n");
    printf("=========================================\n\n");

    test_1_shared_author_self();
    test_2_no_shared_author();
    test_3_empty_author_ids();
    test_4_hash_collision();
    test_5_duplicate_author_ids();
    test_6_multiple_cited_papers_reuse_table();
    test_7_missing_metadata_handled_safely();
    test_8_compare_nested_loop_vs_hash();

    printf("\n-----------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("-----------------------------------------\n");

    return (failed == 0) ? 0 : 1;
}
