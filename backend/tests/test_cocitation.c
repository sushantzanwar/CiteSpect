/*
 * test_cocitation.c
 *
 * Offline unit and CSV integration tests for Co-Citation Detection
 * (openalex_parse_citing_page, openalex_collect_citing_pages,
 * csv_load_citing_papers, citation_compute_cocitation, and
 * compare_papers_cocitation edge-case guards).
 *
 * CSV tests execute inside an isolated temporary directory so real
 * CSV files in backend/data/ are never touched.
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

    snprintf(temp_dir, temp_size, "%s/citespect_cocitation_test_%d", base, (int)GETPID());
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

static void test_one_common_citer(void)
{
    const char citers_a[2][PAPER_ID_LEN] = {"W300", "W301"};
    const char citers_b[2][PAPER_ID_LEN] = {"W300", "W302"};
    CoCitationResult res;

    citation_compute_cocitation("W100", citers_a, 2, 1,
                                "W200", citers_b, 2, 1, &res);

    check("T1: One common citing paper yields YES, common=1, union=3, Jaccard=33.33%",
          res.status == COCITATION_YES &&
          res.exact_jaccard == 1 &&
          res.unique_citers_a == 2 &&
          res.unique_citers_b == 2 &&
          res.common_count == 1 &&
          res.union_count == 3 &&
          fabs(res.jaccard_pct - 33.3333) < 0.02 &&
          strcmp(res.co_citing_ids[0], "W300") == 0);
}

static void test_multiple_common_citers(void)
{
    const char citers_a[4][PAPER_ID_LEN] = {"W301", "W302", "W303", "W304"};
    const char citers_b[3][PAPER_ID_LEN] = {"W302", "W304", "W305"};
    CoCitationResult res;

    citation_compute_cocitation("W100", citers_a, 4, 1,
                                "W200", citers_b, 3, 1, &res);

    check("T2: Multiple common citing papers yield YES, common=2, union=5, Jaccard=40.00%",
          res.status == COCITATION_YES &&
          res.exact_jaccard == 1 &&
          res.unique_citers_a == 4 &&
          res.unique_citers_b == 3 &&
          res.common_count == 2 &&
          res.union_count == 5 &&
          fabs(res.jaccard_pct - 40.0) < 0.01 &&
          strcmp(res.co_citing_ids[0], "W302") == 0 &&
          strcmp(res.co_citing_ids[1], "W304") == 0);
}

static void test_disjoint_complete_citers(void)
{
    const char citers_a[2][PAPER_ID_LEN] = {"W301", "W302"};
    const char citers_b[2][PAPER_ID_LEN] = {"W401", "W402"};
    CoCitationResult res;

    citation_compute_cocitation("W100", citers_a, 2, 1,
                                "W200", citers_b, 2, 1, &res);

    check("T3: Disjoint complete citing sets yield NO and 0.00%",
          res.status == COCITATION_NO &&
          res.exact_jaccard == 1 &&
          res.unique_citers_a == 2 &&
          res.unique_citers_b == 2 &&
          res.common_count == 0 &&
          res.union_count == 4 &&
          fabs(res.jaccard_pct) < 0.001);
}

static void test_duplicates_urls_and_pair_filtering(void)
{
    const char citers_a[5][PAPER_ID_LEN] = {
        "W100",
        "W200",
        "https://openalex.org/W301",
        "W301",
        "W302"
    };
    const char citers_b[5][PAPER_ID_LEN] = {
        "W200",
        "W100",
        "W301",
        "https://openalex.org/W301",
        "W303"
    };
    CoCitationResult res;

    citation_compute_cocitation("W100", citers_a, 5, 1,
                                "W200", citers_b, 5, 1, &res);

    check("T4: Duplicates, full URLs, and self/pair IDs handled cleanly (common=1, union=3, 33.33%)",
          res.status == COCITATION_YES &&
          res.unique_citers_a == 2 &&
          res.unique_citers_b == 2 &&
          res.common_count == 1 &&
          res.union_count == 3 &&
          fabs(res.jaccard_pct - 33.3333) < 0.02 &&
          strcmp(res.co_citing_ids[0], "W301") == 0);
}

static void test_incomplete_and_missing_data(void)
{
    const char citers_a[2][PAPER_ID_LEN] = {"W301", "W302"};
    const char citers_disjoint_b[1][PAPER_ID_LEN] = {"W401"};
    const char citers_common_b[2][PAPER_ID_LEN] = {"W302", "W402"};
    CoCitationResult res_unk_a, res_unk_b, res_partial_yes;

    citation_compute_cocitation("W100", NULL, 0, 0,
                                "W200", citers_disjoint_b, 1, 1, &res_unk_a);
    check("T5a: Missing OpenAlex data for Paper A with 0 overlap yields UNKNOWN and N/A Jaccard",
          res_unk_a.status == COCITATION_UNKNOWN &&
          res_unk_a.exact_jaccard == 0 &&
          res_unk_a.jaccard_pct < 0.0);

    citation_compute_cocitation("W100", citers_a, 2, 1,
                                "W200", citers_disjoint_b, 1, 0, &res_unk_b);
    check("T5b: Incomplete Paper B data with 0 overlap yields UNKNOWN and N/A Jaccard",
          res_unk_b.status == COCITATION_UNKNOWN &&
          res_unk_b.exact_jaccard == 0 &&
          res_unk_b.jaccard_pct < 0.0);

    citation_compute_cocitation("W100", citers_a, 2, 0,
                                "W200", citers_common_b, 2, 1, &res_partial_yes);
    check("T5c: Common citer on partial data yields YES but disables exact Jaccard",
          res_partial_yes.status == COCITATION_YES &&
          res_partial_yes.common_count == 1 &&
          res_partial_yes.exact_jaccard == 0 &&
          res_partial_yes.jaccard_pct < 0.0);
}

static void test_pagination_truncation_and_malformed_json(void)
{
    static CiterFetchResult res;
    char small_ids[2][PAPER_ID_LEN];
    int parsed = 0;
    int meta_count = -1;
    int trunc = 0;
    int code;

    const char *pages_complete[2] = {
        "{\"meta\":{\"count\":3},\"results\":[{\"id\":\"https://openalex.org/W11\"},{\"id\":\"https://openalex.org/W12\"}]}",
        "{\"meta\":{\"count\":3},\"results\":[{\"id\":\"https://openalex.org/W13\"}]}"
    };
    const char *pages_empty[1] = {
        "{\"meta\":{\"count\":0},\"results\":[]}"
    };
    const char *pages_short[1] = {
        "{\"meta\":{\"count\":5},\"results\":[{\"id\":\"https://openalex.org/W21\"}]}"
    };
    const char *pages_bad[2] = {
        "{\"meta\":{\"count\":3},\"results\":[{\"id\":\"https://openalex.org/W11\"},{\"id\":\"https://openalex.org/W12\"}]}",
        "{\"meta\":{\"count\":3},\"results\":[{\"id\":\"bad_id\"}]}"
    };

    code = openalex_collect_citing_pages(pages_complete, 2, 5, 2, &res);
    check("T6a: Multi-page collection completes cleanly when total_seen reaches meta.count",
          code == OPENALEX_OK &&
          res.api_succeeded == 1 &&
          res.is_complete == 1 &&
          res.is_truncated == 0 &&
          res.total_available == 3 &&
          res.count == 3 &&
          strcmp(res.ids[0], "W11") == 0 &&
          strcmp(res.ids[2], "W13") == 0);

    code = openalex_collect_citing_pages(pages_complete, 2, 1, 2, &res);
    check("T6b: Page limit reached before meta.count marks result truncated and incomplete",
          code == OPENALEX_OK &&
          res.api_succeeded == 1 &&
          res.is_complete == 0 &&
          res.is_truncated == 1 &&
          res.total_available == 3 &&
          res.count == 2);

    code = openalex_collect_citing_pages(pages_complete, 1, 5, 2, &res);
    check("T6c: Missing subsequent page marks result incomplete and truncated",
          code == OPENALEX_OK &&
          res.api_succeeded == 1 &&
          res.is_complete == 0 &&
          res.is_truncated == 1 &&
          res.count == 2);

    code = openalex_collect_citing_pages(pages_short, 1, 5, 2, &res);
    check("T6d: Fewer items than per_page before meta.count marks result truncated",
          code == OPENALEX_OK &&
          res.api_succeeded == 1 &&
          res.is_complete == 0 &&
          res.is_truncated == 1 &&
          res.count == 1);

    code = openalex_collect_citing_pages(pages_empty, 1, 5, 2, &res);
    check("T6e: Verified empty OpenAlex citing list marks is_complete=1 and count=0",
          code == OPENALEX_OK &&
          res.api_succeeded == 1 &&
          res.is_complete == 1 &&
          res.is_truncated == 0 &&
          res.total_available == 0 &&
          res.count == 0);

    code = openalex_parse_citing_page(pages_complete[0], small_ids, 1, 2, &parsed, &meta_count, &trunc);
    check("T6f: max_ids buffer cap sets truncated flag without overflow",
          code == OPENALEX_OK &&
          parsed == 2 &&
          meta_count == 3 &&
          trunc == 1 &&
          strcmp(small_ids[1], "W11") == 0);

    code = openalex_collect_citing_pages(pages_bad, 2, 5, 2, &res);
    check("T6g: Malformed item ID on page 2 fails cleanly with OPENALEX_ERR_PARSE",
          code == OPENALEX_ERR_PARSE &&
          res.api_succeeded == 0 &&
          res.is_complete == 0);

    check("T6h: Malformed OpenAlex JSON payloads rejected safely",
          openalex_parse_citing_page("{\"results\":[]}", small_ids, 0, 2, &parsed, &meta_count, &trunc) == OPENALEX_ERR_PARSE &&
          openalex_parse_citing_page("{\"meta\":{\"count\":-1},\"results\":[]}", small_ids, 0, 2, &parsed, &meta_count, &trunc) == OPENALEX_ERR_PARSE &&
          openalex_parse_citing_page("{\"meta\":{\"count\":1},\"results\":[{\"id\":\"W1\"},]}", small_ids, 0, 2, &parsed, &meta_count, &trunc) == OPENALEX_ERR_PARSE &&
          openalex_parse_citing_page("{\"meta\":{\"count\":1},\"results\":[{\"id\":\"W1\"}", small_ids, 0, 2, &parsed, &meta_count, &trunc) == OPENALEX_ERR_PARSE &&
          openalex_parse_citing_page(NULL, small_ids, 0, 2, &parsed, &meta_count, &trunc) == OPENALEX_ERR_PARSE);
}

static void test_identical_and_invalid_ids(void)
{
    const char citers[1][PAPER_ID_LEN] = {"W300"};
    CoCitationResult r_same, r_url_same, r_null;

    citation_compute_cocitation("W100", citers, 1, 1,
                                "W100", citers, 1, 1, &r_same);
    citation_compute_cocitation("https://openalex.org/W100", citers, 1, 1,
                                "W100", citers, 1, 1, &r_url_same);
    citation_compute_cocitation(NULL, citers, 1, 1,
                                "W200", citers, 1, 1, &r_null);

    check("T7: Identical Work IDs and invalid inputs rejected with COCITATION_INVALID",
          r_same.status == COCITATION_INVALID &&
          r_url_same.status == COCITATION_INVALID &&
          r_null.status == COCITATION_INVALID &&
          compare_papers_cocitation("W100", "W100") == 0 &&
          compare_papers_cocitation("https://openalex.org/W100", "W100") == 0 &&
          compare_papers_cocitation("", "W200") == 0);
}

static void test_empty_citing_lists(void)
{
    const char non_empty[1][PAPER_ID_LEN] = {"W300"};
    CoCitationResult r_both_empty, r_one_empty;

    citation_compute_cocitation("W100", NULL, 0, 1,
                                "W200", NULL, 0, 1, &r_both_empty);
    check("T8a: Both verified empty citing lists yield NO and 0.00% without div-by-zero",
          r_both_empty.status == COCITATION_NO &&
          r_both_empty.exact_jaccard == 1 &&
          r_both_empty.union_count == 0 &&
          fabs(r_both_empty.jaccard_pct) < 0.001);

    citation_compute_cocitation("W100", NULL, 0, 1,
                                "W200", non_empty, 1, 1, &r_one_empty);
    check("T8b: One empty and one non-empty complete citing list yields NO and 0.00%",
          r_one_empty.status == COCITATION_NO &&
          r_one_empty.exact_jaccard == 1 &&
          r_one_empty.unique_citers_a == 0 &&
          r_one_empty.unique_citers_b == 1 &&
          r_one_empty.union_count == 1 &&
          fabs(r_one_empty.jaccard_pct) < 0.001);
}

static void test_csv_supplementary_evidence(void)
{
    Paper p_c, p_d, p_empty;
    char csv_citers_a[PAPER_MAX_REFS][PAPER_ID_LEN];
    char csv_citers_b[PAPER_MAX_REFS][PAPER_ID_LEN];
    char csv_citers_z[PAPER_MAX_REFS][PAPER_ID_LEN];
    char combined_a[20][PAPER_ID_LEN];
    char combined_b[20][PAPER_ID_LEN];
    int count_a, count_b, count_z;
    CoCitationResult r_offline_yes, r_offline_no, r_merged;
    CiterFetchResult fetch_a, fetch_b;
    const char *page_a[1] = {
        "{\"meta\":{\"count\":2},\"results\":[{\"id\":\"https://openalex.org/W501\"},{\"id\":\"https://openalex.org/W502\"}]}"
    };
    const char *page_b[1] = {
        "{\"meta\":{\"count\":2},\"results\":[{\"id\":\"https://openalex.org/W501\"},{\"id\":\"https://openalex.org/W503\"}]}"
    };
    int i, tot_a = 0, tot_b = 0;

    paper_init(&p_c, "W501", "Citing Paper C", 2024, "");
    paper_add_reference(&p_c, "W100");
    paper_add_reference(&p_c, "W200");

    paper_init(&p_d, "W502", "Citing Paper D", 2024, "");
    paper_add_reference(&p_d, "W100");
    paper_add_reference(&p_d, "W900");

    paper_init(&p_empty, "W599", "Verified Empty Paper", 2024, "");

    check("T9a: Save synthetic citing papers to isolated CSV cache",
          csv_save_paper(&p_c) == CSV_SAVED &&
          csv_save_paper(&p_d) == CSV_SAVED &&
          csv_save_paper(&p_empty) == CSV_SAVED);

    count_a = csv_load_citing_papers("W100", csv_citers_a, PAPER_MAX_REFS);
    count_b = csv_load_citing_papers("W200", csv_citers_b, PAPER_MAX_REFS);
    count_z = csv_load_citing_papers("W900", csv_citers_z, PAPER_MAX_REFS);

    check("T9b: csv_load_citing_papers finds reverse edges and ignores empty sentinels",
          count_a == 2 && count_b == 1 && count_z == 1 &&
          strcmp(csv_citers_a[0], "W501") == 0 &&
          strcmp(csv_citers_a[1], "W502") == 0 &&
          strcmp(csv_citers_b[0], "W501") == 0);

    citation_compute_cocitation("W100", (const char (*)[PAPER_ID_LEN])csv_citers_a, count_a, 0,
                                "W200", (const char (*)[PAPER_ID_LEN])csv_citers_b, count_b, 0,
                                &r_offline_yes);
    check("T9c: Supplementary CSV confirms YES when common citer exists offline (Jaccard N/A)",
          r_offline_yes.status == COCITATION_YES &&
          r_offline_yes.common_count == 1 &&
          r_offline_yes.exact_jaccard == 0 &&
          r_offline_yes.jaccard_pct < 0.0);

    citation_compute_cocitation("W200", (const char (*)[PAPER_ID_LEN])csv_citers_b, count_b, 0,
                                "W900", (const char (*)[PAPER_ID_LEN])csv_citers_z, count_z, 0,
                                &r_offline_no);
    check("T9d: Disjoint CSV-only citers with incomplete OpenAlex data yield UNKNOWN (never false NO)",
          r_offline_no.status == COCITATION_UNKNOWN &&
          r_offline_no.common_count == 0 &&
          r_offline_no.exact_jaccard == 0);

    openalex_collect_citing_pages(page_a, 1, 5, 200, &fetch_a);
    openalex_collect_citing_pages(page_b, 1, 5, 200, &fetch_b);
    for (i = 0; i < count_a; i++)
        snprintf(combined_a[tot_a++], PAPER_ID_LEN, "%s", csv_citers_a[i]);
    for (i = 0; i < fetch_a.count; i++)
        snprintf(combined_a[tot_a++], PAPER_ID_LEN, "%s", fetch_a.ids[i]);
    for (i = 0; i < count_b; i++)
        snprintf(combined_b[tot_b++], PAPER_ID_LEN, "%s", csv_citers_b[i]);
    for (i = 0; i < fetch_b.count; i++)
        snprintf(combined_b[tot_b++], PAPER_ID_LEN, "%s", fetch_b.ids[i]);

    citation_compute_cocitation("W100", (const char (*)[PAPER_ID_LEN])combined_a, tot_a, fetch_a.is_complete,
                                "W200", (const char (*)[PAPER_ID_LEN])combined_b, tot_b, fetch_b.is_complete,
                                &r_merged);
    check("T9e: Merged CSV + complete OpenAlex pages deduplicates overlapping citers (common=1, union=3, 33.33%)",
          r_merged.status == COCITATION_YES &&
          r_merged.exact_jaccard == 1 &&
          r_merged.unique_citers_a == 2 &&
          r_merged.unique_citers_b == 2 &&
          r_merged.common_count == 1 &&
          r_merged.union_count == 3 &&
          fabs(r_merged.jaccard_pct - 33.3333) < 0.02);
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
    printf("  CITESPECT CO-CITATION DETECTION UNIT TESTS       \n");
    printf("===================================================\n\n");

    test_one_common_citer();
    test_multiple_common_citers();
    test_disjoint_complete_citers();
    test_duplicates_urls_and_pair_filtering();
    test_incomplete_and_missing_data();
    test_pagination_truncation_and_malformed_json();
    test_identical_and_invalid_ids();
    test_empty_citing_lists();
    test_csv_supplementary_evidence();

    printf("\n---------------------------------------------------\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("---------------------------------------------------\n");

    cleanup_isolated_workdir(orig_cwd, temp_dir);
    return (failed == 0) ? 0 : 1;
}
