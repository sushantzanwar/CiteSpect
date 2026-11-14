/*
 * test_reciprocal_graph.c
 *
 * Unit tests for citation_graph_has_reference() and reciprocal citation detection.
 *
 * Test cases:
 *   1. Search in graph with multiple references (B found -> 1, C found -> 1)
 *   2. Search for absent reference (D -> 0)
 *   3. Guard checks: NULL graph, NULL target_id, empty target_id ("") -> all return 0
 *   4. Empty graph (no references added) -> returns 0
 *   5. Reciprocal example:
 *      Graph A contains B (A -> B)
 *      Graph B contains A (B -> A)
 *      Reciprocity confirmed: has_ref(A, B) == 1 && has_ref(B, A) == 1
 *   6. Non-reciprocal example:
 *      Graph A contains C (A -> C)
 *      Graph C does NOT contain A
 *      Non-reciprocal confirmed: has_ref(A, C) == 1 && has_ref(C, A) == 0
 */

#include <stdio.h>
#include <string.h>
#include "citation_graph.h"

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

static void test_basic_search_and_guards(void)
{
    CitationGraph *graph_a = citation_graph_create("W_PAPER_A");

    /* Test 4: Empty graph */
    CHECK("T1: Empty graph returns 0 for any search",
          citation_graph_has_reference(graph_a, "W_PAPER_B") == 0);

    /* Add references B and C */
    citation_graph_add_reference(graph_a, "W_PAPER_B");
    citation_graph_add_reference(graph_a, "W_PAPER_C");

    /* Test 1: Searching for present references */
    CHECK("T2: Searching for present reference B returns 1",
          citation_graph_has_reference(graph_a, "W_PAPER_B") == 1);
    CHECK("T3: Searching for present reference C returns 1",
          citation_graph_has_reference(graph_a, "W_PAPER_C") == 1);

    /* Test 2: Searching for absent reference */
    CHECK("T4: Searching for absent reference D returns 0",
          citation_graph_has_reference(graph_a, "W_PAPER_D") == 0);

    /* Test 3: Safety guards */
    CHECK("T5: Searching with NULL target_id returns 0",
          citation_graph_has_reference(graph_a, NULL) == 0);
    CHECK("T6: Searching with empty target_id returns 0",
          citation_graph_has_reference(graph_a, "") == 0);
    CHECK("T7: Searching on NULL graph returns 0",
          citation_graph_has_reference(NULL, "W_PAPER_B") == 0);
    CHECK("T8: Searching NULL graph with NULL target returns 0",
          citation_graph_has_reference(NULL, NULL) == 0);

    citation_graph_free(graph_a);
}

static void test_reciprocal_scenario(void)
{
    /* Graph A references B */
    CitationGraph *graph_a = citation_graph_create("W_PAPER_A");
    citation_graph_add_reference(graph_a, "W_PAPER_B");

    /* Graph B references A */
    CitationGraph *graph_b = citation_graph_create("W_PAPER_B");
    citation_graph_add_reference(graph_b, "W_PAPER_A");

    /* Verify both directed edges exist (A -> B and B -> A) */
    int a_cites_b = citation_graph_has_reference(graph_a, "W_PAPER_B");
    int b_cites_a = citation_graph_has_reference(graph_b, "W_PAPER_A");

    CHECK("T9: Reciprocal test - A cites B", a_cites_b == 1);
    CHECK("T10: Reciprocal test - B cites A", b_cites_a == 1);
    CHECK("T11: Reciprocal relationship confirmed (A <-> B)",
          a_cites_b == 1 && b_cites_a == 1);

    citation_graph_free(graph_a);
    citation_graph_free(graph_b);
}

static void test_non_reciprocal_scenario(void)
{
    /* Graph A references C */
    CitationGraph *graph_a = citation_graph_create("W_PAPER_A");
    citation_graph_add_reference(graph_a, "W_PAPER_C");

    /* Graph C references other papers, but NOT A */
    CitationGraph *graph_c = citation_graph_create("W_PAPER_C");
    citation_graph_add_reference(graph_c, "W_PAPER_X");
    citation_graph_add_reference(graph_c, "W_PAPER_Y");

    /* Verify A -> C exists, but C -> A does not */
    int a_cites_c = citation_graph_has_reference(graph_a, "W_PAPER_C");
    int c_cites_a = citation_graph_has_reference(graph_c, "W_PAPER_A");

    CHECK("T12: Non-reciprocal test - A cites C", a_cites_c == 1);
    CHECK("T13: Non-reciprocal test - C does not cite A", c_cites_a == 0);
    CHECK("T14: Non-reciprocal relationship confirmed (A -> C only)",
          a_cites_c == 1 && c_cites_a == 0);

    citation_graph_free(graph_a);
    citation_graph_free(graph_c);
}

int main(void)
{
    printf("==============================================\n");
    printf("  CITESPECT RECIPROCAL GRAPH SEARCH TESTS    \n");
    printf("==============================================\n\n");

    test_basic_search_and_guards();
    test_reciprocal_scenario();
    test_non_reciprocal_scenario();

    printf("\n----------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("----------------------------------------------\n");

    return (failed == 0) ? 0 : 1;
}

