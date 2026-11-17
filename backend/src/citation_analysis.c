#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "citation_analysis.h"
#include "author_hash.h"
#include "citation_graph.h"
#include "csv_store.h"
#include "openalex.h"

typedef struct {
    int self_count;
    int not_self_count;
    int recip_conf_count;
    int recip_not_count;
    int recip_unk_count;
    int self_ref_count;
    int dup_count;
} CitationSummary;

static int find_self_authors_hash(const AuthorHash *citing_ht,
                                  const Paper *cited,
                                  char out_ids[][PAPER_ID_LEN],
                                  char out_names[][AUTHOR_NAME_LEN],
                                  int max_matches)
{
    int i, k;
    int count = 0;
    const AuthorNode *hit;

    for (i = 0; i < cited->author_count && i < PAPER_MAX_AUTHORS && count < max_matches; i++) {
        if (cited->author_ids[i][0] == '\0')
            continue;
        hit = hash_search(citing_ht, cited->author_ids[i]);
        if (hit != NULL) {

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

static void print_citation_result(const char *cited_id,
                                  int is_self,
                                  const char shared_ids[][PAPER_ID_LEN],
                                  const char shared_names[][AUTHOR_NAME_LEN],
                                  int shared_count,
                                  int recip_status)
{
    int i;

    printf("Cited Paper: %s\n", cited_id);

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

    if (recip_status == RECIPROCAL_CONFIRMED) {
        printf("Reciprocal Citation: CONFIRMED RECIPROCAL\n");
    } else if (recip_status == RECIPROCAL_NOT) {
        printf("Reciprocal Citation: NOT RECIPROCAL\n");
    } else {
        printf("Reciprocal Citation: UNKNOWN\n");
    }

    printf("\n---------------------------------------\n\n");
}

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

    *refs_available = 1;

    if (csv_save_paper(out) == CSV_ERR_FILE)
        printf("Warning: could not save %s to the CSV files.\n", paper_id);
    return OPENALEX_OK;
}

int citation_check_reciprocal(const Paper *cited, const char *citing_id, int refs_available)
{
    CitationGraph *graph;
    int has_rev;
    int i;

    if (!refs_available || cited == NULL)
        return RECIPROCAL_UNKNOWN;

    graph = citation_graph_create(cited->paper_id);
    if (graph == NULL)
        return RECIPROCAL_UNKNOWN;

    for (i = 0; i < cited->reference_count && i < PAPER_MAX_REFS; i++) {
        citation_graph_add_reference(graph, cited->references[i]);
    }

    has_rev = citation_graph_has_reference(graph, citing_id);
    citation_graph_free(graph);

    return has_rev ? RECIPROCAL_CONFIRMED : RECIPROCAL_NOT;
}

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

    shared_count = find_self_authors_hash(citing_ht, &cited,
                                          shared_ids, shared_names,
                                          PAPER_MAX_AUTHORS);
    is_self = (shared_count > 0);
    if (is_self)
        summary->self_count++;
    else
        summary->not_self_count++;

    recip_status = citation_check_reciprocal(&cited, citing_id, refs_available);
    if (recip_status == RECIPROCAL_CONFIRMED)
        summary->recip_conf_count++;
    else if (recip_status == RECIPROCAL_NOT)
        summary->recip_not_count++;
    else
        summary->recip_unk_count++;

    print_citation_result(cited_id, is_self, shared_ids, shared_names, shared_count, recip_status);
}

int citation_is_duplicate(char cited_ids[][PAPER_ID_LEN], int index)
{
    int i;

    if (cited_ids == NULL || index < 0)
        return 0;

    for (i = 0; i < index; i++) {
        if (strcmp(cited_ids[i], cited_ids[index]) == 0)
            return 1;
    }
    return 0;
}

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
        } else if (citation_is_duplicate(cited_ids, i)) {
            summary->dup_count++;
        } else {
            analyze_one(citing_id, citing_ht, cited_ids[i], summary);
        }
    }
}

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

int analyze_paper_citations(const char *citing_id)
{
    static Paper citing;
    static char cited_ids[PAPER_MAX_REFS][PAPER_ID_LEN];
    CitationSummary summary = {0, 0, 0, 0, 0, 0, 0};
    AuthorHash citing_ht;
    int total;

    if (citing_id == NULL || citing_id[0] == '\0' || strlen(citing_id) >= PAPER_ID_LEN) {
        printf("Invalid paper ID.\n");
        return 0;
    }

    if (!paper_already_saved(citing_id)) {
        printf("Paper %s is not in %s. Add it first with option 3.\n",
               citing_id, PAPERS_CSV);
        return 0;
    }

    paper_init(&citing, citing_id, "", 0, "");
    csv_load_authors(citing_id, &citing);

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

    author_hash_free(&citing_ht);
    return 1;
}

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

#define REF_HASH_BUCKETS 1009

typedef struct RefNode {
    char work_id[PAPER_ID_LEN];
    struct RefNode *next;
} RefNode;

typedef struct {
    RefNode *buckets[REF_HASH_BUCKETS];
} RefHash;

static int is_valid_ref_id(const char *id)
{
    return id != NULL && id[0] != '\0' && strlen(id) < PAPER_ID_LEN;
}

static unsigned int ref_hash_bucket(const char *id)
{
    unsigned int h = 0;
    const unsigned char *p;

    for (p = (const unsigned char *)id; *p != '\0'; p++)
        h = h * 31u + (unsigned int)(*p);
    return h % (unsigned int)REF_HASH_BUCKETS;
}

static void ref_hash_init(RefHash *ht)
{
    int i;

    for (i = 0; i < REF_HASH_BUCKETS; i++)
        ht->buckets[i] = NULL;
}

static int ref_hash_contains(const RefHash *ht, const char *id)
{
    const RefNode *node = ht->buckets[ref_hash_bucket(id)];

    while (node != NULL) {
        if (strcmp(node->work_id, id) == 0)
            return 1;
        node = node->next;
    }
    return 0;
}

static int ref_hash_insert(RefHash *ht, const char *id, int *is_new)
{
    unsigned int idx;
    RefNode *node;

    *is_new = 0;
    if (!is_valid_ref_id(id))
        return 1;
    if (ref_hash_contains(ht, id))
        return 1;

    node = malloc(sizeof(RefNode));
    if (node == NULL)
        return 0;

    snprintf(node->work_id, PAPER_ID_LEN, "%s", id);
    idx = ref_hash_bucket(id);
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
    *is_new = 1;
    return 1;
}

static void ref_hash_free(RefHash *ht)
{
    int i;
    RefNode *node;
    RefNode *next;

    for (i = 0; i < REF_HASH_BUCKETS; i++) {
        node = ht->buckets[i];
        while (node != NULL) {
            next = node->next;
            free(node);
            node = next;
        }
        ht->buckets[i] = NULL;
    }
}

static int populate_ref_hash(RefHash *ht, const Paper *p, int *unique_count)
{
    int i;
    int is_new;

    *unique_count = 0;
    for (i = 0; i < p->reference_count && i < PAPER_MAX_REFS; i++) {
        const char *ref = p->references[i];
        if (!is_valid_ref_id(ref) || strcmp(ref, p->paper_id) == 0)
            continue;
        if (!ref_hash_insert(ht, ref, &is_new))
            return 0;
        *unique_count += is_new;
    }
    return 1;
}

static int scan_common_refs(const RefHash *ht_a, RefHash *ht_b,
                            const Paper *paper_b, CouplingResult *out)
{
    int i;
    int is_new;

    for (i = 0; i < paper_b->reference_count && i < PAPER_MAX_REFS; i++) {
        const char *ref = paper_b->references[i];
        if (!is_valid_ref_id(ref) || strcmp(ref, paper_b->paper_id) == 0)
            continue;
        if (!ref_hash_insert(ht_b, ref, &is_new))
            return 0;
        if (!is_new)
            continue;
        out->unique_refs_b++;
        if (ref_hash_contains(ht_a, ref) && out->common_count < PAPER_MAX_REFS) {
            snprintf(out->common_refs[out->common_count], PAPER_ID_LEN, "%s", ref);
            out->common_count++;
        }
    }
    return 1;
}

static void reset_coupling_result(CouplingResult *out)
{
    out->status = COUPLING_UNKNOWN;
    out->unique_refs_a = 0;
    out->unique_refs_b = 0;
    out->common_count = 0;
    out->union_count = 0;
    out->jaccard_pct = 0.0;
}

int citation_compute_coupling(const Paper *paper_a, int refs_a_available,
                              const Paper *paper_b, int refs_b_available,
                              CouplingResult *out)
{
    RefHash ht_a;
    RefHash ht_b;

    if (out == NULL)
        return COUPLING_UNKNOWN;
    reset_coupling_result(out);

    if (paper_a == NULL || paper_b == NULL ||
        !is_valid_ref_id(paper_a->paper_id) || !is_valid_ref_id(paper_b->paper_id) ||
        !refs_a_available || !refs_b_available)
        return COUPLING_UNKNOWN;

    ref_hash_init(&ht_a);
    ref_hash_init(&ht_b);

    if (!populate_ref_hash(&ht_a, paper_a, &out->unique_refs_a) ||
        !scan_common_refs(&ht_a, &ht_b, paper_b, out)) {
        ref_hash_free(&ht_a);
        ref_hash_free(&ht_b);
        reset_coupling_result(out);
        return COUPLING_UNKNOWN;
    }

    ref_hash_free(&ht_a);
    ref_hash_free(&ht_b);

    out->union_count = out->unique_refs_a + out->unique_refs_b - out->common_count;
    out->jaccard_pct = (out->union_count > 0)
        ? ((double)out->common_count * 100.0 / (double)out->union_count)
        : 0.0;
    out->status = (out->common_count > 0) ? COUPLING_YES : COUPLING_NO;
    return out->status;
}

static void print_coupling_report(const char *paper_a_id, const char *paper_b_id,
                                  const CouplingResult *res)
{
    int i;

    printf("\n========== BIBLIOGRAPHIC COUPLING ==========\n\n");
    printf("Paper A: %s\n", paper_a_id);
    printf("Paper B: %s\n", paper_b_id);

    if (res->status == COUPLING_UNKNOWN) {
        printf("Bibliographic Coupling: UNKNOWN\n");
        return;
    }

    printf("Bibliographic Coupling: %s\n",
           (res->status == COUPLING_YES) ? "YES" : "NO");
    printf("Paper A Unique References: %d\n", res->unique_refs_a);
    printf("Paper B Unique References: %d\n", res->unique_refs_b);
    printf("Common References: %d\n", res->common_count);
    printf("Union References: %d\n", res->union_count);
    printf("Jaccard Similarity: %.2f%%\n", res->jaccard_pct);
    for (i = 0; i < res->common_count; i++)
        printf("Shared Reference: %s\n", res->common_refs[i]);
}

int compare_papers_coupling(const char *paper_a_id, const char *paper_b_id)
{
    static Paper paper_a;
    static Paper paper_b;
    CouplingResult res;
    int refs_a = 0;
    int refs_b = 0;
    int code_a;
    int code_b;

    reset_coupling_result(&res);
    if (!is_valid_ref_id(paper_a_id) || !is_valid_ref_id(paper_b_id)) {
        printf("Invalid paper ID.\n");
        return 0;
    }

    code_a = load_cited_paper(paper_a_id, &paper_a, &refs_a);
    code_b = load_cited_paper(paper_b_id, &paper_b, &refs_b);

    if (code_a == OPENALEX_OK && code_b == OPENALEX_OK)
        citation_compute_coupling(&paper_a, refs_a, &paper_b, refs_b, &res);

    print_coupling_report(paper_a_id, paper_b_id, &res);
    return res.status != COUPLING_UNKNOWN;
}

static int normalize_work_id_str(const char *raw, char *out, size_t size)
{
    static const char prefix[] = "https://openalex.org/";
    size_t plen = sizeof(prefix) - 1;
    const char *id = raw;

    if (raw == NULL || out == NULL || size == 0)
        return 0;
    if (strncmp(raw, prefix, plen) == 0)
        id = raw + plen;
    if (!is_valid_ref_id(id) || strlen(id) >= size)
        return 0;
    snprintf(out, size, "%s", id);
    return 1;
}

static void reset_cocitation_result(CoCitationResult *out)
{
    out->status = COCITATION_UNKNOWN;
    out->citers_a_complete = 0;
    out->citers_b_complete = 0;
    out->exact_jaccard = 0;
    out->unique_citers_a = 0;
    out->unique_citers_b = 0;
    out->common_count = 0;
    out->union_count = 0;
    out->jaccard_pct = -1.0;
}

static int populate_citer_hash(RefHash *ht,
                               const char citers[][PAPER_ID_LEN],
                               int count,
                               const char *id_a,
                               const char *id_b,
                               int *unique_count)
{
    int i;

    *unique_count = 0;
    for (i = 0; i < count; i++) {
        char norm[PAPER_ID_LEN];
        int is_new = 0;

        if (!normalize_work_id_str(citers[i], norm, sizeof(norm)))
            continue;
        if (strcmp(norm, id_a) == 0 || strcmp(norm, id_b) == 0)
            continue;
        if (!ref_hash_insert(ht, norm, &is_new))
            return 0;
        *unique_count += is_new;
    }
    return 1;
}

static int scan_common_citers(const RefHash *ht_a,
                              RefHash *ht_b,
                              const char citers_b[][PAPER_ID_LEN],
                              int count_b,
                              const char *id_a,
                              const char *id_b,
                              CoCitationResult *out)
{
    int i;

    for (i = 0; i < count_b; i++) {
        char norm[PAPER_ID_LEN];
        int is_new = 0;

        if (!normalize_work_id_str(citers_b[i], norm, sizeof(norm)))
            continue;
        if (strcmp(norm, id_a) == 0 || strcmp(norm, id_b) == 0)
            continue;
        if (!ref_hash_insert(ht_b, norm, &is_new))
            return 0;
        if (!is_new)
            continue;
        out->unique_citers_b++;
        if (ref_hash_contains(ht_a, norm)) {
            if (out->common_count < PAPER_MAX_REFS)
                snprintf(out->co_citing_ids[out->common_count], PAPER_ID_LEN, "%s", norm);
            out->common_count++;
        }
    }
    return 1;
}

int citation_compute_cocitation(const char *paper_a_id,
                                const char citers_a[][PAPER_ID_LEN],
                                int count_a,
                                int a_complete,
                                const char *paper_b_id,
                                const char citers_b[][PAPER_ID_LEN],
                                int count_b,
                                int b_complete,
                                CoCitationResult *out)
{
    char norm_a[PAPER_ID_LEN];
    char norm_b[PAPER_ID_LEN];
    RefHash ht_a;
    RefHash ht_b;

    if (out == NULL)
        return COCITATION_UNKNOWN;
    reset_cocitation_result(out);

    if (!normalize_work_id_str(paper_a_id, norm_a, sizeof(norm_a)) ||
        !normalize_work_id_str(paper_b_id, norm_b, sizeof(norm_b)) ||
        count_a < 0 || count_b < 0 ||
        (count_a > 0 && citers_a == NULL) ||
        (count_b > 0 && citers_b == NULL)) {
        out->status = COCITATION_INVALID;
        return COCITATION_INVALID;
    }

    if (strcmp(norm_a, norm_b) == 0) {
        out->status = COCITATION_INVALID;
        return COCITATION_INVALID;
    }

    ref_hash_init(&ht_a);
    ref_hash_init(&ht_b);

    if (!populate_citer_hash(&ht_a, citers_a, count_a, norm_a, norm_b, &out->unique_citers_a) ||
        !scan_common_citers(&ht_a, &ht_b, citers_b, count_b, norm_a, norm_b, out)) {
        ref_hash_free(&ht_a);
        ref_hash_free(&ht_b);
        reset_cocitation_result(out);
        return COCITATION_UNKNOWN;
    }

    ref_hash_free(&ht_a);
    ref_hash_free(&ht_b);

    out->citers_a_complete = (a_complete != 0);
    out->citers_b_complete = (b_complete != 0);
    out->union_count = out->unique_citers_a + out->unique_citers_b - out->common_count;
    out->exact_jaccard = (out->citers_a_complete && out->citers_b_complete);

    if (out->exact_jaccard) {
        out->jaccard_pct = (out->union_count > 0)
            ? ((double)out->common_count * 100.0 / (double)out->union_count)
            : 0.0;
    } else {
        out->jaccard_pct = -1.0;
    }

    if (out->common_count > 0)
        out->status = COCITATION_YES;
    else if (out->exact_jaccard)
        out->status = COCITATION_NO;
    else
        out->status = COCITATION_UNKNOWN;

    return out->status;
}

static void collect_citers_for_paper(const char *paper_id,
                                     char out_ids[][PAPER_ID_LEN],
                                     int max_out,
                                     int *out_count,
                                     int *out_complete)
{
    static CiterFetchResult fetch_res;
    int csv_count;
    int code;
    int i;

    *out_count = 0;
    *out_complete = 0;

    csv_count = csv_load_citing_papers(paper_id, out_ids, max_out);
    *out_count = csv_count;

    printf("Fetching citing papers for %s from OpenAlex...\n", paper_id);
    code = openalex_fetch_citing_ids(paper_id, OPENALEX_MAX_CITER_PAGES, &fetch_res);
    if (code != OPENALEX_OK) {
        printf("Note: OpenAlex citing-paper fetch for %s failed (%s). Using local CSV only.\n",
               paper_id, openalex_error_message(code));
    }

    for (i = 0; i < fetch_res.count && *out_count < max_out; i++) {
        snprintf(out_ids[*out_count], PAPER_ID_LEN, "%s", fetch_res.ids[i]);
        (*out_count)++;
    }

    *out_complete = (code == OPENALEX_OK && fetch_res.api_succeeded && fetch_res.is_complete);
}

static const char *cocitation_status_label(int status)
{
    if (status == COCITATION_YES)
        return "YES";
    if (status == COCITATION_NO)
        return "NO";
    if (status == COCITATION_INVALID)
        return "INVALID";
    return "UNKNOWN";
}

static void print_cocitation_report(const char *paper_a_id,
                                    const char *paper_b_id,
                                    const CoCitationResult *res)
{
    int i;
    int printed;

    printf("\n========== CO-CITATION ANALYSIS ==========\n\n");
    printf("Paper A: %s\n", paper_a_id);
    printf("Paper B: %s\n", paper_b_id);
    printf("Co-Citation: %s\n", cocitation_status_label(res->status));
    printf("Citing Data Complete: %s\n", res->exact_jaccard ? "YES" : "NO");
    printf("Paper A Unique Citers: %d\n", res->unique_citers_a);
    printf("Paper B Unique Citers: %d\n", res->unique_citers_b);
    printf("Co-Citing Papers (Common): %d\n", res->common_count);
    printf("Union Citers: %d\n", res->union_count);
    if (res->exact_jaccard)
        printf("Jaccard Similarity: %.2f%%\n", res->jaccard_pct);
    else
        printf("Jaccard Similarity: N/A (incomplete citing data)\n");

    printed = (res->common_count < PAPER_MAX_REFS) ? res->common_count : PAPER_MAX_REFS;
    for (i = 0; i < printed; i++)
        printf("Co-Citing Paper: %s\n", res->co_citing_ids[i]);
}

int compare_papers_cocitation(const char *paper_a_id, const char *paper_b_id)
{
    static char citers_a[2 * PAPER_MAX_REFS][PAPER_ID_LEN];
    static char citers_b[2 * PAPER_MAX_REFS][PAPER_ID_LEN];
    char id_a[PAPER_ID_LEN];
    char id_b[PAPER_ID_LEN];
    CoCitationResult res;
    int count_a = 0;
    int count_b = 0;
    int comp_a = 0;
    int comp_b = 0;

    reset_cocitation_result(&res);
    if (!normalize_work_id_str(paper_a_id, id_a, sizeof(id_a)) ||
        !normalize_work_id_str(paper_b_id, id_b, sizeof(id_b))) {
        printf("Invalid paper ID.\n");
        return 0;
    }

    if (strcmp(id_a, id_b) == 0) {
        printf("Error: Paper A and Paper B must have distinct Work IDs.\n");
        return 0;
    }

    collect_citers_for_paper(id_a, citers_a, 2 * PAPER_MAX_REFS, &count_a, &comp_a);
    collect_citers_for_paper(id_b, citers_b, 2 * PAPER_MAX_REFS, &count_b, &comp_b);

    citation_compute_cocitation(id_a, (const char (*)[PAPER_ID_LEN])citers_a, count_a, comp_a,
                                id_b, (const char (*)[PAPER_ID_LEN])citers_b, count_b, comp_b,
                                &res);

    print_cocitation_report(id_a, id_b, &res);
    return (res.status == COCITATION_YES || res.status == COCITATION_NO);
}
