/*
 * test_reciprocal_detection.c
 *
 * Unit & integration tests for Stage 2 Reciprocal Cross-Citation Detection.
 * Tests the actual production functions in citation_analysis.c:
 *   - citation_check_reciprocal()
 *   - citation_is_duplicate()
 *   - analyze_paper_citations()
 *
 * Uses an isolated temporary working directory for all CSV operations so real
 * project CSV files are never touched and no tests are conditionally skipped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define MKDIR(p) _mkdir(p)
#define CHDIR(p) _chdir(p)
#define GETCWD(b, s) _getcwd(b, s)
#define GETPID() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir((p), 0700)
#define CHDIR(p) chdir(p)
#define GETCWD(b, s) getcwd(b, s)
#define GETPID() getpid()
#endif

#include "citation_analysis.h"
#include "citation_graph.h"
#include "author_hash.h"
#include "csv_store.h"
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

static int setup_isolated_workdir(char *orig_cwd, size_t orig_size,
                                  char *temp_dir, size_t temp_size)
{
    const char *base = getenv("TEMP");
    char data_dir[512];

    if (GETCWD(orig_cwd, (int)orig_size) == NULL)
        return 0;

    if (base == NULL || base[0] == '\0')
        base = getenv("TMP");
    if (base == NULL || base[0] == '\0')
        base = ".";

    snprintf(temp_dir, temp_size, "%s/citespect_recip_test_%d", base, (int)GETPID());
    MKDIR(temp_dir);
    snprintf(data_dir, sizeof(data_dir), "%s/data", temp_dir);
    MKDIR(data_dir);

    return CHDIR(temp_dir) == 0;
}

static void cleanup_isolated_workdir(const char *orig_cwd, const char *temp_dir)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/data/papers.csv", temp_dir);
    remove(path);
    snprintf(path, sizeof(path), "%s/data/authors.csv", temp_dir);
    remove(path);
    snprintf(path, sizeof(path), "%s/data/citations.csv", temp_dir);
    remove(path);
    snprintf(path, sizeof(path), "%s/data", temp_dir);
#ifdef _WIN32
    _rmdir(path);
    CHDIR(orig_cwd);
    _rmdir(temp_dir);
#else
    rmdir(path);
    CHDIR(orig_cwd);
    rmdir(temp_dir);
#endif
}

static void test_confirmed_reciprocal(void)
{
    Paper paper_b;
    int status;

    paper_init(&paper_b, "W_PAPER_B", "Cited Paper B", 2020, "");
    paper_add_reference(&paper_b, "W_PAPER_A");
    paper_add_reference(&paper_b, "W_OTHER_1");

    status = citation_check_reciprocal(&paper_b, "W_PAPER_A", 1);
    CHECK("Test 1: A -> B and B -> A yields RECIPROCAL_CONFIRMED",
          status == RECIPROCAL_CONFIRMED);
}

static void test_not_reciprocal_with_references(void)
{
    Paper paper_c;
    int status;

    paper_init(&paper_c, "W_PAPER_C", "Cited Paper C", 2019, "");
    paper_add_reference(&paper_c, "W_OTHER_1");
    paper_add_reference(&paper_c, "W_OTHER_2");

    status = citation_check_reciprocal(&paper_c, "W_PAPER_A", 1);
    CHECK("Test 2: A -> C where C does not cite A yields RECIPROCAL_NOT",
          status == RECIPROCAL_NOT);
}

static void test_reciprocal_unknown_when_refs_unavailable(void)
{
    Paper paper_d;
    int status;

    paper_init(&paper_d, "W_PAPER_D", "Cited Paper D", 2021, "");
    status = citation_check_reciprocal(&paper_d, "W_PAPER_A", 0);
    CHECK("Test 3: References unavailable yields RECIPROCAL_UNKNOWN",
          status == RECIPROCAL_UNKNOWN);
}

static void test_not_reciprocal_empty_references_verified(void)
{
    Paper paper_e;
    int status;

    paper_init(&paper_e, "W_PAPER_E", "Cited Paper E", 2022, "");
    status = citation_check_reciprocal(&paper_e, "W_PAPER_A", 1);
    CHECK("Test 4: Verified empty references yields RECIPROCAL_NOT",
          status == RECIPROCAL_NOT);
}

static void test_failed_metadata_simulation(void)
{
    Paper dummy;
    int status;

    paper_init(&dummy, "W_FAILED", "Failed", 0, "");
    status = citation_check_reciprocal(&dummy, "W_PAPER_A", 0);
    CHECK("Test 5: Failed metadata retrieval simulation yields RECIPROCAL_UNKNOWN",
          status == RECIPROCAL_UNKNOWN);
}

static void test_duplicate_citation_edges_suppressed(void)
{
    char cited_ids[4][PAPER_ID_LEN] = {
        "W_PAPER_B",
        "W_PAPER_C",
        "W_PAPER_B",
        "W_PAPER_D"
    };

    CHECK("Test 6a: First occurrence is not duplicate",
          citation_is_duplicate(cited_ids, 0) == 0);
    CHECK("Test 6b: Second unique occurrence is not duplicate",
          citation_is_duplicate(cited_ids, 1) == 0);
    CHECK("Test 6c: Duplicate occurrence is correctly identified",
          citation_is_duplicate(cited_ids, 2) == 1);
    CHECK("Test 6d: Third unique occurrence is not duplicate",
          citation_is_duplicate(cited_ids, 3) == 0);
}

static void test_combined_self_and_reciprocal_matrix(void)
{
    AuthorHash ht;
    Paper citing, cited_a, cited_b;
    int is_self_a, recip_a, is_self_b, recip_b;

    author_hash_init(&ht);

    paper_init(&citing, "W_MAIN", "Main Paper", 2020, "");
    paper_add_author(&citing, "AUTH_1", "Alice");
    author_hash_build(&ht, &citing);

    paper_init(&cited_a, "W_CITED_A", "Paper A", 2020, "");
    paper_add_author(&cited_a, "AUTH_1", "Alice");
    paper_add_reference(&cited_a, "W_MAIN");

    is_self_a = (hash_search(&ht, cited_a.author_ids[0]) != NULL);
    recip_a = citation_check_reciprocal(&cited_a, "W_MAIN", 1);
    CHECK("Test 7a: Case A is SELF", is_self_a == 1);
    CHECK("Test 7b: Case A is CONFIRMED RECIPROCAL", recip_a == RECIPROCAL_CONFIRMED);

    paper_init(&cited_b, "W_CITED_B", "Paper B", 2020, "");
    paper_add_author(&cited_b, "AUTH_2", "Bob");
    paper_add_reference(&cited_b, "W_MAIN");

    is_self_b = (hash_search(&ht, cited_b.author_ids[0]) != NULL);
    recip_b = citation_check_reciprocal(&cited_b, "W_MAIN", 1);
    CHECK("Test 7c: Case B is NOT SELF", is_self_b == 0);
    CHECK("Test 7d: Case B is CONFIRMED RECIPROCAL (Reciprocal Cross-Citation)",
          recip_b == RECIPROCAL_CONFIRMED);

    author_hash_free(&ht);
}

static void test_csv_reference_cache_reliability(void)
{
    Paper p_zero, p_missing, p_overwrite;
    char loaded[PAPER_MAX_REFS][PAPER_ID_LEN];

    /* 1. Paper with verified empty reference list (0 references) */
    paper_init(&p_zero, "W_ZERO_REFS", "Zero Refs Paper", 2024, "10.0/zero");
    paper_add_author(&p_zero, "A_ZERO", "Zero Author");
    CHECK("Test 8a: Save paper with 0 references succeeds",
          csv_save_paper(&p_zero) == CSV_SAVED);
    CHECK("Test 8b: paper_has_citations_saved is 1 for verified empty reference list",
          paper_has_citations_saved("W_ZERO_REFS") == 1);
    CHECK("Test 8c: csv_load_citations returns 0 for verified empty reference list",
          csv_load_citations("W_ZERO_REFS", loaded, PAPER_MAX_REFS) == 0);

    /* 2. Paper cached without references, then references fetched and saved */
    CHECK("Test 8d: Unsaved paper has paper_has_citations_saved == 0",
          paper_has_citations_saved("W_LATE_REFS") == 0);
    paper_init(&p_missing, "W_LATE_REFS", "Late Refs Paper", 2023, "10.0/late");
    paper_add_reference(&p_missing, "W_REF_1");
    paper_add_reference(&p_missing, "W_REF_2");
    CHECK("Test 8e: csv_save_citations saves newly fetched references",
          csv_save_citations(&p_missing) == CSV_SAVED);
    CHECK("Test 8f: paper_has_citations_saved is now 1",
          paper_has_citations_saved("W_LATE_REFS") == 1);
    CHECK("Test 8g: csv_load_citations loads both saved references",
          csv_load_citations("W_LATE_REFS", loaded, PAPER_MAX_REFS) == 2);

    /* 3. Prevent overwriting previously stored references with empty list */
    paper_init(&p_overwrite, "W_LATE_REFS", "Late Refs Paper", 2023, "10.0/late");
    CHECK("Test 8h: csv_save_citations refuses to overwrite existing references",
          csv_save_citations(&p_overwrite) == CSV_ALREADY_SAVED);
    CHECK("Test 8i: Original 2 references remain intact",
          csv_load_citations("W_LATE_REFS", loaded, PAPER_MAX_REFS) == 2);
}

static void test_analyze_paper_citations_production(void)
{
    Paper main_p, cited_a, cited_b;

    paper_init(&main_p, "W_MAIN", "Main Paper", 2024, "10.0/main");
    paper_add_author(&main_p, "AUTH_1", "Alice");
    paper_add_reference(&main_p, "W_CITED_A");
    paper_add_reference(&main_p, "W_CITED_B");
    paper_add_reference(&main_p, "W_ZERO_REFS");
    paper_add_reference(&main_p, "W_CITED_A"); /* duplicate */
    paper_add_reference(&main_p, "W_MAIN");    /* self-reference */

    paper_init(&cited_a, "W_CITED_A", "Cited A", 2023, "10.0/a");
    paper_add_author(&cited_a, "AUTH_1", "Alice");
    paper_add_reference(&cited_a, "W_MAIN");

    paper_init(&cited_b, "W_CITED_B", "Cited B", 2023, "10.0/b");
    paper_add_author(&cited_b, "AUTH_2", "Bob");
    paper_add_reference(&cited_b, "W_MAIN");

    CHECK("Test 9a: Save synthetic citing/cited papers in isolated CSVs",
          csv_save_paper(&main_p) == CSV_SAVED &&
          csv_save_paper(&cited_a) == CSV_SAVED &&
          csv_save_paper(&cited_b) == CSV_SAVED);
    CHECK("Test 9b: Production analyze_paper_citations runs end-to-end on cached CSVs",
          analyze_paper_citations("W_MAIN") == 1);
    CHECK("Test 9c: Unsaved paper returns 0 from analyze_paper_citations",
          analyze_paper_citations("W_MISSING_999") == 0);
}

int main(void)
{
    char orig_cwd[512];
    char temp_dir[512];

    if (!setup_isolated_workdir(orig_cwd, sizeof(orig_cwd), temp_dir, sizeof(temp_dir))) {
        fprintf(stderr, "Failed to create isolated test directory.\n");
        return 1;
    }

    printf("===================================================\n");
    printf("  CITESPECT STAGE 2 RECIPROCAL CITATION UNIT TESTS \n");
    printf("===================================================\n\n");

    test_confirmed_reciprocal();
    test_not_reciprocal_with_references();
    test_reciprocal_unknown_when_refs_unavailable();
    test_not_reciprocal_empty_references_verified();
    test_failed_metadata_simulation();
    test_duplicate_citation_edges_suppressed();
    test_combined_self_and_reciprocal_matrix();
    test_csv_reference_cache_reliability();
    test_analyze_paper_citations_production();

    printf("\n---------------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("---------------------------------------------------\n");

    cleanup_isolated_workdir(orig_cwd, temp_dir);
    return (failed == 0) ? 0 : 1;
}
