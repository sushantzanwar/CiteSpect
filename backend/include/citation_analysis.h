#ifndef CITATION_ANALYSIS_H
#define CITATION_ANALYSIS_H

#include "paper.h"

#define CITATION_SELF 0
#define CITATION_CROSS 1
#define CITATION_UNKNOWN 2

#define RECIPROCAL_CONFIRMED 0
#define RECIPROCAL_NOT 1
#define RECIPROCAL_UNKNOWN 2

#define COUPLING_YES 0
#define COUPLING_NO 1
#define COUPLING_UNKNOWN 2

typedef struct {
    int status;
    int unique_refs_a;
    int unique_refs_b;
    int common_count;
    int union_count;
    double jaccard_pct;
    char common_refs[PAPER_MAX_REFS][PAPER_ID_LEN];
} CouplingResult;

int citation_check_reciprocal(const Paper *cited, const char *citing_id, int refs_available);

int citation_is_duplicate(char cited_ids[][PAPER_ID_LEN], int index);

int citation_compute_coupling(const Paper *paper_a, int refs_a_available,
                              const Paper *paper_b, int refs_b_available,
                              CouplingResult *out);

int compare_papers_coupling(const char *paper_a_id, const char *paper_b_id);

int classify_citation(const Paper *citing, const Paper *cited, int shared_index[], int *shared_count);

int analyze_paper_citations(const char *citing_id);

#endif
