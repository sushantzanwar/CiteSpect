/*
 * test_bibliographic_coupling.c
 *
 * Focused unit & CSV integration tests for Paper A vs. Paper B
 * Bibliographic Coupling (citation_compute_coupling & compare_papers_coupling).
 *
 * Runs CSV tests in an isolated temporary directory so real CSV files
 * in backend/data/ are never modified.
 */

#include <math.h>
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
#include "csv_store.h"
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

    snprintf(temp_dir, temp_size, "%s/citespect_coupling_test_%d", base, (int)GETPID());
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

static void test_prompt_example(void)
{
    Paper a, b;
    CouplingResult res;

    paper_init(&a, "W_A", "Paper A", 2024, "");
    paper_add_reference(&a, "W_C");
    paper_add_reference(&a, "W_D");
    paper_add_reference(&a, "W_E");

    paper_init(&b, "W_B", "Paper B", 2024, "");
    paper_add_reference(&b, "W_C");
    paper_add_reference(&b, "W_E");
    paper_add_reference(&b, "W_F");

    citation_compute_coupling(&a, 1, &b, 1, &res);
    check("T1: Example overlap yields COUPLING_YES, common=2, union=4, Jaccard=50.00%",
          res.status == COUPLING_YES &&
          res.unique_refs_a == 3 &&
          res.unique_refs_b == 3 &&
          res.common_count == 2 &&
          res.union_count == 4 &&
          fabs(res.jaccard_pct - 50.0) < 0.01 &&
          strcmp(res.common_refs[0], "W_C") == 0 &&
          strcmp(res.common_refs[1], "W_E") == 0);
}

static void test_disjoint_and_identical(void)
{
    Paper a, b, c;
    CouplingResult res_disjoint, res_identical;

    paper_init(&a, "W1", "A", 2023, "");
    paper_add_reference(&a, "W10");
    paper_add_reference(&a, "W11");

    paper_init(&b, "W2", "B", 2023, "");
    paper_add_reference(&b, "W20");
    paper_add_reference(&b, "W21");

    paper_init(&c, "W3", "C", 2023, "");
    paper_add_reference(&c, "W10");
    paper_add_reference(&c, "W11");

    citation_compute_coupling(&a, 1, &b, 1, &res_disjoint);
    check("T2: Disjoint reference sets yield COUPLING_NO and 0.00%",
          res_disjoint.status == COUPLING_NO &&
          res_disjoint.common_count == 0 &&
          res_disjoint.union_count == 4 &&
          fabs(res_disjoint.jaccard_pct) < 0.001);

    citation_compute_coupling(&a, 1, &c, 1, &res_identical);
    check("T3: Identical reference sets yield COUPLING_YES and 100.00%",
          res_identical.status == COUPLING_YES &&
          res_identical.common_count == 2 &&
          res_identical.union_count == 2 &&
          fabs(res_identical.jaccard_pct - 100.0) < 0.01);
}

static void test_duplicates_and_self_refs(void)
{
    Paper a, b;
    CouplingResult res;

    paper_init(&a, "W_A", "A", 2024, "");
    paper_add_reference(&a, "W_A"); /* self-reference ignored */
    paper_add_reference(&a, "W_C");
    paper_add_reference(&a, "W_C"); /* duplicate */
    paper_add_reference(&a, "W_D");

    paper_init(&b, "W_B", "B", 2024, "");
    paper_add_reference(&b, "W_B"); /* self-reference ignored */
    paper_add_reference(&b, "W_C");
    paper_add_reference(&b, "W_C"); /* duplicate */
    paper_add_reference(&b, "W_C"); /* duplicate */
    paper_add_reference(&b, "W_E");

    citation_compute_coupling(&a, 1, &b, 1, &res);
    check("T4: Duplicates and self-references deduplicated (unique A=2, B=2, common=1, 33.33%)",
          res.status == COUPLING_YES &&
          res.unique_refs_a == 2 &&
          res.unique_refs_b == 2 &&
          res.common_count == 1 &&
          res.union_count == 3 &&
          fabs(res.jaccard_pct - 33.3333) < 0.02);
}

static void test_empty_and_missing_refs(void)
{
    Paper empty_a, empty_b, with_ref;
    CouplingResult r_both_empty, r_one_empty, r_missing_a, r_missing_b;

    paper_init(&empty_a, "W10", "Empty A", 2024, "");
    paper_init(&empty_b, "W20", "Empty B", 2024, "");
    paper_init(&with_ref, "W30", "With Ref", 2024, "");
    paper_add_reference(&with_ref, "W99");

    citation_compute_coupling(&empty_a, 1, &empty_b, 1, &r_both_empty);
    check("T5a: Both verified empty reference lists yield COUPLING_NO and 0.00% (no div-by-zero)",
          r_both_empty.status == COUPLING_NO &&
          r_both_empty.common_count == 0 &&
          r_both_empty.union_count == 0 &&
          fabs(r_both_empty.jaccard_pct) < 0.001);

    citation_compute_coupling(&empty_a, 1, &with_ref, 1, &r_one_empty);
    check("T5b: One verified empty and one non-empty list yields COUPLING_NO",
          r_one_empty.status == COUPLING_NO &&
          r_one_empty.unique_refs_a == 0 &&
          r_one_empty.unique_refs_b == 1 &&
          r_one_empty.union_count == 1);

    citation_compute_coupling(&empty_a, 0, &with_ref, 1, &r_missing_a);
    citation_compute_coupling(&with_ref, 1, &empty_b, 0, &r_missing_b);
    check("T6: Missing/unverified reference metadata yields COUPLING_UNKNOWN",
          r_missing_a.status == COUPLING_UNKNOWN &&
          r_missing_b.status == COUPLING_UNKNOWN);
}

static void test_large_scale_1000_refs(void)
{
    static Paper a, b;
    static CouplingResult res;
    char ref_id[PAPER_ID_LEN];
    int i;

    paper_init(&a, "W_BIG_A", "Big A", 2024, "");
    paper_init(&b, "W_BIG_B", "Big B", 2024, "");

    /* A cites W1000..W1999; B cites W1500..W2499 -> 500 shared out of 1500 union (33.33%) */
    for (i = 0; i < PAPER_MAX_REFS; i++) {
        sprintf(ref_id, "W%d", 1000 + i);
        paper_add_reference(&a, ref_id);
        sprintf(ref_id, "W%d", 1500 + i);
        paper_add_reference(&b, ref_id);
    }

    citation_compute_coupling(&a, 1, &b, 1, &res);
    check("T7: 1,000 references per paper handled with RefHash (common=500, union=1500, 33.33%)",
          res.status == COUPLING_YES &&
          res.unique_refs_a == 1000 &&
          res.unique_refs_b == 1000 &&
          res.common_count == 500 &&
          res.union_count == 1500 &&
          fabs(res.jaccard_pct - (500.0 * 100.0 / 1500.0)) < 0.01);
}

static void test_null_and_invalid_inputs(void)
{
    Paper p;
    CouplingResult res;

    paper_init(&p, "W1", "Valid", 2024, "");
    check("T8: NULL and invalid inputs rejected safely with COUPLING_UNKNOWN",
          citation_compute_coupling(&p, 1, &p, 1, NULL) == COUPLING_UNKNOWN &&
          citation_compute_coupling(NULL, 1, &p, 1, &res) == COUPLING_UNKNOWN &&
          citation_compute_coupling(&p, 1, NULL, 1, &res) == COUPLING_UNKNOWN &&
          compare_papers_coupling(NULL, "W1") == 0 &&
          compare_papers_coupling("W1", "") == 0);
}

static void test_csv_compare_papers_coupling(void)
{
    Paper a, b, empty_p;

    paper_init(&a, "W100", "CSV Paper A", 2024, "10.1/a");
    paper_add_reference(&a, "W501");
    paper_add_reference(&a, "W502");
    paper_add_reference(&a, "W503");

    paper_init(&b, "W200", "CSV Paper B", 2024, "10.1/b");
    paper_add_reference(&b, "W502");
    paper_add_reference(&b, "W503");
    paper_add_reference(&b, "W504");

    paper_init(&empty_p, "W300", "CSV Empty Refs", 2024, "10.1/e");

    check("T9a: Save synthetic papers to isolated CSV cache",
          csv_save_paper(&a) == CSV_SAVED &&
          csv_save_paper(&b) == CSV_SAVED &&
          csv_save_paper(&empty_p) == CSV_SAVED);

    check("T9b: compare_papers_coupling succeeds on cached papers W100 and W200",
          compare_papers_coupling("W100", "W200") == 1);

    check("T9c: compare_papers_coupling succeeds with verified empty cached paper W300",
          compare_papers_coupling("W100", "W300") == 1);
}

int main(void)
{
    char orig_cwd[512];
    char temp_dir[512];

    if (!setup_isolated_workdir(orig_cwd, sizeof(orig_cwd), temp_dir, sizeof(temp_dir))) {
        fprintf(stderr, "Failed to setup isolated test directory.\n");
        return 1;
    }

    printf("===================================================\n");
    printf("  CITESPECT BIBLIOGRAPHIC COUPLING UNIT TESTS      \n");
    printf("===================================================\n\n");

    test_prompt_example();
    test_disjoint_and_identical();
    test_duplicates_and_self_refs();
    test_empty_and_missing_refs();
    test_large_scale_1000_refs();
    test_null_and_invalid_inputs();
    test_csv_compare_papers_coupling();

    printf("\n---------------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("---------------------------------------------------\n");

    cleanup_isolated_workdir(orig_cwd, temp_dir);
    return (failed == 0) ? 0 : 1;
}

